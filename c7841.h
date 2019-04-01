#ifndef C_7841_H
#define C_7841_H

//****************************************************************************************************
//Класс управления платой CAN 7841
//****************************************************************************************************

//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include <stdint.h>
#include <vector>
#include <hw/pci.h>
#include <sys/neutrino.h>

#include "ciocontrol.h"
#include "c7841canpackage.h"
#include "c7841canchannel.h"
#include "cuniqueptr.h"
#include "cthread.h"
#include "c7841protectedpart.h"

//****************************************************************************************************
//структуры
//****************************************************************************************************

#pragma pack(1)
//регистр статуса
struct S7841StatusReg
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

//****************************************************************************************************
//протитипы функций
//****************************************************************************************************

void* ThreadFunction(void *param);//функция потока

//****************************************************************************************************
//Класс управления платой CAN 7841
//****************************************************************************************************
class C7841
{
 //-дружественные функции и классы---------------------------------------------------------------------
 friend void* ThreadFunction(void *param);
 //-переменные-----------------------------------------------------------------------------------------
 private: 	
  static const uint32_t CAN7841_VendorID=0x144A;//идентификатор производителя
  static const uint32_t CAN7841_DeviceID=0x7841;//идентификатор устройства
  static const uint32_t CAN_CHANNEL_AMOUNT=2;//два канала на плате
  static const uint32_t THREAD_PRIORITY=50;//приоритет потока
  static const uint32_t RECEIVER_RING_BUFFER_SIZE=500;//размер очереди приёма данных
  static const uint32_t TRANSMITTER_RING_BUFFER_SIZE=500;//размер очереди передачи данных
    
  uint32_t DeviceIndex;//номер устройства на шине
  int PCI_Handle;//дескриптор PCI
  
  C7841CANChannel c7841CANChannel[CAN_CHANNEL_AMOUNT];//настройки канала
  
  sigevent CANInterruptEvent;//событие прерывания CAN
  
  CUniquePtr<CThread> cThread_Ptr;//указатель на поток управления
  //класс защищённой части
  CUniquePtr<C7841ProtectedPart> c7841ProtectedPart_Ptr;//указатель на класс защищённой части
 //-конструктор----------------------------------------------------------------------------------------
 public:
  C7841(uint32_t device_index=0);
 //-деструктор-----------------------------------------------------------------------------------------
  ~C7841(void);
 //-открытые функции-----------------------------------------------------------------------------------
 public:
  bool Init(void);//найти плату на шине PCI и инициализировать
  void Release(void);//освободить ресурсы
  
  bool CANConfig(uint32_t channel,const C7841CANChannel &c7841CANChannel_Set);//настроить канал
  bool SendPackage(const C7841CANPackage &c7841CANPackage);//отправить пакет
  void GetReceivedPackage(std::vector<C7841CANPackage> &vector_C7841CANPackage);//получить принятые пакеты
  void EnableReceive(uint32_t channel);//разрешить приём данных
  void DisableReceive(uint32_t channel);//запретить приём данных
  void ClearOverrun(uint32_t channel);//очистить ошибки
  void ClearReceiveBuffer(uint32_t channel);//очистить буфер приёма
  void ClearTransmitBuffer(uint32_t channel);//очистить буфер передачи
  uint8_t GetArbitrationLostBit(uint32_t channel);//получить количество ошибок приёма арбитража
  uint8_t GetReceiveErrorCounter(uint32_t channel);//получить количество ошибок приёма
  uint8_t GetTransmitErrorCounter(uint32_t channel);//получить количество ошибок передачи
  S7841StatusReg GetChannelStatus(uint32_t channel);//получить состояние канала
  uint8_t GetErrorWarningLimit(uint32_t channel);//получить ограничение на количество ошибок
  void SetErrorWarningLimit(uint32_t channel,uint8_t value);//задать ограничение на количество ошибок
  uint8_t GetErrorCode(uint32_t channel);//получить код ошибки    
 //-закрытые функции-----------------------------------------------------------------------------------
 private:
  bool IsChannelValid(uint32_t channel);//получить, допустим ли такой номер канала
  bool CANInterruptAttach(void);//подключиться к прерыванию
  void CANInterruptDetach(void);//отключиться от прерывания
  bool IsExitThread(void);//получить, нужно ли выходить из потока
  void SetExitThread(bool state);//задать, нужно ли выходить из потока
  void StartThread(void);//запустить поток
  void StopThread(void);//остановить поток  
  void OnInterrupt(void);//выполнить обработку прерывания
};



#endif


