//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "c7841.h"
#include <string.h>
#include <unistd.h>

#include <stdio.h>

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//глобальные переменные
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//прототипы функций
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
const struct sigevent *CANInterruptProcedure(void *area,int size);//обработчик прерывания CAN

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
C7841::C7841(uint32_t device_index)
{
 DeviceIndex=device_index;
 PCI_Handle=-1;	
 ISR_CANInterruptID=-1;
 DeviceHandle=NULL;
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
C7841::~C7841()
{
 Release();	
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//получить, допустим ли такой номер канала
//----------------------------------------------------------------------------------------------------
bool C7841::IsChannelValid(uint32_t channel)
{
 if (channel>=CAN_CHANNEL_AMOUNT) return(false);
 return(true);	
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//найти плату на шине PCI и инициализировать
//----------------------------------------------------------------------------------------------------
bool C7841::Init(void)
{
 //ищем устройство
 memset(&PCI_Dev_Info,0,sizeof(pci_dev_info));
 PCI_Handle=pci_attach(0);
 if (PCI_Handle<0) return(false);
 int rescan=pci_rescan_bus();
 if (rescan!=PCI_SUCCESS) return(false);
 PCI_Dev_Info.VendorId=CAN7841_VendorID;
 PCI_Dev_Info.DeviceId=CAN7841_DeviceID;
 DeviceHandle=pci_attach_device(NULL,0,DeviceIndex,&PCI_Dev_Info);
 if (DeviceHandle==NULL) return(false);
 if((pci_attach_device(DeviceHandle,PCI_INIT_ALL,DeviceIndex,&PCI_Dev_Info))==0)
 {
  pci_detach_device(DeviceHandle); 	
  DeviceHandle=NULL;
  return(false);
 }
 //printf("IRQ:%i\r\n",PCI_Dev_Info.Irq); 
 
 //подключаем порты 
 for(uint8_t bar=0;bar<6;bar++)
 {
  uint32_t addr=PCI_Dev_Info.CpuBaseAddress[bar]; 	
  if (addr==0x00000000) continue;
  if (!(PCI_IS_IO(addr))) continue;
  //физический адрес
  uint64_t p_port=PCI_IO_ADDR(addr);
  uint64_t size=PCI_Dev_Info.BaseAddressSize[bar];
  //создаём класс работы с адресами
  vector_CIOControl.push_back(CIOControl(p_port,size));
  CIOControl &cIOControl=vector_CIOControl[0];
  if (cIOControl.IsPort(0))
  {
   //printf("Found addr:0x%x ok.\r\n",p_port);
  }
  else
  {
   //printf("Found addr:0x%x error!\r\n",p_port);
  }
 } 
 if (vector_CIOControl.size()!=3)
 {    	
  Release();
  return(false); 	
 } 
 return(true);
}
//----------------------------------------------------------------------------------------------------
//освободить ресурсы
//----------------------------------------------------------------------------------------------------
void C7841::Release(void)
{	
 vector_CIOControl.clear(); 
 CANInterruptDetach();
 if (DeviceHandle!=NULL) pci_detach_device(DeviceHandle);
 if (PCI_Handle>=0) pci_detach(PCI_Handle);
 PCI_Handle=-1;
 DeviceHandle=NULL;
}
//----------------------------------------------------------------------------------------------------
//получить, подключена ли плата
//----------------------------------------------------------------------------------------------------
bool C7841::IsEnabled(void)
{
 if (DeviceHandle==NULL) return(false);
 return(true);
}

//----------------------------------------------------------------------------------------------------
//прочесть регистр
//----------------------------------------------------------------------------------------------------
uint8_t C7841::ReadRegister(uint64_t reg)
{
 if (IsEnabled()==false) return(0);
 return(vector_CIOControl[1].In8(reg)); 	
}
//----------------------------------------------------------------------------------------------------
//записать регистр
//----------------------------------------------------------------------------------------------------
bool C7841::WriteRegister(uint64_t reg,uint8_t value)
{
 if (IsEnabled()==false) return(false);
 return(vector_CIOControl[1].Out8(reg,value)); 	
}

//----------------------------------------------------------------------------------------------------
//настроить порт
//----------------------------------------------------------------------------------------------------
void C7841::CANConfig(uint32_t channel,const C7841CANChannel &c7841CANChannel_Set)
{
 if (IsChannelValid(channel)==false) return;	
 if (IsEnabled()==false) return;
 c7841CANChannel[channel]=c7841CANChannel_Set;
 
 C7841CANChannel &c7841CANChannel_local=c7841CANChannel[channel]; 
 
 uint8_t tmp;
 uint64_t offset=channel*128;
 //запрещаем прерывания
 tmp=ReadRegister(offset+4);
 WriteRegister(offset+4,0x00);
 //выполняем сброс канала
 WriteRegister(offset+0,0x01);
 //настраиваем CBP=1, TX1 и Clk отключены 
 WriteRegister(offset+31,0xC8);
 
 //стандартный режим с одной маской
 uint32_t arbitration=c7841CANChannel_local.Arbitration;
 uint32_t arbitration_mask=c7841CANChannel_local.ArbitrationMask;
 
 if (c7841CANChannel_local.IsExtendedMode()==false)//обычный режим
 {
  //задаём арбитраж
  WriteRegister(offset+16,static_cast<uint8_t>(arbitration>>3));
  WriteRegister(offset+17,(static_cast<uint8_t>(arbitration<<5))|0x10);
  WriteRegister(offset+18,0x00);
  WriteRegister(offset+19,0x00);
  //задаём маску
  WriteRegister(offset+20,static_cast<uint8_t>(arbitration_mask>>3));
  WriteRegister(offset+21,(static_cast<uint8_t>(arbitration_mask<<5))|0x10);
  WriteRegister(offset+22,0xff);
  WriteRegister(offset+23,0xff);
 }
 else//расширенный режим
 {
  //задаём арбитраж 	
  WriteRegister(offset+16,static_cast<uint8_t>(arbitration>>21));
  WriteRegister(offset+17,static_cast<uint8_t>(arbitration>>13));
  WriteRegister(offset+18,static_cast<uint8_t>(arbitration>>5));
  WriteRegister(offset+19,static_cast<uint8_t>(arbitration<<3));
  //задаём маску
  WriteRegister(offset+20,static_cast<uint8_t>(arbitration_mask>>21));
  WriteRegister(offset+21,static_cast<uint8_t>(arbitration_mask>>13));
  WriteRegister(offset+22,static_cast<uint8_t>(arbitration_mask>>5));
  WriteRegister(offset+23,static_cast<uint8_t>(arbitration_mask<<3)|0x04);//добавляем RTR
 }
 //настраиваем скорость
 if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_125KBS)==true)
 {
  WriteRegister(offset+6,0x03);
  WriteRegister(offset+7,0x1c);
 }
 if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_250KBS)==true)
 {
  WriteRegister(offset+6,0x01);
  WriteRegister(offset+7,0x1c);
 }
 if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_500KBS)==true)
 {
  WriteRegister(offset+6,0x00);
  WriteRegister(offset+7,0x1c);
 }
 if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_1MBS)==true)
 {
  WriteRegister(offset+6,0x00);
  WriteRegister(offset+7,0x14);
 }
 if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_USER)==true)
 {
  C7841UserSpeed &c7841UserSpeed=c7841CANChannel_local.c7841UserSpeed;
 	
  WriteRegister(offset+6,c7841UserSpeed.BRP|(c7841UserSpeed.SJW<<6));
  WriteRegister(offset+7,(c7841UserSpeed.SAM<<7)|(c7841UserSpeed.TSeg2<<4)|c7841UserSpeed.TSeg1);
 }
 //настраиваем регистр управления: нормальный режим, тяни-толкай
 WriteRegister(offset+8,0xfa);
 //разрешаем прерывания по приёму и передаче
 WriteRegister(offset+0,0x08);
 WriteRegister(offset+4,tmp|0x03); 
}

