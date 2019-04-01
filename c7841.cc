//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "c7841.h"
#include "craiicmutex.h"
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
 sProtectedVariables.ISR_CANInterruptID=-1;
 sProtectedVariables.DeviceHandle=NULL;
 cThread_Ptr.Set(NULL);
 sProtectedVariables.cRingBuffer_Ptr.Set(new CRingBuffer<C7841CANPackage>(RECEIVE_RING_BUFFER_SIZE));
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
C7841::~C7841()
{
 Release();
 sProtectedVariables.cRingBuffer_Ptr.Set(NULL);	
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

//----------------------------------------------------------------------------------------------------
//сбросить флаг прерывания
//----------------------------------------------------------------------------------------------------
void C7841::Unsafe_ClearIRQ(void)
{
 if (Unsafe_IsEnabled()==false) return;
 uint64_t offset=0*128;
 Unsafe_ReadRegister(offset+3);//сброс производится чтением регистра
 offset=1*128;
 Unsafe_ReadRegister(offset+3);//сброс производится чтением регистра
 InterruptUnmask(Unsafe_GetIRQ(),sProtectedVariables.ISR_CANInterruptID);
}
//----------------------------------------------------------------------------------------------------
//получить номер прерывания
//----------------------------------------------------------------------------------------------------
uint8_t C7841::Unsafe_GetIRQ(void)
{
 if (Unsafe_IsEnabled()==false) return(0);
 return(sProtectedVariables.PCI_Dev_Info.Irq);
}
//----------------------------------------------------------------------------------------------------
//подключиться к прерыванию
//----------------------------------------------------------------------------------------------------
bool C7841::CANInterruptAttach(void)
{
 CANInterruptDetach(); 
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  //подключаем прерывание
  SIGEV_INTR_INIT(&CANInterruptEvent);//настраиваем событие
  sProtectedVariables.ISR_CANInterruptID=InterruptAttachEvent(Unsafe_GetIRQ(),&CANInterruptEvent,_NTO_INTR_FLAGS_END);
  if (sProtectedVariables.ISR_CANInterruptID<0) return(false);
 }
 return(true);
}
//----------------------------------------------------------------------------------------------------
//отключиться от прерывания
//----------------------------------------------------------------------------------------------------
void C7841::CANInterruptDetach(void)
{
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (sProtectedVariables.ISR_CANInterruptID>=0) InterruptDetach(sProtectedVariables.ISR_CANInterruptID);//отключим прерывание
  sProtectedVariables.ISR_CANInterruptID=-1;
 }
}
//----------------------------------------------------------------------------------------------------
//получить, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
bool C7841::IsExitThread(void)
{
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  return(sProtectedVariables.ExitThread);
 } 
}
//----------------------------------------------------------------------------------------------------
//задать, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
void C7841::SetExitThread(bool state)
{
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  sProtectedVariables.ExitThread=state;
 }
}
//----------------------------------------------------------------------------------------------------
//прочесть регистр
//----------------------------------------------------------------------------------------------------
uint8_t C7841::Unsafe_ReadRegister(uint64_t reg)
{
 if (Unsafe_IsEnabled()==false) return(0);
 return(sProtectedVariables.vector_CIOControl[1].In8(reg)); 	
}
//----------------------------------------------------------------------------------------------------
//записать регистр
//----------------------------------------------------------------------------------------------------
bool C7841::Unsafe_WriteRegister(uint64_t reg,uint8_t value)
{
 if (Unsafe_IsEnabled()==false) return(false);
 return(sProtectedVariables.vector_CIOControl[1].Out8(reg,value)); 	
}
//----------------------------------------------------------------------------------------------------
//запустить поток
//----------------------------------------------------------------------------------------------------
void C7841::StartThread(void)
{
 StopThread();	
 SetExitThread(false);
 cThread_Ptr.Set(new CThread(ThreadFunction,THREAD_PRIORITY,this)); 
}
//----------------------------------------------------------------------------------------------------
//остановить поток
//----------------------------------------------------------------------------------------------------
void C7841::StopThread(void)
{
 SetExitThread(true);
 if (cThread_Ptr.Get()!=NULL)
 {
  cThread_Ptr.Get()->Join();
  cThread_Ptr.Set(NULL);
 }
}
//----------------------------------------------------------------------------------------------------
//получить, подключена ли плата
//----------------------------------------------------------------------------------------------------
bool C7841::Unsafe_IsEnabled(void)
{
 if (sProtectedVariables.DeviceHandle==NULL) return(false);
 return(true);
}

