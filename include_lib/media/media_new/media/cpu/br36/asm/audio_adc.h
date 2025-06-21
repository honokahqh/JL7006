#ifndef AUDIO_ADC_H
#define AUDIO_ADC_H

#include "generic/typedef.h"
#include "generic/list.h"
#include "generic/atomic.h"
#include "media/audio_def.h"

/*无电容电路*/
#define SUPPORT_MIC_CAPLESS     1

#define LADC_STATE_INIT			1
#define LADC_STATE_OPEN      	2
#define LADC_STATE_START     	3
#define LADC_STATE_STOP      	4

#define FPGA_BOARD          	0

#define LADC_MIC                0
#define LADC_LINEIN0            1
#define LADC_LINEIN1            2
#define LADC_LINEIN             3

/* 通道选择 */
#define AUDIO_ADC_MIC_L		    BIT(0)
#define AUDIO_ADC_MIC_R		    BIT(1)
#define AUDIO_ADC_LINEIN_L		BIT(0)
#define AUDIO_ADC_LINEIN_R		BIT(1)
#define AUDIO_ADC_MIC_CH		BIT(0)

#define LADC_CH_MIC_L		    BIT(0)	//ADC MIC
#define LADC_CH_MIC_R		    BIT(1)	//ADC MIC
#define LADC_CH_LINEIN_L		BIT(0)
#define LADC_CH_LINEIN_R		BIT(1)
#define LADC_CH_PLNK			BIT(4)	//PLNK->ADC
#define LADC_CH_LINE1_R			BIT(5)
#define PLNK_MIC				BIT(6)	//PDM MIC
#define ALNK_MIC				BIT(7)	//IIS MIC

#define LADC_MIC_MASK			(BIT(0) | BIT(1))
#define LADC_LINEIN_MASK		(BIT(0) | BIT(1))

/*Audio ADC输入源选择*/
#define AUDIO_ADC_SEL_AMIC0			0
#define AUDIO_ADC_SEL_AMIC1			1
#define AUDIO_ADC_SEL_DMIC0			2
#define AUDIO_ADC_SEL_DMIC1			3

struct adc_platform_data {
    /*MIC上拉电阻配置，影响MIC的偏置电压
        19:1.2K 	18:1.4K 	17:1.8K 	16:2.2K 	15:2.7K		14:3.2K
    	13:3.6K 	12:3.9K 	11:4.3K 	10:4.8K  	9:5.3K 	 	8:6.0K
    	7:6.6K		6:7.2K		5:7.7K		4:8.2K		3:8.9K		2:9.4K		1:10K	*/
    u8 mic_bias_res;
    u8 mic1_bias_res;
    u8 mic_ldo_state;			//记录当前micldo使能情况

    u32 mic_mode : 3;  			//MIC0工作模式
    u32 mic_ldo_isel: 2; 		//MIC0通道电流档位选择
    u32 mic_ldo_vsel : 3;		//MIC0_LDO[000:1.5v 001:1.8v 010:2.1v 011:2.4v 100:2.7v 101:3.0v]
    u32 mic_bias_inside : 1;	//MIC0电容隔直模式使用内部mic偏置(PC7)
    u32 mic_bias_keep : 1;		//保持内部MIC0偏置输出
    u32 mic_in_sel : 1;			//MICIN选择[0:MIC0 1:MICEXT(ldo5v)]

    u32 mic1_mode : 3;			//MIC1工作模式
    u32 mic1_ldo_isel: 2; 		//MIC0通道电流档位选择
    u32 mic1_ldo_vsel : 3;		//MIC1_LDO 00:2.3v 01:2.5v 10:2.7v 11:3.0v
    u32 mic1_bias_inside : 1;	//MIC1电容隔直模式使用内部mic偏置(PC7)
    u32 mic1_bias_keep : 1;		//保持内部MIC1偏置输出
    u32 mic1_in_sel : 1;		//MICIN1选择[0:MIC1 1:MICEXT(ldo5v)]

    u32 mic_ldo_pwr : 1;		//MICLDO供电到PAD(PA0)控制使能
    u32 adc0_sel : 2;
    u32 adc1_sel : 2;
    u32 adc2_sel : 2;
    u32 adc3_sel : 2;
    u32 reserved : 1;
    u8  adca_reserved0;
    u8  adcb_reserved0;
};