//----------------------------------------------------------------------------------------------------
//отправить пакет
//----------------------------------------------------------------------------------------------------
bool C7841::SendPackage(uint32_t channel,const C7841CANPackage &c7841CANPackage)
{
 if (IsChannelValid(channel)==false) return(false);
 if (IsEnabled()==false) return(false);
 uint64_t offset=channel*128;
 usleep(1000);
 
 C7841CANChannel &c7841CANChannel_local=c7841CANChannel[channel]; 
 //узнаем состояние передачи
 uint8_t v=ReadRegister(offset+2);
 if ((v&0x04)==0) return(false);//передача невозможна
 uint64_t data_begin=0;
 if (c7841CANChannel_local.ExtendedMode==false)//обычный режим
 {
  data_begin=19; 	
  if (c7841CANPackage.RTR==false) WriteRegister(offset+16,c7841CANPackage.Length&0x0f);
                             else WriteRegister(offset+16,(c7841CANPackage.Length&0x0f)|0x40);
  WriteRegister(offset+18,c7841CANPackage.Arbitration<<5);
  WriteRegister(offset+17,c7841CANPackage.Arbitration>>3);  
 }
 else//расширенный арбитраж
 {
  data_begin=21;
  if (c7841CANPackage.RTR==false) WriteRegister(offset+16,(c7841CANPackage.Length&0x0f)|0x80);
                             else WriteRegister(offset+16,(c7841CANPackage.Length&0x0f)|0x40);
  WriteRegister(offset+17,c7841CANPackage.Arbitration>>21);
  WriteRegister(offset+18,c7841CANPackage.Arbitration>>13);  
  WriteRegister(offset+19,c7841CANPackage.Arbitration>>5);
  WriteRegister(offset+20,c7841CANPackage.Arbitration<<3);
 }
 for(uint8_t n=0;n<c7841CANPackage.Length;n++)
 {
  WriteRegister(offset+data_begin+n,c7841CANPackage.Data[n]);	
 } 
 //запускаем передачу 
 WriteRegister(offset+1,0x01);
 return(true);
}

