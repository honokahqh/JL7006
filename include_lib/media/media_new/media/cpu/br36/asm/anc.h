#ifndef _ANC_H_
#define _ANC_H_

#include "generic/typedef.h"
#include "system/task.h"
#include "media/audio_def.h"

enum {
    ANCDB_ERR_CRC = 1,
    ANCDB_ERR_TAG,
    ANCDB_ERR_PARAM,
    ANCDB_ERR_STATE,
};

typedef enum {
    ANC_OFF = 1,		/*关闭模式*/
    ANC_ON,				/*降噪模式*/
    ANC_TRANSPARENCY,	/*通透模式*/
    ANC_BYPASS,			/*训练模式*/
    ANC_TRAIN,			/*训练模式*/
    ANC_TRANS_TRAIN,	/*通透训练模式*/
} ANC_mode_t;

/*ANC状态*/
enum {
    ANC_STA_CLOSE = 0,
    ANC_STA_INIT,
    ANC_STA_OPEN,
};

typedef enum {
    TRANS_ERR_TIMEOUT = 1,	//训练超时
    TRANS_ERR_MICERR,		//训练中MIC异常
    TRANS_ERR_FORCE_EXIT	//强制退出训练
} ANC_trans_errmsg_t;


typedef enum {
    ANC_L_FF_IIR  = 0x0,		//左FF滤波器 ID
    ANC_L_FB_IIR  = 0x1,		//左FB滤波器 ID
    ANC_L_CMP_IIR = 0x2,		//左音乐补偿滤波器 ID
    ANC_L_TRANS_IIR = 0x3,		//左通透滤波器 ID
    ANC_L_FIR = 0x4,			//(保留位，暂时不用)
    ANC_L_TRANS2_IIR = 0x5,		//左通透滤波器2 ID

    ANC_R_FF_IIR  = 0x10,		//右FF滤波器 ID
    ANC_R_FB_IIR  = 0x11,       //右FB滤波器 ID
    ANC_R_CMP_IIR = 0x12,       //右音乐补偿滤波器 ID
    ANC_R_TRANS_IIR = 0x13,     //右通透滤波器 ID
    ANC_R_FIR = 0x14,            //(保留位，暂时不用)
    ANC_R_TRANS2_IIR = 0x15,	//右通透滤波器2 ID
} ANC_config_seg_id_t;

typedef enum {
    ANC_ALOGM1  = 0,
    ANC_ALOGM2,
    ANC_ALOGM3,
    ANC_ALOGM4,
    ANC_ALOGM5,
    ANC_ALOGM6,
    ANC_ALOGM7,
    ANC_ALOGM8,
} ANC_alogm_t;		//ANC算法模式

typedef enum {
    A_MIC0 = 0x0,			//模拟MIC0 PA1 PA2
    A_MIC1,                 //模拟MIC1 PB7 PB8
    D_MIC0,                 //数字MIC0(plnk_dat0_pin-上升沿采样)
    D_MIC1,                 //数字MIC1(plnk_dat1_pin-上升沿采样)
    D_MIC2,                 //数字MIC2(plnk_dat0_pin-下降沿采样)
    D_MIC3,                 //数字MIC3(plnk_dat1_pin-下降沿采样)
    MIC_NULL = 0XFF,		//没有定义相关的MIC
} ANC_mic_type_t;

//ANC Ch ---eg:双声道 ANC_L_CH|ANC_R_CH
#define ANC_L_CH 					BIT(0)	//通道左
#define ANC_R_CH 	 				BIT(1)	//通道右

//ANC各类增益对应符号位
//gain_sign 对应符号
#define ANCL_FF_SIGN				BIT(0)	//左声道FF增益符号
#define ANCL_FB_SIGN				BIT(1)	//左声道FB增益符号
#define ANCL_TRANS_SIGN				BIT(2)  //左声道通透增益符号
#define ANCL_CMP_SIGN				BIT(3)  //左声道音乐补偿增益符号
#define ANCR_FF_SIGN				BIT(4)	//右声道FF增益符号
#define ANCR_FB_SIGN				BIT(5)	//右声道FB增益符号
#define ANCR_TRANS_SIGN				BIT(6)  //右声道通透增益符号
#define ANCR_CMP_SIGN				BIT(7)  //右声道音乐补偿增益符号

