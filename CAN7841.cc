 //****************************************************************************************************
//Программа-эмулятор комплекса ИПК
//****************************************************************************************************

//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************

#include <stdint.h>
#include <sys/neutrino.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "c7841.h"

//****************************************************************************************************
//сигналы
//****************************************************************************************************
void Signal_Exit(int32_t s_code);//уничтожение процесса

//****************************************************************************************************
//глобальные переменные
//****************************************************************************************************

//****************************************************************************************************
//прототипы функций
//****************************************************************************************************

//****************************************************************************************************
//функции
//****************************************************************************************************

//----------------------------------------------------------------------------------------------------
//основная функция программы
//----------------------------------------------------------------------------------------------------
int main(int32_t argc, char *argv[]) 
{
 //разрешаем доступ потоку к ресурсам аппаратуры
 ThreadCtl(_NTO_TCTL_IO,NULL);
 //подключим сигналы
 printf("Connecting signals.\r\n");
 signal(SIGINT,Signal_Exit);
 signal(SIGTERM,Signal_Exit);
 signal(SIGKILL,Signal_Exit);
 signal(SIGCLD,Signal_Exit);
 signal(SIGQUIT,Signal_Exit); 

 C7841 c7841;
 c7841.Init();
 
 S7841Port s7841Port;
 s7841Port.ExtendedMode=true;
 s7841Port.Arbitration=0xFFFFFFFF;
 s7841Port.ArbitrationMask=0xFFFFFFFF;//маска инверсная?
 s7841Port.Speed=CAN7841_SPEED_500KBS;
 
 c7841.CANConfig(0,s7841Port);
 c7841.CANConfig(1,s7841Port);
 
 
 S7841CANPackage s7841CANPackage;
 s7841CANPackage.Arbitration=0;
 s7841CANPackage.RTR=false;
 s7841CANPackage.Length=8;
 for(uint8_t n=0;n<s7841CANPackage.Length;n++) s7841CANPackage.Data[n]=n;
 while(1)
 {  
 // if (c7841.SendPackage(0,s7841CANPackage)==true) printf("Send package ok.\r\n");
  delay(1);
  if (c7841.GetPackage(0,s7841CANPackage)==true)
  {
   printf("Arb:%08x ",s7841CANPackage.Arbitration);
   printf("Len:%02x Data:",s7841CANPackage.Length);
   for(uint8_t n=0;n<s7841CANPackage.Length;n++) printf("%02x ",s7841CANPackage.Data[n]);
   printf("\r\n");
  }
  if (c7841.GetPackage(1,s7841CANPackage)==true)
  {
   printf("Arb:%08x ",s7841CANPackage.Arbitration);
   printf("Len:%02x Data:",s7841CANPackage.Length);
   for(uint8_t n=0;n<s7841CANPackage.Length;n++) printf("%02x ",s7841CANPackage.Data[n]);
   printf("\r\n");
  }
 }
 
 return(EXIT_SUCCESS);
}

//****************************************************************************************************
//сигналы
//****************************************************************************************************

//----------------------------------------------------------------------------------------------------
//уничтожение подчинённого процесса
//----------------------------------------------------------------------------------------------------
void Signal_Exit(int32_t s_code)
{
 printf("Receive signal TERM.\r\n");
 exit(EXIT_SUCCESS);
}