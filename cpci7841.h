#ifndef C_PCI_7841_H
#define C_PCI_7841_H

//****************************************************************************************************
//Класс управления платой CAN PCI-7841
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
#include "cthread.h"
#include "cpci7841protectedpart.h"

//****************************************************************************************************
//протитипы функций
//****************************************************************************************************

static void* ThreadFunction(void *param);//функция потока

//****************************************************************************************************
//Класс управления платой CAN PCI-7841
//****************************************************************************************************
class CPCI7841
{
 //-дружественные функции и классы---------------------------------------------------------------------
 friend void* ThreadFunction(void *param);
 //-переменные-----------------------------------------------------------------------------------------
 private: 	
  static const uint32_t CAN7841_VendorID=0x144A;//идентификатор производителя
  static const uint32_t CAN7841_DeviceID=0x7841;//идентификатор устройства
  static const uint32_t THREAD_PRIORITY=50;//приоритет потока
  static const uint32_t RECEIVER_RING_BUFFER_SIZE=500;//размер очереди приёма данных
  static const uint32_t TRANSMITTER_RING_BUFFER_SIZE=500;//размер очереди передачи данных
    
  uint32_t DeviceIndex;//номер устройства на шине
  int PCI_Handle;//дескриптор PCI
  
  sigevent CANInterruptEvent;//событие прерывания CAN  
  CUniquePtr<CThread> cThread_Ptr;//указатель на поток управления
  //класс защищённой части
  CUniquePtr<CPCI7841ProtectedPart> cPCI7841ProtectedPart_Ptr;//указатель на класс защищённой части
 //-конструктор----------------------------------------------------------------------------------------
 public:
  CPCI7841(uint32_t device_index=0);
 //-деструктор-----------------------------------------------------------------------------------------
  ~CPCI7841(void);
 //-открытые функции-----------------------------------------------------------------------------------
 public:
  void SetDeviceIndex(uint32_t device_index);//задать индекс устройства на шине
  bool Init(void);//найти плату на шине PCI и инициализировать
  void Release(void);//освободить ресурсы
  
  bool CANConfig(uint32_t channel,const CPCI7841CANChannel &cPCI7841CANChannel_Set);//настроить канал
  bool TransmittPackage(const CPCI7841CANPackage &cPCI7841CANPackage);//отправить пакет
  void GetAllReceivedPackage(std::vector<CPCI7841CANPackage> &vector_CPCI7841CANPackage);//получить все принятые пакеты
  void GetPartReceivedPackage(std::vector<CPCI7841CANPackage> &vector_CPCI7841CANPackage,size_t counter);//получить не больше заданного количества принятых пакетов
  bool GetOneReceivedPackage(CPCI7841CANPackage &cPCI7841CANPackage);//получить один принятый пакет
  void EnableReceiver(uint32_t channel);//разрешить приём данных
  void DisableReceiver(uint32_t channel);//запретить приём данных
  void ClearOverrun(uint32_t channel);//очистить ошибки
  void ClearReceiverBuffer(uint32_t channel);//очистить буфер приёма
  void ClearTransmitterBuffer(uint32_t channel);//очистить буфер передачи
  uint8_t GetArbitrationLostBit(uint32_t channel);//получить количество ошибок приёма арбитража
  uint8_t GetReceiveErrorCounter(uint32_t channel);//получить количество ошибок приёма
  uint8_t GetTransmittErrorCounter(uint32_t channel);//получить количество ошибок передачи
  CPCI7841ProtectedPart::SPCI7841StatusReg GetChannelStatus(uint32_t channel);//получить состояние канала
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
  bool Processing(void);//обработка платы до прерывания
};



#endif


