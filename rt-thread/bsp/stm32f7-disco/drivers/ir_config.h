#ifndef _IR_BASE_H_
#define _IR_BASE_H_

#include <stm32f7xx.h>
#include "stm32f7xx_hal.h"

// 红外接收及PWM输出共用一个引脚
#define IR_GPIO_PIN                             GPIO_PIN_4
#define IR_GPIO_PORT                            GPIOB
#define IR_GPIO_CLK_ENABLE()                    __HAL_RCC_GPIOB_CLK_ENABLE()

#define IR_GPIO_AF_TIM_CHANNEL                  GPIO_AF2_TIM3   // PWM通道输出引脚映射

#define IR_GPIO_PIN_READ                        ((IR_GPIO_PORT->IDR & IR_GPIO_PIN) ? GPIO_PIN_SET : GPIO_PIN_RESET)

// 红外接收更新及PWM输出共用一个定时器
#define IR_TIM                                  TIM3                                   // TIM6
#define IR_TIM_IRQn                             TIM3_IRQn                              // TIM6_DAC_IRQn
#define IR_TIM_IRQHandler                       TIM3_IRQHandler                        // TIM6_DAC_IRQHandler
#define IR_TIM_CLK_ENABLE()                     __HAL_RCC_TIM3_CLK_ENABLE()            // __HAL_RCC_TIM6_CLK_ENABLE()
#define IR_TIM_ENABLE                           (IR_TIM->CR1|=(TIM_CR1_CEN))
#define IR_TIM_DISABLE                          (IR_TIM->CR1&=(~TIM_CR1_CEN))




#endif // _IR_BASE_H_
