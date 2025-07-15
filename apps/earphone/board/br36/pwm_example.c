#include "dual_pwm_driver.h"
#include "system/includes.h"

#define LOG_TAG_CONST       PWM_EXAMPLE
#define LOG_TAG             "[PWM_EXAMPLE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

/**
 * @brief 简单延时函数
 * @param ms 延时毫秒数
 */
static void delay_ms(u32 ms)
{
    extern void wdt_clr();
    // 简单的软件延时，实际项目中建议使用定时器
    for (u32 i = 0; i < ms * 1000; i++) {
        wdt_clr();  // 清看门狗
    }
}

/**
 * @brief 基本PWM使用示例
 */
void pwm_basic_example(void)
{
    log_info("=== PWM基本使用示例 ===\n");
    
    // 1. 初始化PWM
    // PWM4(PC4): 1KHz, 50%
    // PWM5(PA5): 2KHz, 75%
    if (dual_pwm_init(1000, 5000, 2000, 7500) != 0) {
        log_error("PWM初始化失败\n");
        return;
    }
    log_info("PWM初始化成功\n");
    
    // 等待2秒
    delay_ms(2000);
    
    // 2. 调节PWM4频率为500Hz
    log_info("调节PWM4频率为500Hz\n");
    dual_pwm_set_frequency(PWM_CHANNEL_4, 500);
    delay_ms(2000);
    
    // 3. 调节PWM5占空比为25%
    log_info("调节PWM5占空比为25%%\n");
    dual_pwm_set_duty_cycle(PWM_CHANNEL_5, 2500);
    delay_ms(2000);
    
    // 4. 临时关闭PWM4
    log_info("关闭PWM4\n");
    dual_pwm_disable(PWM_CHANNEL_4);
    delay_ms(2000);
    
    // 5. 重新启用PWM4
    log_info("重新启用PWM4\n");
    dual_pwm_enable(PWM_CHANNEL_4);
    delay_ms(2000);
    
    // 6. 查询当前状态
    log_info("当前PWM状态：\n");
    log_info("PWM4: %dHz, %d.%02d%%, 状态=%s\n", 
             dual_pwm_get_frequency(PWM_CHANNEL_4),
             dual_pwm_get_duty_cycle(PWM_CHANNEL_4)/100,
             dual_pwm_get_duty_cycle(PWM_CHANNEL_4)%100,
             dual_pwm_get_state(PWM_CHANNEL_4) ? "启用" : "禁用");
    
    log_info("PWM5: %dHz, %d.%02d%%, 状态=%s\n", 
             dual_pwm_get_frequency(PWM_CHANNEL_5),
             dual_pwm_get_duty_cycle(PWM_CHANNEL_5)/100,
             dual_pwm_get_duty_cycle(PWM_CHANNEL_5)%100,
             dual_pwm_get_state(PWM_CHANNEL_5) ? "启用" : "禁用");
    
    log_info("基本示例完成\n");
}

/**
 * @brief PWM呼吸灯效果示例
 */
void pwm_breathing_example(void)
{
    log_info("=== PWM呼吸灯效果示例 ===\n");
    
    // 初始化PWM，占空比从0%开始
    if (dual_pwm_init(1000, 0, 1000, 0) != 0) {
        log_error("PWM初始化失败\n");
        return;
    }
    
    // 3个呼吸周期
    for (int cycle = 0; cycle < 3; cycle++) {
        log_info("呼吸周期 %d/3\n", cycle + 1);
        
        // 亮度从0%渐变到100%
        for (int duty = 0; duty <= 10000; duty += 200) {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_4, duty);
            dual_pwm_set_duty_cycle(PWM_CHANNEL_5, duty);
            delay_ms(20);  // 延时20ms
        }
        
        // 亮度从100%渐变到0%
        for (int duty = 10000; duty >= 0; duty -= 200) {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_4, duty);
            dual_pwm_set_duty_cycle(PWM_CHANNEL_5, duty);
            delay_ms(20);
        }
        
        delay_ms(500);  // 间隔500ms
    }
    
    log_info("呼吸灯效果完成\n");
}

/**
 * @brief 频率扫描示例
 */
void pwm_frequency_sweep_example(void)
{
    log_info("=== PWM频率扫描示例 ===\n");
    
    // 初始化PWM，50%占空比
    if (dual_pwm_init(100, 5000, 100, 5000) != 0) {
        log_error("PWM初始化失败\n");
        return;
    }
    
    // 频率从100Hz扫描到5KHz
    log_info("频率扫描: 100Hz -> 5KHz\n");
    for (u32 freq = 100; freq <= 5000; freq += 200) {
        log_info("当前频率: PWM4=%dHz, PWM5=%dHz\n", freq, freq * 2);
        dual_pwm_set_frequency(PWM_CHANNEL_4, freq);
        dual_pwm_set_frequency(PWM_CHANNEL_5, freq * 2);  // PWM5频率是PWM4的2倍
        delay_ms(300);  // 每个频率持续300ms
    }
    
    log_info("频率扫描完成\n");
}

