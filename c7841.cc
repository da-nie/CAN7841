//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "c7841.h"
#include <string.h>
#include <unistd.h>

#include <stdio.h>

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
C7841::C7841(uint32_t device_index)
{
 DeviceIndex=device_index;
 PCI_Handle=-1;	
 DeviceHandle=NULL;
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
C7841::~C7841()
{
 Release();	
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//найти плату на шине PCI и инициализировать
//----------------------------------------------------------------------------------------------------
bool C7841::Init(void)
{
 //ищем устройство
 memset(&PCI_Dev_Info,0,sizeof(pci_dev_info));
 PCI_Handle=pci_attach(0);
 if (PCI_Handle<0) return(false);
 int rescan=pci_rescan_bus();
 if (rescan!=PCI_SUCCESS) return(false);
 PCI_Dev_Info.VendorId=CAN7841_VendorID;
 PCI_Dev_Info.DeviceId=CAN7841_DeviceID;
 DeviceHandle=pci_attach_device(NULL,0,DeviceIndex,&PCI_Dev_Info);
 if (DeviceHandle==NULL) return(false);
 if((pci_attach_device(DeviceHandle,PCI_INIT_ALL,DeviceIndex,&PCI_Dev_Info))==0)
 {
  pci_detach_device(DeviceHandle); 	
  DeviceHandle=NULL;
  return(false);
 }
 //подключаем порты 
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
  CIOControl &cIOControl=vector_CIOControl[0];
  if (cIOControl.IsPort(0))
  {
   //printf("Found addr:0x%x ok.\r\n",p_port);
  }
  else
  {
   //printf("Found addr:0x%x error!\r\n",p_port);
  }
 }
 
 if (vector_CIOControl.size()!=3)
 {    	
  Release();
  return(false); 	
 }
 
 return(true);
}
//----------------------------------------------------------------------------------------------------
//освободить ресурсы
//----------------------------------------------------------------------------------------------------
void C7841::Release(void)
{	
 vector_CIOControl.clear();
 if (DeviceHandle!=NULL) pci_detach_device(DeviceHandle);
 if (PCI_Handle>=0) pci_detach(PCI_Handle);
 PCI_Handle=-1;
 DeviceHandle=NULL;
}
//----------------------------------------------------------------------------------------------------
//получить, подключена ли плата
//----------------------------------------------------------------------------------------------------
bool C7841::IsEnabled(void)
{
 if (DeviceHandle==NULL) return(false);
 return(true);
}

//----------------------------------------------------------------------------------------------------
//прочесть регистр
//----------------------------------------------------------------------------------------------------
uint8_t C7841::ReadRegister(uint64_t reg)
{
 if (IsEnabled()==false) return(0);
 return(vector_CIOControl[1].In8(reg)); 	
}
//----------------------------------------------------------------------------------------------------
//записать регистр
//----------------------------------------------------------------------------------------------------
bool C7841::WriteRegister(uint64_t reg,uint8_t value)
{
 if (IsEnabled()==false) return(false);
 return(vector_CIOControl[1].Out8(reg,value)); 	
}

//----------------------------------------------------------------------------------------------------
//настроить порт
//----------------------------------------------------------------------------------------------------
void C7841::CANConfig(uint32_t channel,const S7841Port &s7841Port_Set)
{
 if (IsEnabled()==false) return;
 s7841Port=s7841Port_Set;
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
 
 if (s7841Port.ExtendedMode==false)//обычный режим
 {
  //задаём арбитраж
  WriteRegister(offset+16,static_cast<uint8_t>(s7841Port.Arbitration>>3));
  WriteRegister(offset+17,(static_cast<uint8_t>(s7841Port.Arbitration<<5))|0x10);
  WriteRegister(offset+18,0x00);
  WriteRegister(offset+19,0x00);
  //задаём маску
  WriteRegister(offset+20,static_cast<uint8_t>(s7841Port.ArbitrationMask>>3));
  WriteRegister(offset+21,(static_cast<uint8_t>(s7841Port.ArbitrationMask<<5))|0x10);
  WriteRegister(offset+22,0xff);
  WriteRegister(offset+23,0xff);
 }
 else//расширенный режим
 {
  //задаём арбитраж 	
  WriteRegister(offset+16,static_cast<uint8_t>(s7841Port.Arbitration>>21));
  WriteRegister(offset+17,static_cast<uint8_t>(s7841Port.Arbitration>>13));
  WriteRegister(offset+18,static_cast<uint8_t>(s7841Port.Arbitration>>5));
  WriteRegister(offset+19,static_cast<uint8_t>(s7841Port.Arbitration<<3));
  //задаём маску
  WriteRegister(offset+20,static_cast<uint8_t>(s7841Port.ArbitrationMask>>21));
  WriteRegister(offset+21,static_cast<uint8_t>(s7841Port.ArbitrationMask>>13));
  WriteRegister(offset+22,static_cast<uint8_t>(s7841Port.ArbitrationMask>>5));
  WriteRegister(offset+23,static_cast<uint8_t>(s7841Port.ArbitrationMask<<3)|0x04);//добавляем RTR
 }
 //настраиваем скорость
 if (s7841Port.Speed==CAN7841_SPEED_125KBS)
 {
  WriteRegister(offset+6,0x03);
  WriteRegister(offset+7,0x1c);
 }
 if (s7841Port.Speed==CAN7841_SPEED_250KBS)
 {
  WriteRegister(offset+6,0x01);
  WriteRegister(offset+7,0x1c);
 }
 if (s7841Port.Speed==CAN7841_SPEED_500KBS)
 {
  WriteRegister(offset+6,0x00);
  WriteRegister(offset+7,0x1c);
 }
 if (s7841Port.Speed==CAN7841_SPEED_1MBS)
 {
  WriteRegister(offset+6,0x00);
  WriteRegister(offset+7,0x14);
 }
 if (s7841Port.Speed==CAN7841_SPEED_USER)
 {
  WriteRegister(offset+6,s7841Port.sUserSpeed.BRP|(s7841Port.sUserSpeed.SJW<<6));
  WriteRegister(offset+7,(s7841Port.sUserSpeed.SAM<<7)|(s7841Port.sUserSpeed.TSeg2<<4)|s7841Port.sUserSpeed.TSeg1);
 }
 //настраиваем регистр управления: нормальный режим
 WriteRegister(offset+8,0xfa);
 //разрешаем прерывания по приёму и передаче
 WriteRegister(offset+0,0x08);
 WriteRegister(offset+4,tmp|0x03); 
}

//----------------------------------------------------------------------------------------------------
//отправить пакет
//----------------------------------------------------------------------------------------------------
bool C7841::SendPackage(uint32_t channel,const S7841CANPackage &s7841CANPackage)
{
 if (IsEnabled()==false) return(false);
 uint64_t offset=channel*128;
 usleep(1000);
 //узнаем состояние передачи
 uint8_t v=ReadRegister(offset+2);
 if ((v&0x04)==0) return(false);//передача невозможна
 uint64_t data_begin=0;
 if (s7841Port.ExtendedMode==false)//обычный режим
 {
  data_begin=19; 	
  if (s7841CANPackage.RTR==false) WriteRegister(offset+16,s7841CANPackage.Length&0x0f);
                             else WriteRegister(offset+16,(s7841CANPackage.Length&0x0f)|0x40);
  WriteRegister(offset+18,s7841CANPackage.Arbitration<<5);
  WriteRegister(offset+17,s7841CANPackage.Arbitration>>3);  
 }
 else//расширенный арбитраж
 {
  data_begin=21;
  if (s7841CANPackage.RTR==false) WriteRegister(offset+16,(s7841CANPackage.Length&0x0f)|0x80);
                             else WriteRegister(offset+16,(s7841CANPackage.Length&0x0f)|0x40);
  WriteRegister(offset+17,s7841CANPackage.Arbitration>>21);
  WriteRegister(offset+18,s7841CANPackage.Arbitration>>13);  
  WriteRegister(offset+19,s7841CANPackage.Arbitration>>5);
  WriteRegister(offset+20,s7841CANPackage.Arbitration<<3);
 }
 for(uint8_t n=0;n<s7841CANPackage.Length;n++)
 {
  WriteRegister(offset+data_begin+n,s7841CANPackage.Data[n]);	
 } 
 //запускаем передачу 
 WriteRegister(offset+1,0x01);
 return(true);
}

//----------------------------------------------------------------------------------------------------
//получить пакет
//----------------------------------------------------------------------------------------------------
bool C7841::GetPackage(uint32_t channel,S7841CANPackage &s7841CANPackage)
{
 if (IsEnabled()==false) return(false);
 uint64_t offset=channel*128;
 uint8_t flag=ReadRegister(offset+3);
// printf("Flag:%i\r\n",flag);
 if (flag&0x01)//RIF
 {
  while(1) 
  {
   uint8_t data=ReadRegister(offset+2);
   if ((data&0x01)!=0x01) break;
   data=ReadRegister(offset+16);
   s7841CANPackage.Length=data&0x0f;
   if (data&0x40) s7841CANPackage.RTR=true;
             else s7841CANPackage.RTR=false;
   uint64_t data_begin=0;             
   if (data&0x80)//расширенный режим
   {
	data_begin=21;
    data=ReadRegister(offset+17);
    s7841CANPackage.Arbitration=data;
    s7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+18);
    s7841CANPackage.Arbitration|=data;
    s7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+19);
    s7841CANPackage.Arbitration|=data;
    s7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+20);
    s7841CANPackage.Arbitration|=data;
    s7841CANPackage.Arbitration>>=3;
   }
   else 
   {
    data_begin=19;
    data=ReadRegister(offset+17);
    s7841CANPackage.Arbitration=data;
    s7841CANPackage.Arbitration<<=8;
    data=ReadRegister(offset+18);
    s7841CANPackage.Arbitration=data;
    s7841CANPackage.Arbitration>>=5;
   }
   if (s7841CANPackage.Length>8) s7841CANPackage.Length=8;
   for(uint8_t n=0;n<s7841CANPackage.Length;n++)
   {
    data=ReadRegister(offset+data_begin+n);
    s7841CANPackage.Data[n]=data;
   }
   //очищаем буфер приёма
   WriteRegister(offset+1,0x04);
  }
  return(true);
 }
 return(false);
}

