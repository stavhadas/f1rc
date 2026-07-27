#include <stdbool.h>
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal_rcc.h"
#include "controller.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

static bool controller_bsp_system_clock_config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    return false;
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    return false;
  }
  return true;
}

static bool controller_bsp_rtos_init(void)
{
  osKernelInitialize();
  return true;
}

bool controller_bsp_init(controller_context_t *context)
{
  HAL_Init();
  if (!controller_bsp_system_clock_config())
  {
    return false;
  }

  controller_bsp_rtos_init();

  uart_interface_init(&context->cmd_uart, USART1, 115200, UART_WORDLENGTH_8B, UART_STOPBITS_1, UART_PARITY_NONE, UART_MODE_TX_RX, UART_HWCONTROL_NONE, UART_OVERSAMPLING_16, DMA2_Stream2, DMA_CHANNEL_4, DMA2_Stream2_IRQn, USART1_IRQn);

  spi_interface_init(&context->cc1101_spi, SPI2, SPI_BAUDRATEPRESCALER_4,
                      DMA1_Stream4, DMA_CHANNEL_0, DMA1_Stream4_IRQn,
                      DMA1_Stream3, DMA_CHANNEL_0, DMA1_Stream3_IRQn,
                      SPI2_IRQn,
                      GPIOB, GPIO_PIN_12,
                      GPIOB, GPIO_PIN_14);
  context->cc1101.spi = &context->cc1101_spi;

  return true;
}

bool controller_bsp_start(void)
{
  osKernelStart();
  return true;
}

// Required by configCHECK_FOR_STACK_OVERFLOW (FreeRTOSConfig.h). Without
// this, an overflow corrupts adjacent memory silently instead of trapping.
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  while (1) {}
}
