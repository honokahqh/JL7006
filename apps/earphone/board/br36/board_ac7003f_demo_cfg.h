#ifndef CONFIG_BOARD_AC7003F_DEMO_CFG_H
#define CONFIG_BOARD_AC7003F_DEMO_CFG_H

#include "board_ac7003f_demo_global_build_cfg.h"

#ifdef CONFIG_BOARD_AC7003F_DEMO

#define CONFIG_SDFILE_ENABLE

//*********************************************************************************//
//                                 配置开始                                        //
//*********************************************************************************//
#define ENABLE_THIS_MOUDLE					1
#define DISABLE_THIS_MOUDLE					0

#define ENABLE								1
#define DISABLE								0

#define NO_CONFIG_PORT						(-1)

//*********************************************************************************//
//                                 UART配置                                        //
//*********************************************************************************//
#define TCFG_UART0_ENABLE					ENABLE_THIS_MOUDLE                     //串口打印模块使能
#define TCFG_UART0_RX_PORT					NO_CONFIG_PORT                         //串口接收脚配置（用于打印可以选择NO_CONFIG_PORT）
#define TCFG_UART0_TX_PORT  				IO_PORTA_08                             //串口发送脚配置
#define TCFG_UART0_BAUDRATE  				1000000                                //串口波特率配置

//*********************************************************************************//
//                                  NTC配置                                       //
//*********************************************************************************//
#define NTC_DET_EN      0
#define NTC_POWER_IO    IO_PORTC_03
#define NTC_DETECT_IO   IO_PORTC_04
#define NTC_DET_AD_CH   (0x4)   //根据adc_api.h修改通道号

#define NTC_DET_UPPER        235  //正常范围AD值上限，0度时
#define NTC_DET_LOWER        34  //正常范围AD值下限，45度时

//*********************************************************************************//
//                                 IIC配置                                        //
//*********************************************************************************//
/*软件IIC设置*/
#define TCFG_SW_I2C0_CLK_PORT               IO_PORTA_09                             //软件IIC  CLK脚选择
#define TCFG_SW_I2C0_DAT_PORT               IO_PORTA_10                             //软件IIC  DAT脚选择
#define TCFG_SW_I2C0_DELAY_CNT              50                                      //IIC延时参数，影响通讯时钟频率

/*硬件IIC端口选择
  SCL         SDA
  'A': IO_PORT_DP   IO_PORT_DM
  'B': IO_PORTA_09  IO_PORTA_10
  'C': IO_PORTA_07  IO_PORTA_08
  'D': IO_PORTA_05  IO_PORTA_06
 */
#define TCFG_HW_I2C0_PORTS                  'B'
#define TCFG_HW_I2C0_CLK                    100000                                  //硬件IIC波特率

//*********************************************************************************//
//                                  SD 配置                                        //
//*********************************************************************************//
#define TCFG_SD0_ENABLE						DISABLE_THIS_MOUDLE                    //SD0模块使能
#define TCFG_SD0_DAT_MODE					1               //线数设置，1：一线模式  4：四线模式
#define TCFG_SD0_DET_MODE					SD_IO_DECT     //SD卡检测方式
#define TCFG_SD0_DET_IO                     IO_PORTC_02      //当检测方式为IO检测可用
#define TCFG_SD0_DET_IO_LEVEL               0               //当检测方式为IO检测可用,0：低电平检测到卡。 1：高电平(外部电源)检测到卡。 2：高电平(SD卡电源)检测到卡。
#define TCFG_SD0_CLK						(3000000 * 4L)  //SD卡时钟频率设置
#define TCFG_SD0_PORT_CMD					IO_PORTC_04
#define TCFG_SD0_PORT_CLK					IO_PORTC_05
#define TCFG_SD0_PORT_DA0					IO_PORTC_03
#define TCFG_SD0_PORT_DA1					NO_CONFIG_PORT  //当选择4线模式时要用
#define TCFG_SD0_PORT_DA2					NO_CONFIG_PORT
#define TCFG_SD0_PORT_DA3					NO_CONFIG_PORT
//*********************************************************************************//
//                                 key 配置                                        //
//*********************************************************************************//
#define KEY_NUM_MAX                        	10
#define KEY_NUM                            	3

#define MULT_KEY_ENABLE						DISABLE 		//是否使能组合按键消息, 使能后需要配置组合按键映射表
//*********************************************************************************//
//                                 iokey 配置                                      //
//*********************************************************************************//
#define TCFG_IOKEY_ENABLE					ENABLE_THIS_MOUDLE //是否使能IO按键

#define TCFG_IOKEY_POWER_CONNECT_WAY		ONE_PORT_TO_LOW    //按键一端接低电平一端接IO

#define TCFG_IOKEY_POWER_ONE_PORT			IO_PORTC_05        //IO按键端口

#define TCFG_IOKEY_PREV_CONNECT_WAY			ONE_PORT_TO_LOW  //按键一端接低电平一端接IO
#define TCFG_IOKEY_PREV_ONE_PORT			IO_PORT_DM

#define TCFG_IOKEY_NEXT_CONNECT_WAY 		ONE_PORT_TO_LOW  //按键一端接低电平一端接IO
#define TCFG_IOKEY_NEXT_ONE_PORT			IO_PORT_DP


//*********************************************************************************//
//                                 adkey 配置                                      //
//*********************************************************************************//
#define TCFG_ADKEY_ENABLE                   DISABLE_THIS_MOUDLE//是否使能AD按键
#define TCFG_ADKEY_PORT                     IO_PORT_DM         //AD按键端口(需要注意选择的IO口是否支持AD功能)

#define TCFG_ADKEY_AD_CHANNEL               AD_CH_DM
#define TCFG_ADKEY_EXTERN_UP_ENABLE         ENABLE_THIS_MOUDLE //是否使用外部上拉

