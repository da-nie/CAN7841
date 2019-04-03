//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "cpci7841.h"
#include "craiicmutex.h"
#include <string.h>
#include <unistd.h>

#include <stdio.h>

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
CPCI7841::CPCI7841(uint32_t device_index)
{
 DeviceIndex=device_index;
 PCI_Handle=-1;
 cThread_Ptr.Set(NULL);
 cPCI7841ProtectedPart_Ptr.Set(new CPCI7841ProtectedPart(RECEIVER_RING_BUFFER_SIZE,TRANSMITTER_RING_BUFFER_SIZE));
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
CPCI7841::~CPCI7841()
{
 Release();
 cPCI7841ProtectedPart_Ptr.Release();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//получить, допустим ли такой номер канала
//----------------------------------------------------------------------------------------------------
bool CPCI7841::IsChannelValid(uint32_t channel)
{
 if (channel>=CPCI7841ProtectedPart::CAN_CHANNEL_AMOUNT) return(false);
 return(true);	
}

//----------------------------------------------------------------------------------------------------
//подключиться к прерыванию
//----------------------------------------------------------------------------------------------------
bool CPCI7841::CANInterruptAttach(void)
{
 CANInterruptDetach();
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(false);
  return(cPCI7841ProtectedPart_Ptr.Get()->CANInterruptAttach(&CANInterruptEvent));
 }
 return(true);
}
//----------------------------------------------------------------------------------------------------
//отключиться от прерывания
//----------------------------------------------------------------------------------------------------
void CPCI7841::CANInterruptDetach(void)
{
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  return(cPCI7841ProtectedPart_Ptr.Get()->CANInterruptDetach()); 	
 }
}
//----------------------------------------------------------------------------------------------------
//получить, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
bool CPCI7841::IsExitThread(void)
{
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  return(cPCI7841ProtectedPart_Ptr.Get()->IsExitThread());
 } 
}
//----------------------------------------------------------------------------------------------------
//задать, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
void CPCI7841::SetExitThread(bool state)
{
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {   	
  cPCI7841ProtectedPart_Ptr.Get()->SetExitThread(state);
 }
}
//----------------------------------------------------------------------------------------------------
//запустить поток
//----------------------------------------------------------------------------------------------------
void CPCI7841::StartThread(void)
{
 StopThread();
 SetExitThread(false);
 {
  CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
  {   	
   if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  }
 }
 cThread_Ptr.Set(new CThread(ThreadFunction,THREAD_PRIORITY,this)); 
}
//----------------------------------------------------------------------------------------------------
//остановить поток
//----------------------------------------------------------------------------------------------------
void CPCI7841::StopThread(void)
{
 SetExitThread(true);
 {
  CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
  {   	
   if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  }
 }
 if (cThread_Ptr.Get()!=NULL)
 {
  cThread_Ptr.Get()->Join();
  cThread_Ptr.Set(NULL);
 }
}

//----------------------------------------------------------------------------------------------------
//выполнить обработку прерывания
//----------------------------------------------------------------------------------------------------
void CPCI7841::OnInterrupt(void)
{
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  cPCI7841ProtectedPart_Ptr.Get()->OnInterrupt();
 }
}
//----------------------------------------------------------------------------------------------------
//обработка платы до прерывания
//----------------------------------------------------------------------------------------------------
bool CPCI7841::Processing(void)
{
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsExitThread()==true) return(false);
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(true);
  //запускаем передачу данных на каналах, если она возможна
  for(uint32_t n=0;n<CPCI7841ProtectedPart::CAN_CHANNEL_AMOUNT;n++) cPCI7841ProtectedPart_Ptr.Get()->TransmittProcessing(n);
 }
 return(true); 
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//найти плату на шине PCI и инициализировать
//----------------------------------------------------------------------------------------------------
bool CPCI7841::Init(void)
{
 StopThread();	
 //подключаем порты
 {
  CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
  {
   //ищем устройство
   PCI_Handle=pci_attach(0);
   if (PCI_Handle<0) return(false);
   int rescan=pci_rescan_bus();
   if (rescan!=PCI_SUCCESS) return(false);
   if (cPCI7841ProtectedPart_Ptr.Get()->InitPCI(CAN7841_VendorID,CAN7841_DeviceID,DeviceIndex)==false)
   {
    cRAIICMutex.Unlock();
    Release();
    return(false);	
   }
  }
 }
 //запускаем поток
 StartThread();
 return(true);
}
//----------------------------------------------------------------------------------------------------
//освободить ресурсы
//----------------------------------------------------------------------------------------------------
void CPCI7841::Release(void)
{
 StopThread();
 CANInterruptDetach();
 {
  CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
  {
   if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==true) cPCI7841ProtectedPart_Ptr.Get()->ReleasePCI();
  }
 }
 if (PCI_Handle>=0) pci_detach(PCI_Handle);
 PCI_Handle=-1; 
}
//----------------------------------------------------------------------------------------------------
//настроить порт
//----------------------------------------------------------------------------------------------------
bool CPCI7841::CANConfig(uint32_t channel,const CPCI7841CANChannel &cPCI7841CANChannel_Set)
{
 if (IsChannelValid(channel)==false) return(false);
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(false);
  cPCI7841CANChannel[channel]=cPCI7841CANChannel_Set;
  return(cPCI7841ProtectedPart_Ptr.Get()->CANConfig(channel,cPCI7841CANChannel[channel]));
 }
}