//gain_sign2 对应符号
#define ANCL_TRANS2_SIGN			BIT(0)	//左声道通透2增益符号
#define ANCR_TRANS2_SIGN			BIT(1)	//右声道通透2增益符号

//DRC中断标志
#define ANCL_REF_DRC_ISR			BIT(0)
#define ANCL_ERR_DRC_ISR			BIT(1)
#define ANCR_REF_DRC_ISR			BIT(2)
#define ANCR_ERR_DRC_ISR			BIT(3)
#define ANC_ERR_DRC_ISR				ANCL_ERR_DRC_ISR |ANCR_ERR_DRC_ISR
#define ANC_REF_DRC_ISR				ANCL_REF_DRC_ISR |ANCR_REF_DRC_ISR

//DRC模块使能标志
#define ANC_DRC_FF_EN				BIT(0)	//DRC_FF使能标志
#define ANC_DRC_FB_EN				BIT(1)	//DRC_FB使能标志
#define ANC_DRC_TRANS_EN			BIT(2)	//DRC_TRANS使能标志

/*ANC训练方式*/
#define ANC_AUTO_UART  	   			0
#define ANC_AUTO_BT_SPP    			1
#define ANC_TRAIN_WAY 				ANC_AUTO_BT_SPP

/*SZ频响获取默认算法模式*/
#define ANC_SZ_EXPORT_DEF_ALOGM		ANC_ALOGM5

/*通透模式选择*/
#define ANC_TRANS_MODE_NORMAL				0		//通透普通模式
#define ANC_TRANS_MODE_VOICE_ENHANCE		1		//通透人声增强模式

/*ANC芯片版本定义(只读)*/
#define ANC_VERSION_BR30 			0x01	//AD697N/AC897N
#define ANC_VERSION_BR30C			0x02	//AC699N/AD698N
#define ANC_VERSION_BR36			0x03	//AC700N
#define ANC_CHIP_VERSION			ANC_VERSION_BR36

typedef struct {
    u8  mode;			//当前训练步骤
    u8  train_busy;		//训练繁忙位 0-空闲 1-训练中 2-训练结束测试中
    u8	train_step;		//训练系数
    u8  ret_step;		//自适应训练系数值
    u8  auto_ts_en;		//自动步进微调使能位
    u8  enablebit; 			//训练模式使能标志位 前馈|混合馈|通透
    u8 tool_time_flag;		//工具修改时间标志
    u8	mic_dma_export_en;	//从ANC DAM拿数，会消耗4*4K
    u8 	only_mute_train_en;	//只跑静音训练保持sz,fz系数
    u16 timerout_id;		//训练超时定时器ID
    s16 fb0_gain;
    s16 fb0sz_dly_en;
    u16 fb0sz_dly_num;
    int sz_gain;			//静音训练sz 增益
    u32 sz_lower_thr;		//误差MIC下限能量阈值
    u32 fz_lower_thr;   	//参考MIC下限能量阈值
    u32 sz_noise_thr;		//误差MIC底噪阈值
    u32 fz_noise_thr;   	//参考MIC底噪阈值
    u32 sz_adaptive_thr;	//误差MIC自适应收敛阈值
    u32 fz_adaptive_thr;	//参考MIC自适应收敛阈值
    u32 wz_train_thr;		//噪声训练阈值
} anc_train_para_t;

typedef struct {
    u8 mic_errmsg;
    u8 status;
    u32 dat[4];  //ff_num/ff_dat/fb_num/fb_dat
    u32 pow;
    u32 temp_pow;
    u32 mute_pow;
} anc_ack_msg_t;