#if TCFG_ADKEY_EXTERN_UP_ENABLE
#define R_UP    220                 //22K，外部上拉阻值在此自行设置
#else
#define R_UP    100                 //10K，内部上拉默认10K
#endif

//必须从小到大填电阻，没有则同VDDIO,填0x3ffL
#define TCFG_ADKEY_AD0      (0)                                 //0R
#define TCFG_ADKEY_AD1      (0x3ffL * 30   / (30   + R_UP))     //3k
#define TCFG_ADKEY_AD2      (0x3ffL * 62   / (62   + R_UP))     //6.2k
#define TCFG_ADKEY_AD3      (0x3ffL * 91   / (91   + R_UP))     //9.1k
#define TCFG_ADKEY_AD4      (0x3ffL * 150  / (150  + R_UP))     //15k
#define TCFG_ADKEY_AD5      (0x3ffL * 240  / (240  + R_UP))     //24k
#define TCFG_ADKEY_AD6      (0x3ffL * 330  / (330  + R_UP))     //33k
#define TCFG_ADKEY_AD7      (0x3ffL * 510  / (510  + R_UP))     //51k
#define TCFG_ADKEY_AD8      (0x3ffL * 1000 / (1000 + R_UP))     //100k
#define TCFG_ADKEY_AD9      (0x3ffL * 2200 / (2200 + R_UP))     //220k
#define TCFG_ADKEY_VDDIO    (0x3ffL)

#define TCFG_ADKEY_VOLTAGE0 ((TCFG_ADKEY_AD0 + TCFG_ADKEY_AD1) / 2)
#define TCFG_ADKEY_VOLTAGE1 ((TCFG_ADKEY_AD1 + TCFG_ADKEY_AD2) / 2)
#define TCFG_ADKEY_VOLTAGE2 ((TCFG_ADKEY_AD2 + TCFG_ADKEY_AD3) / 2)
#define TCFG_ADKEY_VOLTAGE3 ((TCFG_ADKEY_AD3 + TCFG_ADKEY_AD4) / 2)
#define TCFG_ADKEY_VOLTAGE4 ((TCFG_ADKEY_AD4 + TCFG_ADKEY_AD5) / 2)
#define TCFG_ADKEY_VOLTAGE5 ((TCFG_ADKEY_AD5 + TCFG_ADKEY_AD6) / 2)
#define TCFG_ADKEY_VOLTAGE6 ((TCFG_ADKEY_AD6 + TCFG_ADKEY_AD7) / 2)
#define TCFG_ADKEY_VOLTAGE7 ((TCFG_ADKEY_AD7 + TCFG_ADKEY_AD8) / 2)
#define TCFG_ADKEY_VOLTAGE8 ((TCFG_ADKEY_AD8 + TCFG_ADKEY_AD9) / 2)
#define TCFG_ADKEY_VOLTAGE9 ((TCFG_ADKEY_AD9 + TCFG_ADKEY_VDDIO) / 2)

#define TCFG_ADKEY_VALUE0                   0
#define TCFG_ADKEY_VALUE1                   1
#define TCFG_ADKEY_VALUE2                   2
#define TCFG_ADKEY_VALUE3                   3
#define TCFG_ADKEY_VALUE4                   4
#define TCFG_ADKEY_VALUE5                   5
#define TCFG_ADKEY_VALUE6                   6
#define TCFG_ADKEY_VALUE7                   7
#define TCFG_ADKEY_VALUE8                   8
#define TCFG_ADKEY_VALUE9                   9

//*********************************************************************************//
//                                 irkey 配置                                      //
//*********************************************************************************//
#define TCFG_IRKEY_ENABLE                   DISABLE_THIS_MOUDLE//是否使能AD按键
#define TCFG_IRKEY_PORT                     IO_PORTA_08        //IR按键端口

//*********************************************************************************//
//                             tocuh key 配置                                      //
//*********************************************************************************//
//*********************************************************************************//
//                             lp tocuh key 配置                                   //
//*********************************************************************************//
#define TCFG_LP_TOUCH_KEY_ENABLE 			DISABLE_THIS_MOUDLE 		//是否使能触摸按键
#define TCFG_LP_EARTCH_KEY_ENABLE 			DISABLE_THIS_MOUDLE 	 	//内置入耳检测是否使能
//内置触摸灵敏度调试工具使能, 使能后可以通过连接PC端上位机通过SPP调试,
//打开该宏需要确保同时打开宏:
//1.USER_SUPPORT_PROFILE_SPP
//2.APP_ONLINE_DEBUG
//可以针对每款样机校准灵敏度参数表(在lp_touch_key.c ch_sensitivity_table), 详细使用方法请参考《低功耗内置触摸介绍》文档.
#define TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE 	DISABLE

//电容检测灵敏度级数配置(范围: 0 ~ 9)
//该参数配置与触摸时电容变化量有关, 触摸时电容变化量跟模具厚度, 触摸片材质, 面积等有关,
//触摸时电容变化量越小, 推荐选择灵敏度级数越大,
//触摸时电容变化量越大, 推荐选择灵敏度级数越小,
//用户可以从灵敏度级数为0开始调试, 级数逐渐增大, 直到选择一个合适的灵敏度配置值.

#define TCFG_LP_TOUCH_KEY_SENSITIVITY 			5 	//触摸按键电容检测灵敏度配置(级数0 ~ 9)
#define TCFG_EARIN_TOUCH_KEY_SENSITIVITY 		5 	//触摸按键电容检测灵敏度配置(级数0 ~ 9)

#if TCFG_LP_TOUCH_KEY_ENABLE

