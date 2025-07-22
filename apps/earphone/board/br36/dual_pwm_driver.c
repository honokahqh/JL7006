#include "dual_pwm_driver.h"
#include "board_ac700n_demo_cfg.h"
#include "system/includes.h"
#include "debug.h"
#include "asm/power/power_api.h"

#define LOG_TAG_CONST       BOARD
#define LOG_TAG             "[BOARD]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE

// 全局PWM驱动实例
static dual_pwm_driver_t g_pwm_driver = {0};

// 定时器时钟频率 (24MHz)
#define TIMER_CLK_HZ    24000000
#define TIMER_CLK_DIV   4  // 时钟分频系数
#define EFFECTIVE_CLK   (TIMER_CLK_HZ / TIMER_CLK_DIV)  // 有效时钟频率

/**
 * @brief 获取PWM通道索引
 * @param channel PWM通道
 * @return 通道索引 (0或1)，-1表示无效通道
 */
static int get_channel_index(pwm_channel_t channel)
{
    switch (channel) {
        case PWM_CHANNEL_0:
            return 0;
        case PWM_CHANNEL_4:
            return 1;
        case PWM_CHANNEL_5:
            return 2;
        default:
            return -1;
    }
}

/**
 * @brief 获取定时器指针
 * @param channel PWM通道
 * @return 定时器指针，NULL表示无效通道
 */
static JL_TIMER_TypeDef* get_timer(pwm_channel_t channel)
{
    switch (channel) {
        case PWM_CHANNEL_0:
            return JL_TIMER0;
        case PWM_CHANNEL_4:
            return JL_TIMER4;
        case PWM_CHANNEL_5:
            return JL_TIMER5;
        default:
            return NULL;
    }
}

/**
 * @brief 获取GPIO引脚
 * @param channel PWM通道
 * @return GPIO引脚编号，0表示无效通道
 */
static u32 get_gpio_pin(pwm_channel_t channel)
{
    switch (channel) {
        case PWM_CHANNEL_0:
            return IO_PORTA_01;
        case PWM_CHANNEL_4:
            return IO_PORTC_04;  // PC4
        case PWM_CHANNEL_5:
            return IO_PORTA_05;  // PA5
        default:
            return 0;
    }
}

/**
 * @brief 配置GPIO用于PWM输出
 * @param channel PWM通道
 */
static void configure_pwm_gpio(pwm_channel_t channel)
{
    u32 pin = get_gpio_pin(channel);
    u8 timer_func;
    
    if (pin == 0) {
        log_info("Invalid GPIO pin for channel %d\n", channel);
        return;
    }
    
    // 根据通道选择定时器功能
    if(channel == PWM_CHANNEL_0){
        timer_func = FO_TMR0_PWM;  // TIMER0 PWM功能
    }else if(channel == PWM_CHANNEL_4){
        timer_func = FO_TMR4_PWM;  // TIMER4 PWM功能
    } else {
        timer_func = FO_TMR5_PWM;  // TIMER5 PWM功能
    }
    
    // 配置GPIO为PWM输出
    gpio_set_fun_output_port(pin, timer_func, 0, 1, LOW_POWER_KEEP);
    gpio_set_die(pin, 1);
    gpio_set_pull_up(pin, 0);
    gpio_set_pull_down(pin, 0);
    gpio_set_direction(pin, 0);  
    
    log_info("PWM%d GPIO configured: pin=0x%x, timer_func=%d\n", channel, pin, timer_func);
}

/**
 * @brief 初始化单个PWM通道
 * @param channel PWM通道
 * @param frequency 频率
 * @param duty_cycle 占空比
 * @return 0:成功, -1:失败
 */
