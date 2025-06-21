#ifndef _APP_AUDIO_H_
#define _APP_AUDIO_H_

#include "generic/typedef.h"
#include "board_config.h"

#if BT_SUPPORT_MUSIC_VOL_SYNC
#define TCFG_MAX_VOL_PROMPT						 0
#else
#define TCFG_MAX_VOL_PROMPT						 1
#endif
/*
 *br36 支持24bit位宽输出
 * */
#define TCFG_AUDIO_DAC_24BIT_MODE           0

//*********************************************************************************//
//                               USB Audio配置                                     //
//*********************************************************************************//
/*usb mic的数据是否经过CVP,包括里面的回音消除(AEC)、非线性回音压制(NLP)、降噪(ANS)模块*/
#define TCFG_USB_MIC_CVP_ENABLE		                DISABLE_THIS_MOUDLE
#define TCFG_USB_MIC_CVP_MODE		                (ANS_EN | NLP_EN)

//*********************************************************************************//
//                                 CVP配置                                        //
//*********************************************************************************//
//麦克风阵列校准使能
#define TCFG_AUDIO_MIC_ARRAY_TRIM_ENABLE    0

/*
 *该配置适用于没有音量按键的产品，防止打开音量同步之后
 *连接支持音量同步的设备，将音量调小过后，连接不支持音
 *量同步的设备，音量没有恢复，导致音量小的问题
 *默认是没有音量同步的，将音量设置到最大值，可以在vol_sync.c
 *该宏里面修改相应的设置。
 */
#define TCFG_VOL_RESET_WHEN_NO_SUPPORT_VOL_SYNC	 0 //不支持音量同步的设备默认最大音量

/*省电容mic模块使能*/
#define TCFG_SUPPORT_MIC_CAPLESS		1

/*省电容mic校准方式选择*/
#define MC_BIAS_ADJUST_DISABLE			0	//省电容mic偏置校准关闭
#define MC_BIAS_ADJUST_ONE			 	1	//省电容mic偏置只校准一次（跟dac trim一样）
#define MC_BIAS_ADJUST_POWER_ON		 	2	//省电容mic偏置每次上电复位都校准(Power_On_Reset)
#define MC_BIAS_ADJUST_ALWAYS		 	3	//省电容mic偏置每次开机都校准(包括上电复位和其他复位)
/*
 *省电容mic偏置电压自动调整(因为校准需要时间，所以有不同的方式)
 *1、烧完程序（完全更新，包括配置区）开机校准一次
 *2、上电复位的时候都校准,即断电重新上电就会校准是否有偏差(默认)
 *3、每次开机都校准，不管有没有断过电，即校准流程每次都跑
 */
#define TCFG_MC_BIAS_AUTO_ADJUST	 	MC_BIAS_ADJUST_POWER_ON
#define TCFG_MC_CONVERGE_PRE			0  //省电容mic预收敛
#define TCFG_MC_CONVERGE_TRACE			0  //省电容mic收敛值跟踪
/*
 *省电容mic收敛步进限制
 *0:自适应步进调整, >0:收敛步进最大值
 *注：当mic的模拟增益或者数字增益很大的时候，mic_capless模式收敛过程,
 *变化的电压放大后，可能会听到哒哒声，这个时候就可以限制住这个收敛步进
 *让收敛平缓进行(前提是预收敛成功的情况下)
 */
#define TCFG_MC_DTB_STEP_LIMIT			3  //最大收敛步进值
/*
 *省电容mic使用固定收敛值
 *可以用来测试默认偏置是否合理：设置固定收敛值7000左右，让mic的偏置维持在1.5v左右即为合理
 *正常使用应该设置为0,让程序动态收敛
 */
#define TCFG_MC_DTB_FIXED				0

#define TCFG_ESCO_PLC					1  	//通话丢包修复
#define TCFG_AEC_ENABLE					1	//通话回音消除使能
#define TCFG_AUDIO_CVP_SYNC				0	//通话上行同步使能
#define TCFG_AUDIO_CONFIG_TRACE			0	//音频模块配置跟踪查看

#define MAX_ANA_VOL             (14)	// 系统最大模拟音量,范围: 0 ~ 15
#define MAX_COM_VOL             (16)    // 数值应该大于等于16，具体数值应小于联合音量等级的数组大小 (combined_vol_list)
#define MAX_DIG_VOL             (16)    // 数值应该大于等于16，因为手机是16级，如果小于16会导致某些情况手机改了音量等级但是小机音量没有变化

#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
#define SYS_MAX_VOL             16
#define SYS_DEFAULT_VOL         16
#define SYS_DEFAULT_TONE_VOL    10
#define SYS_DEFAULT_SIN_VOL    	8

#elif (SYS_VOL_TYPE == VOL_TYPE_DIGITAL_HW)
#define SYS_MAX_VOL             MAX_DIG_VOL
#define SYS_DEFAULT_VOL         SYS_MAX_VOL
#define SYS_DEFAULT_TONE_VOL    10
#define SYS_DEFAULT_SIN_VOL    	8

#elif (SYS_VOL_TYPE == VOL_TYPE_ANALOG)
#define SYS_MAX_VOL             MAX_ANA_VOL
#define SYS_DEFAULT_VOL         SYS_MAX_VOL
#define SYS_DEFAULT_TONE_VOL    10
#define SYS_DEFAULT_SIN_VOL    	8

