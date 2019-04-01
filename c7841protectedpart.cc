//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "c7841protectedpart.h"
#include <string.h>

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
C7841ProtectedPart::C7841ProtectedPart(uint32_t receiver_buffer_size,uint32_t transmitter_buffer_size)
{
 ISR_CANInterruptID=-1;
 DeviceHandle=NULL;
 cRingBuffer_Receiver_Ptr.Set(new CRingBuffer<C7841CANPackage>(receiver_buffer_size));
 cRingBuffer_Transmitter_Ptr.Set(new CRingBuffer<C7841CANPackage>(transmitter_buffer_size));
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
C7841ProtectedPart::~C7841ProtectedPart()
{
 cRingBuffer_Receiver_Ptr.Release();
 cRingBuffer_Transmitter_Ptr.Release();
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//обработчик прерывания канала
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::OnInterruptChannel(uint32_t channel)
{
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;
 uint8_t flag=ReadRegister(offset+3);
 if (flag&0x01) ReceivePackageChannel(channel);//получаем пакеты с канала
 if (flag&0x02) TransmittPackageChannel(channel);//отправляем пакеты в канал	
}

//----------------------------------------------------------------------------------------------------
//получить пакеты с канала
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::ReceivePackageChannel(uint32_t channel)
{
 if (IsEnabled()==false) return;
 C7841CANPackage c7841CANPackage;
 uint64_t offset=channel*128;
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
   c7841CANPackage.ExtendedMode=true;
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
   c7841CANPackage.ExtendedMode=false;
  }
  if (c7841CANPackage.Length>8) c7841CANPackage.Length=8;
  for(uint8_t n=0;n<c7841CANPackage.Length;n++)
  {
   data=ReadRegister(offset+data_begin+n);
   c7841CANPackage.Data[n]=data;
  }
  c7841CANPackage.ChannelIndex=channel;
  cRingBuffer_Receiver_Ptr.Get()->Push(c7841CANPackage);
  //очищаем буфер приёма
  WriteRegister(offset+1,0x04);
 }
}
//----------------------------------------------------------------------------------------------------
//отправить пакеты в канал
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::TransmittPackageChannel(uint32_t channel)
{
 if (IsEnabled()==false) return;
 //читаем список для передачи
 C7841CANPackage c7841CANPackage;
 if (cRingBuffer_Transmitter_Ptr.Get()->Pop(c7841CANPackage)==false) return;//нечего передавать
 //запускаем передачу
 TransmittPackage(c7841CANPackage);
}
//----------------------------------------------------------------------------------------------------
//отправить пакет
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::TransmittPackage(const C7841CANPackage &c7841CANPackage)
{
 if (IsEnabled()==false) return(false);
 uint32_t channel=c7841CANPackage.ChannelIndex;
 uint64_t offset=channel*128;
 //узнаем состояние передачи
 uint8_t v=ReadRegister(offset+2);
 if ((v&0x04)==0) return(false);//передача невозможна
 uint64_t data_begin=0;
 if (c7841CANPackage.ExtendedMode==false)//обычный режим
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


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//сбросить флаг прерывания
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::ClearIRQ(void)
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
uint8_t C7841ProtectedPart::GetIRQ(void)
{
 if (IsEnabled()==false) return(0);
 return(PCI_Dev_Info.Irq);
}
//----------------------------------------------------------------------------------------------------
//прочесть регистр
//----------------------------------------------------------------------------------------------------
uint8_t C7841ProtectedPart::ReadRegister(uint64_t reg)
{
 if (IsEnabled()==false) return(0);
 return(vector_CIOControl[1].In8(reg)); 	
}
//----------------------------------------------------------------------------------------------------
//записать регистр
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::WriteRegister(uint64_t reg,uint8_t value)
{
 if (IsEnabled()==false) return(false);
 return(vector_CIOControl[1].Out8(reg,value)); 	
}
//----------------------------------------------------------------------------------------------------
//получить, подключена ли плата
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::IsEnabled(void)
{
 if (DeviceHandle==NULL) return(false);
 return(true);
}
//----------------------------------------------------------------------------------------------------
//подключить прерывание
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::CANInterruptAttach(sigevent *CANInterruptEvent)
{
 //подключаем прерывание
 SIGEV_INTR_INIT(CANInterruptEvent);//настраиваем событие  
 ISR_CANInterruptID=InterruptAttachEvent(GetIRQ(),CANInterruptEvent,_NTO_INTR_FLAGS_END);
 if (ISR_CANInterruptID<0) return(false);
 return(true);
}
//----------------------------------------------------------------------------------------------------
//отключить прерывание
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::CANInterruptDetach(void)
{
 if (ISR_CANInterruptID>=0) InterruptDetach(ISR_CANInterruptID);//отключим прерывание
 ISR_CANInterruptID=-1; 
}

//----------------------------------------------------------------------------------------------------
//получить, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::IsExitThread(void)
{
 return(ExitThread);	
}
//----------------------------------------------------------------------------------------------------
//задать, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::SetExitThread(bool state)
{
 ExitThread=state;
}
//----------------------------------------------------------------------------------------------------
//инициализировать PCI
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::InitPCI(uint32_t vendor_id,uint32_t device_id,uint32_t device_index)
{
 memset(&PCI_Dev_Info,0,sizeof(pci_dev_info));  
 PCI_Dev_Info.VendorId=vendor_id;
 PCI_Dev_Info.DeviceId=device_id;
 DeviceHandle=pci_attach_device(NULL,0,device_index,&PCI_Dev_Info);
 if (DeviceHandle==NULL) return(false);
 if ((pci_attach_device(DeviceHandle,PCI_INIT_ALL,device_index,&PCI_Dev_Info))==0)
 {
  pci_detach_device(DeviceHandle); 	
  DeviceHandle=NULL;
  return(false);
 }
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
 } 
 if (vector_CIOControl.size()!=3) return(false);
 return(true); 
}
//----------------------------------------------------------------------------------------------------
//освободить PCI
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::ReleasePCI(void)
{
 vector_CIOControl.clear(); 
 if (DeviceHandle!=NULL) pci_detach_device(DeviceHandle);
 DeviceHandle=NULL;
}
//----------------------------------------------------------------------------------------------------
//настроить порт
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::CANConfig(uint32_t channel,const C7841CANChannel &c7841CANChannel_Set)
{
 if (IsEnabled()==false) return(false);
 const C7841CANChannel &c7841CANChannel_local=c7841CANChannel_Set; 
  
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
  const C7841UserSpeed &c7841UserSpeed=c7841CANChannel_local.c7841UserSpeed;
  	
  WriteRegister(offset+6,c7841UserSpeed.BRP|(c7841UserSpeed.SJW<<6));
  WriteRegister(offset+7,(c7841UserSpeed.SAM<<7)|(c7841UserSpeed.TSeg2<<4)|c7841UserSpeed.TSeg1);
 }
 //настраиваем регистр управления: нормальный режим, тяни-толкай
 WriteRegister(offset+8,0xfa);
 //разрешаем прерывания по приёму и передаче
 WriteRegister(offset+0,0x08);
 WriteRegister(offset+4,tmp|0x03); 
 return(true);
}
//----------------------------------------------------------------------------------------------------
//получить принятый пакет
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::GetReceivedPackage(C7841CANPackage &c7841CANPackage)
{
 return(cRingBuffer_Receiver_Ptr.Get()->Pop(c7841CANPackage));
}
//----------------------------------------------------------------------------------------------------
//добавить пакет для отправки
//----------------------------------------------------------------------------------------------------
bool C7841ProtectedPart::SendPackage(const C7841CANPackage &c7841CANPackage)
{	
 cRingBuffer_Transmitter_Ptr.Get()->Push(c7841CANPackage);
 return(true);
}
//----------------------------------------------------------------------------------------------------
//обработчик прерывания
//----------------------------------------------------------------------------------------------------
void C7841ProtectedPart::OnInterrupt(uint32_t channel_amount)
{
 for(uint32_t n=0;n<channel_amount;n++) OnInterruptChannel(n);
 ClearIRQ();
}