//取消外置触摸的一些宏定义
#ifdef TCFG_IOKEY_ENABLE
#undef TCFG_IOKEY_ENABLE
#define TCFG_IOKEY_ENABLE 					DISABLE_THIS_MOUDLE
#endif /* #ifdef TCFG_IOKEY_ENABLE */

#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */

#if TCFG_LP_EARTCH_KEY_ENABLE

#ifdef TCFG_EARTCH_EVENT_HANDLE_ENABLE
#undef TCFG_EARTCH_EVENT_HANDLE_ENABLE
#endif /* #ifdef TCFG_EARTCH_EVENT_HANDLE_ENABLE */

#define TCFG_EARTCH_EVENT_HANDLE_ENABLE 	ENABLE_THIS_MOUDLE 		//入耳检测事件APP流程处理使能

#endif /* #if TCFG_LP_EARTCH_KEY_ENABLE */


//*********************************************************************************//
//                                 Audio配置                                       //
//*********************************************************************************//
/*内存使用自定义*/
#define TCFG_AUDIO_AAC_RAM_MALLOC_ENABLE	DISABLE_THIS_MOUDLE		//AAC编解码内存动态申请配置
#define TCFG_AUDIO_ADC_ENABLE				ENABLE_THIS_MOUDLE
#define TCFG_AUDIO_ADC_LINE_CHA				LADC_LINE0_MASK

#define TCFG_AUDIO_DAC_ENABLE				ENABLE_THIS_MOUDLE
#define TCFG_AUDIO_DAC_LDO_VOLT				DACVDD_LDO_1_80V //普通模式 DACVDD
#define TCFG_AUDIO_DAC_LDO_VOLT_HIGH        DACVDD_LDO_2_50V //音量增强模式 DACVDD
#define TCFG_AUDIO_DAC_DEFAULT_VOL_MODE     (0) //首次开机默认模式 1：音量增强模式  0：普通模式
#define TCFG_AUDIO_DAC_VCM_CAP_EN           (1) //VCM引脚是否有电容  0:没有  1:有
/*DAC硬件上的连接方式,可选的配置：
    DAC_OUTPUT_MONO_L               单左声道差分
    DAC_OUTPUT_LR                   双声道差分
*/
#define TCFG_AUDIO_DAC_CONNECT_MODE         DAC_OUTPUT_LR

/*PDM数字MIC配置*/
#define TCFG_AUDIO_PLNK_SCLK_PIN			IO_PORTA_07
#define TCFG_AUDIO_PLNK_DAT0_PIN			IO_PORTA_08
#define TCFG_AUDIO_PLNK_DAT1_PIN			NO_CONFIG_PORT

/**************
 *ANC配置
 *************/
#define TCFG_AUDIO_ANC_ENABLE				CONFIG_ANC_ENABLE		//ANC总使能,根据global_bulid_cfg板级定义
#define TCFG_ANC_TOOL_DEBUG_ONLINE 			ENABLE_THIS_MOUDLE		//ANC工具蓝牙spp调试
#define TCFG_ANC_EXPORT_RAM_EN				DISABLE_THIS_MOUDLE		//ANCdebug数据释放RAM使能
#if TCFG_ANC_EXPORT_RAM_EN
#define TCFG_AUDIO_CVP_CODE_AT_RAM			DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_AAC_CODE_AT_RAM			DISABLE_THIS_MOUDLE
#endif/*TCFG_ANC_EXPORT_RAM_EN*/
/*
 *当通话与ANC共用FF MIC，并且通话要求更大的MIC模拟增益时，使能TCFG_AUDIO_DYNAMIC_ADC_GAIN，
 *则通话MIC模拟增益以配置工具通话参数为准,通话时，ANC使用通话MIC模拟增益
 */
#define TCFG_AUDIO_DYNAMIC_ADC_GAIN			ENABLE_THIS_MOUDLE
#define ANC_TRAIN_MODE						ANC_FF_EN				//ANC类型配置：单前馈，单后馈，混合馈
#define ANC_CH 			 					ANC_L_CH //| ANC_R_CH	//ANC通道使能：左通道 | 右通道
#define ANCL_FF_MIC							A_MIC0					//ANC左通道FFMIC类型
#define ANCL_FB_MIC							A_MIC1					//ANC左通道FBMIC类型
#define ANCR_FF_MIC							MIC_NULL				//ANC右通道FFMIC类型
#define ANCR_FB_MIC							MIC_NULL                //ANC右通道FBMIC类型
/*ANC最大滤波器阶数限制，若设置超过10，ANC配置区则占用8K(range: 10-20)*/
#define ANC_MAX_ORDER						10

/*限幅器-噪声门限*/
#define TCFG_AUDIO_NOISE_GATE				DISABLE_THIS_MOUDLE

/*通话下行降噪使能*/
#define TCFG_ESCO_DL_NS_ENABLE				DISABLE_THIS_MOUDLE

// AUTOMUTE
#define AUDIO_OUTPUT_AUTOMUTE   DISABLE_THIS_MOUDLE

//*********************************************************************************//
//                   ADC TO DAC 低延时通道功能                                     //
//                                                                                 //
//*********************************************************************************//
#define TCFG_AD2DA_LOW_LATENCY_ENABLE           (0)
#define TCFG_AD2DA_LOW_LATENCY_SAMPLE_RATE      (44100)         // 目前仅支持固定 44100
#define TCFG_AD2DA_LOW_LATENCY_MIC_CHANNEL      (LADC_CH_MIC_L) // 目前仅支持单声道

/*
 *系统音量类型选择
 *软件数字音量是指纯软件对声音进行运算后得到的
 *硬件数字音量是指dac内部数字模块对声音进行运算后输出
 */