static int init_single_pwm(pwm_channel_t channel, u32 frequency, u32 duty_cycle)
{
    int index = get_channel_index(channel);
    if (index < 0) {
        log_info("Invalid PWM channel: %d\n", channel);
        return -1;
    }
    
    JL_TIMER_TypeDef *timer = get_timer(channel);
    if (!timer) {
        log_info("Cannot get timer for channel %d\n", channel);
        return -1;
    }
    
    u32 pin = get_gpio_pin(channel);
    if (pin == 0) {
        log_info("Cannot get GPIO pin for channel %d\n", channel);
        return -1;
    }
    
    // 配置GPIO
    configure_pwm_gpio(channel);
    
    // 初始化定时器
    timer->CON = 0;
    timer->CON |= (6 << 10);     // 时钟源选择STD_24M
    timer->CON |= (0b0001 << 4); // 时钟源4分频
    timer->CNT = 0;              // 清计数值
    
    // 计算周期值
    u32 period = EFFECTIVE_CLK / frequency;
    timer->PRD = period;
    
    // 设置初始占空比
    timer->PWM = (period * duty_cycle) / 10000;
    
    // 启动定时器 (计数模式)
    // timer->CON |= BIT(8) | (0b01 << 0);
    
    // 保存配置
    g_pwm_driver.channels[index].frequency = frequency;
    g_pwm_driver.channels[index].duty_cycle = duty_cycle;
    g_pwm_driver.channels[index].pin = pin;
    g_pwm_driver.channels[index].state = PWM_STATE_ENABLED;
    g_pwm_driver.channels[index].timer = timer;
    
    log_info("PWM%d initialized: freq=%dHz, duty=%d.%02d%%, period=%d\n", 
             channel, frequency, duty_cycle/100, duty_cycle%100, period);
    
    return 0;
}

/**
 * @brief 禁用单个PWM通道的硬件
 * @param channel PWM通道
 */
static void disable_pwm_hardware(pwm_channel_t channel)
{
    JL_TIMER_TypeDef *timer = get_timer(channel);
    u32 pin = get_gpio_pin(channel);
    
    if (timer) {
        // 停止定时器
        timer->CON &= ~(BIT(8) | 0b11);  // 清除计数模式位
        timer->CON = 0;  // 清空控制寄存器
        timer->CNT = 0;  // 清计数值
        timer->PWM = 0;  // 清PWM比较值
    }
    
    if (pin) {
        // 设置GPIO为高阻输入状态
        gpio_set_pull_up(pin, 0);
        gpio_direction_input(pin);
        gpio_set_pull_down(pin, 0);
        gpio_set_output_value(pin, 0);
        gpio_set_die(pin, 0);
    }
    
    log_info("PWM%d hardware disabled\n", channel);
}

/**
 * @brief 启用单个PWM通道的硬件
 * @param channel PWM通道
 */
static void enable_pwm_hardware(pwm_channel_t channel)
{
    int index = get_channel_index(channel);
    if (index < 0) {
        return;
    }
    
    pwm_config_t *config = &g_pwm_driver.channels[index];
    
    // 重新配置GPIO
    configure_pwm_gpio(channel);
    
    // 重新启动定时器
    if (config->timer) {
        config->timer->CON |= BIT(8) | (0b01 << 0);
    }
    
    log_info("PWM%d hardware enabled\n", channel);
}

// 同步启动函数
int dual_pwm_sync_start_with_phase(u32 pwm_freq, u32 pwm_duty,
                                   u32 phase_diff_degrees) {
    // 配置但不启动PWM4
    init_single_pwm(PWM_CHANNEL_4, pwm_freq, pwm_duty);
    init_single_pwm(PWM_CHANNEL_5, pwm_freq, pwm_duty);

    // 配置但不启动PWM5，设置相位偏移
    u32 period = EFFECTIVE_CLK / pwm_freq;
    u32 phase_count = (period * phase_diff_degrees) / 360;
    
    JL_TIMER5->CNT = phase_count;
    
    // 同时启动两个定时器
    local_irq_disable();
    JL_TIMER4->CON |= BIT(8) | (0b01 << 0);
    JL_TIMER5->CON |= BIT(8) | (0b01 << 0);
    local_irq_enable();
    
    return 0;
}

// 公共函数实现

int dual_pwm_init(u32 pwm4_freq, u32 pwm4_duty, u32 pwm5_freq, u32 pwm5_duty)
{
    log_info("Initializing dual PWM driver...\n");
    
    // 检查参数有效性
    if (pwm4_freq == 0 || pwm5_freq == 0) {
        log_info("Invalid frequency parameters\n");
        return -1;
    }
    
    if (pwm4_duty > 10000 || pwm5_duty > 10000) {
        log_info("Invalid duty cycle parameters\n");
        return -1;
    }
    
    // 清零驱动结构体
    memset(&g_pwm_driver, 0, sizeof(dual_pwm_driver_t));
    
    // 初始化PWM4 (PC4)
    if (init_single_pwm(PWM_CHANNEL_4, pwm4_freq, pwm4_duty) != 0) {
        log_info("Failed to initialize PWM4\n");
        return -1;
    }
    
    // 初始化PWM5 (PA5)
    if (init_single_pwm(PWM_CHANNEL_5, pwm5_freq, pwm5_duty) != 0) {
        log_info("Failed to initialize PWM5\n");
        // 清理PWM4
        disable_pwm_hardware(PWM_CHANNEL_4);
        return -1;
    }
    
    g_pwm_driver.initialized = 1;
    
    log_info("Dual PWM driver initialized successfully\n");
    log_info("PWM4(PC4): %dHz, %d.%02d%%\n", pwm4_freq, pwm4_duty/100, pwm4_duty%100);
    log_info("PWM5(PA5): %dHz, %d.%02d%%\n", pwm5_freq, pwm5_duty/100, pwm5_duty%100);
    
    return 0;
}