/**
 * @brief 占空比变化示例
 */
void pwm_duty_cycle_example(void)
{
    log_info("=== PWM占空比变化示例 ===\n");
    
    // 初始化PWM，1KHz频率
    if (dual_pwm_init(1000, 1000, 1000, 1000) != 0) {
        log_error("PWM初始化失败\n");
        return;
    }
    
    // 占空比从10%变化到90%
    log_info("占空比变化: 10%% -> 90%%\n");
    for (int duty = 1000; duty <= 9000; duty += 500) {
        log_info("当前占空比: %d.%02d%%\n", duty/100, duty%100);
        dual_pwm_set_duty_cycle(PWM_CHANNEL_4, duty);
        dual_pwm_set_duty_cycle(PWM_CHANNEL_5, 10000 - duty);  // PWM5与PWM4互补
        delay_ms(500);
    }
    
    log_info("占空比变化完成\n");
}

/**
 * @brief 独立控制示例
 */
void pwm_independent_control_example(void)
{
    log_info("=== PWM独立控制示例 ===\n");
    
    // 初始化PWM
    if (dual_pwm_init(1000, 5000, 2000, 5000) != 0) {
        log_error("PWM初始化失败\n");
        return;
    }
    
    // 演示独立控制
    log_info("PWM4快闪，PWM5慢闪\n");
    for (int i = 0; i < 20; i++) {
        // PWM4快闪 (200ms周期)
        if (i % 2 == 0) {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_4, 8000);  // 80%
        } else {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_4, 1000);  // 10%
        }
        
        // PWM5慢闪 (800ms周期)
        if ((i / 4) % 2 == 0) {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_5, 9000);  // 90%
        } else {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_5, 500);   // 5%
        }
        
        delay_ms(200);
    }
    
    log_info("独立控制示例完成\n");
}

/**
 * @brief 运行所有PWM示例
 */
void run_all_pwm_examples(void)
{
    log_info("开始运行所有PWM示例...\n");
    
    // 1. 基本使用示例
    pwm_basic_example();
    delay_ms(1000);
    dual_pwm_deinit();
    delay_ms(500);
    
    // 2. 呼吸灯效果
    pwm_breathing_example();
    delay_ms(1000);
    dual_pwm_deinit();
    delay_ms(500);
    
    // 3. 频率扫描
    pwm_frequency_sweep_example();
    delay_ms(1000);
    dual_pwm_deinit();
    delay_ms(500);
    
    // 4. 占空比变化
    pwm_duty_cycle_example();
    delay_ms(1000);
    dual_pwm_deinit();
    delay_ms(500);
    
    // 5. 独立控制
    pwm_independent_control_example();
    delay_ms(1000);
    dual_pwm_deinit();
    
    log_info("所有PWM示例运行完成！\n");
}

/**
 * @brief PWM性能测试
 */
void pwm_performance_test(void)
{
    log_info("=== PWM性能测试 ===\n");
    
    if (dual_pwm_init(1000, 5000, 1000, 5000) != 0) {
        log_error("PWM初始化失败\n");
        return;
    }
    
    // 测试快速切换频率
    log_info("测试快速切换频率（100次）\n");
    u32 start_time = get_sys_time();  // 假设有这个函数
    for (int i = 0; i < 100; i++) {
        dual_pwm_set_frequency(PWM_CHANNEL_4, 1000 + (i % 100) * 10);
        dual_pwm_set_frequency(PWM_CHANNEL_5, 2000 + (i % 100) * 20);
    }
    u32 end_time = get_sys_time();
    log_info("频率切换性能: 100次操作耗时 %d ms\n", end_time - start_time);
    
    // 测试快速切换占空比
    log_info("测试快速切换占空比（100次）\n");
    start_time = get_sys_time();
    for (int i = 0; i < 100; i++) {
        dual_pwm_set_duty_cycle(PWM_CHANNEL_4, 1000 + (i % 90) * 100);
        dual_pwm_set_duty_cycle(PWM_CHANNEL_5, 5000 + (i % 50) * 100);
    }
    end_time = get_sys_time();
    log_info("占空比切换性能: 100次操作耗时 %d ms\n", end_time - start_time);
    
    log_info("性能测试完成\n");
    dual_pwm_deinit();
} 