#define VOL_TYPE_DIGITAL		0	//软件数字音量(调节解码输出数据的音量)
#define VOL_TYPE_ANALOG			1	//(暂未支持)硬件模拟音量
#define VOL_TYPE_AD				2	//(暂未支持)联合音量(模拟数字混合调节)
#define VOL_TYPE_DIGITAL_HW		3  	//硬件数字音量(调节DAC模块的硬件音量)
/*注意:ANC使能情况下使用软件数字音量*/
#if (TCFG_AUDIO_ANC_ENABLE || TCFG_AD2DA_LOW_LATENCY_ENABLE)
#define SYS_VOL_TYPE            VOL_TYPE_DIGITAL
#else
#define SYS_VOL_TYPE            VOL_TYPE_DIGITAL_HW
#endif/*TCFG_AUDIO_ANC_ENABLE*/
/*
 *通话的时候使用数字音量
 *0：通话使用和SYS_VOL_TYPE一样的音量调节类型
 *1：通话使用数字音量调节，更加平滑
 */
#define TCFG_CALL_USE_DIGITAL_VOLUME		0

//第三方清晰语音开发使能
#define TCFG_CVP_DEVELOP_ENABLE             DISABLE_THIS_MOUDLE

/*通话降噪模式配置*/
#define CVP_ANS_MODE	0	/*传统降噪*/
#define CVP_DNS_MODE	1	/*神经网络降噪*/
#define TCFG_AUDIO_CVP_NS_MODE				CVP_ANS_MODE

/*
 * ENC(双mic降噪)配置
 * 双mic降噪包括DMS_NORMAL和DMS_FLEXIBLE，在使能TCFG_AUDIO_DUAL_MIC_ENABLE
 * 的前提下，根据具体需求，选择对应的DMS模式
 */
/*ENC(双mic降噪)使能*/
#define TCFG_AUDIO_DUAL_MIC_ENABLE			DISABLE_THIS_MOUDLE

/*DMS模式选择*/
#define DMS_NORMAL		1	//普通双mic降噪(mic距离固定)
#define DMS_FLEXIBLE	2	//适配mic距离不固定且距离比较远的情况，比如头戴式话务耳机
#define TCFG_AUDIO_DMS_SEL					DMS_NORMAL

/*ENC双mic配置主mic副mic对应的mic port*/
#define DMS_MASTER_MIC0		0 //mic0是主mic
#define DMS_MASTER_MIC1		1 //mic1是主mic
#define TCFG_AUDIO_DMS_MIC_MANAGE			DMS_MASTER_MIC0
/*双mic降噪/单麦mic降噪 DUT测试模式，配合设备测试mic频响和(双mic)降噪量*/
#define TCFG_AUDIO_DMS_DUT_ENABLE			DISABLE_THIS_MOUDLE

//MIC通道配置
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#define TCFG_AUDIO_ADC_MIC_CHA				(LADC_CH_MIC_L | LADC_CH_MIC_R)
#else
#define TCFG_AUDIO_ADC_MIC_CHA				LADC_CH_MIC_L
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/

/*MIC模式配置:单端隔直电容模式/差分隔直电容模式/单端省电容模式*/
#if TCFG_AUDIO_ANC_ENABLE
/*注意:ANC使能情况下，使用差分mic*/
#define TCFG_AUDIO_MIC_MODE					AUDIO_MIC_CAP_DIFF_MODE
#define TCFG_AUDIO_MIC1_MODE				AUDIO_MIC_CAP_DIFF_MODE
#else
#define TCFG_AUDIO_MIC_MODE					AUDIO_MIC_CAP_MODE
#define TCFG_AUDIO_MIC1_MODE				AUDIO_MIC_CAP_MODE
#endif/*TCFG_AUDIO_ANC_ENABLE*/

/*
 *>>MIC电源管理:根据具体方案，选择对应的mic供电方式
 *(1)如果是多种方式混合，则将对应的供电方式或起来即可，比如(MIC_PWR_FROM_GPIO | MIC_PWR_FROM_MIC_BIAS)
 *(2)如果使用固定电源供电(比如dacvdd)，则配置成DISABLE_THIS_MOUDLE
 */
#define MIC_PWR_FROM_GPIO		(1UL << 0)	//使用普通IO输出供电
#define MIC_PWR_FROM_MIC_BIAS	(1UL << 1)	//使用内部mic_ldo供电(有上拉电阻可配)
#define MIC_PWR_FROM_MIC_LDO	(1UL << 2)	//使用内部mic_ldo供电
//配置MIC电源
#define TCFG_AUDIO_MIC_PWR_CTL				MIC_PWR_FROM_MIC_BIAS

//使用普通IO输出供电:不用的port配置成NO_CONFIG_PORT
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_GPIO)
#define TCFG_AUDIO_MIC_PWR_PORT				IO_PORTC_02
#define TCFG_AUDIO_MIC1_PWR_PORT			NO_CONFIG_PORT
#define TCFG_AUDIO_MIC2_PWR_PORT			NO_CONFIG_PORT
#endif/*MIC_PWR_FROM_GPIO*/

//使用内部mic_ldo供电(有上拉电阻可配)
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_MIC_BIAS)
#define TCFG_AUDIO_MIC0_BIAS_EN				ENABLE_THIS_MOUDLE/*Port:PA2*/
#define TCFG_AUDIO_MIC1_BIAS_EN				ENABLE_THIS_MOUDLE/*Port:PB7*/
#endif/*MIC_PWR_FROM_MIC_BIAS*/

//使用内部mic_ldo供电(Port:PA0)
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_MIC_LDO)
#define TCFG_AUDIO_MIC_LDO_EN				ENABLE_THIS_MOUDLE
#endif/*MIC_PWR_FROM_MIC_LDO*/
/*>>MIC电源管理配置结束*/

