#ifndef C_MUTEX_H
#define C_MUTEX_H

//****************************************************************************************************
//Класс работы с мютексом
//****************************************************************************************************

//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include <pthread.h>

//****************************************************************************************************
//макроопределения
//****************************************************************************************************

//****************************************************************************************************
//Класс работы с мютексом
//****************************************************************************************************

class CMutex
{
 private:
  pthread_mutex_t MutexID;//мьютекс   
 public:  
  //конструктор
  CMutex(void);
  //конструктор копирования
  CMutex(const CMutex& cMutex);  
  //деструктор
  ~CMutex();
 public:
  void Lock(void);//заблокировать мютекс
  void Unlock(void);//разблокировать мютекс
};

#endif
