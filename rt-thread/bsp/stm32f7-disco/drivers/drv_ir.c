#include <rtthread.h>
#include <board.h>
#include "drv_ir.h"
#include "ir_config.h"
#include "IRremote.h"
#include "irmp.h"


#define USE_IRREMOTE        0
#define USE_IRMP            1

extern void irRecCallback(void);
extern void irInit(void);
extern bool irDecode(decodeResults *result);
static void Error_Handler(void);

static TIM_HandleTypeDef TimHandle = {NULL};
static irWorkMode_t irWorkMode = IR_IDL;  

// 得到MCU的主频
static uint32_t SysCtlClockGet(void)
{
   return SystemCoreClock; 
}

// 红外中断更新定时器初始化
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
static void irTimRecInit(void)
{   
  static uint32_t uwPrescalerValue = 0;
  
  /* Compute the prescaler value to have TIMx counter clock equal to 10000 Hz */
  uwPrescalerValue = (uint32_t)((SystemCoreClock / 2) / 20000) - 1;

  /* Set TIMx instance */
  TimHandle.Instance = IR_TIM;

  /* Initialize TIMx peripheral as follows:
       + Period = 10000 - 1
       + Prescaler = ((SystemCoreClock / 2)/10000) - 1
       + ClockDivision = 0
       + Counter direction = Up
  */
  TimHandle.Init.Period            = uwPrescalerValue;
  TimHandle.Init.Prescaler         = 0;
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
  IR_TIM_DISABLE;
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
  IR_TIM_CLK_ENABLE(); 
  
  /*##-2- Configure the NVIC for TIMx ########################################*/
  /* Set the TIMx priority */
  HAL_NVIC_SetPriority(IR_TIM_IRQn, 3, 0);

  /* Enable the TIMx global Interrupt */
  HAL_NVIC_EnableIRQ(IR_TIM_IRQn);
}


// 红外定时器回调函数
void IR_TIM_IRQHandler(void)                                             
{
  IR_TIM->SR = ~TIM_IT_UPDATE;
  
  #if USE_IRMP
  (void) irmp_ISR();   
  #elif USE_IRREMOTE
  irRecCallback();
  #endif  
}

// 红外PWM载波输出定时器初始化
/*##-1- Configure the TIM peripheral #######################################*/
/* -----------------------------------------------------------------------
TIM2 Configuration: generate 4 PWM signals with 4 different duty cycles.

  In this example TIM2 input clock (TIM2CLK) is set to APB1 clock (PCLK1) x2,
  since APB1 prescaler is equal to 4.
    TIM2CLK = PCLK1*2
    PCLK1 = HCLK/2
    => TIM2CLK = HCLK/2 = SystemCoreClock/2

  To get TIM2 counter clock at 20 MHz, the prescaler is computed as follows:
     Prescaler = (TIM2CLK / TIM2 counter clock) - 1
     Prescaler = ((SystemCoreClock/2) /20 MHz) - 1

  To get TIM2 output clock at 24 KHz, the period (ARR)) is computed as follows:
     ARR = (TIM2 counter clock / TIM2 output clock) - 1
         = 832

  TIM2 Channel1 duty cycle = (TIM2_CCR1/ TIM2_ARR + 1)* 100 = 50%
  TIM2 Channel2 duty cycle = (TIM2_CCR2/ TIM2_ARR + 1)* 100 = 37.5%
  TIM2 Channel3 duty cycle = (TIM2_CCR3/ TIM2_ARR + 1)* 100 = 25%
  TIM2 Channel4 duty cycle = (TIM2_CCR4/ TIM2_ARR + 1)* 100 = 12.5%

  Note:
   SystemCoreClock variable holds HCLK frequency and is defined in system_stm32f7xx.c file.
   Each time the core clock (HCLK) changes, user had to update SystemCoreClock
   variable value. Otherwise, any configuration based on this variable will be incorrect.
   This variable is updated in three ways:
    1) by calling CMSIS function SystemCoreClockUpdate()
    2) by calling HAL API function HAL_RCC_GetSysClockFreq()
    3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
----------------------------------------------------------------------- */