/*
  if(flag & 0x02) TIF
  {
       pDevExt->SendFlag[nport]=1;
       if (pDevExt->sCnt[nport] != 0) {
		//	Standards frame
	   if (pDevExt->PortMode[nport] == 0) {
//printk("KERNAL : TIF %ld   LEN %d  MSG %s\n",nport, pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].len, pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].data);
		dataStart = 19;
		if (pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].rtr==0) {
		    //nport = basenport + 16;
				outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].len & 0x0F),pDevExt->Address1+(128 * nport)+16);
		} 
                else {
				outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].len | 0x40),pDevExt->Address1+(128 * nport)+16);
		}
		outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].CAN_ID << 5),pDevExt->Address1+(128 * nport)+18);
		outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].CAN_ID >>3),pDevExt->Address1+(128 * nport)+17);
		} 
		else {
	    //	eff 29-bit ID
			dataStart = 21;
			if (pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].rtr == 0) {
				outb((UCHAR)((pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].len & 0x0F) | 0x80),(pDevExt->Address1+(128 * nport)+16));
			} 
			else {
				outb((UCHAR)(((pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].len & 0x0F) | 0x40) | 0x80),(pDevExt->Address1+(128 * nport)+16));
			}
			outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].CAN_ID >> 21),pDevExt->Address1+(128 * nport)+17);
			outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].CAN_ID >> 13),pDevExt->Address1+(128 * nport)+18);
			outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].CAN_ID >> 5),pDevExt->Address1+(128 * nport)+19);
			outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].CAN_ID << 3),pDevExt->Address1+(128 * nport)+20);
		}
		for(count=0;count<pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].len;count++) {
			outb((UCHAR)(pDevExt->sBuf[nport][pDevExt->sPtrL[nport]].data[count]),pDevExt->Address1+(128 * nport)+dataStart+count);
            }
		//	Request transmission
            outb(0x01,pDevExt->Address1+(128 * nport)+1);
	    cout2[nport]++;
   	    pDevExt->sPtrL[nport]++;
            pDevExt->sCnt[nport]--; 
            if(pDevExt->sPtrL[nport]==FIFOSIZE)
 		pDevExt->sPtrL[nport]=0;
       }
  }
}


irqreturn_t p7841_interrupt_handler(int irq, void *dev_id, struct pt_regs *regs)	// 2006/06/05
{
    PCI_Info* ppci_info;
    U16  wRetCode;
    U32 dwPort;

    ppci_info = (PCI_Info*) dev_id;
    dwPort=ppci_info->Address0;
    //    _asm int 3
    wRetCode = W7841_IsrCheckSource(dwPort);

    //yuan modify 02/16/01
    if ( (! (wRetCode & 0xff00)) ||(!(wRetCode & 0x0003))) //hw sw interrupt
	return	IRQ_NONE; //FALSE;	// 2006/06/05
    //yuan add 06/23/00
    if (!ppci_info->initFlag ) {
	//clear IRQ
	inb(ppci_info->Address1+3);
	inb(ppci_info->Address1+3+128);
	return IRQ_HANDLED; //TRUE;	// 2006/06/05
    }
    if (wRetCode & 0x0001)  //int1
    {
	  W7841_IsrProgram(ppci_info ,0);	
          return IRQ_HANDLED; //TRUE;	// 2006/06/05
    }
    if (wRetCode & 0x0002) //int2
    {

	  W7841_IsrProgram(ppci_info ,1);	
          return IRQ_HANDLED; //TRUE;	// 2006/06/05
    }
    return IRQ_HANDLED; //TRUE;	// 2006/06/05
}
*/