/*Audio数据导出配置:通过蓝牙spp导出或者sd写卡导出*/
#define AUDIO_DATA_EXPORT_USE_SD	1
#define AUDIO_DATA_EXPORT_USE_SPP 	2
#define TCFG_AUDIO_DATA_EXPORT_ENABLE		DISABLE_THIS_MOUDLE

/*通话清洗语音处理数据导出*/
//#define AUDIO_PCM_DEBUG

/*通话参数在线调试*/
#define TCFG_AEC_TOOL_ONLINE_ENABLE         DISABLE_THIS_MOUDLE

/*麦克风在线调试*/
#define TCFG_AUDIO_MIC_DUT_ENABLE         	DISABLE_THIS_MOUDLE

//*********************************************************************************//
//                                 (Yes/No)语音识别使能                            //
//*********************************************************************************//
#define TCFG_KWS_VOICE_RECOGNITION_ENABLE   DISABLE_THIS_MOUDLE

//*********************************************************************************//
//                                 Audio Smart Voice                               //
//*********************************************************************************//
#define TCFG_SMART_VOICE_ENABLE 		    DISABLE_THIS_MOUDLE     //ENABLE_THIS_MOUDLE
#define TCFG_KWS_VOICE_EVENT_HANDLE_ENABLE 	TCFG_SMART_VOICE_ENABLE //语音事件处理流程开关
#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
#define TCFG_CALL_KWS_SWITCH_ENABLE         DISABLE_THIS_MOUDLE
#else
#define TCFG_CALL_KWS_SWITCH_ENABLE         TCFG_SMART_VOICE_ENABLE
#endif

/*识别语言选择*/
#define KWS_CH          1   /*中文*/
#define KWS_INDIA_EN    2   /*印度英语*/
#define TCFG_AUDIO_KWS_LANGUAGE_SEL    KWS_CH

//*********************************************************************************//
//                                  编解码配置(codec)                              //
//*********************************************************************************//
/*解码格式使能配置*/
#define TCFG_DEC_G729_ENABLE                DISABLE	/*format:*.wtg*/
#define TCFG_DEC_MTY_ENABLE					DISABLE
#define TCFG_DEC_WTGV2_ENABLE				ENABLE	/*format:*.wts*/
#define TCFG_DEC_WAV_ENABLE					DISABLE
#define TCFG_DEC_MP3_ENABLE                 DISABLE
#define TCFG_DEC_AAC_ENABLE                 DISABLE

/*提示音叠加配置*/
#define TCFG_WAV_TONE_MIX_ENABLE			DISABLE
#define TCFG_MP3_TONE_MIX_ENABLE			DISABLE
#define TCFG_WTG_TONE_MIX_ENABLE			DISABLE
#define TCFG_WTS_TONE_MIX_ENABLE			ENABLE
#define TCFG_AAC_TONE_MIX_ENABLE			DISABLE //叠加AAC解码需要使用malloc

#if TCFG_AAC_TONE_MIX_ENABLE
#undef TCFG_AUDIO_AAC_CODE_AT_RAM
#define TCFG_AUDIO_AAC_CODE_AT_RAM			DISABLE_THIS_MOUDLE
#endif/* TCFG_AAC_TONE_MIX_ENABLE */

//*********************************************************************************//
//                                  IIS 配置                                     //
//*********************************************************************************//
#define TCFG_IIS_ENABLE                     DISABLE_THIS_MOUDLE

#define TCFG_IIS_MODE                       (0)     // 0:master  1:slave

#define TCFG_AUDIO_INPUT_IIS                (ENABLE && TCFG_IIS_ENABLE)
#define TCFG_IIS_INPUT_DATAPORT_SEL         ALINK_CH1

#define TCFG_AUDIO_OUTPUT_IIS               (DISABLE && TCFG_IIS_ENABLE)
#define TCFG_IIS_OUTPUT_DATAPORT_SEL        ALINK_CH0

#define TCFG_IIS_SAMPLE_RATE                (16000L)

//*********************************************************************************//
//                                  充电仓配置                                     //
//*********************************************************************************//
#define TCFG_CHARGESTORE_ENABLE				DISABLE_THIS_MOUDLE       //是否支持智能充点仓
#define TCFG_TEST_BOX_ENABLE			    1
#define TCFG_CHARGE_CALIBRATION_ENABLE      DISABLE_THIS_MOUDLE       //是否支持充电电流校准功能
#if TCFG_AUDIO_ANC_ENABLE
#define TCFG_ANC_BOX_ENABLE			        1                         //是否支持ANC测试盒
#else
#define TCFG_ANC_BOX_ENABLE			        0                         //是否支持ANC测试盒
#endif/*TCFG_AUDIO_ANC_ENABLE*/
#define TCFG_CHARGESTORE_PORT				IO_PORTP_00               //耳机和充点仓通讯的IO口
#define TCFG_CHARGESTORE_UART_ID			IRQ_UART1_IDX             //通讯使用的串口号

//*********************************************************************************//
//                                  充电参数配置                                   //
//*********************************************************************************//
//是否支持芯片内置充电
#define TCFG_CHARGE_ENABLE					ENABLE_THIS_MOUDLE
//是否支持开机充电
#define TCFG_CHARGE_POWERON_ENABLE			DISABLE
//是否支持拔出充电自动开机功能
#define TCFG_CHARGE_OFF_POWERON_NE			ENABLE
/*充电截止电压可选配置*/
#define TCFG_CHARGE_FULL_V					CHARGE_FULL_V_4199
/*充电截止电流可选配置*/
#define TCFG_CHARGE_FULL_MA					CHARGE_FULL_mA_10
/*充电电流可选配置*/
#define TCFG_CHARGE_MA						CHARGE_mA_50
#if (TCFG_CHARGE_ENABLE == DISABLE_THIS_MOUDLE) || (TCFG_CHARGE_POWERON_ENABLE == ENABLE)
#undef TCFG_CHARGE_CALIBRATION_ENABLE
#define TCFG_CHARGE_CALIBRATION_ENABLE      DISABLE_THIS_MOUDLE
#endif