#define ANC_GAINS_VERSION 		0X7004	//结构体版本号信息
typedef struct {
//cfg
    u16 version;		//当前结构体版本号
    u8 dac_gain;		//dac模拟增益 			range 0-15;  default 13
    u8 ref_mic_gain;	//参考mic	  			range 0-19;  default 10
    u8 err_mic_gain;	//误差mic	  			range 0-19;  default 10
    u8 cmp_en;			//音乐补偿使能			range 0-1;   default 1
    u8 drc_en;		    //DRC使能				range 0-7;   default 0
    u8 ahs_en;		    //AHS使能				range 0-1;   default 1
    u8 dcc_sel;			//DCC选择 				range 0-8;   default 4
    u8 gain_sign; 		//ANC各类增益的符号     range 0-255; default 0
    u8 noise_lvl;		//训练的噪声等级		range 0-255; default 0
    u8 fade_step;		//淡入淡出步进			range 0-15;  default 1
    u32 alogm;			//ANC算法因素
    u32 trans_alogm;    //通透算法因素
    float l_ffgain;	    //ANCL FF增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float l_fbgain;	    //ANCL FB增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float l_transgain;  //ANCL 通透增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float l_cmpgain;	//ANCL 音乐补偿增益		range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_ffgain;	    //ANCR FF增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_fbgain;	    //ANCR FB增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_transgain;  //ANCR 通透增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_cmpgain;	//ANCR 音乐补偿增益		range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)

    u8 drcff_zero_det;	  //DRCFF过零检测使能	range 0-1; 	   default 0;
    u8 drcff_dat_mode;	  //DRCFF_DAT模式		range 0-3; 	   default 0;
    u8 drcff_lpf_sel;	  //DRCFF_LPF档位		range 0-3; 	   default 0;
    u8 drcfb_zero_det;	  //DRCFB过零检测使		range 0-1; 	   default 0;
    u8 drcfb_dat_mode;    //DRCFB_DAT模式		range 0-3;	   default 0;
    u8 drcfb_lpf_sel;     //DRCFB_LPF档位		range 0-3;	   default 0;

    u16 drcff_lthr;		  //DRCFF_LOW阈值		range 0-32767; default 0;
    u16 drcff_hthr;       //DRCFF_HIGH阈值		range 0-32767; default 32767;
    s16 drcff_lgain;      //DRCFF_LOW增益		range 0-32767; default 0;
    s16 drcff_hgain;	  //DRCFF_HIGH增益		range 0-32767; default 1024;
    s16 drcff_norgain;    //DRCFF_NOR增益		range 0-32767; default 1024;

    u16 drcfb_lthr;       //DRCFB_LOW阈值		range 0-32767; default 0;
    u16 drcfb_hthr;       //DRCFB_HIGH阈值		range 0-32767; default 32767;
    s16 drcfb_lgain;	  //DRCFB_LOW增益		range 0-32767; default 0;
    s16 drcfb_hgain;  	  //DRCFB_HIGH增益		range 0-32767; default 1024;
    s16 drcfb_norgain;    //DRCFB_NOR增益		range 0-32767; default 1024;

    u16 drctrans_lthr;    //DRCTRANS_LOW阈值	range 0-32767; default 0;
    u16 drctrans_hthr;    //DRCTRANS_HIGH阈值	range 0-32767; default 32767;
    s16 drctrans_lgain;   //DRCTRANS_LOW增益	range 0-32767; default 0;
    s16 drctrans_hgain;   //DRCTRANS_HIGH增益	range 0-32767; default 1024;
    s16 drctrans_norgain; //DRCTRANS_NOR增益	range 0-32767; default 1024;

    u8 ahs_dly;			  //AHS_DLY				range 0-15;	   default 1;
    u8 ahs_tap;			  //AHS_TAP				range 0-255;   default 100;
    u8 ahs_wn_shift;	  //AHS_WN_SHIFT		range 0-15;	   default 9;
    u8 ahs_wn_sub;	  	  //AHS_WN_SUB  		range 0-1;	   default 1;
    u16 ahs_shift;		  //AHS_SHIFT   		range 0-65536; default 210;
    u16 ahs_u;			  //AHS步进				range 0-65536; default 4000;
    s16 ahs_gain;		  //AHS增益				range -32767-32767;default -1024;
    u8 reserve;		      //保留位
    u8 developer_mode;	  //GAIN开发者模式		range 0-1;	   default 0;
    //version 0x7001
    float audio_drc_thr;  //Audio DRC阈值       range -6.0-0;  default -6.0dB;

    u8 sw_howl_en;		  //啸叫抑制使能
    u8 sw_howl_thr;  	  //啸叫检测阈值， 越小越灵敏 range 10-35; default 25;
    u8 sw_howl_dither_thr;//啸叫标记消抖阈值，越大越稳定 range 0-10; default 5;
    u16 sw_howl_gain;	  //啸叫之后的目标增益，建议写0	range 0-1024;

    u16 gain_sign2;	  	  //降噪衍生模式增益符号
    float l_transgain2;   //左耳通透模式2增益
    float r_transgain2;	  //右耳通透模式2增益

} anc_gain_param_t;