//----------------------------------------------------------------------------------------------------
//получить пакеты
//----------------------------------------------------------------------------------------------------
bool C7841::GetPackage(uint32_t channel,std::vector<C7841CANPackage> &vector_C7841CANPackage)
{
 vector_C7841CANPackage.clear();
 if (IsChannelValid(channel)==false) return(false);
 if (IsEnabled()==false) return(false);
 C7841CANPackage c7841CANPackage;
 uint64_t offset=channel*128;
 uint8_t flag=ReadRegister(offset+3);
 if (flag&0x01)//RIF (приём окончен)
 {
  while(1) 
  {
   uint8_t data=ReadRegister(offset+2);
   if ((data&0x01)!=0x01) break;
   data=ReadRegister(offset+16);
   c7841CANPackage.Length=data&0x0f;
   if (data&0x40) c7841CANPackage.RTR=true;
             else c7841CANPackage.RTR=false;
   uint64_t data_begin=0;             
   if (data&0x80)//расширенный режим
   {
	data_begin=21;
    data=ReadRegister(offset+17);
    c7841CANPackage.Arbitration=data;
    c7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+18);
    c7841CANPackage.Arbitration|=data;
    c7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+19);
    c7841CANPackage.Arbitration|=data;
    c7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+20);
    c7841CANPackage.Arbitration|=data;
    c7841CANPackage.Arbitration>>=3;
   }
   else 
   {
    data_begin=19;
    data=ReadRegister(offset+17);
    c7841CANPackage.Arbitration=data;
    c7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+18);
    c7841CANPackage.Arbitration=data;
    c7841CANPackage.Arbitration>>=5;
   }
   if (c7841CANPackage.Length>8) c7841CANPackage.Length=8;
   for(uint8_t n=0;n<c7841CANPackage.Length;n++)
   {
    data=ReadRegister(offset+data_begin+n);
    c7841CANPackage.Data[n]=data;
   }
   vector_C7841CANPackage.push_back(c7841CANPackage);
   //очищаем буфер приёма
   WriteRegister(offset+1,0x04);
  }
  return(true);
 }
 return(false);
}