//*********************************************************************************//
//                                  LED 配置                                       //
//*********************************************************************************//
#define TCFG_PWMLED_ENABLE					ENABLE_THIS_MOUDLE			//是否支持PMW LED推灯模块
#define TCFG_PWMLED_IOMODE					LED_ONE_IO_MODE				//LED模式，单IO还是两个IO推灯
#define TCFG_PWMLED_PIN						IO_PORTC_04					//LED使用的IO口
//*********************************************************************************//
//                                  时钟配置                                       //
//*********************************************************************************//
#define TCFG_CLOCK_SYS_SRC					SYS_CLOCK_INPUT_PLL_BT_OSC   //系统时钟源选择
//#define TCFG_CLOCK_SYS_HZ					12000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					16000000                     //系统时钟设置
#define TCFG_CLOCK_SYS_HZ					24000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					32000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					48000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					54000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					64000000                     //系统时钟设置
#define TCFG_CLOCK_OSC_HZ					24000000                     //外界晶振频率设置
#define TCFG_CLOCK_MODE                     CLOCK_MODE_ADAPTIVE

//*********************************************************************************//
//                                  低功耗配置                                     //
//*********************************************************************************//
#define TCFG_LOWPOWER_POWER_SEL				PWR_DCDC15//PWR_LDO15                    //电源模式设置，可选DCDC和LDO
#define TCFG_LOWPOWER_BTOSC_DISABLE			0                            //低功耗模式下BTOSC是否保持
#define TCFG_LOWPOWER_LOWPOWER_SEL			1   //芯片是否进入powerdown
/*强VDDIO等级配置,可选：
    VDDIOM_VOL_20V    VDDIOM_VOL_22V    VDDIOM_VOL_24V    VDDIOM_VOL_26V
    VDDIOM_VOL_30V    VDDIOM_VOL_30V    VDDIOM_VOL_32V    VDDIOM_VOL_36V*/
#define TCFG_LOWPOWER_VDDIOM_LEVEL			VDDIOM_VOL_28V
/*弱VDDIO等级配置，可选：
    VDDIOW_VOL_21V    VDDIOW_VOL_24V    VDDIOW_VOL_28V    VDDIOW_VOL_32V*/
#define TCFG_LOWPOWER_VDDIOW_LEVEL			VDDIOW_VOL_26V               //弱VDDIO等级配置
#define TCFG_LOWPOWER_OSC_TYPE              OSC_TYPE_LRC
#define TCFG_LOWPOWER_LIGHT_SLEEP_ATTRIBUTE 	LOWPOWER_LIGHT_SLEEP_ATTRIBUTE_KEEP_CLOCK 		//低功耗LIGHT模式属性, 可以选择是否保持住一些电源和时钟

//*********************************************************************************//
//                                  EQ配置                                         //
//*********************************************************************************//
//EQ配置，使用在线EQ时，EQ文件和EQ模式无效。有EQ文件时，使能TCFG_USE_EQ_FILE,默认不用EQ模式切换功能
#define TCFG_EQ_ENABLE                            1     //支持EQ功能,EQ总使能
// #if TCFG_EQ_ENABLE
#define TCFG_EQ_ONLINE_ENABLE                     0     //支持在线EQ调试,如果使用蓝牙串口调试，需要打开宏 APP_ONLINE_DEBUG，否则，默认使用uart调试(二选一)
#define TCFG_BT_MUSIC_EQ_ENABLE                   1     //支持蓝牙音乐EQ
#define TCFG_PHONE_EQ_ENABLE                      0     //支持通话近端EQ
#define TCFG_AUDIO_OUT_EQ_ENABLE                  0     //mix out级，增加eq高低音接口
#define TCFG_AEC_DCCS_EQ_ENABLE           		  1     // AEC DCCS
#define TCFG_AEC_UL_EQ_ENABLE           		  1     // AEC UL
#define TCFG_MUSIC_MODE_EQ_ENABLE                 0     //支持音乐模式EQ
#define TCFG_PC_MODE_EQ_ENABLE                    0     //支持pc模式EQ

#define TCFG_USE_EQ_FILE                          1    //离线eq使用配置文件还是默认系数表 1：使用文件  0 使用默认系数表

#if !TCFG_USE_EQ_FILE
#define TCFG_USER_EQ_MODE_NUM                     7    //eq默认系数表的模式个数，默认是7个
#endif

#define EQ_SECTION_MAX                            10     //eq 段数，最大20
#define TCFG_CALL_DL_EQ_SECTION                   3 //通话下行EQ段数,小于等于EQ_SECTION_MAX ，最大10段
#define TCFG_CALL_UL_EQ_SECTION                   3  //通话上行EQ段数,小于等于EQ_SECTION_MAX ,最大10段

// #endif//TCFG_EQ_ENABLE


#define TCFG_DRC_ENABLE							  0	  /*DRC 总使能*/
// #if TCFG_DRC_ENABLE
#define TCFG_BT_MUSIC_DRC_ENABLE            	  0     //支持蓝牙音乐DRC
#define TCFG_MUSIC_MODE_DRC_ENABLE                0     //支持音乐模式DRC
#define TCFG_PC_MODE_DRC_ENABLE                   0     //支持PC模式DRC
#define TCFG_CVP_UL_DRC_ENABLE           		  0     //支持通话上行DRC
// #endif

