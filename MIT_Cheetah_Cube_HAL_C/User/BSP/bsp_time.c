#include "bsp_time.h"                         
#include "tim.h"                             

#define BSP_TIME_EXPECTED_CORE_HZ   180000000U // 预期 CPU 主频：180 MHz
#define BSP_TIME_EXPECTED_PCLK1_HZ   45000000U // 预期 APB1 时钟：45 MHz
#define BSP_TIME_TIM2_PRESCALER            89U // TIM2 预分频值：90 MHz / 90 = 1 MHz
#define BSP_TIME_TIM2_PERIOD         0xFFFFFFFFU // TIM2 32 位最大重装值

static volatile uint32_t s_tim2_wrap_count = 0U; // 记录 TIM2 回卷次数
static bool s_time_initialized = false;          // 记录时间模块是否已初始化


/**
 * @brief 检查系统时钟和 TIM2 配置是否符合时间基准要求。
 *
 * @retval true  配置正确。
 * @retval false 配置错误。
 */
static bool BSP_Time_ConfigIsValid(void)
{
    if (htim2.Instance != TIM2)                   // 检查句柄是否属于 TIM2
    {
        return false;                             // 不是 TIM2，检查失败
    }

    if (SystemCoreClock != BSP_TIME_EXPECTED_CORE_HZ) // 检查 CPU 是否为 180 MHz
    {
        return false;                             // CPU 时钟不正确
    }

    if (HAL_RCC_GetPCLK1Freq() != BSP_TIME_EXPECTED_PCLK1_HZ) // 检查 APB1 是否为 45 MHz
    {
        return false;                             // APB1 时钟不正确
    }

    if ((RCC->DCKCFGR & RCC_DCKCFGR_TIMPRE) != 0U) // 检查 TIMPRE 是否保持关闭
    {
        return false;                             // TIMPRE 打开会改变定时器时钟
    }

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV4) // 检查 APB1 是否为 HCLK/4
    {
        return false;                             // APB1 分频配置不正确
    }

    if (htim2.Init.Prescaler != BSP_TIME_TIM2_PRESCALER) // 检查预分频器是否为 89
    {
        return false;                             // 无法得到 1 MHz 计数频率
    }

    if (htim2.Init.CounterMode != TIM_COUNTERMODE_UP) // 检查是否向上计数
    {
        return false;                             // 计数方向配置错误
    }

    if (htim2.Init.Period != BSP_TIME_TIM2_PERIOD) // 检查自动重装值是否为最大值
    {
        return false;                             // TIM2 不是完整的 32 位计数范围
    }

    if (htim2.Init.ClockDivision != TIM_CLOCKDIVISION_DIV1) // 检查 CKD 是否为不分频
    {
        return false;                             // 内部时钟分频配置错误
    }

    return true;                                  // 所有配置均正确
}


/**
 * @brief 启用 Cortex-M4 的 DWT 周期计数器。
 *
 * @retval true  DWT 周期计数器启动成功。
 * @retval false DWT 周期计数器启动失败。
 */
static bool BSP_Time_EnableCycleCounter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // 打开内核跟踪功能

    DWT->CYCCNT = 0U;                               // 将周期计数器清零
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // 启动周期计数器

    return ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U); // 回读确认是否启动
}


/**
 * @brief 验证并启动专用的 1 MHz TIM2 时间基准。
 *
 * 本函数必须在 MX_TIM2_Init() 执行完成后调用一次。
 *
 * @retval true  TIM2 配置有效，并且计数器已成功启动。
 * @retval false TIM2 配置无效，或者计数器启动失败。
 */
bool BSP_Time_Init(void)
{
    if (s_time_initialized)                         // 检查是否已经初始化
    {
        return false;                               // 禁止重复初始化
    }

    if (!BSP_Time_ConfigIsValid())                  // 检查时钟和 TIM2 配置
    {
        return false;                               // 配置不符合要求
    }

    if (!BSP_Time_EnableCycleCounter())             // 尝试启动 DWT 周期计数器
    {
        return false;                               // DWT 启动失败
    }

    s_tim2_wrap_count = 0U;                         // 清零 TIM2 回卷次数

    __HAL_TIM_SET_COUNTER(&htim2, 0U);              // 清零 TIM2 当前计数值
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);  // 清除已有的更新标志
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);             // 清除等待处理的 TIM2 中断

    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)    // 启动 TIM2 和溢出中断
    {
        return false;                               // TIM2 启动失败
    }

    s_time_initialized = true;                      // 标记时间模块已初始化

    return true;                                    // 初始化成功
}


