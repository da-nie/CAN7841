#ifndef C_7841_USER_SPEED_H
#define C_7841_USER_SPEED_H

//****************************************************************************************************
//Класс настройки пользовательской скорости
//****************************************************************************************************

//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include <stdint.h>

//****************************************************************************************************
//Класс настройки пользовательской скорости
//****************************************************************************************************

class C7841UserSpeed
{
 //-переменные-----------------------------------------------------------------------------------------
 public:  	
  uint8_t BRP;
  uint8_t TSeg1;
  uint8_t TSeg2;	
  uint8_t SJW;
  uint8_t SAM;
 //-конструктор----------------------------------------------------------------------------------------
 public:
  C7841UserSpeed(uint8_t brp=0,uint8_t tseg1=0,uint8_t tseg2=0,uint8_t sjw=0,uint8_t sam=0);
 //-деструктор-----------------------------------------------------------------------------------------
  ~C7841UserSpeed(void);
 //-открытые функции-----------------------------------------------------------------------------------
 public:
 //-закрытые функции-----------------------------------------------------------------------------------
 private: 
};

#endif