struct capless_low_pass {
    u16 bud; //快调边界
    u16 count;
    u16 pass_num;
    u16 tbidx;
    u32 bud_factor;
};

struct audio_adc_output_hdl {
    struct list_head entry;
    void *priv;
    void (*handler)(void *, s16 *, int);
};

struct audio_adc_hdl {
    u8 state;
    u8 channel;
    struct list_head head;
    const struct adc_platform_data *pd;
    //atomic_t ref;
#if SUPPORT_MIC_CAPLESS
    struct capless_low_pass lp0;
    struct capless_low_pass lp1;
    struct capless_low_pass *lp;
#endif/*SUPPORT_MIC_CAPLESS*/
};

struct adc_mic_ch {
    struct audio_adc_hdl *adc;
    u8 gain;
    u8 gain1;
    u8 buf_num;
    u16 buf_size;
    s16 *bufs;
    u16 sample_rate;
    void (*handler)(struct adc_mic_ch *, s16 *, u16);
};

struct adc_linein_ch {
    struct audio_adc_hdl *adc;
    u8 gain;
    u8 gain1;
    u8 buf_num;
    u16 buf_size;
    s16 *bufs;
    u16 sample_rate;
    void (*handler)(struct adc_linein_ch *, s16 *, u16);
};

/*
*********************************************************************
*                  Audio ADC Initialize
* Description: 初始化Audio_ADC模块的相关数据结构
* Arguments  : adc	ADC模块操作句柄
*			   pd	ADC模块硬件相关配置参数
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_init(struct audio_adc_hdl *, const struct adc_platform_data *);

/*
*********************************************************************
*                  Audio ADC Output Callback
* Description: 注册adc采样输出回调函数
* Arguments  : adc		adc模块操作句柄
*			   output  	采样输出回调
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_add_output_handler(struct audio_adc_hdl *, struct audio_adc_output_hdl *);

/*
*********************************************************************
*                  Audio ADC Output Callback
* Description: 删除adc采样输出回调函数
* Arguments  : adc		adc模块操作句柄
*			   output  	采样输出回调
* Return	 : None.
* Note(s)    : 采样通道关闭的时候，对应的回调也要同步删除，防止内存释
*              放出现非法访问情况
*********************************************************************
*/
void audio_adc_del_output_handler(struct audio_adc_hdl *, struct audio_adc_output_hdl *);

/*
*********************************************************************
*                  Audio ADC IRQ Handler
* Description: Audio ADC中断回调函数
* Arguments  : adc  adc模块操作句柄
* Return	 : None.
* Note(s)    : 仅供Audio_ADC中断使用
*********************************************************************
*/
void audio_adc_irq_handler(struct audio_adc_hdl *adc);

/*
*********************************************************************
*                  Audio ADC Mic Open
* Description: 打开mic采样通道
* Arguments  : mic	mic操作句柄
*			   ch	mic通道索引
*			   adc  adc模块操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_mic_open(struct adc_mic_ch *mic, int ch, struct audio_adc_hdl *adc);
int audio_adc_mic1_open(struct adc_mic_ch *mic, int ch, struct audio_adc_hdl *adc);

/*
*********************************************************************
*                  Audio ADC Mic Sample Rate
* Description: 设置mic采样率
* Arguments  : mic			mic操作句柄
*			   sample_rate	采样率
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_mic_set_sample_rate(struct adc_mic_ch *mic, int sample_rate);

/*
*********************************************************************
*                  Audio ADC Mic Gain
* Description: 设置mic增益
* Arguments  : mic	mic操作句柄
*			   gain	mic增益
* Return	 : 0 成功	其他 失败
* Note(s)    : MIC增益范围：0(-8dB)~19(30dB),step:2dB,level(4)=0dB
*********************************************************************
*/
int audio_adc_mic_set_gain(struct adc_mic_ch *mic, int gain);
int audio_adc_mic1_set_gain(struct adc_mic_ch *mic, int gain);

