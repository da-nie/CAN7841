//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "c7841.h"
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
C7841::C7841(uint32_t device_index)
{
 DeviceIndex=device_index;
 PCI_Handle=-1;
 cThread_Ptr.Set(NULL);
 c7841ProtectedPart_Ptr.Set(new C7841ProtectedPart(RECEIVER_RING_BUFFER_SIZE,TRANSMITTER_RING_BUFFER_SIZE));
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
C7841::~C7841()
{
 Release();
 c7841ProtectedPart_Ptr.Release();
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
//подключиться к прерыванию
//----------------------------------------------------------------------------------------------------
bool C7841::CANInterruptAttach(void)
{
 CANInterruptDetach(); 
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  return(c7841ProtectedPart_Ptr.Get()->CANInterruptAttach(&CANInterruptEvent));
 }
 return(true);
}
//----------------------------------------------------------------------------------------------------
//отключиться от прерывания
//----------------------------------------------------------------------------------------------------
void C7841::CANInterruptDetach(void)
{
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  return(c7841ProtectedPart_Ptr.Get()->CANInterruptDetach()); 	
 }
}
//----------------------------------------------------------------------------------------------------
//получить, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
bool C7841::IsExitThread(void)
{
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  return(c7841ProtectedPart_Ptr.Get()->IsExitThread());
 } 
}
//----------------------------------------------------------------------------------------------------
//задать, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
void C7841::SetExitThread(bool state)
{
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {   	
  c7841ProtectedPart_Ptr.Get()->SetExitThread(state);
 }
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
//выполнить обработку прерывания
//----------------------------------------------------------------------------------------------------
void C7841::OnInterrupt(void)
{
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  c7841ProtectedPart_Ptr.Get()->OnInterrupt(CAN_CHANNEL_AMOUNT);
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
 {
  CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
  {
   //ищем устройство
   PCI_Handle=pci_attach(0);
   if (PCI_Handle<0) return(false);
   int rescan=pci_rescan_bus();
   if (rescan!=PCI_SUCCESS) return(false);
   if (c7841ProtectedPart_Ptr.Get()->InitPCI(CAN7841_VendorID,CAN7841_DeviceID,DeviceIndex)==false)
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
void C7841::Release(void)
{
 StopThread();
 CANInterruptDetach();
 {
  CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
  {
   c7841ProtectedPart_Ptr.Get()->ReleasePCI();
  }
 }
 if (PCI_Handle>=0) pci_detach(PCI_Handle);
 PCI_Handle=-1; 
}
//----------------------------------------------------------------------------------------------------
//настроить порт
//----------------------------------------------------------------------------------------------------
bool C7841::CANConfig(uint32_t channel,const C7841CANChannel &c7841CANChannel_Set)
{
 if (IsChannelValid(channel)==false) return(false);
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  c7841CANChannel[channel]=c7841CANChannel_Set;
  return(c7841ProtectedPart_Ptr.Get()->CANConfig(channel,c7841CANChannel[channel]));
 }
}

//----------------------------------------------------------------------------------------------------
//отправить пакет
//----------------------------------------------------------------------------------------------------
bool C7841::SendPackage(const C7841CANPackage &c7841CANPackage)
{
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  return(c7841ProtectedPart_Ptr.Get()->SendPackage(c7841CANPackage));
 }
}
//----------------------------------------------------------------------------------------------------
//получить принятые пакеты
//----------------------------------------------------------------------------------------------------
void C7841::GetReceivedPackage(std::vector<C7841CANPackage> &vector_C7841CANPackage)
{
 vector_C7841CANPackage.clear();
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  while(1)
  {
   C7841CANPackage c7841CANPackage;
   if (c7841ProtectedPart_Ptr.Get()->GetReceivedPackage(c7841CANPackage)==false) break;
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
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  uint8_t byte=c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+4);
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+4,byte|0x01);
 }
}
//----------------------------------------------------------------------------------------------------
//запретить приём данных
//----------------------------------------------------------------------------------------------------
void C7841::DisableReceive(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;	
  uint8_t byte=c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+4);
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+4,byte&(0xff^0x01));
 }
}
//----------------------------------------------------------------------------------------------------
//очистить ошибки
//----------------------------------------------------------------------------------------------------
void C7841::ClearOverrun(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  //очищаем ошибки
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+1,0x08);
  //разрешаем прерывания ошибки и приёма данных
  uint8_t byte=c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+4); 
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+4,byte|0x09);
 }
}
//----------------------------------------------------------------------------------------------------
//очистить буфер приёма
//----------------------------------------------------------------------------------------------------
void C7841::ClearReceiveBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  while(c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+29)!=0)
  {
   c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+1,0x04); 	
  }	
 } 
}
//----------------------------------------------------------------------------------------------------
//очистить буфер передачи
//----------------------------------------------------------------------------------------------------
void C7841::ClearTransmitBuffer(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+1,0x02);
 }
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма арбитража
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetArbitrationLostBit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+11));
 }
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetReceiveErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+14));
 }
}
//----------------------------------------------------------------------------------------------------
//получить количество ошибок передачи
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetTransmitErrorCounter(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+15));
 }
}
//----------------------------------------------------------------------------------------------------
//получить состояние канала
//----------------------------------------------------------------------------------------------------
S7841StatusReg C7841::GetChannelStatus(uint32_t channel)
{
 S7841StatusReg s7841StatusReg; 
 if (IsChannelValid(channel)==false) return(s7841StatusReg);
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(s7841StatusReg);
  uint64_t offset=channel*128; 
  *(reinterpret_cast<uint8_t*>(&s7841StatusReg))=c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+2);
  return(s7841StatusReg);
 }
}
//----------------------------------------------------------------------------------------------------
//получить ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetErrorWarningLimit(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+13));	
 }
}
//----------------------------------------------------------------------------------------------------
//задать ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
void C7841::SetErrorWarningLimit(uint32_t channel,uint8_t value)
{
 if (IsChannelValid(channel)==false) return;
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return;
  uint64_t offset=channel*128;
  uint8_t byte=c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+0);  	
  //входим в режим сброса
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+0,0);
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+13,value);
  c7841ProtectedPart_Ptr.Get()->WriteRegister(offset+0,byte);
 }
}
//----------------------------------------------------------------------------------------------------
//получить код ошибки
//----------------------------------------------------------------------------------------------------
uint8_t C7841::GetErrorCode(uint32_t channel)
{
 if (IsChannelValid(channel)==false) return(0);
 CRAIICMutex cRAIICMutex(&c7841ProtectedPart_Ptr.Get()->cMutex);
 {
  if (c7841ProtectedPart_Ptr.Get()->IsEnabled()==false) return(0);
  uint64_t offset=channel*128;
  return(c7841ProtectedPart_Ptr.Get()->ReadRegister(offset+12));
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
  c7841_Ptr->OnInterrupt();//выполняем обработчик прерывания
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
