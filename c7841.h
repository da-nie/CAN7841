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
#include "ciocontrol.h"


//****************************************************************************************************
//структуры
//****************************************************************************************************

//скорость
enum CAN7841_SPEED
{
 CAN7841_SPEED_125KBS,
 CAN7841_SPEED_250KBS,
 CAN7841_SPEED_500KBS,
 CAN7841_SPEED_1MBS,
 CAN7841_SPEED_USER 
};

#pragma pack(1)
//структура порта CAN
struct S7841Port
{
 bool ExtendedMode;//расширенный режим 
 uint32_t Arbitration;//арбитраж
 uint32_t ArbitrationMask;//маска арбитража
 CAN7841_SPEED Speed;//скорость 
 //структура настройки пользовательской скорости
 struct SUserSpeed
 {
  uint8_t BRP;
  uint8_t TSeg1;
  uint8_t TSeg2;	
  uint8_t SJW;
  uint8_t SAM;
 } sUserSpeed;
};

//структура пакета CAN
struct S7841CANPackage
{
 uint32_t Arbitration;       //  CAN id
 bool RTR;           //  RTR bit
 uint8_t  Length;           //  Data length
 uint8_t  Data[8];       //  Data
};

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
  static const uint32_t CAN7841_VendorID=0x144A;
  static const uint32_t CAN7841_DeviceID=0x7841;
    
  std::vector<CIOControl> vector_CIOControl;//массив адресов ввода-вывода платы
    
  pci_dev_info PCI_Dev_Info;//описатель устройства
   
  uint32_t DeviceIndex;//номер устройства на шине
  int PCI_Handle;//дескриптор PCI
  void *DeviceHandle;//дескриптор устройства
  
  S7841Port s7841Port;//настройки порта
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
  
  void CANConfig(uint32_t channel,const S7841Port &s7841Port_Set);//настроить порт
  bool SendPackage(uint32_t channel,const S7841CANPackage &s7841CANPackage);//отправить пакет
  bool GetPackage(uint32_t channel,S7841CANPackage &s7841CANPackage);//получить пакет
 //-закрытые функции-----------------------------------------------------------------------------------
};

#endif

