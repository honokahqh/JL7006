# board_ac700n_demo 双路PWM驱动使用说明

## 概述

本驱动实现了AC700N芯片上PA5-PWM5和PC4-PWM4两路PWM输出功能，支持频率和占空比的实时调节，以及PWM通道的开关控制。

## 硬件引脚配置

- **PWM4**: PC4引脚，使用TIMER4
- **PWM5**: PA5引脚，使用TIMER5

## 功能特性

- ✅ 可调频率：支持1Hz到数百kHz的频率范围
- ✅ 可调占空比：0-100%精确控制（分辨率0.01%）
- ✅ 随时开关：支持单独启用/禁用每个PWM通道
- ✅ 实时调节：运行时动态修改频率和占空比
- ✅ 状态查询：可查询当前PWM参数和状态
- ✅ 错误处理：完善的参数检查和错误返回
- ✅ 中文注释：所有代码和接口都有详细中文注释

## API接口说明

### 1. 初始化函数

```c
int dual_pwm_init(u32 pwm4_freq, u32 pwm4_duty, u32 pwm5_freq, u32 pwm5_duty);
```

**功能**: 初始化双路PWM驱动
**参数**: 
- `pwm4_freq`: PWM4初始频率(Hz)
- `pwm4_duty`: PWM4初始占空比(0-10000对应0-100%)  
- `pwm5_freq`: PWM5初始频率(Hz)
- `pwm5_duty`: PWM5初始占空比(0-10000对应0-100%)

**返回值**: 0=成功, -1=失败

### 2. 开关控制函数

```c
int dual_pwm_enable(pwm_channel_t channel);   // 启用PWM通道
int dual_pwm_disable(pwm_channel_t channel);  // 禁用PWM通道
```

**参数**: `channel` - PWM_CHANNEL_4 或 PWM_CHANNEL_5

### 3. 参数调节函数

```c
int dual_pwm_set_frequency(pwm_channel_t channel, u32 frequency);   // 设置频率
int dual_pwm_set_duty_cycle(pwm_channel_t channel, u32 duty_cycle); // 设置占空比
```

### 4. 状态查询函数

```c
u32 dual_pwm_get_frequency(pwm_channel_t channel);     // 获取当前频率
s32 dual_pwm_get_duty_cycle(pwm_channel_t channel);    // 获取当前占空比
pwm_state_t dual_pwm_get_state(pwm_channel_t channel); // 获取PWM状态
```

### 5. 清理函数

```c
void dual_pwm_deinit(void);  // 反初始化PWM驱动
```

## 使用示例

### 基本使用

```c
#include "dual_pwm_driver.h"

void pwm_example(void)
{
    // 1. 初始化PWM
    // PWM4: 1KHz, 50%
    // PWM5: 2KHz, 75%
    if (dual_pwm_init(1000, 5000, 2000, 7500) != 0) {
        printf("PWM初始化失败\n");
        return;
    }
    
    // 2. 运行时调节PWM4频率为500Hz
    dual_pwm_set_frequency(PWM_CHANNEL_4, 500);
    
    // 3. 调节PWM5占空比为25%
    dual_pwm_set_duty_cycle(PWM_CHANNEL_5, 2500);
    
    // 4. 临时关闭PWM4
    dual_pwm_disable(PWM_CHANNEL_4);
    
    // 延时...
    
    // 5. 重新启用PWM4
    dual_pwm_enable(PWM_CHANNEL_4);
    
    // 6. 查询当前状态
    printf("PWM4频率: %dHz\n", dual_pwm_get_frequency(PWM_CHANNEL_4));
    printf("PWM5占空比: %d.%02d%%\n", 
           dual_pwm_get_duty_cycle(PWM_CHANNEL_5)/100,
           dual_pwm_get_duty_cycle(PWM_CHANNEL_5)%100);
}
```

### 高级应用：PWM呼吸灯效果

```c
void pwm_breathing_effect(void)
{
    // 初始化PWM
    dual_pwm_init(1000, 0, 1000, 0);  // 1KHz, 0%占空比
    
    // 呼吸灯效果
    for (int cycle = 0; cycle < 10; cycle++) {
        // 亮度从0%渐变到100%
        for (int duty = 0; duty <= 10000; duty += 100) {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_4, duty);
            dual_pwm_set_duty_cycle(PWM_CHANNEL_5, duty);
            delay_ms(10);  // 延时10ms
        }
        
        // 亮度从100%渐变到0%
        for (int duty = 10000; duty >= 0; duty -= 100) {
            dual_pwm_set_duty_cycle(PWM_CHANNEL_4, duty);
            dual_pwm_set_duty_cycle(PWM_CHANNEL_5, duty);
            delay_ms(10);
        }
    }
}
```

### 动态频率扫描

```c
void pwm_frequency_sweep(void)
{
    // 初始化PWM
    dual_pwm_init(100, 5000, 100, 5000);  // 100Hz, 50%
    
    // 频率从100Hz扫描到10KHz
    for (u32 freq = 100; freq <= 10000; freq += 100) {
        dual_pwm_set_frequency(PWM_CHANNEL_4, freq);
        dual_pwm_set_frequency(PWM_CHANNEL_5, freq * 2);  // PWM5频率是PWM4的2倍
        delay_ms(100);  // 每个频率持续100ms
    }
}
```

## 技术参数

- **时钟源**: 24MHz系统时钟
- **分频系数**: 4分频 (有效时钟6MHz)
- **频率范围**: 约1Hz - 600KHz (理论值)
- **占空比精度**: 0.01% (10000级)
- **资源占用**: TIMER4, TIMER5
- **功耗**: 低功耗设计，支持动态关闭

## 注意事项

1. **引脚冲突**: 使用PWM功能前，确保PC4和PA5引脚未被其他功能占用
2. **定时器冲突**: TIMER4和TIMER5不能再用于其他用途
3. **频率限制**: 过高频率可能导致占空比精度下降
4. **参数范围**: 占空比参数范围为0-10000，超出会返回错误
5. **初始化顺序**: 必须先调用`dual_pwm_init()`再使用其他功能
6. **错误处理**: 所有函数都有返回值，建议检查返回值

## 故障排除

1. **PWM无输出**: 
   - 检查初始化是否成功
   - 确认引脚配置正确
   - 验证占空比不为0

2. **频率不准确**:
   - 检查系统时钟是否为24MHz
   - 验证频率参数计算是否正确

3. **编译错误**:
   - 确保包含了头文件
   - 检查工程中是否添加了源文件

## 测试功能

驱动提供了内置测试函数：

```c
void dual_pwm_test(void);  // 执行PWM功能测试
```

该函数会自动测试所有PWM功能，包括初始化、开关控制、频率调节、占空比调节等，并输出详细的测试日志。

## 版本信息

- **版本**: v1.0
- **作者**: [您的名字]
- **日期**: 2024年
- **兼容性**: AC700N芯片 + board_ac700n_demo 