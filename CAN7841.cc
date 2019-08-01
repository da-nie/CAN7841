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

#include "cpci7841.h"

//****************************************************************************************************
//сигналы
//****************************************************************************************************
void Signal_Exit(int32_t s_code);//уничтожение процесса

//****************************************************************************************************
//глобальные переменные
//****************************************************************************************************

CPCI7841 cPCI7841;//класс платы требует ОБЯЗАТЕЛЬНОГО вызова деструктора при получении сигнала на завершение. Поэтому в данном случае он будет глобальным. На стеке деструктор не вызовется.

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

 cPCI7841.Init();
 
 uint32_t arbitration=0;//0xFFFFFFFF;
 uint32_t arbitration_mask=0xFFFFFFFF;//маска инверсная?
 
 CPCI7841CANChannel cPCI7841CANChannel(true,arbitration,arbitration_mask,CPCI7841CANChannel::CAN_SPEED_500KBS);
 
 cPCI7841.CANConfig(0,cPCI7841CANChannel);
 cPCI7841.CANConfig(1,cPCI7841CANChannel);
 
 CPCI7841CANPackage cPCI7841CANPackage(true,0,false,8,1);
 for(uint8_t n=0;n<cPCI7841CANPackage.Length;n++) cPCI7841CANPackage.Data[n]=n; 

 int32_t index=0;
 while(1)
 {  
  cPCI7841CANPackage.Data[0]=index&0xff;
  cPCI7841CANPackage.ChannelIndex=index%2;
  cPCI7841.TransmittPackage(cPCI7841CANPackage);
  index++;
  delay(1000);
  std::vector<CPCI7841CANPackage> vector_CPCI7841CANPackage;
  cPCI7841.GetAllReceivedPackage(vector_CPCI7841CANPackage);
  size_t size=vector_CPCI7841CANPackage.size();
  if (size>0) 
  {
   printf("Received %i \r\n",size);
   for(size_t v=0;v<size;v++)
   {
    printf("Channel %i ",vector_CPCI7841CANPackage[v].ChannelIndex);
    printf("Arb:%08x ",vector_CPCI7841CANPackage[v].Arbitration);
    printf("Len:%02x Data:",vector_CPCI7841CANPackage[v].Length);
    for(uint8_t n=0;n<vector_CPCI7841CANPackage[v].Length;n++) printf("%02x ",vector_CPCI7841CANPackage[v].Data[n]);
    printf("\r\n");
   }
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