//----------------------------------------------------------------------------------------------------
//отправить пакет
//----------------------------------------------------------------------------------------------------
bool CPCI7841::SendPackage(const CPCI7841CANPackage &cPCI7841CANPackage)
{
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(false);
  return(cPCI7841ProtectedPart_Ptr.Get()->SendPackage(cPCI7841CANPackage));
 }
}
//----------------------------------------------------------------------------------------------------
//получить принятые пакеты
//----------------------------------------------------------------------------------------------------
void CPCI7841::GetReceivedPackage(std::vector<CPCI7841CANPackage> &vector_CPCI7841CANPackage)
{
 vector_CPCI7841CANPackage.clear();
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  while(1)
  {
   CPCI7841CANPackage cPCI7841CANPackage;
   if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
   if (cPCI7841ProtectedPart_Ptr.Get()->GetReceivedPackage(cPCI7841CANPackage)==false) break;
   vector_CPCI7841CANPackage.push_back(cPCI7841CANPackage);
  }
 }
}
//----------------------------------------------------------------------------------------------------
//получить один принятый пакет
//----------------------------------------------------------------------------------------------------
bool CPCI7841::GetOneReceivedPackage(CPCI7841CANPackage &cPCI7841CANPackage)
{
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(false);
  if (cPCI7841ProtectedPart_Ptr.Get()->GetReceivedPackage(cPCI7841CANPackage)==false) return(false);
  return(true);
 }
}
//----------------------------------------------------------------------------------------------------
//разрешить приём данных
//----------------------------------------------------------------------------------------------------
void CPCI7841::EnableReceiver(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  uint8_t byte=cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+4);
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+4,byte|0x01);
 }
}
//----------------------------------------------------------------------------------------------------
//запретить приём данных
//----------------------------------------------------------------------------------------------------
void CPCI7841::DisableReceiver(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;	
  uint8_t byte=cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+4);
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+4,byte&(0xff^0x01));
 }
}
//----------------------------------------------------------------------------------------------------
//очистить ошибки
//----------------------------------------------------------------------------------------------------
void CPCI7841::ClearOverrun(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  //очищаем ошибки
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+1,0x08);
  //разрешаем прерывания ошибки и приёма данных
  uint8_t byte=cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+4); 
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+4,byte|0x09);
 }
}
//----------------------------------------------------------------------------------------------------
//очистить буфер приёма
//----------------------------------------------------------------------------------------------------
void CPCI7841::ClearReceiverBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
//  while (cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+29)!=0)
  {
  // cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+1,0x04); 	
  }
  cPCI7841ProtectedPart_Ptr.Get()->ClearReceiverBuffer();
 } 
}
//----------------------------------------------------------------------------------------------------
//очистить буфер передачи
//----------------------------------------------------------------------------------------------------
void CPCI7841::ClearTransmitterBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+1,0x02);
  cPCI7841ProtectedPart_Ptr.Get()->ClearTransmitterBuffer();
 }
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма арбитража
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841::GetArbitrationLostBit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+11));
 }
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841::GetReceiveErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+14));
 }
}
//----------------------------------------------------------------------------------------------------
//получить количество ошибок передачи
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841::GetTransmitErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+15));
 }
}
//----------------------------------------------------------------------------------------------------
//получить состояние канала
//----------------------------------------------------------------------------------------------------
SPCI7841StatusReg CPCI7841::GetChannelStatus(uint32_t channel)
{
 SPCI7841StatusReg sPCI7841StatusReg; 
 if (IsChannelValid(channel)==false) return(sPCI7841StatusReg);
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(sPCI7841StatusReg);
  uint64_t offset=channel*128; 
  *(reinterpret_cast<uint8_t*>(&sPCI7841StatusReg))=cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+2);
  return(sPCI7841StatusReg);
 }
}
//----------------------------------------------------------------------------------------------------
//получить ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841::GetErrorWarningLimit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+13));	
 }
}
//----------------------------------------------------------------------------------------------------
//задать ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
void CPCI7841::SetErrorWarningLimit(uint32_t channel,uint8_t value)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  uint8_t byte=cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+0);  	
  //входим в режим сброса
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+0,0);
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+13,value);
  cPCI7841ProtectedPart_Ptr.Get()->WriteRegister(offset+0,byte);
 }
}
//----------------------------------------------------------------------------------------------------
//получить код ошибки
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841::GetErrorCode(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&cPCI7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (cPCI7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(cPCI7841ProtectedPart_Ptr.Get()->ReadRegister(offset+12));
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
 CPCI7841 *cPCI7841_Ptr=reinterpret_cast<CPCI7841*>(param); 
 //подключаем прерывание
 cPCI7841_Ptr->CANInterruptAttach(); 
 while(1)
 {
  if (cPCI7841_Ptr->Processing()==false) break;//выходим из потока
  if (WaitInterrupt()==false) continue;//ждём прерывания
  cPCI7841_Ptr->OnInterrupt();//выполняем обработчик прерывания
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
