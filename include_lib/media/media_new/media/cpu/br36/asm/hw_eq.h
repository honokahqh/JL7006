
#ifndef __HW_EQ_H
#define __HW_EQ_H

#include "generic/typedef.h"
#include "generic/list.h"
#include "os/os_api.h"
#include "application/eq_func_define.h"



enum {				//运行模式
    NORMAL = 0,		//正常模式
    MONO,			//单声道模式
    STEREO			//立体声模式
};

enum {			 	//输出数据类型
    DATO_SHORT = 0, //short
    DATO_INT,		//int
    DATO_FLOAT		//float
};
enum {				//输入数据类型
    DATI_SHORT = 0, //short
    DATI_INT,		// int
    DATI_FLOAT		//float
};
enum {					//输入数据存放模式
    BLOCK_DAT_IN = 0, 	//块模式，例如输入数据是2通道，先存放完第1通道的所有数据，再存放第2通道的所有数据
    SEQUENCE_DAT_IN,	//序列模式，例如输入数据是2通道，先存放第通道的第一个数据，再存放第2个通道的第一个数据，以此类推。
};
enum {				 //输出数据存放模式
    BLOCK_DAT_OUT = 0,//块模式，例如输出数据是2通道，先存放完第1通道的所有数据，再存放第2通道的所有数据
    SEQUENCE_DAT_OUT, //序列模式，例如输入数据是2通道，先存放第通道的第一个数据，再存放第2个通道的第一个数据，以此类推。
};


/*eq IIR type*/
typedef enum {
    EQ_IIR_TYPE_HIGH_PASS = 0x00,
    EQ_IIR_TYPE_LOW_PASS,
    EQ_IIR_TYPE_BAND_PASS,
    EQ_IIR_TYPE_HIGH_SHELF,
    EQ_IIR_TYPE_LOW_SHELF,
} EQ_IIR_TYPE;

struct eq_seg_info {
    u16 index;
    u16 iir_type; ///<EQ_IIR_TYPE
    int freq;
    float gain;
    float q;
};

struct eq_coeff_info {
    u16 nsection : 6;
    u16 no_coeff : 1;	//不是滤波系数
#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    u16 sr;
#endif
    float *L_coeff;
    float *R_coeff;
    float L_gain;
    float R_gain;
    float *N_coeff[8];
    float N_gain[8];

};

struct hw_eq_ch;

struct hw_eq {
    struct list_head head;
    OS_MUTEX mutex;
    struct hw_eq_ch *cur_ch;
};

enum {
    HW_EQ_CMD_CLEAR_MEM = 0xffffff00,
    HW_EQ_CMD_CLEAR_MEM_L,
    HW_EQ_CMD_CLEAR_MEM_R,
};

struct hw_eq_handler {
    int (*eq_probe)(struct hw_eq_ch *);
    int (*eq_output)(struct hw_eq_ch *, s16 *, u16);
    int (*eq_post)(struct hw_eq_ch *);
    int (*eq_input)(struct hw_eq_ch *, void **, void **);
};

struct hw_eq_ch {
    unsigned char updata;             //更新参数以及中间数据
    unsigned char updata_coeff_only;  //只更新参数，不更新中间数据
    unsigned char no_wait;            //是否是异步eq处理  0：同步的eq  1：异步的eq
    unsigned char channels;           //输入通道数
    unsigned char SHI;                //eq运算输出数据左移位数控制,记录
    unsigned char countL;             //eq运算输出数据左移位数控制临时记录
    unsigned short stage;             //eq运算开始位置标识
    unsigned char nsection;           //eq段数
    unsigned char no_coeff;	          // 非滤波系数
    volatile unsigned char active;    //已启动eq处理  1：busy  0:处理结束
    unsigned char run_mode;           //0按照输入的数据排布方式 ，输出数据 1:单入多出，  2：立体声入多出
    unsigned char in_mode;            //输入数据的位宽 0：short  1:int  2:float
    unsigned char out_32bit;          //输出数据的位宽 0：short  1:int  2:float
    unsigned char out_channels;       //输出通道数
    unsigned char data_in_mode;       //输入数据存放模式
    unsigned char data_out_mode;      //输入数据存放模式

#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    u16 sr;
#endif
    float *L_coeff;
    float *R_coeff;
    float L_gain;
    float R_gain;
    float *N_coeff[8];
    float N_gain[8];
    float *eq_LRmem;
    s16 *out_buf;
    s16 *in_buf;
    int in_len;
    void *priv;
    volatile OS_SEM sem;
    struct list_head entry;
    struct hw_eq *eq;
    const struct hw_eq_handler *eq_handler;
};

//系数计算子函数

void eq_get_AllpassCoeff(void *Coeff);
void design_hp(int fc, int fs, float quality_factor, float *coeff);
void design_lp(int fc, int fs, float quality_factor, float *coeff);
void design_pe(int fc, int fs, float gain, float quality_factor, float *coeff);
void design_hs(int fc, int fs, float gain, float quality_factor, float *coeff);
void design_ls(int fc, int fs, float gain, float quality_factor, float *coeff);
int eq_stable_check(float *coeff);
float eq_db2mag(float x);

// eq滤波系数计算
int eq_seg_design(struct eq_seg_info *seg, int sample_rate, float *coeff);


// 在EQ中断中调用
void audio_hw_eq_irq_handler(struct hw_eq *eq);

// EQ初始化
int audio_hw_eq_init(struct hw_eq *eq, u32 eq_section_num);

// 打开一个通道
int audio_hw_eq_ch_open(struct hw_eq_ch *ch, struct hw_eq *eq);
// 设置回调接口
int audio_hw_eq_ch_set_handler(struct hw_eq_ch *ch, struct hw_eq_handler *handler);
// 设置通道基础信息
int audio_hw_eq_ch_set_info(struct hw_eq_ch *ch, u8 channels, u8 out_32bit);
int audio_hw_eq_ch_set_info_new(struct hw_eq_ch *ch, u8 channels, u8 in_mode, u8 out_mode, u8 run_mode, u8 data_in_mode, u8 data_out_mode);

// 设置硬件转换系数
int audio_hw_eq_ch_set_coeff(struct hw_eq_ch *ch, struct eq_coeff_info *info);

// 启动一次转换
int audio_hw_eq_ch_start(struct hw_eq_ch *ch, void *input, void *output, int len);

// 关闭一个通道
int audio_hw_eq_ch_close(struct hw_eq_ch *ch);

// 挤出eq中的数据
int audio_hw_eq_flush_out(struct hw_eq *eq);

// eq正在运行
int audio_hw_eq_is_running(struct hw_eq *eq);

#endif /*__HW_EQ_H*/

