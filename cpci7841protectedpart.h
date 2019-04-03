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

//класс защищённых переменных
class CPCI7841ProtectedPart
{
 //-переменные-----------------------------------------------------------------------------------------
 public:  	  	  	
  CMutex cMutex;//мютекс для доступа к классу
  static const uint32_t CAN_CHANNEL_AMOUNT=2;//количество каналов на плате  
 private:  
  int32_t ISR_CANInterruptID;//идентификатор прерывания CAN
  pci_dev_info PCI_Dev_Info;//описатель устройства   
  void *DeviceHandle;//дескриптор устройства
  bool ExitThread;//нужно ли выходить из потока
  std::vector<CIOControl> vector_CIOControl;//массив адресов ввода-вывода платы 
  CUniquePtr<CRingBuffer<CPCI7841CANPackage> > cRingBuffer_Receiver_Ptr;//указатель на класс буфера принятых данных
  bool TransmittIsDone[CAN_CHANNEL_AMOUNT];//завершена ли передача в канале
  CUniquePtr<CRingBuffer<CPCI7841CANPackage> > cRingBuffer_Transmitter_Ptr[CAN_CHANNEL_AMOUNT];//указатель на класс буфера передаваемых данных
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
  bool SendPackage(const CPCI7841CANPackage &cPCI7841CANPackage);//добавить пакет для отправки
  void OnInterrupt(void);//обработчик прерывания
  void ClearTransmitterBuffer(void);//очистить буфер передатчика
  void ClearReceiverBuffer(void);//очистить буфер приёмника
  void TransmittProcessing(uint32_t channel);//выполнить передачу данных, если это возможно
 //-закрытые функции-----------------------------------------------------------------------------------
 private:
  void OnInterruptChannel(uint32_t channel);//обработчик прерывания канала 
  void ReceivePackageChannel(uint32_t channel);//получить пакеты с канала
  void TransmittPackageChannel(uint32_t channel);//отправить пакеты в канал
  bool TransmittPackage(const CPCI7841CANPackage &cPCI7841CANPackage);//добавить пакет для отправки
};
#endif


