#include "uart_interface.h"

void NMI_Handler(void)
{
  while (1) {}
}

void HardFault_Handler(void)
{
  while (1) {}
}

void MemManage_Handler(void)
{
  while (1) {}
}

void BusFault_Handler(void)
{
  while (1) {}
}

void UsageFault_Handler(void)
{
  while (1) {}
}

void DebugMon_Handler(void)
{
}

void TIM1_UP_TIM10_IRQHandler(void)
{
  //TODO: uncomment this
  // HAL_TIM_IRQHandler(&htim1);
}

void DMA2_Stream2_IRQHandler(void)
{
  uart_interface_dma_rx_irq_handler(DMA2_Stream2);
}

void USART1_IRQHandler(void)
{
  uart_interface_usart_irq_handler(USART1);
}