//----------------------------------------------------------------------------------------------------
//разрешить приём данных
//----------------------------------------------------------------------------------------------------
void C7841::EnableReceive(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;	
 uint8_t byte=ReadRegister(offset+4);
 WriteRegister(offset+4,byte|0x01);
}
//----------------------------------------------------------------------------------------------------
//запретить приём данных
//----------------------------------------------------------------------------------------------------
void C7841::DisableReceive(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;	
 uint8_t byte=ReadRegister(offset+4);
 WriteRegister(offset+4,byte&(0xff^0x01));
}
//----------------------------------------------------------------------------------------------------
//очистить ошибки
//----------------------------------------------------------------------------------------------------
void C7841::ClearOverrun(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;
 //очищаем ошибки
 WriteRegister(offset+1,0x08);
 //разрешаем прерывания ошибки и приёма данных
 uint8_t byte=ReadRegister(offset+4); 
 WriteRegister(offset+4,byte|0x09);
}
//----------------------------------------------------------------------------------------------------
//очистить буфер приёма
//----------------------------------------------------------------------------------------------------
void C7841::ClearReceiveBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;
 while(ReadRegister(offset+29)!=0)
 {
  WriteRegister(offset+1,0x04); 	
 }	
}
//----------------------------------------------------------------------------------------------------
//очистить буфер передачи
//----------------------------------------------------------------------------------------------------
void C7841::ClearTransmitBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;
 WriteRegister(offset+1,0x02);
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма арбитража
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetArbitrationLostBit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 if (IsEnabled()==false) return(0);
 uint64_t offset=channel*128;
 return(ReadRegister(offset+11));
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetReceiveErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 if (IsEnabled()==false) return(0);
 uint64_t offset=channel*128;
 return(ReadRegister(offset+14));
}
//----------------------------------------------------------------------------------------------------
//получить количество ошибок передачи
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetTransmitErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 if (IsEnabled()==false) return(0);
 uint64_t offset=channel*128;
 return(ReadRegister(offset+15));
}
//----------------------------------------------------------------------------------------------------
//получить состояние канала
//----------------------------------------------------------------------------------------------------
S7841StatusReg C7841::GetChannelStatus(uint32_t channel)
{
 S7841StatusReg s7841StatusReg; 
 if (IsChannelValid(channel)==false) return(s7841StatusReg);
 if (IsEnabled()==false) return(s7841StatusReg);
 uint64_t offset=channel*128; 
 *(reinterpret_cast<uint8_t*>(&s7841StatusReg))=ReadRegister(offset+2);
 return(s7841StatusReg);
}
//----------------------------------------------------------------------------------------------------
//получить ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetErrorWarningLimit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 if (IsEnabled()==false) return(0);
 uint64_t offset=channel*128;
 return(ReadRegister(offset+13));	
}
//----------------------------------------------------------------------------------------------------
//задать ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
void C7841::SetErrorWarningLimit(uint32_t channel,uint8_t value)
{
 if (IsChannelValid(channel)==false) return;
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;
 uint8_t byte=ReadRegister(offset+0);  	
 //входим в режим сброса
 WriteRegister(offset+0,0);
 WriteRegister(offset+13,value);
 WriteRegister(offset+0,byte);
}
//----------------------------------------------------------------------------------------------------
//получить код ошибки
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetErrorCode(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 if (IsEnabled()==false) return(0);
 uint64_t offset=channel*128;
 return(ReadRegister(offset+12));
}
//----------------------------------------------------------------------------------------------------
//сбросить флаг прерывания
//----------------------------------------------------------------------------------------------------
void C7841::ClearIRQ(void)
{
 if (IsEnabled()==false) return;
 uint64_t offset=0*128;
 ReadRegister(offset+3);//сброс производится чтением регистра
 offset=1*128;
 ReadRegister(offset+3);//сброс производится чтением регистра
 InterruptUnmask(GetIRQ(),ISR_CANInterruptID);
}
//----------------------------------------------------------------------------------------------------
//получить номер прерывания
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetIRQ(void)
{
 if (IsEnabled()==false) return(0);
 return(PCI_Dev_Info.Irq);
}
//----------------------------------------------------------------------------------------------------
//подключиться к прерыванию
//----------------------------------------------------------------------------------------------------
bool C7841::CANInterruptAttach(void)
{
 CANInterruptDetach();
 //подключаем прерывание
 SIGEV_INTR_INIT(&CANInterruptEvent);//настраиваем событие
 ISR_CANInterruptID=InterruptAttachEvent(GetIRQ(),&CANInterruptEvent,_NTO_INTR_FLAGS_END);
 if (ISR_CANInterruptID<0) return(false);
 return(true);
}
//----------------------------------------------------------------------------------------------------
//отключиться от прерывания
//----------------------------------------------------------------------------------------------------
void C7841::CANInterruptDetach(void)
{
 if (ISR_CANInterruptID>=0) InterruptDetach(ISR_CANInterruptID);//отключим прерывание
 ISR_CANInterruptID=-1;
}