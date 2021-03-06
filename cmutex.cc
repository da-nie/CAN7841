//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include "cmutex.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//конструктор и деструктор класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//конструктор
//----------------------------------------------------------------------------------------------------
CMutex::CMutex(void)
{
 pthread_mutex_init(&MutexID,NULL);
}
//----------------------------------------------------------------------------------------------------
//конструктор копирования
//----------------------------------------------------------------------------------------------------
CMutex::CMutex(const CMutex& cMutex)
{
 //нам нужно создать новый мютекс
 pthread_mutex_init(&MutexID,NULL);
}
//----------------------------------------------------------------------------------------------------
//деструктор
//----------------------------------------------------------------------------------------------------
CMutex::~CMutex()
{
 pthread_mutex_destroy(&MutexID);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//закрытые функции класса
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//----------------------------------------------------------------------------------------------------
//заблокировать мютекс
//----------------------------------------------------------------------------------------------------
void CMutex::Lock(void)
{
 pthread_mutex_lock(&MutexID);	
}
//----------------------------------------------------------------------------------------------------
//разблокировать мютекс
//----------------------------------------------------------------------------------------------------
void CMutex::Unlock(void)
{
 pthread_mutex_unlock(&MutexID);	
}

