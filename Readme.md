# 开发记录

## 关于烧录
烧录器v4.0,所有拨码均关闭,连接上电后按键由红灯常亮+绿灯闪烁转为红灯常亮+绿灯熄灭,进入烧录模式
codeBlocks build,自动执行编译以及烧录,而后插拔烧录器即可(需确认日志是否检索到设备烧录成功)

## 功能开发
1. 配置文件  
    - board_config.h  
    - board_ac700n_demo_cfg.h  
    - board_ac700n_demo_global_build_cfg.h  
    - board_ac700n_demo.c  
    - app_config.h
    - user_cfg.c
2. 日志打印
    - 可选:安装杰理虚拟串口工具,使用烧录器实现烧录+日志打印
    - CONFIG_DEBUG_ENABLE
    - LIB_DEBUG 1   
    - TCFG_UART0_ENABLE
    - TCFG_UART0_TX_PORT IO_PORT_DP
3. 触摸按键处理
    - 使能touch key,失能io key,按键数量为1,使能wake up
    - user_cfg.c内的按键使用cfg.bin只对io key和ad key生效,touch key需配置key table
    - key_event_deal.c内实现具体的控制逻辑
    - lp_touch_key_hw.h 配置触摸时间
4. MIC处理
    - CONFIG_ANC_ENABLE 
    - TCFG_AUDIO_DUAL_MIC_ENABLE
    - 仅使用MIC时会输出MIC_VDD
5. speaker处理
    - TCFG_AUDIO_DAC_CONNECT_MODE 配置声道
    - audio_dac_power_state 覆写接口,配置PA IO
6. 霍尔开关处理
    - port_wakeup 注册
    - port_protect 注册
    - sys_timer_add 非中断形式,检测IO电平pwr off
7. 双io驱双LED处理
    - 无改动
8. EQ调节
9. tws左右耳配置
    - CONFIG_TWS_CHANNEL_SELECT
10. ble gatt server 
    