/***********************************非用户配置区***********************************/
#if TCFG_EQ_ONLINE_ENABLE
#if (TCFG_USE_EQ_FILE == 0)
#undef TCFG_USE_EQ_FILE
#define TCFG_USE_EQ_FILE                    1    //开在线调试时，打开使用离线配置文件宏定义
#endif
#if TCFG_AUDIO_OUT_EQ_ENABLE
#undef TCFG_AUDIO_OUT_EQ_ENABLE
#define TCFG_AUDIO_OUT_EQ_ENABLE            0    //开在线调试时，关闭高低音
#endif
#endif
/*ANC模式下DRC使能配置*/
#if TCFG_AUDIO_ANC_ENABLE
#if (ANC_TRAIN_MODE != ANC_FB_EN)
#undef TCFG_DRC_ENABLE
#define TCFG_DRC_ENABLE						1	  /*DRC 总使能*/
#endif/*!ANC_FB_EN*/
#endif/*TCFG_AUDIO_ANC_ENABLE*/

//*********************************************************************************//
//                          新配置工具 && 调音工具                             //
//*********************************************************************************//
#define TCFG_CFG_TOOL_ENABLE				DISABLE		  	//是否支持在线配置工具
#define TCFG_EFFECT_TOOL_ENABLE				DISABLE		  	//是否支持在线音效调试,使能该项还需使能EQ总使能TCFG_EQ_ENABL,
#define TCFG_NULL_COMM						0				//不支持通信
#define TCFG_UART_COMM						1				//串口通信
#define TCFG_USB_COMM						2				//USB通信
#define TCFG_SPP_COMM						3				//蓝牙SPP通信
#if (TCFG_CFG_TOOL_ENABLE || TCFG_EFFECT_TOOL_ENABLE)
#define TCFG_COMM_TYPE						TCFG_SPP_COMM	//通信方式选择
#else
#define TCFG_COMM_TYPE						TCFG_NULL_COMM
#endif
#define TCFG_ONLINE_TX_PORT					IO_PORT_DP      //UART模式调试TX口选择
#define TCFG_ONLINE_RX_PORT					IO_PORT_DM      //UART模式调试RX口选择
#define TCFG_ONLINE_ENABLE                  (TCFG_EFFECT_TOOL_ENABLE)    //是否支持音效在线调试功能

/***********************************非用户配置区***********************************/
#include "usb_std_class_def.h"
#if (TCFG_CFG_TOOL_ENABLE || TCFG_EFFECT_TOOL_ENABLE)
#if (TCFG_COMM_TYPE == TCFG_USB_COMM)
#define TCFG_USB_CDC_BACKGROUND_RUN         ENABLE
#if (TCFG_USB_CDC_BACKGROUND_RUN && (!TCFG_PC_ENABLE))
#define TCFG_OTG_USB_DEV_EN                 0
#endif
#if TCFG_LOWPOWER_LOWPOWER_SEL
#error "Please disable TCFG_LOWPOWER_LOWPOWER_SEL when you choose TCFG_USB_COMM"
#endif
#endif
#if (TCFG_COMM_TYPE == TCFG_UART_COMM)
#define TCFG_USB_CDC_BACKGROUND_RUN         DISABLE
#endif
#endif
/**********************************************************************************/

//*********************************************************************************//
//                                  g-sensor配置                                   //
//*********************************************************************************//
#define TCFG_GSENSOR_ENABLE                       0     //gSensor使能
#define TCFG_DA230_EN                             0
#define TCFG_SC7A20_EN                            0
#define TCFG_STK8321_EN                           0
#define TCFG_GSENOR_USER_IIC_TYPE                 0     //0:软件IIC  1:硬件IIC

//*********************************************************************************//
//                                  系统配置                                         //
//*********************************************************************************//
#define TCFG_AUTO_SHUT_DOWN_TIME		          180   //没有蓝牙连接自动关机时间
#define TCFG_SYS_LVD_EN						      1   //电量检测使能
#define TCFG_POWER_ON_NEED_KEY				      1	  //是否需要按按键开机配置

//*********************************************************************************//
//                                  蓝牙配置                                       //
//*********************************************************************************//
#define TCFG_USER_TWS_ENABLE                      0   //tws功能使能
#define TCFG_USER_BLE_ENABLE                      0   //BLE功能使能
#define TCFG_BT_SUPPORT_AAC                       1   //AAC格式支持

#define USER_SUPPORT_PROFILE_SPP                  0
#define USER_SUPPORT_PROFILE_HFP                  1
#define USER_SUPPORT_PROFILE_A2DP                 1
#define USER_SUPPORT_PROFILE_AVCTP                1
#define USER_SUPPORT_PROFILE_HID                  1
#define USER_SUPPORT_PROFILE_PNP                  1
#define USER_SUPPORT_PROFILE_PBAP                 0

#if TCFG_USER_TWS_ENABLE
#define TCFG_BD_NUM						          1   //连接设备个数配置
#define TCFG_AUTO_STOP_PAGE_SCAN_TIME             0   //配置一拖二第一台连接后自动关闭PAGE SCAN的时间(单位分钟)
#else
#define TCFG_BD_NUM						          2   //连接设备个数配置
#define TCFG_LOWEST_POWER_EN					  0   //最低功耗配置使能
#define TCFG_AUTO_STOP_PAGE_SCAN_TIME             0 //配置一拖二第一台连接后自动关闭PAGE SCAN的时间(单位分钟)
#endif

#if TCFG_LOWEST_POWER_EN
#undef EQ_SECTION_MAX
#define EQ_SECTION_MAX                            5    //eq 段数，最大20
#endif