/* Initialize TIMx peripheral as follows:
     + Prescaler = ((SystemCoreClock/2) / 20000000) - 1
     + Period = (833 - 1)
     + ClockDivision = 0
     + Counter direction = Up
*/
static void irTimSndInit()
{     
  uint32_t uhPrescalerValue = 0;
  uint32_t periodValue = 0;
  uint32_t pulseValue = 0;
  TIM_OC_InitTypeDef sConfig;
  
  /* Compute the prescaler value to have TIM2 counter clock equal to 20000000 Hz */
  uhPrescalerValue = (uint32_t)((SystemCoreClock/2) / 38000000) - 1;
  periodValue = (uhPrescalerValue/38000) - 1;
  pulseValue = (periodValue*33)/100;  // 占空比33%
  
  TimHandle.Instance = IR_TIM;
  TimHandle.Init.Prescaler         = uhPrescalerValue;
  TimHandle.Init.Period            = periodValue;
  TimHandle.Init.ClockDivision     = 0;
  TimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
  TimHandle.Init.RepetitionCounter = 0;
  if (HAL_TIM_PWM_Init(&TimHandle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /*##-2- Configure the PWM channels #########################################*/
  /* Common configuration for all channels */
  sConfig.OCMode       = TIM_OCMODE_PWM1;
  sConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
  sConfig.OCFastMode   = TIM_OCFAST_DISABLE;
  sConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  sConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;

  /* Set the pulse value for channel 1 */
  sConfig.Pulse = pulseValue;
  if (HAL_TIM_PWM_ConfigChannel(&TimHandle, &sConfig, TIM_CHANNEL_1) != HAL_OK)
  {
    /* Configuration Error */
    Error_Handler();
  }

  /*##-3- Start PWM signals generation #######################################*/
  /* Start channel 1 */
  if (HAL_TIM_PWM_Start(&TimHandle, TIM_CHANNEL_1) != HAL_OK)
  {
    /* PWM Generation Error */
    Error_Handler();
  }        
  IR_TIM_DISABLE;
}

/**
  * @brief TIM MSP Initialization
  *        This function configures the hardware resources used in this example:
  *           - Peripheral's clock enable
  *           - Peripheral's GPIO Configuration
  * @param htim: TIM handle pointer
  * @retval None
  */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
  GPIO_InitTypeDef   GPIO_InitStruct;
  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* TIMx Peripheral clock enable */
  IR_TIM_CLK_ENABLE();

  /* Enable GPIO Channels Clock */
  IR_GPIO_CLK_ENABLE();

  /* Configure PB.04 (TIM3_Channel1)in output, push-pull, alternate function mode
  */
  /* Common configuration for all channels */
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;

  GPIO_InitStruct.Alternate = IR_GPIO_AF_TIM_CHANNEL;
  GPIO_InitStruct.Pin = IR_GPIO_PIN;
  HAL_GPIO_Init(IR_GPIO_PORT, &GPIO_InitStruct);
}

// 红外引脚接收初始化
static void irGPIORecInit()
{
   GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOI_CLK_ENABLE();

  /* Configure GPIO pin: PI1 (LD1) */
  GPIO_InitStruct.Pin   = IR_GPIO_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  HAL_GPIO_Init(IR_GPIO_PORT, &GPIO_InitStruct);    
}

static void irGPIOSndInit()
{
    // See void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim) .
}

// 红外硬件复位
int irHwDeInit(void)
{
  HAL_GPIO_DeInit(IR_GPIO_PORT, IR_GPIO_PIN);
  HAL_TIM_PWM_DeInit(&TimHandle);
  HAL_TIM_Base_DeInit(&TimHandle);
  HAL_TIM_Base_Stop_IT(&TimHandle);
  return true;
}
INIT_BOARD_EXPORT(irHwDeInit);

// 红外硬件状态初始化
int irHwInit(irWorkMode_t irMode)
{
  if(irWorkMode == irMode)
  {
    return true;
  }
    
  irHwDeInit();
  irWorkMode = IR_IDL;
  
  switch(irMode)
  {
    case IR_SND:
      irGPIOSndInit(); 
      irTimSndInit();            
      break;
    
    case IR_REC:
      irGPIORecInit(); 
      irTimRecInit();  
      break;
    
    case IR_IDL:
      break;
    
    default:
      break;
  }
  
  irWorkMode = irMode;
  return true; 
}

// 红外处理主进程
static void irSndRecThreadEntry(void *parameter)
{
  IRMP_DATA irmp_data;   
  decodeResults result;

  rt_kprintf("irmp task start.\n");
  
  irInit();
    
  for (;;)
  {
    #if USE_IRMP
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
    #elif USE_IRREMOTE
    if(irDecode(&result))
    {
      rt_kprintf("ir decode succ: %X %X %X %X %X %X %X\n"
                                    , result.decode_type
                                    , result.address    
                                    , result.value      
                                    , result.bits       
                                    , result.rawbuf     
                                    , result.rawlen     
                                    , result.overflow);
    }
    #endif
    
    rt_thread_delay(100);        
  }
}
      
int irSndRecInit(void)         
{
    rt_thread_t tid;

    tid = rt_thread_create("irSndRec",
                           irSndRecThreadEntry, RT_NULL,
                           512, 12, 5);
    
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
    }

    return 0;
}
INIT_APP_EXPORT(irSndRecInit);


static void Error_Handler(void)
{
  while(1);
}