//----------------------------------------------------------------------------------------------------
//выполнить приём данных
//----------------------------------------------------------------------------------------------------
void C7841::ReceivePackage(void)
{
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  for(uint32_t n=0;n<CAN_CHANNEL_AMOUNT;n++) Unsafe_ReceivePackageChannel(n);	
  Unsafe_ClearIRQ();
 }
}


//----------------------------------------------------------------------------------------------------
//получить пакеты с канала
//----------------------------------------------------------------------------------------------------
void C7841::Unsafe_ReceivePackageChannel(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 if (Unsafe_IsEnabled()==false) return;
 C7841CANPackage c7841CANPackage;
 uint64_t offset=channel*128;
 uint8_t flag=Unsafe_ReadRegister(offset+3);
 if (flag&0x01)//RIF (приём окончен)
 {
  while(1) 
  {
   uint8_t data=Unsafe_ReadRegister(offset+2);
   if ((data&0x01)!=0x01) break;
   data=Unsafe_ReadRegister(offset+16);
   c7841CANPackage.Length=data&0x0f;
   if (data&0x40) c7841CANPackage.RTR=true;
             else c7841CANPackage.RTR=false;
   uint64_t data_begin=0;             
   if (data&0x80)//расширенный режим
   {
	data_begin=21;
    data=Unsafe_ReadRegister(offset+17);
    c7841CANPackage.Arbitration=data;
    c7841CANPackage.Arbitration<<=8;
    data=Unsafe_ReadRegister(offset+18);
    c7841CANPackage.Arbitration|=data;
    c7841CANPackage.Arbitration<<=8;
    data=Unsafe_ReadRegister(offset+19);
    c7841CANPackage.Arbitration|=data;
    c7841CANPackage.Arbitration<<=8;
    data=Unsafe_ReadRegister(offset+20);
    c7841CANPackage.Arbitration|=data;
    c7841CANPackage.Arbitration>>=3;
   }
   else 
   {
    data_begin=19;
    data=Unsafe_ReadRegister(offset+17);
    c7841CANPackage.Arbitration=data;
    c7841CANPackage.Arbitration<<=8;
    data=Unsafe_ReadRegister(offset+18);
    c7841CANPackage.Arbitration=data;
    c7841CANPackage.Arbitration>>=5;
   }
   if (c7841CANPackage.Length>8) c7841CANPackage.Length=8;
   for(uint8_t n=0;n<c7841CANPackage.Length;n++)
   {
    data=Unsafe_ReadRegister(offset+data_begin+n);
    c7841CANPackage.Data[n]=data;
   }
   c7841CANPackage.ChannelIndex=channel;
   sProtectedVariables.cRingBuffer_Ptr.Get()->Push(c7841CANPackage);
   //очищаем буфер приёма
   Unsafe_WriteRegister(offset+1,0x04);
  }
 }
}



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//найти плату на шине PCI и инициализировать
//----------------------------------------------------------------------------------------------------
bool C7841::Init(void)
{
 StopThread();	
 //подключаем порты 
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  //ищем устройство
  memset(&sProtectedVariables.PCI_Dev_Info,0,sizeof(pci_dev_info));
  PCI_Handle=pci_attach(0);
  if (PCI_Handle<0) return(false);
  int rescan=pci_rescan_bus();
  if (rescan!=PCI_SUCCESS) return(false);
  sProtectedVariables.PCI_Dev_Info.VendorId=CAN7841_VendorID;
  sProtectedVariables.PCI_Dev_Info.DeviceId=CAN7841_DeviceID;
  sProtectedVariables.DeviceHandle=pci_attach_device(NULL,0,DeviceIndex,&sProtectedVariables.PCI_Dev_Info);
  if (sProtectedVariables.DeviceHandle==NULL) return(false);
  if((pci_attach_device(sProtectedVariables.DeviceHandle,PCI_INIT_ALL,DeviceIndex,&sProtectedVariables.PCI_Dev_Info))==0)
  {
   pci_detach_device(sProtectedVariables.DeviceHandle); 	
   sProtectedVariables.DeviceHandle=NULL;
   return(false);
  }
  for(uint8_t bar=0;bar<6;bar++)
  {
   uint32_t addr=sProtectedVariables.PCI_Dev_Info.CpuBaseAddress[bar]; 	
   if (addr==0x00000000) continue;
   if (!(PCI_IS_IO(addr))) continue;
   //физический адрес
   uint64_t p_port=PCI_IO_ADDR(addr);
   uint64_t size=sProtectedVariables.PCI_Dev_Info.BaseAddressSize[bar];
   //создаём класс работы с адресами
   sProtectedVariables.vector_CIOControl.push_back(CIOControl(p_port,size));
  } 
  if (sProtectedVariables.vector_CIOControl.size()!=3)
  {
   cRAIICMutex.Unlock();
   Release();
   return(false); 	
  } 
 }
 //запускаем поток
 StartThread();
 return(true);
}
//----------------------------------------------------------------------------------------------------
//освободить ресурсы
//----------------------------------------------------------------------------------------------------
void C7841::Release(void)
{	
 StopThread();
 CANInterruptDetach();
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  sProtectedVariables.vector_CIOControl.clear(); 
  if (sProtectedVariables.DeviceHandle!=NULL) pci_detach_device(sProtectedVariables.DeviceHandle);
  sProtectedVariables.DeviceHandle=NULL;
 }
 if (PCI_Handle>=0) pci_detach(PCI_Handle);
 PCI_Handle=-1; 
}
//----------------------------------------------------------------------------------------------------
//настроить порт
//----------------------------------------------------------------------------------------------------
void C7841::CANConfig(uint32_t channel,const C7841CANChannel &c7841CANChannel_Set)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return;
  c7841CANChannel[channel]=c7841CANChannel_Set;
  
  C7841CANChannel &c7841CANChannel_local=c7841CANChannel[channel]; 
  
  uint8_t tmp;
  uint64_t offset=channel*128;
  //запрещаем прерывания
  tmp=Unsafe_ReadRegister(offset+4);
  Unsafe_WriteRegister(offset+4,0x00);
  //выполняем сброс канала
  Unsafe_WriteRegister(offset+0,0x01);
  //настраиваем CBP=1, TX1 и Clk отключены 
  Unsafe_WriteRegister(offset+31,0xC8);
  
  //стандартный режим с одной маской
  uint32_t arbitration=c7841CANChannel_local.Arbitration;
  uint32_t arbitration_mask=c7841CANChannel_local.ArbitrationMask;
  
  if (c7841CANChannel_local.IsExtendedMode()==false)//обычный режим
  {
   //задаём арбитраж
   Unsafe_WriteRegister(offset+16,static_cast<uint8_t>(arbitration>>3));
   Unsafe_WriteRegister(offset+17,(static_cast<uint8_t>(arbitration<<5))|0x10);
   Unsafe_WriteRegister(offset+18,0x00);
   Unsafe_WriteRegister(offset+19,0x00);
   //задаём маску
   Unsafe_WriteRegister(offset+20,static_cast<uint8_t>(arbitration_mask>>3));
   Unsafe_WriteRegister(offset+21,(static_cast<uint8_t>(arbitration_mask<<5))|0x10);
   Unsafe_WriteRegister(offset+22,0xff);
   Unsafe_WriteRegister(offset+23,0xff);
  }
  else//расширенный режим
  {
   //задаём арбитраж 	
   Unsafe_WriteRegister(offset+16,static_cast<uint8_t>(arbitration>>21));
   Unsafe_WriteRegister(offset+17,static_cast<uint8_t>(arbitration>>13));
   Unsafe_WriteRegister(offset+18,static_cast<uint8_t>(arbitration>>5));
   Unsafe_WriteRegister(offset+19,static_cast<uint8_t>(arbitration<<3));
   //задаём маску
   Unsafe_WriteRegister(offset+20,static_cast<uint8_t>(arbitration_mask>>21));
   Unsafe_WriteRegister(offset+21,static_cast<uint8_t>(arbitration_mask>>13));
   Unsafe_WriteRegister(offset+22,static_cast<uint8_t>(arbitration_mask>>5));
   Unsafe_WriteRegister(offset+23,static_cast<uint8_t>(arbitration_mask<<3)|0x04);//добавляем RTR
  }
  //настраиваем скорость
  if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_125KBS)==true)
  {
   Unsafe_WriteRegister(offset+6,0x03);
   Unsafe_WriteRegister(offset+7,0x1c);
  }
  if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_250KBS)==true)
  {
   Unsafe_WriteRegister(offset+6,0x01);
   Unsafe_WriteRegister(offset+7,0x1c);
  }
  if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_500KBS)==true)
  {
   Unsafe_WriteRegister(offset+6,0x00);
   Unsafe_WriteRegister(offset+7,0x1c);
  }
  if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_1MBS)==true)
  {
   Unsafe_WriteRegister(offset+6,0x00);
   Unsafe_WriteRegister(offset+7,0x14);
  }
  if (c7841CANChannel_local.IsSpeed(C7841CANChannel::CAN7841_SPEED_USER)==true)
  {
   C7841UserSpeed &c7841UserSpeed=c7841CANChannel_local.c7841UserSpeed;
  	
   Unsafe_WriteRegister(offset+6,c7841UserSpeed.BRP|(c7841UserSpeed.SJW<<6));
   Unsafe_WriteRegister(offset+7,(c7841UserSpeed.SAM<<7)|(c7841UserSpeed.TSeg2<<4)|c7841UserSpeed.TSeg1);
  }
  //настраиваем регистр управления: нормальный режим, тяни-толкай
  Unsafe_WriteRegister(offset+8,0xfa);
  //разрешаем прерывания по приёму и передаче
  Unsafe_WriteRegister(offset+0,0x08);
  Unsafe_WriteRegister(offset+4,tmp|0x03); 
 }
}

