#ifndef C_7841_PROTECTED_PART_H
#define C_7841_PROTECTED_PART_H

//****************************************************************************************************
//Класс защищённой части платы CAN 7841
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
#include "cmutex.h"
#include "cringbuffer.h"

//****************************************************************************************************
//Класс защищённой части платы CAN 7841
//****************************************************************************************************

//Сейчас передача при добавлении пакета НЕ НАЧИНАЕТСЯ! Это нужно исправить.
//Прерывание показывает КАКОЙ канал готов передавать - нужно ДВА буфере для отправки данных.



//класс защищённых переменных
class C7841ProtectedPart
{
 //-переменные-----------------------------------------------------------------------------------------
 public:  	  	  	
  CMutex cMutex;//мютекс для доступа к классу
 private:  
  int32_t ISR_CANInterruptID;//идентификатор прерывания CAN
  pci_dev_info PCI_Dev_Info;//описатель устройства   
  void *DeviceHandle;//дескриптор устройства
  bool ExitThread;//нужно ли выходить из потока
  std::vector<CIOControl> vector_CIOControl;//массив адресов ввода-вывода платы
  CUniquePtr<CRingBuffer<C7841CANPackage> > cRingBuffer_Receiver_Ptr;//указатель на класс буфера принятых данных
  CUniquePtr<CRingBuffer<C7841CANPackage> > cRingBuffer_Transmitter_Ptr;//указатель на класс буфера передаваемых данных
 //-конструктор----------------------------------------------------------------------------------------
 public:
  C7841ProtectedPart(uint32_t receiver_buffer_size=1,uint32_t transmitter_buffer_size=1);//конструктор
 //-деструктор-----------------------------------------------------------------------------------------
  ~C7841ProtectedPart();//деструктор
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
  bool CANConfig(uint32_t channel,const C7841CANChannel &c7841CANChannel_Set);//настроить порт
  bool GetReceivedPackage(C7841CANPackage &c7841CANPackage);//получить принятый пакет
  bool SendPackage(const C7841CANPackage &c7841CANPackage);//добавить пакет для отправки
  void OnInterrupt(uint32_t channel_amount);//обработчик прерывания
 //-закрытые функции-----------------------------------------------------------------------------------
 private:
  void OnInterruptChannel(uint32_t channel);//обработчик прерывания канала 
  void ReceivePackageChannel(uint32_t channel);//получить пакеты с канала
  void TransmittPackageChannel(uint32_t channel);//отправить пакеты в канал
  bool TransmittPackage(const C7841CANPackage &c7841CANPackage);//добавить пакет для отправки
};
#endif