int dual_pwm_enable(pwm_channel_t channel)
{
    if (!g_pwm_driver.initialized) {
        log_info("PWM driver not initialized\n");
        return -1;
    }
    
    int index = get_channel_index(channel);
    if (index < 0) {
        log_info("Invalid PWM channel: %d\n", channel);
        return -1;
    }
    
    if (g_pwm_driver.channels[index].state == PWM_STATE_ENABLED) {
        log_info("PWM%d already enabled\n", channel);
        return 0;
    }
    
    enable_pwm_hardware(channel);
    g_pwm_driver.channels[index].state = PWM_STATE_ENABLED;
    
    log_info("PWM%d enabled\n", channel);
    return 0;
}

int dual_pwm_disable(pwm_channel_t channel)
{
    if (!g_pwm_driver.initialized) {
        log_info("PWM driver not initialized\n");
        return -1;
    }
    
    int index = get_channel_index(channel);
    if (index < 0) {
        log_info("Invalid PWM channel: %d\n", channel);
        return -1;
    }
    
    if (g_pwm_driver.channels[index].state == PWM_STATE_DISABLED) {
        log_info("PWM%d already disabled\n", channel);
        return 0;
    }
    
    disable_pwm_hardware(channel);
    g_pwm_driver.channels[index].state = PWM_STATE_DISABLED;
    
    log_info("PWM%d disabled\n", channel);
    return 0;
}

int dual_pwm_set_frequency(pwm_channel_t channel, u32 frequency)
{
    if (!g_pwm_driver.initialized) {
        log_info("PWM driver not initialized\n");
        return -1;
    }
    
    if (frequency == 0) {
        log_info("Invalid frequency: %d\n", frequency);
        return -1;
    }
    
    int index = get_channel_index(channel);
    if (index < 0) {
        log_info("Invalid PWM channel: %d\n", channel);
        return -1;
    }
    
    pwm_config_t *config = &g_pwm_driver.channels[index];
    JL_TIMER_TypeDef *timer = config->timer;
    
    if (!timer) {
        log_info("Timer not available for channel %d\n", channel);
        return -1;
    }
    
    // 计算新的周期值
    u32 new_period = EFFECTIVE_CLK / frequency;
    
    // 更新定时器周期
    timer->PRD = new_period;
    
    // 根据当前占空比重新计算PWM比较值
    timer->PWM = (new_period * config->duty_cycle) / 10000;
    
    // 更新配置
    config->frequency = frequency;
    
    log_info("PWM%d frequency updated to %dHz (period=%d)\n", channel, frequency, new_period);
    
    return 0;
}

int dual_pwm_set_duty_cycle(pwm_channel_t channel, u32 duty_cycle)
{
    if (!g_pwm_driver.initialized) {
        log_info("PWM driver not initialized\n");
        return -1;
    }
    
    if (duty_cycle > 10000) {
        log_info("Invalid duty cycle: %d (max: 10000)\n", duty_cycle);
        return -1;
    }
    
    int index = get_channel_index(channel);
    if (index < 0) {
        log_info("Invalid PWM channel: %d\n", channel);
        return -1;
    }
    
    pwm_config_t *config = &g_pwm_driver.channels[index];
    JL_TIMER_TypeDef *timer = config->timer;
    
    if (!timer) {
        log_info("Timer not available for channel %d\n", channel);
        return -1;
    }
    
    // 更新PWM比较值
    timer->PWM = (timer->PRD * duty_cycle) / 10000;
    
    // 更新配置
    config->duty_cycle = duty_cycle;
    
    log_info("PWM%d duty cycle updated to %d.%02d%%\n", 
             channel, duty_cycle/100, duty_cycle%100);
    
    return 0;
}