/**
 * @brief 获取当前 32 位微秒计数值。
 *
 * @retval TIM2 当前计数值，单位为微秒。
 */
uint32_t BSP_Time_NowUs32(void)
{
    return TIM2->CNT;                               // TIM2 每计数一次代表 1 微秒
}


/**
 * @brief 计算从指定时刻开始已经经过的微秒数。
 *
 * @param start_us 开始时刻，单位为微秒。
 *
 * @retval 已经过的微秒数。
 */
uint32_t BSP_Time_ElapsedUs32(uint32_t start_us)
{
    return BSP_Time_NowUs32() - start_us;           // 无符号减法可跨一次 32 位回卷
}


/**
 * @brief 判断指定的微秒时间是否已经到达。
 *
 * @param start_us    开始时刻，单位为微秒。
 * @param interval_us 需要等待的时间，单位为微秒。
 *
 * @retval true  指定时间已经到达。
 * @retval false 指定时间尚未到达。
 */
bool BSP_Time_HasElapsedUs32(uint32_t start_us,
                             uint32_t interval_us)
{
    return (BSP_Time_ElapsedUs32(start_us) >= interval_us); // 比较经过时间和目标时间
}


/**
 * @brief 获取长期运行的 64 位微秒时间戳。
 *
 * @retval 从时间模块启动后累计的微秒数。
 */
uint64_t BSP_Time_NowUs64(void)
{
    uint32_t primask;                               // 保存原来的全局中断状态
    uint32_t high;                                  // TIM2 回卷次数，即时间高 32 位
    uint32_t status_before;                         // 读取 CNT 前的 TIM2 状态
    uint32_t status_after;                          // 读取 CNT 后的 TIM2 状态
    uint32_t low;                                   // TIM2 当前值，即时间低 32 位

    primask = __get_PRIMASK();                      // 保存当前 PRIMASK
    __disable_irq();                                // 暂时禁止中断，保证读取一致

    high = s_tim2_wrap_count;                       // 读取 TIM2 回卷次数
    status_before = TIM2->SR;                       // 读取计数器之前的状态
    low = TIM2->CNT;                                // 读取当前微秒计数值
    status_after = TIM2->SR;                        // 再次读取 TIM2 状态

    if ((status_before & TIM_SR_UIF) != 0U)         // 读取 CNT 前已经发生回卷
    {
        high++;                                     // 本地高位补加一次
    }
    else if (((status_after & TIM_SR_UIF) != 0U) && // 读取 CNT 期间发生了回卷
             (low < 0x80000000U))                   // CNT 已进入新一轮低值区域
    {
        high++;                                     // 本地高位补加一次
    }
    else
    {
        /* 当前不需要修正高位。 */
    }

    __set_PRIMASK(primask);                         // 恢复进入函数前的中断状态

    return (((uint64_t)high << 32) |                // 将回卷次数放入高 32 位
            (uint64_t)low);                         // 将 TIM2 当前值放入低 32 位
}


/**
 * @brief 获取当前 32 位 CPU 周期计数值。
 *
 * @retval DWT 当前 CPU 周期计数值。
 */
uint32_t BSP_Time_NowCycles32(void)
{
    return DWT->CYCCNT;                             // 读取 Cortex-M4 周期计数器
}


/**
 * @brief 计算从指定时刻开始已经经过的 CPU 周期数。
 *
 * @param start_cycles 开始时的 CPU 周期计数值。
 *
 * @retval 已经过的 CPU 周期数。
 */
uint32_t BSP_Time_ElapsedCycles32(uint32_t start_cycles)
{
    return DWT->CYCCNT - start_cycles;              // 无符号减法可跨一次 DWT 回卷
}


/**
 * @brief HAL 定时器周期完成回调。
 *
 * TIM2 每次从 0xFFFFFFFF 回卷到 0 时，HAL 会调用本函数。
 *
 * @param htim 发生周期完成事件的定时器句柄。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)                     // 确认本次事件来自 TIM2
    {
        s_tim2_wrap_count++;                        // 累计一次 TIM2 回卷
    }
}