/*
*********************************************************************
*                  Audio ADC Mic Buffer
* Description: 设置采样buf和采样长度
* Arguments  : mic		mic操作句柄
*			   bufs		采样buf地址
*			   buf_size	采样buf长度，即一次采样中断数据长度
*			   buf_num 	采样buf的数量
* Return	 : 0 成功	其他 失败
* Note(s)    : (1)需要的总buf大小 = buf_size * ch_num * buf_num
* 		       (2)buf_num = 2表示，第一次数据放在buf0，第二次数据放在
*			   buf1,第三次数据放在buf0，依此类推。如果buf_num = 0则表
*              示，每次数据都是放在buf0
*********************************************************************
*/
int audio_adc_mic_set_buffs(struct adc_mic_ch *mic, s16 *bufs, u16 buf_size, u8 buf_num);

/*
*********************************************************************
*                  Audio ADC Mic Start
* Description: 启动audio_adc采样
* Arguments  : mic	mic操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_mic_start(struct adc_mic_ch *mic);

/*
*********************************************************************
*                  Audio ADC Mic Close
* Description: 关闭mic采样
* Arguments  : mic	mic操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_mic_close(struct adc_mic_ch *mic);
int audio_adc_mic1_close(struct adc_mic_ch *mic);

/*
*********************************************************************
*                  Audio ADC is Active
* Description: 判断ADC是否活动
* Return	 : 0 空闲 1 繁忙
* Note(s)    : None.
*********************************************************************
*/
u8 audio_adc_is_active(void);
/*
*********************************************************************
*                  Audio ADC Mic Pre_Gain
* Description: 设置mic第一级/前级增益
* Arguments  : en 前级增益使能(0:6dB 1:0dB)
* Return	 : None.
* Note(s)    : 前级增益只有0dB和6dB两个档位，使能即为0dB，否则为6dB
*********************************************************************
*/
void audio_mic_0dB_en(bool en);
void audio_mic1_0dB_en(bool en);

/*
*********************************************************************
*                  AUDIO MIC_LDO Control
* Description: mic电源mic_ldo控制接口
* Arguments  : index	ldo索引(MIC_LDO/MIC_LDO_BIAS0/MIC_LDO_BIAS1)
* 			   en		使能控制
*			   pd		audio_adc模块配置
* Return	 : 0 成功 其他 失败
* Note(s)    : (1)MIC_LDO输出不经过上拉电阻分压
*				  MIC_LDO_BIAS输出经过上拉电阻分压
*			   (2)打开一个mic_ldo示例：
*				audio_mic_ldo_en(MIC_LDO,1,&adc_data);
*			   (2)打开多个mic_ldo示例：
*				audio_mic_ldo_en(MIC_LDO | MIC_LDO_BIAS,1,&adc_data);
*********************************************************************
*/
/*MIC LDO index输出定义*/
#define MIC_LDO					BIT(0)	//PA0输出原始MIC_LDO
#define MIC_LDO_BIAS0			BIT(1)	//PA2输出i经过内部上拉电阻分压的偏置
#define MIC_LDO_BIAS1			BIT(2)	//PB7输出i经过内部上拉电阻分压的偏置
int audio_mic_ldo_en(u8 index, u8 en, struct adc_platform_data *pd);

/*
*********************************************************************
*                  Audio MIC Mute
* Description: mic静音使能控制
* Arguments  : mute 静音使能
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_set_mic_mute(bool mute);
void audio_set_mic1_mute(bool mute);



///////////////////////////////////////// linein adc /////////////////////////////////////////////////

/*
*********************************************************************
*                  Audio ADC Linein Open
* Description: 打开linein采样通道
* Arguments  : lienin	linein采样通道句柄
*			   ch	    linein通道索引
*			            LADC_CH_LINEIN_L:只操作左声道
*			            LADC_CH_LINEIN_R:只操作右声道
*			            LADC_CH_LINEIN_L|LADC_CH_LINEIN_R:左右声道同时操作
*			   adc  adc模块操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_linein_open(struct adc_linein_ch *linein, int ch, struct audio_adc_hdl *adc);

/*
*********************************************************************
*                  Audio ADC Linein Gain
* Description: 设置linein增益
* Arguments  : linein	linein操作句柄
*			   ch	    linein通道索引
*			            LADC_CH_LINEIN_L:只操作左声道
*			            LADC_CH_LINEIN_R:只操作右声道
*			            LADC_CH_LINEIN_L|LADC_CH_LINEIN_R:左右声道同时操作
*			   gain	    linein增益
* Return	 : 0 成功	其他 失败
* Note(s)    : MIC增益范围：0(-8dB)~19(30dB),step:2dB level(4) = 0dB
*********************************************************************
*/
int audio_adc_linein_set_gain(struct adc_linein_ch *linein, int ch, int gain);

