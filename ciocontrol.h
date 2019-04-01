#ifndef C_IO_CONTROL_H
#define C_IO_CONTROL_H

//****************************************************************************************************
//Класс управления портами ввода/вывода
//****************************************************************************************************

//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include <stdint.h>


//****************************************************************************************************
//Класс управления портами ввода/вывода
//****************************************************************************************************

 class CIOControl
 {
  //-переменные-----------------------------------------------------------------------------------------
  private: 	
   uint64_t PhisicalPort;//физический адрес
   uintptr_t VirtualPort;//виртуальный адрес
   uint64_t Size;//диапазон портов
  //-конструктор----------------------------------------------------------------------------------------
  public:
   CIOControl(void);
   CIOControl(uint64_t phisical_port,uint64_t size);
   CIOControl(const CIOControl &cIOControl);
  //-деструктор-----------------------------------------------------------------------------------------
   ~CIOControl(void);
  //-открытые функции-----------------------------------------------------------------------------------
  public:     
   CIOControl& operator=(const CIOControl &cIOControl);//оператор =
   void Release(void);//освободить ресурсы
   bool SetPort(uint64_t phisical_port,uint64_t size);//задать порты
   bool IsPort(uint64_t offset);//получить, есть ли данный порт
   uintptr_t GetPort(void);//получить порт
   uint64_t GetSize(void);//получить размер области портов
   bool Out8(uint64_t offset,uint8_t value);//записать байт в порт
   uint8_t In8(uint64_t offset);//считать байт из порта
  //-закрытые функции-----------------------------------------------------------------------------------
 };

#endif
