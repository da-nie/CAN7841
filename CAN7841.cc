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
 
 uint32_t arbitration=0xFFFFFFFF;
 uint32_t arbitration_mask=0xFFFFFFFF;//маска инверсная?
 
 C7841CANChannel c7841CANChannel(true,arbitration,arbitration_mask,C7841CANChannel::CAN7841_SPEED_500KBS);
 
 c7841.CANConfig(0,c7841CANChannel);
 c7841.CANConfig(1,c7841CANChannel);
 
 C7841CANPackage c7841CANPackage(0,false,8);
 for(uint8_t n=0;n<c7841CANPackage.Length;n++)c7841CANPackage.Data[n]=n;
 
 c7841.CANInterruptAttach();
 while(1)
 {  
  //задаём таймер ожидания прерывания
  sigevent event_timeout;//событие "время вышло"
  uint64_t timeout=10000;
  SIGEV_UNBLOCK_INIT(&event_timeout);
  TimerTimeout(CLOCK_REALTIME,_NTO_TIMEOUT_INTR,&event_timeout,&timeout,NULL);//тайм-аут ядра на ожидание прерывания
  //ждём прерывания
  if (InterruptWait(0,NULL)<0) continue;//прерывание не поступало
 // if (c7841.SendPackage(0,c7841CANPackage)==true) printf("Send package ok.\r\n");
  printf("----------\r\n");
  std::vector<C7841CANPackage> vector_C7841CANPackage;
  if (c7841.GetPackage(0,vector_C7841CANPackage)==true)
  {
   printf("Channel 1\r\n");
   size_t size=vector_C7841CANPackage.size();
   for(size_t v=0;v<size;v++)
   {
    printf("Arb:%08x ",vector_C7841CANPackage[v].Arbitration);
    printf("Len:%02x Data:",vector_C7841CANPackage[v].Length);
    for(uint8_t n=0;n<vector_C7841CANPackage[v].Length;n++) printf("%02x ",vector_C7841CANPackage[v].Data[n]);
    printf("\r\n");
   }
  }
  if (c7841.GetPackage(1,vector_C7841CANPackage)==true)
  {
   printf("Channel 2\r\n");
   size_t size=vector_C7841CANPackage.size();
   for(size_t v=0;v<size;v++)
   {
    printf("Arb:%08x ",vector_C7841CANPackage[v].Arbitration);
    printf("Len:%02x Data:",vector_C7841CANPackage[v].Length);
    for(uint8_t n=0;n<vector_C7841CANPackage[v].Length;n++) printf("%02x ",vector_C7841CANPackage[v].Data[n]);
    printf("\r\n");
   }
  }
  c7841.ClearIRQ();
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