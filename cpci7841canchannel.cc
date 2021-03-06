//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "cpci7841canchannel.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
CPCI7841CANChannel::CPCI7841CANChannel(bool extended_mode,uint32_t arbitration,uint32_t arbitration_mask,CAN_SPEED speed,CPCI7841UserSpeed cPCI7841UserSpeed_Set)
{
 ExtendedMode=extended_mode;
 Arbitration=arbitration;
 ArbitrationMask=arbitration_mask;
 Speed=speed;
 cPCI7841UserSpeed=cPCI7841UserSpeed_Set;
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
CPCI7841CANChannel::~CPCI7841CANChannel()
{
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//получить, расширенный ли режим
//----------------------------------------------------------------------------------------------------
bool CPCI7841CANChannel::IsExtendedMode(void) const
{
 return(ExtendedMode);
}
//----------------------------------------------------------------------------------------------------
//получить, соответствует ли скорость заданной
//----------------------------------------------------------------------------------------------------
bool CPCI7841CANChannel::IsSpeed(CAN_SPEED speed) const
{
 if (Speed==speed) return(true);
 return(false);
}