//----------------------------------------------------------------------------------------------------
//отправить пакет
//----------------------------------------------------------------------------------------------------
bool C7841::SendPackage(uint32_t channel,const C7841CANPackage &c7841CANPackage)
{
 if (IsChannelValid(channel)==false) return(false);
 usleep(1000);
 
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return(false);
  uint64_t offset=channel*128;
  C7841CANChannel &c7841CANChannel_local=c7841CANChannel[channel]; 
  //узнаем состояние передачи
  uint8_t v=Unsafe_ReadRegister(offset+2);
  if ((v&0x04)==0) return(false);//передача невозможна
  uint64_t data_begin=0;
  if (c7841CANChannel_local.ExtendedMode==false)//обычный режим
  {
   data_begin=19; 	
   if (c7841CANPackage.RTR==false) Unsafe_WriteRegister(offset+16,c7841CANPackage.Length&0x0f);
                              else Unsafe_WriteRegister(offset+16,(c7841CANPackage.Length&0x0f)|0x40);
   Unsafe_WriteRegister(offset+18,c7841CANPackage.Arbitration<<5);
   Unsafe_WriteRegister(offset+17,c7841CANPackage.Arbitration>>3);  
  }
  else//расширенный арбитраж
  {
   data_begin=21;
   if (c7841CANPackage.RTR==false) Unsafe_WriteRegister(offset+16,(c7841CANPackage.Length&0x0f)|0x80);
                              else Unsafe_WriteRegister(offset+16,(c7841CANPackage.Length&0x0f)|0x40);
   Unsafe_WriteRegister(offset+17,c7841CANPackage.Arbitration>>21);
   Unsafe_WriteRegister(offset+18,c7841CANPackage.Arbitration>>13);  
   Unsafe_WriteRegister(offset+19,c7841CANPackage.Arbitration>>5);
   Unsafe_WriteRegister(offset+20,c7841CANPackage.Arbitration<<3);
  }
  for(uint8_t n=0;n<c7841CANPackage.Length;n++)
  {
   Unsafe_WriteRegister(offset+data_begin+n,c7841CANPackage.Data[n]);	
  } 
  //запускаем передачу 
  Unsafe_WriteRegister(offset+1,0x01);
  return(true);
 }
 }