u32 dual_pwm_get_frequency(pwm_channel_t channel)
{
    if (!g_pwm_driver.initialized) {
        return 0;
    }
    
    int index = get_channel_index(channel);
    if (index < 0) {
        return 0;
    }
    
    return g_pwm_driver.channels[index].frequency;
}

s32 dual_pwm_get_duty_cycle(pwm_channel_t channel)
{
    if (!g_pwm_driver.initialized) {
        return -1;
    }
    
    int index = get_channel_index(channel);
    if (index < 0) {
        return -1;
    }
    
    return g_pwm_driver.channels[index].duty_cycle;
}

pwm_state_t dual_pwm_get_state(pwm_channel_t channel)
{
    if (!g_pwm_driver.initialized) {
        return PWM_STATE_DISABLED;
    }
    
    int index = get_channel_index(channel);
    if (index < 0) {
        return PWM_STATE_DISABLED;
    }
    
    return g_pwm_driver.channels[index].state;
}

void dual_pwm_deinit(void)
{
    if (!g_pwm_driver.initialized) {
        return;
    }
    
    log_info("Deinitializing dual PWM driver...\n");
    
    // 禁用所有PWM通道
    disable_pwm_hardware(PWM_CHANNEL_4);
    disable_pwm_hardware(PWM_CHANNEL_5);
    
    // 清零驱动结构体
    memset(&g_pwm_driver, 0, sizeof(dual_pwm_driver_t));
    
    log_info("Dual PWM driver deinitialized\n");
}

void dual_pwm_test(void)
{
    log_info("Starting dual PWM test...\n");
    
    // 初始化PWM：PWM4=1KHz,50%；PWM5=2KHz,75%
    if (dual_pwm_init(1000, 5000, 2000, 7500) != 0) {
        log_info("PWM initialization failed\n");
        return;
    }
    
    log_info("Test 1: Initial configuration\n");
    log_info("PWM4: %dHz, %d.%02d%%, state=%d\n", 
             dual_pwm_get_frequency(PWM_CHANNEL_4),
             dual_pwm_get_duty_cycle(PWM_CHANNEL_4)/100,
             dual_pwm_get_duty_cycle(PWM_CHANNEL_4)%100,
             dual_pwm_get_state(PWM_CHANNEL_4));
    log_info("PWM5: %dHz, %d.%02d%%, state=%d\n", 
             dual_pwm_get_frequency(PWM_CHANNEL_5),
             dual_pwm_get_duty_cycle(PWM_CHANNEL_5)/100,
             dual_pwm_get_duty_cycle(PWM_CHANNEL_5)%100,
             dual_pwm_get_state(PWM_CHANNEL_5));
    
    // 等待一段时间
    extern void wdt_clr();
    for (int i = 0; i < 100000; i++) {
        wdt_clr();
    }
    
    log_info("Test 2: Disable PWM4\n");
    dual_pwm_disable(PWM_CHANNEL_4);
    
    for (int i = 0; i < 100000; i++) {
        wdt_clr();
    }
    
    log_info("Test 3: Change PWM5 frequency to 500Hz\n");
    dual_pwm_set_frequency(PWM_CHANNEL_5, 500);
    
    for (int i = 0; i < 100000; i++) {
        wdt_clr();
    }
    
    log_info("Test 4: Change PWM5 duty cycle to 25%%\n");
    dual_pwm_set_duty_cycle(PWM_CHANNEL_5, 2500);
    
    for (int i = 0; i < 100000; i++) {
        wdt_clr();
    }
    
    log_info("Test 5: Re-enable PWM4\n");
    dual_pwm_enable(PWM_CHANNEL_4);
    
    for (int i = 0; i < 100000; i++) {
        wdt_clr();
    }
    
    log_info("Test 6: Change PWM4 to 10KHz, 10%%\n");
    dual_pwm_set_frequency(PWM_CHANNEL_4, 10000);
    dual_pwm_set_duty_cycle(PWM_CHANNEL_4, 1000);
    
    log_info("Final status:\n");
    log_info("PWM4: %dHz, %d.%02d%%, state=%d\n", 
             dual_pwm_get_frequency(PWM_CHANNEL_4),
             dual_pwm_get_duty_cycle(PWM_CHANNEL_4)/100,
             dual_pwm_get_duty_cycle(PWM_CHANNEL_4)%100,
             dual_pwm_get_state(PWM_CHANNEL_4));
    log_info("PWM5: %dHz, %d.%02d%%, state=%d\n", 
             dual_pwm_get_frequency(PWM_CHANNEL_5),
             dual_pwm_get_duty_cycle(PWM_CHANNEL_5)/100,
             dual_pwm_get_duty_cycle(PWM_CHANNEL_5)%100,
             dual_pwm_get_state(PWM_CHANNEL_5));
    
    log_info("Dual PWM test completed\n");
} 

