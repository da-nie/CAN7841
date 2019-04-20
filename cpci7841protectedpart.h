#ifndef C_PCI_7841_PROTECTED_PART_H
#define C_PCI_7841_PROTECTED_PART_H

//****************************************************************************************************
//Класс защищённой части платы CAN PCI-7841
//****************************************************************************************************

//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include <stdint.h>
#include <vector>
#include <hw/pci.h>
#include <sys/neutrino.h>

#include "ciocontrol.h"
#include "cpci7841canpackage.h"
#include "cpci7841canchannel.h"
#include "cuniqueptr.h"
#include "cmutex.h"
#include "cringbuffer.h"

//****************************************************************************************************
//Класс защищённой части платы CAN PCI-7841
//****************************************************************************************************

class CPCI7841ProtectedPart
{
 public:	
 //-структуры------------------------------------------------------------------------------------------
  #pragma pack(1)
  //регистр статуса
  struct SPCI7841StatusReg
  {
   uint8_t RxBuffer:1;
   uint8_t DataOverrun:1;
   uint8_t TxBuffer:1;
   uint8_t TxEnd:1;
   uint8_t RxStatus:1;
   uint8_t TxStatus:1;
   uint8_t ErrorStatus:1;
   uint8_t BusStatus:1;
  };
  #pragma pack()	
 //-переменные-----------------------------------------------------------------------------------------
 public:  	  	  	
  CMutex cMutex;//мютекс для доступа к классу
  static const uint32_t CAN_CHANNEL_AMOUNT=2;//количество каналов на плате  
 private:  
  static const long double CAN_CHANNEL_BUSS_OFF_DELAY_MS=500.0;//интервал перезапуска линии при Buss-off
  static const long double TRANSMITT_WAIT_TIME_MS=1.0;//время ожидания завершения передачи  
 
  int32_t ISR_CANInterruptID;//идентификатор прерывания CAN
  pci_dev_info PCI_Dev_Info;//описатель устройства   
  void *DeviceHandle;//дескриптор устройства
  bool ExitThread;//нужно ли выходить из потока
  std::vector<CIOControl> vector_CIOControl;//массив адресов ввода-вывода платы 
  CUniquePtr<CRingBuffer<CPCI7841CANPackage> > cRingBuffer_Receiver_Ptr;//указатель на класс буфера принятых данных
  bool TransmittIsDone[CAN_CHANNEL_AMOUNT];//завершена ли передача в канале
  uint64_t TransmittStartTime[CAN_CHANNEL_AMOUNT];//время начала передачи  
  CUniquePtr<CRingBuffer<CPCI7841CANPackage> > cRingBuffer_Transmitter_Ptr[CAN_CHANNEL_AMOUNT];//указатель на класс буфера передаваемых данных
  
  uint64_t BussOffTime[CAN_CHANNEL_AMOUNT];//время Buss Off
  CPCI7841CANChannel cPCI7841CANChannel[CPCI7841ProtectedPart::CAN_CHANNEL_AMOUNT];//настройки канала  
  
  uint64_t CPS;//частота процессора
 //-конструктор----------------------------------------------------------------------------------------
 public:
  CPCI7841ProtectedPart(uint32_t receiver_buffer_size=1,uint32_t transmitter_buffer_size=1);//конструктор
 //-деструктор-----------------------------------------------------------------------------------------
  ~CPCI7841ProtectedPart();//деструктор
 //-открытые функции-----------------------------------------------------------------------------------
 public:
  void ClearIRQ(void);//сбросить флаг прерывания
  uint8_t GetIRQ(void);//получить номер прерывания
  uint8_t ReadRegister(uint64_t reg);//прочесть регистр
  bool WriteRegister(uint64_t reg,uint8_t value);//записать регистр
  bool IsEnabled(void);//получить, подключена ли плата   
  bool CANInterruptAttach(sigevent *CANInterruptEvent);//подключить прерывание
  void CANInterruptDetach(void);//отключить прерывание
  bool IsExitThread(void);//получить, нужно ли выходить из потока
  void SetExitThread(bool state);//задать, нужно ли выходить из потока
  bool InitPCI(uint32_t vendor_id,uint32_t device_id,uint32_t device_index);//инициализировать PCI
  void ReleasePCI(void);//освободить PCI
  bool CANConfig(uint32_t channel,const CPCI7841CANChannel &cPCI7841CANChannel_Set);//настроить порт
  bool GetReceivedPackage(CPCI7841CANPackage &cPCI7841CANPackage);//получить принятый пакет
  bool TransmittPackage(const CPCI7841CANPackage &cPCI7841CANPackage);//добавить пакет для отправки
  void OnInterrupt(void);//обработчик прерывания
  void ClearTransmitterBuffer(uint32_t channel);//очистить буфер передатчика
  void ClearReceiverBuffer(uint32_t channel);//очистить буфер приёмника
  void TransmittProcessing(uint32_t channel);//выполнить передачу данных, если это возможно
  void EnableReceiver(uint32_t channel);//разрешить приём данных
  void DisableReceiver(uint32_t channel);//запретить приём данных
  void ClearOverrun(uint32_t channel);//очистить ошибки
  uint8_t GetArbitrationLostBit(uint32_t channel);//получить количество ошибок приёма арбитража
  uint8_t GetReceiveErrorCounter(uint32_t channel);//получить количество ошибок приёма
  uint8_t GetTransmittErrorCounter(uint32_t channel);//получить количество ошибок передачи
  SPCI7841StatusReg GetChannelStatus(uint32_t channel);//получить состояние канала
  uint8_t GetErrorWarningLimit(uint32_t channel);//получить ограничение на количество ошибок
  void SetErrorWarningLimit(uint32_t channel,uint8_t value);//задать ограничение на количество ошибок
  uint8_t GetErrorCode(uint32_t channel);//получить код ошибки  
  bool BussOffControl(uint32_t channel);//проверить работоспособность контроллера канала и сбросить канал при необходимости
 //-закрытые функции-----------------------------------------------------------------------------------
 private:
  bool IsWaitable(uint32_t channel);//получить, можно ли выполнять действия с каналом
  bool IsOverTransmittWaitTime(uint32_t channel);//получить, закончилось ли время ожидания передачи
  void SetWaitState(uint32_t channel);//запустить счётчик запрета операций с каналом
  void OnInterruptChannel(uint32_t channel);//обработчик прерывания канала 
  void ReceiveInterrupt(uint32_t channel);//получить пакеты с канала
  void TransmittInterrupt(uint32_t channel);//отправить пакеты в канал
  bool StartTransmittPackage(const CPCI7841CANPackage &cPCI7841CANPackage);//добавить пакет для отправки
};
#endif


