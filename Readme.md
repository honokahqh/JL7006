# 开发记录

## 关于烧录
烧录器v4.0,所有拨码均关闭,连接上电后按键由红灯常亮+绿灯闪烁转为红灯常亮+绿灯熄灭,进入烧录模式
codeBlocks build,自动执行编译以及烧录,而后插拔烧录器即可(需确认日志是否检索到设备烧录成功)

## 功能开发
1. 配置文件  
    - board_config.h  
    - board_ac7006f_earphone_cfg.h  
    - board_ac7006f_earphone_global_build_cfg.h  
    - board_ac7006f_earphone.c  
2. 日志打印
    - 可选:安装杰理虚拟串口工具,使用烧录器实现烧录+日志打印
    - CONFIG_DEBUG_ENABLE
    - LIB_DEBUG 1   
    - TCFG_UART0_ENABLE
    - TCFG_UART0_TX_PORT IO_PORTA_08 (测试IO_PORT_DP不行)
3. 按键处理
    - io key测试ok
    - touch key测试失败
    - key_table配置处理逻辑
    - 通话时的key deal在函数app_earphone_key_event_handler内处理,sdk默认将其hand up等逻辑与music pp等进行绑定
4. MIC处理
    - CONFIG_ANC_ENABLE 
    - TCFG_AUDIO_DUAL_MIC_ENABLE
    - 仅使用MIC时会输出MIC_VDD
5. speaker处理
    - PA IO
    - 覆写接口audio_dac_power_state
6. 霍尔开关处理
    - PB3 
7. 双io驱双LED处理
    - 
8. EQ调节
9. 蓝牙无播放时保活休眠
10. tws左右耳配置
    - CONFIG_TWS_CHANNEL_SELECT
    - 