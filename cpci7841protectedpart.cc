//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "cpci7841protectedpart.h"
#include <string.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
CPCI7841ProtectedPart::CPCI7841ProtectedPart(uint32_t receiver_buffer_size,uint32_t transmitter_buffer_size)
{
 ISR_CANInterruptID=-1;
 DeviceHandle=NULL;
 
 cRingBuffer_Receiver_Ptr.Set(new CRingBuffer<CPCI7841CANPackage>(receiver_buffer_size));
 for(uint32_t n=0;n<CAN_CHANNEL_AMOUNT;n++)
 {
  cRingBuffer_Transmitter_Ptr[n].Set(new CRingBuffer<CPCI7841CANPackage>(transmitter_buffer_size));
  TransmittIsDone[n]=true;
  BussOffTime[n]=0;
 }
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
CPCI7841ProtectedPart::~CPCI7841ProtectedPart()
{
 ReleasePCI();	
 cRingBuffer_Receiver_Ptr.Release();
 for(uint32_t n=0;n<CAN_CHANNEL_AMOUNT;n++) cRingBuffer_Transmitter_Ptr[n].Release();
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//получить, можно ли выполнять действия с каналом
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::IsWaitable(uint32_t channel)
{
 uint64_t cps=SYSPAGE_ENTRY(qtime)->cycles_per_sec;     
 long double delta=(1000.0*(ClockCycles()-BussOffTime[channel]))/cps;
 if (delta<static_cast<long double>(CAN_CHANNEL_BUSS_OFF_DELAY_MS)) return(true);//слишком рано
 return(false);
}
//----------------------------------------------------------------------------------------------------
//запустить счётчик запрета операций с каналом
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::SetWaitState(uint32_t channel)
{
 BussOffTime[channel]=ClockCycles();
}

//----------------------------------------------------------------------------------------------------
//обработчик прерывания канала
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::OnInterruptChannel(uint32_t channel)
{
 if (IsEnabled()==false) return;
 uint64_t offset=channel*128;
 uint8_t flag=ReadRegister(offset+3);
 if (flag&0x01) ReceiveInterrupt(channel);//получаем пакеты с канала
 if (flag&0x02) TransmittInterrupt(channel);//передача пакета завершена, отправляем новые пакеты в канал
}

//----------------------------------------------------------------------------------------------------
//получить пакеты с канала
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::ReceiveInterrupt(uint32_t channel)
{
 CPCI7841CANPackage cPCI7841CANPackage;
 uint64_t offset=channel*128;
 while(1) 
 {
  uint8_t data=ReadRegister(offset+2);
  if ((data&0x01)!=0x01) break;
  data=ReadRegister(offset+16);
  cPCI7841CANPackage.Length=data&0x0f;
  if (data&0x40) cPCI7841CANPackage.RTR=true;
            else cPCI7841CANPackage.RTR=false;
  uint64_t data_begin=0;             
  if (data&0x80)//расширенный режим
  {
   data_begin=21;
   data=ReadRegister(offset+17);
   cPCI7841CANPackage.Arbitration=data;
   cPCI7841CANPackage.Arbitration<<=8;
   data=ReadRegister(offset+18);
   cPCI7841CANPackage.Arbitration|=data;
   cPCI7841CANPackage.Arbitration<<=8;
   data=ReadRegister(offset+19);
   cPCI7841CANPackage.Arbitration|=data;
   cPCI7841CANPackage.Arbitration<<=8;
   data=ReadRegister(offset+20);
   cPCI7841CANPackage.Arbitration|=data;
   cPCI7841CANPackage.Arbitration>>=3;
   cPCI7841CANPackage.ExtendedMode=true;
  }
  else 
  {
   data_begin=19;
   data=ReadRegister(offset+17);
   cPCI7841CANPackage.Arbitration=data;
   cPCI7841CANPackage.Arbitration<<=8;
   data=ReadRegister(offset+18);
   cPCI7841CANPackage.Arbitration=data;
   cPCI7841CANPackage.Arbitration>>=5;
   cPCI7841CANPackage.ExtendedMode=false;
  }
  if (cPCI7841CANPackage.Length>8) cPCI7841CANPackage.Length=8;
  for(uint8_t n=0;n<cPCI7841CANPackage.Length;n++)
  {
   data=ReadRegister(offset+data_begin+n);
   cPCI7841CANPackage.Data[n]=data;
  }
  cPCI7841CANPackage.ChannelIndex=channel;
  cRingBuffer_Receiver_Ptr.Get()->Push(cPCI7841CANPackage);
  
  //очищаем буфер приёма
  WriteRegister(offset+1,0x04);
 }
}
//----------------------------------------------------------------------------------------------------
//отправить пакеты в канал
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::TransmittInterrupt(uint32_t channel)
{
 //отмечаем, что можно передавать пакеты	
 TransmittIsDone[channel]=true;
 //выполняем передачу пакетов, если они ещё есть в очереди
 TransmittProcessing(channel);
}
//----------------------------------------------------------------------------------------------------
//отправить пакет
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::StartTransmittPackage(const CPCI7841CANPackage &cPCI7841CANPackage)
{
 uint32_t channel=cPCI7841CANPackage.ChannelIndex;
 uint64_t offset=channel*128;
 //узнаем состояние передачи
 SPCI7841StatusReg sPCI7841StatusReg=GetChannelStatus(channel);
 if (sPCI7841StatusReg.TxBuffer==0) return(false);//передача невозможна
 uint64_t data_begin=0;
 if (cPCI7841CANPackage.ExtendedMode==false)//обычный режим
 {
  data_begin=19;
  if (cPCI7841CANPackage.RTR==false) WriteRegister(offset+16,cPCI7841CANPackage.Length&0x0f);
                                else WriteRegister(offset+16,(cPCI7841CANPackage.Length&0x0f)|0x40);
  WriteRegister(offset+18,cPCI7841CANPackage.Arbitration<<5);
  WriteRegister(offset+17,cPCI7841CANPackage.Arbitration>>3);  
 }
 else//расширенный арбитраж
 {
  data_begin=21;
  if (cPCI7841CANPackage.RTR==false) WriteRegister(offset+16,(cPCI7841CANPackage.Length&0x0f)|0x80);
                                else WriteRegister(offset+16,(cPCI7841CANPackage.Length&0x0f)|0x40);
  WriteRegister(offset+17,cPCI7841CANPackage.Arbitration>>21);
  WriteRegister(offset+18,cPCI7841CANPackage.Arbitration>>13);  
  WriteRegister(offset+19,cPCI7841CANPackage.Arbitration>>5);
  WriteRegister(offset+20,cPCI7841CANPackage.Arbitration<<3);
 } 
 for(uint8_t n=0;n<cPCI7841CANPackage.Length;n++)
 {
  WriteRegister(offset+data_begin+n,cPCI7841CANPackage.Data[n]);	
 } 
 //запускаем передачу
 WriteRegister(offset+1,0x01);
 TransmittIsDone[channel]=false;
 return(true);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//сбросить флаг прерывания
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::ClearIRQ(void)
{
 uint64_t offset=0*128;
 ReadRegister(offset+3);//сброс производится чтением регистра
 offset=1*128;
 ReadRegister(offset+3);//сброс производится чтением регистра
 InterruptUnmask(GetIRQ(),ISR_CANInterruptID);
}
//----------------------------------------------------------------------------------------------------
//получить номер прерывания
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841ProtectedPart::GetIRQ(void)
{
 return(PCI_Dev_Info.Irq);
}
//----------------------------------------------------------------------------------------------------
//прочесть регистр
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841ProtectedPart::ReadRegister(uint64_t reg)
{
 return(vector_CIOControl[1].In8(reg)); 	
}
//----------------------------------------------------------------------------------------------------
//записать регистр
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::WriteRegister(uint64_t reg,uint8_t value)
{
 return(vector_CIOControl[1].Out8(reg,value)); 	
}
//----------------------------------------------------------------------------------------------------
//получить, подключена ли плата
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::IsEnabled(void)
{
 if (DeviceHandle==NULL) return(false);
 return(true);
}
//----------------------------------------------------------------------------------------------------
//подключить прерывание
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::CANInterruptAttach(sigevent *CANInterruptEvent)
{
 //подключаем прерывание
 SIGEV_INTR_INIT(CANInterruptEvent);//настраиваем событие  
 ISR_CANInterruptID=InterruptAttachEvent(GetIRQ(),CANInterruptEvent,_NTO_INTR_FLAGS_END);
 if (ISR_CANInterruptID<0) return(false);
 return(true);
}
//----------------------------------------------------------------------------------------------------
//отключить прерывание
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::CANInterruptDetach(void)
{
 if (ISR_CANInterruptID>=0) InterruptDetach(ISR_CANInterruptID);//отключим прерывание
 ISR_CANInterruptID=-1; 
}

//----------------------------------------------------------------------------------------------------
//получить, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::IsExitThread(void)
{
 return(ExitThread);	
}
//----------------------------------------------------------------------------------------------------
//задать, нужно ли выходить из потока
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::SetExitThread(bool state)
{
 ExitThread=state;
}
//----------------------------------------------------------------------------------------------------
//инициализировать PCI
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::InitPCI(uint32_t vendor_id,uint32_t device_id,uint32_t device_index)
{
 ReleasePCI();
 memset(&PCI_Dev_Info,0,sizeof(pci_dev_info));  
 PCI_Dev_Info.VendorId=vendor_id;
 PCI_Dev_Info.DeviceId=device_id;
 DeviceHandle=pci_attach_device(NULL,0,device_index,&PCI_Dev_Info);
 if (DeviceHandle==NULL) return(false);
 if ((pci_attach_device(DeviceHandle,PCI_INIT_ALL,device_index,&PCI_Dev_Info))==0)
 {
  pci_detach_device(DeviceHandle); 	
  DeviceHandle=NULL;
  return(false);
 }
 for(uint8_t bar=0;bar<6;bar++)
 {
  uint32_t addr=PCI_Dev_Info.CpuBaseAddress[bar]; 	
  if (addr==0x00000000) continue;
  if (!(PCI_IS_IO(addr))) continue;
  //физический адрес
  uint64_t p_port=PCI_IO_ADDR(addr);
  uint64_t size=PCI_Dev_Info.BaseAddressSize[bar];
  //создаём класс работы с адресами
  vector_CIOControl.push_back(CIOControl(p_port,size));
 } 
 if (vector_CIOControl.size()!=3) return(false);
 for(uint32_t n=0;n<CAN_CHANNEL_AMOUNT;n++) BussOffTime[n]=0;
 return(true); 
}
//----------------------------------------------------------------------------------------------------
//освободить PCI
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::ReleasePCI(void)
{
 vector_CIOControl.clear(); 
 if (DeviceHandle!=NULL) pci_detach_device(DeviceHandle);
 DeviceHandle=NULL;
}
//----------------------------------------------------------------------------------------------------
//настроить порт
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::CANConfig(uint32_t channel,const CPCI7841CANChannel &cPCI7841CANChannel_Set)
{
 cPCI7841CANChannel[channel]=cPCI7841CANChannel_Set;
 const CPCI7841CANChannel &cPCI7841CANChannel_local=cPCI7841CANChannel_Set;
  
 uint8_t tmp;
 uint64_t offset=channel*128;
 //запрещаем прерывания
 tmp=ReadRegister(offset+4);
 WriteRegister(offset+4,0x00);
 //выполняем сброс канала
 WriteRegister(offset+0,0x01);
 //настраиваем CBP=1, TX1 и Clk отключены 
 WriteRegister(offset+31,0xC8);
  
 //стандартный режим с одной маской
 uint32_t arbitration=cPCI7841CANChannel_local.Arbitration;
 uint32_t arbitration_mask=cPCI7841CANChannel_local.ArbitrationMask;
  
 if (cPCI7841CANChannel_local.IsExtendedMode()==false)//обычный режим
 {
  //задаём арбитраж
  WriteRegister(offset+16,static_cast<uint8_t>(arbitration>>3));
  WriteRegister(offset+17,(static_cast<uint8_t>(arbitration<<5))|0x10);
  WriteRegister(offset+18,0x00);
  WriteRegister(offset+19,0x00);
  //задаём маску
  WriteRegister(offset+20,static_cast<uint8_t>(arbitration_mask>>3));
  WriteRegister(offset+21,(static_cast<uint8_t>(arbitration_mask<<5))|0x10);
  WriteRegister(offset+22,0xff);
  WriteRegister(offset+23,0xff);
 }
 else//расширенный режим
 {
  //задаём арбитраж 	
  WriteRegister(offset+16,static_cast<uint8_t>(arbitration>>21));
  WriteRegister(offset+17,static_cast<uint8_t>(arbitration>>13));
  WriteRegister(offset+18,static_cast<uint8_t>(arbitration>>5));
  WriteRegister(offset+19,static_cast<uint8_t>(arbitration<<3));
  //задаём маску
  WriteRegister(offset+20,static_cast<uint8_t>(arbitration_mask>>21));
  WriteRegister(offset+21,static_cast<uint8_t>(arbitration_mask>>13));
  WriteRegister(offset+22,static_cast<uint8_t>(arbitration_mask>>5));
  WriteRegister(offset+23,static_cast<uint8_t>(arbitration_mask<<3)|0x04);//добавляем RTR
 }
 //настраиваем скорость
 if (cPCI7841CANChannel_local.IsSpeed(CPCI7841CANChannel::CAN_SPEED_125KBS)==true)
 {
  WriteRegister(offset+6,0x03);
  WriteRegister(offset+7,0x1c);
 }
 if (cPCI7841CANChannel_local.IsSpeed(CPCI7841CANChannel::CAN_SPEED_250KBS)==true)
 {
  WriteRegister(offset+6,0x01);
  WriteRegister(offset+7,0x1c);
 }
 if (cPCI7841CANChannel_local.IsSpeed(CPCI7841CANChannel::CAN_SPEED_500KBS)==true)
 {
  WriteRegister(offset+6,0x00);
  WriteRegister(offset+7,0x1c);
 }
 if (cPCI7841CANChannel_local.IsSpeed(CPCI7841CANChannel::CAN_SPEED_1MBS)==true)
 {
  WriteRegister(offset+6,0x00);
  WriteRegister(offset+7,0x14);
 }
 if (cPCI7841CANChannel_local.IsSpeed(CPCI7841CANChannel::CAN_SPEED_USER)==true)
 {
  const CPCI7841UserSpeed &cPCI7841UserSpeed=cPCI7841CANChannel_local.cPCI7841UserSpeed;
  	
  WriteRegister(offset+6,cPCI7841UserSpeed.BRP|(cPCI7841UserSpeed.SJW<<6));
  WriteRegister(offset+7,(cPCI7841UserSpeed.SAM<<7)|(cPCI7841UserSpeed.TSeg2<<4)|cPCI7841UserSpeed.TSeg1);
 }
 //настраиваем регистр управления: нормальный режим, тяни-толкай
 WriteRegister(offset+8,0xfa);
 //разрешаем прерывания по приёму и передаче
 WriteRegister(offset+0,0x08);
 WriteRegister(offset+4,tmp|0x03); 
 return(true);
}
//----------------------------------------------------------------------------------------------------
//получить принятый пакет
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::GetReceivedPackage(CPCI7841CANPackage &cPCI7841CANPackage)
{
 return(cRingBuffer_Receiver_Ptr.Get()->Pop(cPCI7841CANPackage));
}
//----------------------------------------------------------------------------------------------------
//добавить пакет для отправки
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::TransmittPackage(const CPCI7841CANPackage &cPCI7841CANPackage)
{	
 cRingBuffer_Transmitter_Ptr[cPCI7841CANPackage.ChannelIndex].Get()->Push(cPCI7841CANPackage);
 return(true);
}
//----------------------------------------------------------------------------------------------------
//обработчик прерывания
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::OnInterrupt(void)
{
 for(uint32_t n=0;n<CPCI7841ProtectedPart::CAN_CHANNEL_AMOUNT;n++) OnInterruptChannel(n);
 ClearIRQ();
}




//----------------------------------------------------------------------------------------------------
//очистить буфер передатчика
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::ClearTransmitterBuffer(uint32_t channel)
{
 uint64_t offset=channel*128;
 WriteRegister(offset+1,0x02);
 
 cRingBuffer_Transmitter_Ptr[channel].Get()->Reset();
 TransmittIsDone[channel]=true;
}
//----------------------------------------------------------------------------------------------------
//очистить буфер приёмника
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::ClearReceiverBuffer(uint32_t channel)
{
 //буфер приёмника в данный момент один, так что очищаем его целиком и так же очищаем буфер платы для канала
 uint64_t offset=channel*128;
 while (1)
 {
  uint8_t b=ReadRegister(offset+29);
  if (b==0) break;
  WriteRegister(offset+1,0x04); 	
 }
 cRingBuffer_Receiver_Ptr.Get()->Reset();
}

//----------------------------------------------------------------------------------------------------
//выполнить передачу данных, если это возможно
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::TransmittProcessing(uint32_t channel)
{
 if (TransmittIsDone[channel]==false) return;
 if (IsWaitable(channel)==true) return;//передача заблокирована на время Buss-off
 //читаем список для передачи
 CPCI7841CANPackage cPCI7841CANPackage; 
 if (cRingBuffer_Transmitter_Ptr[channel].Get()->Pop(cPCI7841CANPackage)==false) return;//нечего передавать
 //запускаем передачу
 StartTransmittPackage(cPCI7841CANPackage);
}

//----------------------------------------------------------------------------------------------------
//разрешить приём данных
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::EnableReceiver(uint32_t channel)
{
 uint64_t offset=channel*128;
 uint8_t byte=ReadRegister(offset+4);
 WriteRegister(offset+4,byte|0x01);
}
//----------------------------------------------------------------------------------------------------
//запретить приём данных
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::DisableReceiver(uint32_t channel)
{
 uint64_t offset=channel*128;	
 uint8_t byte=ReadRegister(offset+4);
 WriteRegister(offset+4,byte&(0xff^0x01));
}
//----------------------------------------------------------------------------------------------------
//очистить ошибки
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::ClearOverrun(uint32_t channel)
{
 uint64_t offset=channel*128;
 //очищаем ошибки
 WriteRegister(offset+1,0x08);
 //разрешаем прерывания ошибки и приёма данных
 uint8_t byte=ReadRegister(offset+4);
 WriteRegister(offset+4,byte|0x09);
 //блокируем передачу
 SetWaitState(channel); 
}
//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма арбитража
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841ProtectedPart::GetArbitrationLostBit(uint32_t channel)
{
 uint64_t offset=channel*128;
 return(ReadRegister(offset+11));
}

//----------------------------------------------------------------------------------------------------
//получить количество ошибок приёма
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841ProtectedPart::GetReceiveErrorCounter(uint32_t channel)
{
 uint64_t offset=channel*128;
 return(ReadRegister(offset+14));
}
//----------------------------------------------------------------------------------------------------
//получить количество ошибок передачи
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841ProtectedPart::GetTransmittErrorCounter(uint32_t channel)
{
 uint64_t offset=channel*128;
 return(ReadRegister(offset+15));
}
//----------------------------------------------------------------------------------------------------
//получить состояние канала
//----------------------------------------------------------------------------------------------------
CPCI7841ProtectedPart::SPCI7841StatusReg CPCI7841ProtectedPart::GetChannelStatus(uint32_t channel)
{
 SPCI7841StatusReg sPCI7841StatusReg; 
 uint64_t offset=channel*128; 
 *(reinterpret_cast<uint8_t*>(&sPCI7841StatusReg))=ReadRegister(offset+2);
 return(sPCI7841StatusReg);
}
//----------------------------------------------------------------------------------------------------
//получить ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841ProtectedPart::GetErrorWarningLimit(uint32_t channel)
{
 uint64_t offset=channel*128;
 return(ReadRegister(offset+13));	
}
//----------------------------------------------------------------------------------------------------
//задать ограничение на количество ошибок
//----------------------------------------------------------------------------------------------------
void CPCI7841ProtectedPart::SetErrorWarningLimit(uint32_t channel,uint8_t value)
{
 uint64_t offset=channel*128;
 uint8_t byte=ReadRegister(offset+0);  	
 //входим в режим сброса
 WriteRegister(offset+0,0);
 WriteRegister(offset+13,value);
 WriteRegister(offset+0,byte);
}
//----------------------------------------------------------------------------------------------------
//получить код ошибки
//----------------------------------------------------------------------------------------------------
uint8_t CPCI7841ProtectedPart::GetErrorCode(uint32_t channel)
{
 uint64_t offset=channel*128;
 return(ReadRegister(offset+12));
}
//----------------------------------------------------------------------------------------------------
//проверить работоспособность контроллера канала и сбросить канал при необходимости
//----------------------------------------------------------------------------------------------------
bool CPCI7841ProtectedPart::BussOffControl(uint32_t channel)
{
 if (IsWaitable(channel)==true) return(true);//мы уже отрабатываем Buss Off	
 
 SPCI7841StatusReg sPCI7841StatusReg;
 sPCI7841StatusReg=GetChannelStatus(channel);
  
 bool print=false;
 if (sPCI7841StatusReg.DataOverrun!=0) print=true;
 if (sPCI7841StatusReg.ErrorStatus!=0) print=true;
 if (sPCI7841StatusReg.BusStatus!=0) print=true;
 
 
 print=false;  
 if (print==true)
 {
  printf("Channel:%i ",channel);
  printf("DataOverrun:%i ",sPCI7841StatusReg.DataOverrun);
  printf("ErrorStatus:%i ",sPCI7841StatusReg.ErrorStatus);
  printf("BusStatus:%i ",sPCI7841StatusReg.BusStatus);
  printf("\r\n");   	
 }
   
 if (sPCI7841StatusReg.DataOverrun!=0) ClearOverrun(channel);
 
 if (sPCI7841StatusReg.BusStatus!=0)
 {
  static uint32_t counter=0;
  time_t time_main=time(NULL);
  tm *tm_main=localtime(&time_main);
  printf("[%02i.%02i.%04i %02i:%02i:%02i] ChannelIndex:%lx Buss Off: %ld!\r\n",tm_main->tm_mday,tm_main->tm_mon+1,tm_main->tm_year+1900,tm_main->tm_hour,tm_main->tm_min,tm_main->tm_sec,channel,static_cast<long>(counter));
  counter++;
  CANConfig(channel,cPCI7841CANChannel[channel]);
  SetWaitState(channel);
  return(false);
 } 	
 return(true);
}