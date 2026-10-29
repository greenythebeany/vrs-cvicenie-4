/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);

/* Waits until previous DMA Tx is finished and returns buffer free for new message */
uint8_t* getTxBuffer(void);

/* Sends information about DMA Rx buffer state */
void sendBufferStatus(void);


/* Space for your global variables. */

#define MAX_STRING_LENGTH 35

/* Result for last valid string */
volatile letter_count_ letter_count = {0};
uint8_t valid_string[MAX_STRING_LENGTH + 1];
volatile uint8_t valid_string_ready = 0;

/* String currently being received */
static uint8_t rx_string[MAX_STRING_LENGTH + 1];
static uint8_t rx_length = 0;
static uint8_t rx_active = 0;

/* Memory buffer used by DMA for USART Tx */
static uint8_t tx_buffer[100];


int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();

  /* Space for your local variables, callback registration ...*/

  USART2_RegisterCallback(proccesDmaData);

  uint16_t ms_counter = 0;

  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */

	  /* SysTick counter flag is set every 1 ms */
	  if(LL_SYSTICK_IsActiveCounterFlag())
	  {
		  ms_counter++;
	  }

	  if(ms_counter >= 2000)
	  {
		  ms_counter = 0;
		  sendBufferStatus();
	  }

	  if(valid_string_ready)
	  {
		  uint8_t *tx = getTxBuffer();

		  __disable_irq();
		  int len = sprintf((char*)tx, "Valid string: %s, lower-case: %d, upper-case: %d\r\n",
				  valid_string, letter_count.small_letter, letter_count.capital_letter);
		  valid_string_ready = 0;
		  __enable_irq();

		  USART2_PutBuffer(tx, len);
	  }
  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
	/* Process received data */

	if(!rx_active)
	{
		if(sign == '#')
		{
			rx_active = 1;
			rx_length = 0;
		}
		return;
	}

	if(sign == '$')
	{
		uint8_t small = 0;
		uint8_t capital = 0;

		for(uint8_t i = 0; i < rx_length; i++)
		{
			if(rx_string[i] >= 'a' && rx_string[i] <= 'z') small++;
			else if(rx_string[i] >= 'A' && rx_string[i] <= 'Z') capital++;

			valid_string[i] = rx_string[i];
		}
		valid_string[rx_length] = '\0';

		letter_count.small_letter = small;
		letter_count.capital_letter = capital;
		valid_string_ready = 1;
		rx_active = 0;
		return;
	}

	rx_string[rx_length++] = sign;

	/* End sign not received within 35 signs - discard data */
	if(rx_length >= MAX_STRING_LENGTH)
	{
		rx_active = 0;
	}
}


/* DMA Tx channel is disabled in DMA1_Channel7_IRQHandler after transfer is complete */
uint8_t* getTxBuffer(void)
{
	while(LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7));

	return tx_buffer;
}


void sendBufferStatus(void)
{
	uint16_t occupied = USART2_GetRxOccupied();
	/* Load in tenths of percent */
	uint16_t load = (uint32_t)occupied * 1000 / DMA_USART2_BUFFER_SIZE;

	uint8_t *tx = getTxBuffer();
	int len = sprintf((char*)tx, "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %%]: %d.%d%%\r\n",
			DMA_USART2_BUFFER_SIZE, occupied, load / 10, load % 10);

	USART2_PutBuffer(tx, len);
}


void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