// #if (TCFG_BD_NUM == 2)
// #undef TCFG_BT_SUPPORT_AAC
// #define TCFG_BT_SUPPORT_AAC                       0   //1to2 AAC暂时不支持
// #endif
/*spp数据导出配置*/
#if ((TCFG_AUDIO_DATA_EXPORT_ENABLE == AUDIO_DATA_EXPORT_USE_SPP) || (TCFG_AUDIO_MIC_DUT_ENABLE == 1))
#undef TCFG_USER_TWS_ENABLE
#undef TCFG_USER_BLE_ENABLE
#undef TCFG_BD_NUM
#undef USER_SUPPORT_PROFILE_SPP
#undef USER_SUPPORT_PROFILE_A2DP
#define TCFG_USER_TWS_ENABLE        0//spp数据导出，关闭tws
#define TCFG_USER_BLE_ENABLE        0//spp数据导出，关闭ble
#define TCFG_BD_NUM					1//连接设备个数配置
#define USER_SUPPORT_PROFILE_SPP	1
#define USER_SUPPORT_PROFILE_A2DP   0
#define APP_ONLINE_DEBUG            1//通过spp导出数据
#else
#define APP_ONLINE_DEBUG            0//在线APP调试,发布默认不开
#endif/*TCFG_AUDIO_DATA_EXPORT_ENABLE*/

/*以下功能需要打开SPP和在线调试功能
 *1、回音消除参数在线调试
 *2、双mic降噪DUT模式
 *3、ANC工具蓝牙spp调试
 */
#if (TCFG_AEC_TOOL_ONLINE_ENABLE || TCFG_AUDIO_DMS_DUT_ENABLE || TCFG_ANC_TOOL_DEBUG_ONLINE)|| (TCFG_COMM_TYPE == TCFG_SPP_COMM)
#undef USER_SUPPORT_PROFILE_SPP
#undef APP_ONLINE_DEBUG
#define USER_SUPPORT_PROFILE_SPP	1
#define APP_ONLINE_DEBUG            1
#endif/*TCFG_AEC_TOOL_ONLINE_ENABLE*/

#if ((TCFG_CFG_TOOL_ENABLE || TCFG_EFFECT_TOOL_ENABLE) && (TCFG_COMM_TYPE == TCFG_SPP_COMM))
#if ((!USER_SUPPORT_PROFILE_SPP) || (!APP_ONLINE_DEBUG))
#error "Please enable USER_SUPPORT_PROFILE_SPP and APP_ONLINE_DEBUG ! !"
#endif
#endif

#define BT_INBAND_RINGTONE                        1   //是否播放手机自带来电铃声
#define BT_PHONE_NUMBER                           0   //是否播放来电报号
#define BT_SUPPORT_DISPLAY_BAT                    1   //是否使能电量检测
#define BT_SUPPORT_MUSIC_VOL_SYNC                 1   //是否使能音量同步


//*********************************************************************************//
//                                 环绕音效使能
//*********************************************************************************//
#define AUDIO_SURROUND_CONFIG     0//3d环绕

#define AUDIO_VBASS_CONFIG        0//虚拟低音,虚拟低音不支持四声道

#define AEC_PITCHSHIFTER_CONFIG   0   //通话上行变声,左耳触发变大叔声，右耳触发变女声



//*********************************************************************************//
//                                 电源切换配置                                    //
//*********************************************************************************//
#define CONFIG_PHONE_CALL_USE_LDO15	    0


//*********************************************************************************//
//                                 时钟切换配置                                    //
//*********************************************************************************//
#define CONFIG_BT_NORMAL_HZ	            (24 * 1000000L)
#define CONFIG_BT_CONNECT_HZ            (48 * 1000000L)

#if TCFG_BT_MUSIC_EQ_ENABLE
#define CONFIG_BT_A2DP_HZ	        	(96 * 1000000L)
#else
#define CONFIG_BT_A2DP_HZ	        	(48 * 1000000L)
#endif

#define CONFIG_MUSIC_DEC_CLOCK			(48 * 1000000L)
#define CONFIG_MUSIC_IDLE_CLOCK		    (48 * 1000000L)

/*以下情况需要提高通话系统时钟：
 *(1)使能通话下行降噪
 *(2)语音处理算法放在text段
 */
#if (TCFG_ESCO_DL_NS_ENABLE ||  \
	(defined TCFG_AUDIO_CVP_CODE_AT_RAM && (TCFG_AUDIO_CVP_CODE_AT_RAM == 0)))
#define CONFIG_BT_CALL_HZ		        (60 * 1000000L)
#define CONFIG_BT_CALL_ADVANCE_HZ       (80 * 1000000L)
#define CONFIG_BT_CALL_16k_HZ	        (80 * 1000000L)
#define CONFIG_BT_CALL_16k_ADVANCE_HZ   (96 * 1000000L)
#else
#define CONFIG_BT_CALL_HZ		        (48 * 1000000L)
#define CONFIG_BT_CALL_ADVANCE_HZ       (60 * 1000000L)
#define CONFIG_BT_CALL_16k_HZ	        (70 * 1000000L)
#define CONFIG_BT_CALL_16k_ADVANCE_HZ   (80 * 1000000L)
#endif/*TCFG_ESCO_DL_NS_ENABLE*/
#define CONFIG_BT_CALL_DNS_HZ           (96 * 1000000L)

#if (TCFG_EQ_ONLINE_ENABLE && (!TCFG_EFFECT_TOOL_ENABLE) || (!TCFG_EQ_ONLINE_ENABLE && (TCFG_EFFECT_TOOL_ENABLE)))
#error "音效在线调试需使能调音工具宏TCFG_EFFECT_TOOL_ENABLE"
#endif

//*********************************************************************************//
//                                 配置结束                                        //
//*********************************************************************************//


#endif //CONFIG_BOARD_AC7003F_DEMO
#endif //CONFIG_BOARD_AC7003F_DEMO_CFG_H
