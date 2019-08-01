#ifndef SJA1000_H
#define SJA1000_H

//****************************************************************************************************
//регистры контроллера SA1000 (режим PeliCAN)
//****************************************************************************************************

//****************************************************************************************************
//подключаемые библиотеки
//****************************************************************************************************
#include <stdint.h>

//****************************************************************************************************
//константы
//****************************************************************************************************
namespace SJA1000
{
 namespace Register
 {	
  static const uint32_t MODE=0;
  static const uint32_t COMMAND=1;
  static const uint32_t STATUS=2;
  static const uint32_t INTERRUPT=3;
  static const uint32_t INTERRUPT_ENABLE=4;
  static const uint32_t RESERVED_1=5;
  static const uint32_t BUS_TIMING_0=6;
  static const uint32_t BUS_TIMING_1=7;
  static const uint32_t OUTPUT_CONTROL=8;
  static const uint32_t TEST=9;
  static const uint32_t RESERVED_2=10;
  static const uint32_t ARBITRATION_LOST_CAPTURE=11;
  static const uint32_t ERROR_CODE_CAPTURE=12;
  static const uint32_t ERROR_WARNING_LIMIT=13;
  static const uint32_t RX_ERROR_COUNTER=14;
  static const uint32_t TX_ERROR_COUNTER=15;
  static const uint32_t FRAME_INFORMATION=16;

  static const uint32_t SFF_IDENTIFIER_1=17;
  static const uint32_t SFF_IDENTIFIER_2=18;
  static const uint32_t SFF_DATA_1=19;
 
  static const uint32_t ACCEPTANCE_CODE_0=16;
  static const uint32_t ACCEPTANCE_CODE_1=17;
  static const uint32_t ACCEPTANCE_CODE_2=18;
  static const uint32_t ACCEPTANCE_CODE_3=19;
  
  static const uint32_t ACCEPTANCE_MASK_0=20;
  static const uint32_t ACCEPTANCE_MASK_1=21;
  static const uint32_t ACCEPTANCE_MASK_2=22;
  static const uint32_t ACCEPTANCE_MASK_3=23;  

  static const uint32_t EFF_IDENTIFIER_1=17;
  static const uint32_t EFF_IDENTIFIER_2=18;
  static const uint32_t EFF_IDENTIFIER_3=19;
  static const uint32_t EFF_IDENTIFIER_4=20;
  static const uint32_t EFF_DATA_1=21;

  static const uint32_t RX_MESSAGE_COUNTER=29;
  static const uint32_t RX_BUFFER_START_ADDRESS=30;

  static const uint32_t CLOCK_DIVIDER=31;
 };
 
 namespace Mode
 {
  static const uint32_t RESET_MODE_MASK=(1<<0);
  static const uint32_t LISTEN_ONLY_MODE_MASK=(1<<1);
  static const uint32_t SELF_TEST_MODE_MASK=(1<<2);
  static const uint32_t ACCEPTANCE_FILTER_MODE_MASK=(1<<3);
  static const uint32_t SLEED_MODE_MASK=(1<<4);
 }; 
 
 namespace Command
 {
  static const uint32_t TRANSMISSION_REQUEST_MASK=(1<<0);
  static const uint32_t ABORT_TRANSMISSION_MASK=(1<<1);
  static const uint32_t RELEASE_RECEIVE_BUFFER_MASK=(1<<2);
  static const uint32_t CLEAR_DATA_OVERRUN_MASK=(1<<3);
  static const uint32_t SELF_RECEPTION_REQUEST_MASK=(1<<4);
 };
 
 namespace Status
 {
  static const uint32_t RECEIVE_BUFFER_STATUS_MASK=(1<<0);
  static const uint32_t DATA_OVERRUN_STATUS_MASK=(1<<1);
  static const uint32_t TRANSMIT_BUFFER_STATUS_MASK=(1<<2);
  static const uint32_t TRANSMISSION_COMPLETE_STATUS_MASK=(1<<3);
  static const uint32_t RECEIVE_STATUS_MASK=(1<<4);
  static const uint32_t TRANSMIT_STATUS_MASK=(1<<5);
  static const uint32_t ERROR_STATUS_MASK=(1<<6);
  static const uint32_t BUS_STATUS_MASK=(1<<7);
 };
 
 namespace Interrupt
 {
  static const uint32_t RECEIVE_INTERRUPT_MASK=(1<<0);
  static const uint32_t TRANSMIT_INTERRUPT_MASK=(1<<1);
  static const uint32_t ERROR_WARNING_INTERRUPT_MASK=(1<<2);
  static const uint32_t DATA_OVERRUN_INTERRUPT_MASK=(1<<3);
  static const uint32_t WAKE_UP_INTERRUPT_MASK=(1<<4);
  static const uint32_t ERROR_PASSIVE_INTERRUPT_MASK=(1<<5);
  static const uint32_t ARBITRATION_LOST_INTERRUPT_MASK=(1<<6);
  static const uint32_t BUS_ERROR_INTERRUPT_MASK=(1<<7);
 };

 namespace InterruptEnable
 {
  static const uint32_t RECEIVE_INTERRUPT_ENABLE_MASK=(1<<0);
  static const uint32_t TRANSMIT_INTERRUPT_ENABLE_MASK=(1<<1);
  static const uint32_t ERROR_WARNING_INTERRUPT_ENABLE_MASK=(1<<2);
  static const uint32_t DATA_OVERRUN_INTERRUPT_ENABLE_MASK=(1<<3);
  static const uint32_t WAKE_UP_INTERRUPT_ENABLE_MASK=(1<<4);
  static const uint32_t ERROR_PASSIVE_INTERRUPT_ENABLE_MASK=(1<<5);
  static const uint32_t ARBITRATION_LOST_INTERRUPT_ENABLE_MASK=(1<<6);
  static const uint32_t BUS_ERROR_INTERRUPT_ENABLE_MASK=(1<<7);
 };

 namespace OutputControl
 {
  static const uint32_t MODE_0_MASK=(1<<0);
  static const uint32_t MODE_1_MASK=(1<<1);
  static const uint32_t POLARITY_0_MASK=(1<<2);
  static const uint32_t TRANSISTOR_N_0_MASK=(1<<3);
  static const uint32_t TRANSISTOR_P_0_MASK=(1<<4);
  static const uint32_t POLARITY_1_MASK=(1<<5);
  static const uint32_t TRANSISTOR_N_1_MASK=(1<<6);
  static const uint32_t TRANSISTOR_P_1_MASK=(1<<7);
 }; 
 static const uint32_t CHANNEL_OFFSET=128;
};

#endif