typedef struct {
    u8 start;                       //ANC状态
    u8 mode;                        //ANC模式：降噪关;降噪开;通透...
    u8 train_way;                   //训练方式
    u8 anc_fade_en;                 //ANC淡入淡出使能
    u8 tool_enablebit;				//ANC动态使能，使用过程中可随意控制
    u8 ch;							//ANC通道选择 ： ANC_L_CH | ANC_R_CH
    u8 ff_yorder;					//FF滤波器阶数
    u8 trans_yorder;				//通透滤波器阶数
    u8 fb_yorder;					//FB滤波器阶数
    u8 debug_sel;					//ANCdebug data输出源
    u8 lff_en;						//左耳FF使能
    u8 lfb_en;						//左耳FB使能
    u8 rff_en;						//右耳FF使能
    u8 rfb_en;						//右耳FB使能
    u8 online_busy;					//在线调试繁忙标志位
    u8 fade_time_lvl;				//淡入时间等级，越大时间越长
    u8 mic_type[4];					//ANC mic类型控制位
    u8 fade_lock;					//ANC淡入淡出增益锁
    u8 anc_mode_sel;				//降噪模式选择
    u8 trans_mode_sel;				//通透模式选择
    u16 anc_fade_gain;				//ANC淡入淡出增益
    u16 drc_fade_gain;				//DRC淡出增益
    u16 drc_time_id;				//DRCtimeout定时器ID
    u16	enablebit;					//ANC模式:FF,FB,HYBRID
    u32 coeff_size;					//ANC 读滤波器总长度
    u32 write_coeff_size;			//ANC 写滤波器总长度
    int train_err;					//训练结果 0:成功 other:失败
    u32 gains_size;					//ANC 滤波器长度

    int *debug_buf;					//测试数据流，最大65536byte(堆)
    s32 *lfir_coeff;				//FZ补偿滤波器表
    s32 *rfir_coeff;				//FZ补偿滤波器表
    double *lff_coeff;				//左耳FF滤波器
    double *lfb_coeff;				//左耳FB滤波器
    double *lcmp_coeff;				//左耳cmp滤波器
    double *ltrans_coeff;			//左耳通透滤波器
    double *rff_coeff;				//右耳FF滤波器
    double *rfb_coeff;  	    	//右耳FB滤波器
    double *rcmp_coeff;				//右耳cmp滤波器
    double *rtrans_coeff;			//右耳通透滤波器
    double *ltrans_coeff2;			//左耳通透滤波器2
    double *rtrans_coeff2;			//右耳通透滤波器2
    anc_gain_param_t gains;

    anc_train_para_t train_para;//训练参数结构体
    void (*train_callback)(u8, u8);
    void (*pow_callback)(anc_ack_msg_t *msg_t, u8 setp);
    void (*post_msg_drc)(void);
    void (*post_msg_debug)(void);
} audio_anc_t;


