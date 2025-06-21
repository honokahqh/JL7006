#ifndef _AUDIO_DEC_MIC2PCM_H_
#define _AUDIO_DEC_MIC2PCM_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"


#define FIXED_SAMPLE_RATE             44100  //固定采样率

#define PCM_DEC_IN_SIZE	   64
#define PCM_DEC_IN_CBUF_SIZE	(1024*4)	//若设置成1024太小，解码资源的put、get操作容易溢出



struct pcm_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;		// 资源等待句柄
    struct audio_mixer_ch mix_ch;	// 叠加句柄
    struct audio_eq_drc *eq_drc;    //eq drc句柄
    struct audio_src_handle *hw_src; //硬件src
    OS_SEM test_sem;
    cbuffer_t pcm_in_cbuf;
    u8 *p_in_cbuf_buf;
    u8 channel;
    u8 input_data_channel_num;		//输入进来的音频数据的声道数
    u8 output_ch;
    u16 two_ch_remain_len;
    u16 two_ch_remain_addr;
    u16 sample_rate;
    u16 mixer_sample_rate;
    u32 coding_type;
    u32 src_out_sr;
    u32 id;				// 唯一标识符，随机值
    volatile int dec_put_res_flag;
    volatile int last_dec_put_res_flag;
    u32 start : 1;		// 正在解码
};

int audio_linein_pcm_dec_open(u16 sample_rate, u8 input_data_channel_num);
int linein_pcm_dec_data_write(s16 *data, int len);		//外部可以调用这条函数来传入pcm数据
void linein_pcm_dec_close(void);

#endif