//----------------------------------------------------------------------------------------------------
//получить пакеты
//----------------------------------------------------------------------------------------------------
void C7841::GetPackage(std::vector<C7841CANPackage> &vector_C7841CANPackage)
{
 vector_C7841CANPackage.clear();
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  while(1)
  {
   C7841CANPackage c7841CANPackage;
   if (sProtectedVariables.cRingBuffer_Ptr.Get()->Pop(c7841CANPackage)==false) break;
   vector_C7841CANPackage.push_back(c7841CANPackage);
  }
 }
}

//----------------------------------------------------------------------------------------------------
//разрешить приём данных
//----------------------------------------------------------------------------------------------------
void C7841::EnableReceive(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return;
  uint64_t offset=channel*128;	
  uint8_t byte=Unsafe_ReadRegister(offset+4);
  Unsafe_WriteRegister(offset+4,byte|0x01);
 }
}
//----------------------------------------------------------------------------------------------------
//запретить приём данных
//----------------------------------------------------------------------------------------------------
void C7841::DisableReceive(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return;
  uint64_t offset=channel*128;	
  uint8_t byte=Unsafe_ReadRegister(offset+4);
  Unsafe_WriteRegister(offset+4,byte&(0xff^0x01));
 }
}
//----------------------------------------------------------------------------------------------------
//очистить ошибки
//----------------------------------------------------------------------------------------------------
void C7841::ClearOverrun(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return;
  uint64_t offset=channel*128;
  //очищаем ошибки
  Unsafe_WriteRegister(offset+1,0x08);
  //разрешаем прерывания ошибки и приёма данных
  uint8_t byte=Unsafe_ReadRegister(offset+4); 
  Unsafe_WriteRegister(offset+4,byte|0x09);
 }
}
//----------------------------------------------------------------------------------------------------
//очистить буфер приёма
//----------------------------------------------------------------------------------------------------
void C7841::ClearReceiveBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return;
  uint64_t offset=channel*128;
  while(Unsafe_ReadRegister(offset+29)!=0)
  {
   Unsafe_WriteRegister(offset+1,0x04); 	
  }	
 } 
}
//----------------------------------------------------------------------------------------------------
//очистить буфер передачи
//----------------------------------------------------------------------------------------------------
void C7841::ClearTransmitBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return;
  uint64_t offset=channel*128;
  Unsafe_WriteRegister(offset+1,0x02);
 }
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма арбитража
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetArbitrationLostBit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(Unsafe_ReadRegister(offset+11));
 }
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetReceiveErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(Unsafe_ReadRegister(offset+14));
 }
}
//----------------------------------------------------------------------------------------------------
//получить количество ошибок передачи
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetTransmitErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(Unsafe_ReadRegister(offset+15));
 }
}
//----------------------------------------------------------------------------------------------------
//получить состояние канала
//----------------------------------------------------------------------------------------------------
S7841StatusReg C7841::GetChannelStatus(uint32_t channel)
{
 S7841StatusReg s7841StatusReg; 
 if (IsChannelValid(channel)==false) return(s7841StatusReg);
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return(s7841StatusReg);
  uint64_t offset=channel*128; 
  *(reinterpret_cast<uint8_t*>(&s7841StatusReg))=Unsafe_ReadRegister(offset+2);
  return(s7841StatusReg);
 }
}
//----------------------------------------------------------------------------------------------------
//получить ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetErrorWarningLimit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(Unsafe_ReadRegister(offset+13));	
 }
}
//----------------------------------------------------------------------------------------------------
//задать ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
void C7841::SetErrorWarningLimit(uint32_t channel,uint8_t value)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return;
  uint64_t offset=channel*128;
  uint8_t byte=Unsafe_ReadRegister(offset+0);  	
  //входим в режим сброса
  Unsafe_WriteRegister(offset+0,0);
  Unsafe_WriteRegister(offset+13,value);
  Unsafe_WriteRegister(offset+0,byte);
 }
}
//----------------------------------------------------------------------------------------------------
//получить код ошибки
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetErrorCode(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&sProtectedVariables.cMutex);
 {
  if (Unsafe_IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(Unsafe_ReadRegister(offset+12));
 }
}

