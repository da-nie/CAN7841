//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "cthread.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
CThread::CThread(void* (*start_routine)(void*),uint32_t priority,void *param)
{
 //запускаем поток
 pthread_attr_t pt_attr;
 pthread_attr_init(&pt_attr);
 pthread_attr_setdetachstate(&pt_attr,PTHREAD_CREATE_JOINABLE);
 pthread_attr_setinheritsched(&pt_attr,PTHREAD_EXPLICIT_SCHED);
 pthread_attr_setschedpolicy(&pt_attr,SCHED_RR);
 pt_attr.param.sched_priority=priority;
 pthread_create(&pthread_ID,&pt_attr,start_routine,param);//создаём поток
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
CThread::~CThread()
{
 Join();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//открытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//ждать завершения потока
//----------------------------------------------------------------------------------------------------
void CThread::Join(void)
{
 if (pthread_ID!=NULL) pthread_join(pthread_ID,NULL);//;ждём завершения потока 
 pthread_ID=NULL;
  
}