/*
*********************************************************************
*                  Audio ADC Linein Pre_Gain
* Description: 设置mic第一级/前级增益
* Arguments  : linein	linein操作句柄
*			   ch	    linein通道索引
*			            LADC_CH_LINEIN_L:只操作左声道
*			            LADC_CH_LINEIN_R:只操作右声道
*			            LADC_CH_LINEIN_L|LADC_CH_LINEIN_R:左右声道同时操作
*              en 前级增益使能(0:6dB 1:0dB)
* Return	 : None.
* Note(s)    : 前级增益只有0dB和6dB两个档位，使能即为0dB，否则为6dB
*********************************************************************
*/
void audio_adc_linein_0dB_en(struct adc_linein_ch *linein, int ch, bool en);

/*
*********************************************************************
*                  Audio MIC Mute
* Description: mic静音使能控制
* Arguments  : linein	linein操作句柄
*			   ch	    linein通道索引
*			            LADC_CH_LINEIN_L:只操作左声道
*			            LADC_CH_LINEIN_R:只操作右声道
*			            LADC_CH_LINEIN_L|LADC_CH_LINEIN_R:左右声道同时操作
*              mute 静音使能
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_lienin_set_mute(struct adc_linein_ch *linein, int ch, bool mute);

/*
*********************************************************************
*                  Audio ADC Linein Sample Rate
* Description: 设置linein采样率
* Arguments  : linein		linein操作句柄
*			   sample_rate	采样率
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_linein_set_sample_rate(struct adc_linein_ch *linein, int sample_rate);

/*
*********************************************************************
*                  Audio ADC Linein Buffer
* Description: 设置采样buf和采样长度
* Arguments  : linein	linein操作句柄
*			   bufs		采样buf地址
*			   buf_size	采样buf长度，即一次采样中断数据长度
*			   buf_num 	采样buf的数量
* Return	 : 0 成功	其他 失败
* Note(s)    : (1)需要的总buf大小 = buf_size * ch_num * buf_num
* 		       (2)buf_num = 2表示，第一次数据放在buf0，第二次数据放在
*			   buf1,第三次数据放在buf0，依此类推。如果buf_num = 0则表
*              示，每次数据都是放在buf0
*********************************************************************
*/
int audio_adc_linein_set_buffs(struct adc_linein_ch *linein, s16 *bufs, u16 buf_size, u8 buf_num);

/*
*********************************************************************
*                  Audio ADC Linein Output Callback
* Description: 注册adc采样输出回调函数
* Arguments  : linein	linein操作句柄
*			   output  	采样输出回调
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_linein_add_output_handler(struct adc_linein_ch *linein, struct audio_adc_output_hdl *output);

/*
*********************************************************************
*                  Audio ADC Linein Output Callback
* Description: 删除adc采样输出回调函数
* Arguments  : linein	linein操作句柄
*			   output  	采样输出回调
* Return	 : None.
* Note(s)    : 采样通道关闭的时候，对应的回调也要同步删除，防止内存释
*              放出现非法访问情况
*********************************************************************
*/
void audio_adc_linein_del_output_handler(struct adc_linein_ch *linein, struct audio_adc_output_hdl *output);

/*
*********************************************************************
*                  Audio ADC Linein Start
* Description: 启动audio_adc采样
* Arguments  : linein	linein操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_linein_start(struct adc_linein_ch *linein);

/*
*********************************************************************
*                  Audio ADC Linein Close
* Description: 关闭linein采样
* Arguments  : linein	linein操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_linein_close(struct adc_linein_ch *linein);


#endif/*AUDIO_ADC_H*/