//****************************************************************************************************
//прототипы функций
//****************************************************************************************************

static bool WaitInterrupt(void);//ожидание прерывания

//****************************************************************************************************
//функции
//****************************************************************************************************

//----------------------------------------------------------------------------------------------------
//функция потока
//----------------------------------------------------------------------------------------------------
void* ThreadFunction(void *param)
{
 //разрешаем доступ потоку к ресурсам аппаратуры
 ThreadCtl(_NTO_TCTL_IO,NULL);
 if (param==NULL) return(NULL);	
 C7841 *c7841_Ptr=reinterpret_cast<C7841*>(param); 
 //подключаем прерывание
 c7841_Ptr->CANInterruptAttach(); 
 while(1)
 {
  if (c7841_Ptr->IsExitThread()==true) break;//выходим из потока
  if (WaitInterrupt()==false) continue;//ждём прерывания
  c7841_Ptr->ReceivePackage();//выполняем приём данных
 } 	
 return(NULL);	
}

//----------------------------------------------------------------------------------------------------
//ожидание прерывания
//----------------------------------------------------------------------------------------------------
bool WaitInterrupt(void)
{
 //задаём таймер ожидания прерывания
 sigevent event_timeout;//событие "время вышло"
 uint64_t timeout=100000;
 SIGEV_UNBLOCK_INIT(&event_timeout);
 TimerTimeout(CLOCK_REALTIME,_NTO_TIMEOUT_INTR,&event_timeout,&timeout,NULL);//тайм-аут ядра на ожидание прерывания
 //ждём прерывания
 if (InterruptWait(0,NULL)<0) return(false);//прерывание не поступало
 return(true);	
}