u8 lightTransmittrance_data;
void set_elecGlass_lightTransmittrance(u32 lightTransmittrance){
    if(lightTransmittrance > 100){
        lightTransmittrance = 100;
    }
    if(lightTransmittrance < 0){
        lightTransmittrance = 0;
    }

#ifdef USE_DC_GLASS
    // 高频降纹波 2.8->-1.5v~1.5v
    disable_pwm_hardware(PWM_CHANNEL_4);
    disable_pwm_hardware(PWM_CHANNEL_5);
    lightTransmittrance = lightTransmittrance * 50;
    init_single_pwm(PWM_CHANNEL_4, 100 * 1000, lightTransmittrance);
    init_single_pwm(PWM_CHANNEL_5, 100 * 1000, 5000 - lightTransmittrance);
    JL_TIMER_TypeDef *timer4 = get_timer(PWM_CHANNEL_4);
    JL_TIMER_TypeDef *timer5 = get_timer(PWM_CHANNEL_5);
    JL_TIMER4->CON |= BIT(8) | (0b01 << 0);
    JL_TIMER5->CON |= BIT(8) | (0b01 << 0);
#endif

#ifdef USE_AC_GLASS_BY_PWM
    #define AC_GLASS_FREQ 128
    // 100Hz,固定180°相位差,占空比不超过50%
    disable_pwm_hardware(PWM_CHANNEL_0);
    disable_pwm_hardware(PWM_CHANNEL_4);
    disable_pwm_hardware(PWM_CHANNEL_5);
    lightTransmittrance = lightTransmittrance * 100;
    init_single_pwm(PWM_CHANNEL_0, AC_GLASS_FREQ, 5000); // 公共GND
    if (lightTransmittrance == 0){
        disable_pwm_hardware(PWM_CHANNEL_4);
        disable_pwm_hardware(PWM_CHANNEL_5);
    } else {
        init_single_pwm(PWM_CHANNEL_4, AC_GLASS_FREQ, lightTransmittrance / 2); // 镜片A 阳极
        init_single_pwm(PWM_CHANNEL_5, AC_GLASS_FREQ, lightTransmittrance / 2); // 镜片B 阳极
        JL_TIMER_TypeDef *timer4 = get_timer(PWM_CHANNEL_4);
        JL_TIMER_TypeDef *timer5 = get_timer(PWM_CHANNEL_5);
        timer4->CON |= BIT(8) | (0b01 << 0);
        timer5->CON |= BIT(8) | (0b01 << 0);
    }   
    JL_TIMER_TypeDef *timer0 = get_timer(PWM_CHANNEL_0);
    u32 period = EFFECTIVE_CLK / AC_GLASS_FREQ;
    timer0->CNT =  period / 2; // 相位偏移180°
    timer0->CON |= BIT(8) | (0b01 << 0);
#endif

#ifdef USE_AC_GLASS_BY_GPIO
    static bool init_flag = false;
    if(!init_flag){
        init_flag = true;
        gpio_direction_output(IO_PORTA_01, 0);
        gpio_direction_output(IO_PORTC_04, 0);
        gpio_direction_output(IO_PORTA_05, 0);  
    }
    lightTransmittrance_data = lightTransmittrance / 10;
#endif
}

void elecGlass_timer_callback(void *priv)
{
    static u32 count = 0;

    if(count < lightTransmittrance_data){
        gpio_write(IO_PORTA_01, 0);
        gpio_write(IO_PORTC_04, 0);
        gpio_write(IO_PORTA_05, 0);
    } else {
        gpio_write(IO_PORTA_01, 1);
        gpio_write(IO_PORTC_04, 1);
        gpio_write(IO_PORTA_05, 1);
    }

    count++;
    if (count >= 10){
        count = 0;
    }
}

static u8 pwm_is_dile(void)
{
    return 0;
}

static enum LOW_POWER_LEVEL pwm_level_query(void)
{   
    return LOW_POWER_MODE_LIGHT_SLEEP;
}

REGISTER_LP_TARGET(pwm_lp_target) = {
    .name = "user_pwm",
    .level = pwm_level_query,
    .is_idle = pwm_is_dile,
};