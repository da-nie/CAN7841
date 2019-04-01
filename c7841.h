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
//Класс управления платой CAN 7841
//****************************************************************************************************

class C7841
{
 //-переменные-----------------------------------------------------------------------------------------
 private: 	
  static const uint32_t CAN7841_VendorID=0x144A;//идентификатор производителя
  static const uint32_t CAN7841_DeviceID=0x7841;//идентификатор устройства
  static const uint32_t CAN_CHANNEL_AMOUNT=2;//два канала на плате
    
  std::vector<CIOControl> vector_CIOControl;//массив адресов ввода-вывода платы
    
  pci_dev_info PCI_Dev_Info;//описатель устройства
   
  uint32_t DeviceIndex;//номер устройства на шине
  int PCI_Handle;//дескриптор PCI
  void *DeviceHandle;//дескриптор устройства
  
  C7841CANChannel c7841CANChannel[CAN_CHANNEL_AMOUNT];//настройки канала
  
  sigevent CANInterruptEvent;//событие прерывания CAN
  int32_t ISR_CANInterruptID;//идентификатор прерывания CAN
  
 //-конструктор----------------------------------------------------------------------------------------
 public:
  C7841(uint32_t device_index=0);
 //-деструктор-----------------------------------------------------------------------------------------
  ~C7841(void);
 //-открытые функции-----------------------------------------------------------------------------------
 public:    
  bool Init(void);//найти плату на шине PCI и инициализировать
  void Release(void);//освободить ресурсы
  bool IsEnabled(void);//получить, подключена ли плата   
  uint8_t ReadRegister(uint64_t reg);//прочесть регистр
  bool WriteRegister(uint64_t reg,uint8_t value);//записать регистр
  
  void CANConfig(uint32_t channel,const C7841CANChannel &c7841CANChannel_Set);//настроить канал
  bool SendPackage(uint32_t channel,const C7841CANPackage &c7841CANPackage);//отправить пакет
  bool GetPackage(uint32_t channel,std::vector<C7841CANPackage> &vector_C7841CANPackage);//получить пакеты
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
  void ClearIRQ(void);//сбросить флаг прерывания
  uint8_t GetIRQ(void);//получить номер прерывания
  bool CANInterruptAttach(void);//подключиться к прерыванию
  void CANInterruptDetach(void);//отключиться от прерывания
 //-закрытые функции-----------------------------------------------------------------------------------
 private:
  bool IsChannelValid(uint32_t channel);//получить, допустим ли такой номер канала
};

#endif

