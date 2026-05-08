#include <stdbool.h>
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal_rcc.h"
#include "car.h"

static bool car_bsp_system_clock_config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    return false;
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
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

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

static bool car_bsp_cmd_uart_init(car_context_t *context)
{
  context->cmd_uart.Instance = USART1;
  context->cmd_uart.Init.BaudRate = 115200;
  context->cmd_uart.Init.WordLength = UART_WORDLENGTH_8B;
  context->cmd_uart.Init.StopBits = UART_STOPBITS_1;
  context->cmd_uart.Init.Parity = UART_PARITY_NONE;
  context->cmd_uart.Init.Mode = UART_MODE_TX_RX;
  context->cmd_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  context->cmd_uart.Init.OverSampling = UART_OVERSAMPLING_16;

  return HAL_UART_Init(&context->cmd_uart) != HAL_OK;
}

static bool car_bsp_rtos_init(void)
{
  osKernelInitialize();
}

bool car_bsp_init(car_context_t *context)
{
  HAL_Init();

  /* Configure the system clock */
  if (!car_bsp_system_clock_config())
  {
    return false;
  }

  // Initialize UART for command communication
  if (!car_bsp_cmd_uart_init(context))
  {
    return false;
  }

  return true;
}

bool car_bsp_start(void)
{
  osKernelStart();
}