#define ANC_DB_HEAD_LEN		20/*ANC配置区数据头长度*/
#define ANCIF_GAIN_TAG_01	"ANCGAIN01"
#define ANCIF_COEFF_TAG_01	"ANCCOEF01"
#define ANCIF_GAIN_TAG		ANCIF_GAIN_TAG_01	//当前增益配置版本
#define ANCIF_COEFF_TAG		ANCIF_COEFF_TAG_01	//当前系数配置版本
#define ANCIF_HEADER_LEN	10
typedef struct {
    u32 total_len;	//4 后面所有数据加起来长度
    u16 group_crc;	//6 group_type开始做的CRC，更新数据后需要对应更新CRC
    u16 group_type;	//8
    u16 group_len;  //10 header开始到末尾的长度
    char header[ANCIF_HEADER_LEN];//20
    int coeff[0];
} anc_db_head_t;

typedef struct {
    u16 id;
    u16 offset;
    u16 len;
} anc_param_head_t;

/*ANCIF配置区滤波器系数coeff*/
typedef struct {
    u16 version;
    u16 cnt;
    u8 dat[0];
} anc_coeff_t;

/*ANCIF配置区滤波器系数gains*/
typedef struct {
    anc_gain_param_t gains;
    u8 reserve[236 - 120];	//236 + 20byte(header)
} anc_gain_t;

int audio_anc_train(audio_anc_t *param, u8 en);

/*
 *Description 	: audio_anc_run
 *Arguements  	: param is audio anc param
 *Returns	 	: 0  open success
 *		   		: -EPERM 不支持ANC
 *		   		: -EINVAL 参数错误
 *Notes			:
 */
int audio_anc_run(audio_anc_t *param, u8 anc_state);

int audio_anc_close();

void audio_anc_gain(int ffwz_gain, int fbwz_gain);

void audio_anc_fade(int gain, u8 en, u8 step);

void audio_anc_test(audio_anc_t *param);

void anc_train_para_init(anc_train_para_t *para);

void anc_coeff_online_update(audio_anc_t *param, u8 reset_en);

int anc_coeff_size_count(audio_anc_t *param);

/*ANC配置id*/
enum {
    ANC_DB_COEFF,	//ANC系数
    ANC_DB_GAIN,	//ANC增益
    ANC_DB_MAX,
};

typedef struct {
    u8 state;		//ANC配置区状态
    u16 total_size;	//ANC配置区总大小
} anc_db_t;

/*anc配置区初始化*/
int anc_db_init(void);
/*根据配置id获取不同的anc配置*/
int *anc_db_get(u8 id, u32 *len);
/*
 *gain:增益配置,没有的话传NULL
 *coeff:系数配置,没有的话传NULL
 */
int anc_db_put(audio_anc_t *param, anc_gain_t *gain, anc_coeff_t *coeff);
/*anc 音乐补偿模块在线开关*/
void audio_anc_cmp_en(audio_anc_t *param, u8 en);

int anc_coeff_single_fill(anc_coeff_t *target_coeff, anc_coeff_t *tmp_coeff);

/*
*********************************************************************
*                  ANC DCC Config
* Description: anc dcc 等级设置
* Arguments  : dcc_sel	dcc等级
* Return	 : None.
* Note(s)    : 不可以异步
*********************************************************************
*/
void audio_anc_dcc_set(u8 dcc_sel);

void audio_anc_gain_sign(audio_anc_t *param);

void audio_anc_drc_ctrl(audio_anc_t *param, float drc_ratio);

void audio_anc_reset(u8 en);

void audio_anc_norgain(s16 ffgain, s16 fbgain);

void audio_anc_tool_en_ctrl(audio_anc_t *param, u8 enablebit);

int audio_anc_gains_version_verify(audio_anc_t *param, anc_gain_t *cfg_target);

void audio_anc_drc_process(void);

void audio_anc_max_yorder_verify(audio_anc_t *param, u8 max_order);

int audio_anc_fade_dly_get(int target_value, int alogm);

void audio_anc_param_normalize(audio_anc_t *param);

void audio_anc_drc_process(void);

void audio_anc_debug_data_output(void);

void audio_anc_debug_cbuf_sel_set(u8 sel);

void audio_anc_debug_extern_trigger(u8 flag);
#endif/*_ANC_H_*/
