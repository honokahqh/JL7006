#ifndef _DUAL_PWM_DRIVER_H_
#define _DUAL_PWM_DRIVER_H_

#include "asm/includes.h"
#include "asm/gpio.h"

// PWM通道定义
typedef enum {
    PWM_CHANNEL_4 = 4,  // PC4对应PWM4，使用TIMER4
    PWM_CHANNEL_5 = 5,  // PA5对应PWM5，使用TIMER5
} pwm_channel_t;

// PWM状态定义
typedef enum {
    PWM_STATE_DISABLED = 0,
    PWM_STATE_ENABLED = 1
} pwm_state_t;

// PWM配置结构体
typedef struct {
    u32 frequency;      // 频率，单位Hz
    u32 duty_cycle;     // 占空比，0~10000对应0~100%
    u32 pin;           // 引脚编号
    pwm_state_t state; // PWM状态
    JL_TIMER_TypeDef *timer; // 对应的定时器
} pwm_config_t;

// PWM驱动结构体
typedef struct {
    pwm_config_t channels[2];  // 两个PWM通道
    u8 initialized;           // 初始化标志
} dual_pwm_driver_t;

// 函数声明

/**
 * @brief 初始化双路PWM驱动
 * @param pwm4_freq PWM4初始频率(Hz)
 * @param pwm4_duty PWM4初始占空比(0-10000对应0-100%)
 * @param pwm5_freq PWM5初始频率(Hz) 
 * @param pwm5_duty PWM5初始占空比(0-10000对应0-100%)
 * @return 0:成功, -1:失败
 */
int dual_pwm_init(u32 pwm4_freq, u32 pwm4_duty, u32 pwm5_freq, u32 pwm5_duty);

/**
 * @brief 启用指定PWM通道
 * @param channel PWM通道(PWM_CHANNEL_4或PWM_CHANNEL_5)
 * @return 0:成功, -1:失败
 */
int dual_pwm_enable(pwm_channel_t channel);

/**
 * @brief 禁用指定PWM通道
 * @param channel PWM通道(PWM_CHANNEL_4或PWM_CHANNEL_5)
 * @return 0:成功, -1:失败
 */
int dual_pwm_disable(pwm_channel_t channel);

/**
 * @brief 设置PWM频率
 * @param channel PWM通道
 * @param frequency 频率(Hz)
 * @return 0:成功, -1:失败
 */
int dual_pwm_set_frequency(pwm_channel_t channel, u32 frequency);

/**
 * @brief 设置PWM占空比
 * @param channel PWM通道
 * @param duty_cycle 占空比(0-10000对应0-100%)
 * @return 0:成功, -1:失败
 */
int dual_pwm_set_duty_cycle(pwm_channel_t channel, u32 duty_cycle);

/**
 * @brief 获取PWM当前频率
 * @param channel PWM通道
 * @return 当前频率(Hz)，0表示错误
 */
u32 dual_pwm_get_frequency(pwm_channel_t channel);

/**
 * @brief 获取PWM当前占空比
 * @param channel PWM通道
 * @return 当前占空比(0-10000)，-1表示错误
 */
s32 dual_pwm_get_duty_cycle(pwm_channel_t channel);

/**
 * @brief 获取PWM状态
 * @param channel PWM通道
 * @return PWM状态
 */
pwm_state_t dual_pwm_get_state(pwm_channel_t channel);

/**
 * @brief 反初始化PWM驱动
 */
void dual_pwm_deinit(void);

/**
 * @brief 测试PWM驱动功能
 */
void dual_pwm_test(void);

int dual_pwm_sync_start_with_phase(u32 pwm_freq, u32 pwm_duty, u32 phase_diff_degrees);

typedef enum{
    GLASS_DC = 0,
    GLASS_AC = 1,
}elecGlass_type_t;

void set_elecGlass_lightTransmittrance(elecGlass_type_t type, u32 lightTransmittrance);

#endif /* _DUAL_PWM_DRIVER_H_ */ 