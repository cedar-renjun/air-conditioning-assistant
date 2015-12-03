#include <rtthread.h>
#include <board.h>
#include "ir_base.h"


#if defined(ARM_STM32F756xx)

void Error_Handler(void);



// 得到MCU的主频
uint32_t SysCtlClockGet(void)
{
   return SystemCoreClock; 
}

// 红外中断更新定时器初始化
void irmpUpdateTimInit(void)
{
  /*##-1- Configure the TIM peripheral #######################################*/
  /* -----------------------------------------------------------------------
    In this example TIM3 input clock (TIM3CLK)  is set to APB1 clock (PCLK1) x2,
    since APB1 prescaler is equal to 4.
      TIM3CLK = PCLK1*2
      PCLK1 = HCLK/4
      => TIM3CLK = HCLK/2 = SystemCoreClock/2
    To get TIM3 counter clock at 10 KHz, the Prescaler is computed as follows:
    Prescaler = (TIM3CLK / TIM3 counter clock) - 1
    Prescaler = ((SystemCoreClock/2) /10 KHz) - 1

    Note:
     SystemCoreClock variable holds HCLK frequency and is defined in system_stm32f7xx.c file.
     Each time the core clock (HCLK) changes, user had to update SystemCoreClock
     variable value. Otherwise, any configuration based on this variable will be incorrect.
     This variable is updated in three ways:
      1) by calling CMSIS function SystemCoreClockUpdate()
      2) by calling HAL API function HAL_RCC_GetSysClockFreq()
      3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
  ----------------------------------------------------------------------- */
    
  TIM_HandleTypeDef    TimHandle;

  /* Prescaler declaration */
  uint32_t uwPrescalerValue = 0;
  /* Compute the prescaler value to have TIMx counter clock equal to 10000 Hz */
  uwPrescalerValue = (uint32_t)((SystemCoreClock / 2) / 15000) - 1;

  /* Set TIMx instance */
  TimHandle.Instance = IR_UPDATE_TIM;

  /* Initialize TIMx peripheral as follows:
       + Period = 10000 - 1
       + Prescaler = ((SystemCoreClock / 2)/10000) - 1
       + ClockDivision = 0
       + Counter direction = Up
  */
  TimHandle.Init.Period            = 10000 - 1;
  TimHandle.Init.Prescaler         = uwPrescalerValue;
  TimHandle.Init.ClockDivision     = 0;
  TimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
  TimHandle.Init.RepetitionCounter = 0;

  if (HAL_TIM_Base_Init(&TimHandle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /*##-2- Start the TIM Base generation in interrupt mode ####################*/
  /* Start Channel1 */
  if (HAL_TIM_Base_Start_IT(&TimHandle) != HAL_OK)
  {
    /* Starting Error */
    Error_Handler();
  }
}

/**
  * @brief TIM MSP Initialization
  *        This function configures the hardware resources used in this example:
  *           - Peripheral's clock enable
  * @param htim: TIM handle pointer
  * @retval None
  */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
  /*##-1- Enable peripheral clock #################################*/
  /* TIMx Peripheral clock enable */
  IR_UPDATE_TIM_CLK_ENABLE(); 
  
  /*##-2- Configure the NVIC for TIMx ########################################*/
  /* Set the TIMx priority */
  HAL_NVIC_SetPriority(IR_UPDATE_TIM_IRQn, 3, 0);

  /* Enable the TIMx global Interrupt */
  HAL_NVIC_EnableIRQ(IR_UPDATE_TIM_IRQn);
}


// 红外中断更新定时器回调函数
void IR_UPDATE_TIM_IRQHandler(void)                                                  // Timer2 Interrupt Handler
{

  IR_UPDATE_TIM->SR = ~TIM_IT_UPDATE;
  (void) irmp_ISR();   
                                                     // call irmp ISR
  // call other timer interrupt routines...
}

// 红外PWM载波输出定时器初始化
void irmpPWMTimInit()
{         
          
}

// 红外收发引脚初始化
void irmpGPIORecInit()
{
   GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOI_CLK_ENABLE();

  /* Configure GPIO pin: PI1 (LD1) */
  GPIO_InitStruct.Pin   = IR_REC_GPIO_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  HAL_GPIO_Init(IR_REC_GPIO_PORT, &GPIO_InitStruct);    
}

void irmpGPIOSndInit()
{
    
}

// 红外硬件初始化
int irmpHwInit(void)
{
    
   irmp_init();                                                            // initialize irmp
   irmpGPIORecInit(); 
   irmpGPIOSndInit(); 
   irmpUpdateTimInit();                                                    // initialize timer
   irmpPWMTimInit();
    

  return 0;
}
INIT_BOARD_EXPORT(irmpHwInit);

// 红外接收处理主进程
static void irmpThreadEntry(void *parameter)
{
  IRMP_DATA irmp_data;   
  //class IRrecv  

  rt_kprintf("irmp task start.\n");
    
  for (;;)
  {
      if (irmp_get_data(&irmp_data))
      {
        // ir signal decoded, do something here...
        // irmp_data.protocol is the protocol, see irmp.h
        // irmp_data.address is the address/manufacturer code of ir sender
        // irmp_data.command is the command code
        // irmp_protocol_names[irmp_data.protocol] is the protocol name (if enabled, see irmpconfig.h)         
        rt_kprintf("%X %X %X %X \n", irmp_data.address 
                       , irmp_data.command
                       , irmp_data.flags
                       , irmp_data.protocol);
      }
  }
}

int irmpInit(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("irmp",
                           irmpThreadEntry, RT_NULL,
                           512, 12, 5);
    
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
    }

    return 0;
}
INIT_APP_EXPORT(irmpInit);


void Error_Handler(void)
{
  while(1);
}

#endif