#elif (SYS_VOL_TYPE == VOL_TYPE_AD)
#define SYS_MAX_VOL             MAX_COM_VOL
#define SYS_DEFAULT_VOL         SYS_MAX_VOL
#define SYS_DEFAULT_TONE_VOL    14
#define SYS_DEFAULT_SIN_VOL    	8
#else
#error "SYS_VOL_TYPE define error"
#endif

/*数字音量最大值定义*/
#define DEFAULT_DIGITAL_VOLUME   16384


#define BT_MUSIC_VOL_LEAVE_MAX	16		/*高级音频音量等级*/
#define BT_CALL_VOL_LEAVE_MAX	15		/*通话音量等级*/
#define BT_CALL_VOL_STEP		(-2.0f)	/*通话音量等级衰减步进*/

/*
 *audio state define
 */
#define APP_AUDIO_STATE_IDLE        0
#define APP_AUDIO_STATE_MUSIC       1
#define APP_AUDIO_STATE_CALL        2
#define APP_AUDIO_STATE_WTONE       3
#define APP_AUDIO_CURRENT_STATE     4


#if TCFG_MIC_EFFECT_ENABLE
#define TCFG_AUDIO_DAC_MIX_ENABLE           (1)
#define TCFG_AUDIO_DAC_MIX_SAMPLE_RATE      (TCFG_REVERB_SAMPLERATE_DEFUAL)
#elif (TCFG_AD2DA_LOW_LATENCY_ENABLE || TCFG_AUDIO_HEARING_AID_ENABLE)
#define TCFG_AUDIO_DAC_MIX_ENABLE           (1)
#define TCFG_AUDIO_DAC_MIX_SAMPLE_RATE      (TCFG_AD2DA_LOW_LATENCY_SAMPLE_RATE)
#elif TCFG_SIDETONE_ENABLE
#define TCFG_AUDIO_DAC_MIX_ENABLE           (1)
#define TCFG_AUDIO_DAC_MIX_SAMPLE_RATE      (44100)
#else
#define TCFG_AUDIO_DAC_MIX_ENABLE           (0)
#define TCFG_AUDIO_DAC_MIX_SAMPLE_RATE      (44100)
#endif

#if TCFG_AUDIO_OUTPUT_IIS && (SYS_VOL_TYPE != VOL_TYPE_DIGITAL)
#error "iis输出需要使用数字音量"
#endif

u8 get_max_sys_vol(void);
u8 get_tone_vol(void);

/*
*********************************************************************
*                  Audio Volume Get
* Description: 音量获取
* Arguments  : state	要获取音量值的音量状态
* Return	 : 返回指定状态对应得音量值
* Note(s)    : None.
*********************************************************************
*/
s8 app_audio_get_volume(u8 state);

/*
*********************************************************************
*          			Audio Volume Set
* Description: 音量设置
* Arguments  : state	目标声音通道
*			   volume	目标音量值
*			   fade		是否淡入淡出
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void app_audio_set_volume(u8 state, s8 volume, u8 fade);

/*
*********************************************************************
*                  Audio Volume Up
* Description: 增加当前音量通道的音量
* Arguments  : value	要增加的音量值
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void app_audio_volume_up(u8 value);

/*
*********************************************************************
*                  Audio Volume Down
* Description: 减少当前音量通道的音量
* Arguments  : value	要减少的音量值
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void app_audio_volume_down(u8 value);

/*
*********************************************************************
*                  Audio State Switch
* Description: 切换声音状态
* Arguments  : state
*			   max_volume
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void app_audio_state_switch(u8 state, s16 max_volume);

/*
*********************************************************************
*                  Audio State Exit
* Description: 退出当前的声音状态
* Arguments  : state	要退出的声音状态
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void app_audio_state_exit(u8 state);

/*
*********************************************************************
*                  Audio State Get
* Description: 获取当前声音状态
* Arguments  : None.
* Return	 : 返回当前的声音状态
* Note(s)    : None.
*********************************************************************
*/
u8 app_audio_get_state(void);

void app_audio_mute(u8 value);

/*
*********************************************************************
*                  Audio Volume_Max Get
* Description: 获取当前声音通道的最大音量
* Arguments  : None.
* Return	 : 返回当前的声音通道最大音量
* Note(s)    : None.
*********************************************************************
*/
s16 app_audio_get_max_volume(void);

/*
*********************************************************************
*                  Audio Set Max Volume
* Description: 设置最大音量
* Arguments  : state		要设置最大音量的声音状态
*			   max_volume	最大音量
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void app_audio_set_max_volume(u8 state, s16 max_volume);

void volume_up_down_direct(s8 value);
void audio_combined_vol_init(u8 cfg_en);

void dac_power_on(void);
void dac_power_off(void);

void mc_trim_init(int update);
void mic_trim_run(void);
int audio_dac_trim_value_check(struct audio_dac_trim *dac_trim);
/*打印audio模块的数字模拟增益：DAC/ADC*/
void audio_gain_dump();
void audio_adda_dump(void); //打印所有的dac,adc寄存器

/*
*********************************************************************
*          audio_dac_volume_enhancement_mode_set
* Description: DAC 音量增强模式切换
* Arguments  : mode		1：音量增强模式  0：普通模式
* Return	 : 0：成功  -1：失败
* Note(s)    : None.
*********************************************************************
*/
int app_audio_dac_vol_mode_set(u8 mode);

/*
*********************************************************************
*           app_audio_dac_vol_mode_get
* Description: DAC 音量增强模式状态获取
* Arguments  : None.
* Return	 : 1：音量增强模式  0：普通模式
* Note(s)    : None.
*********************************************************************
*/
u8 app_audio_dac_vol_mode_get(void);

#endif/*_APP_AUDIO_H_*/
