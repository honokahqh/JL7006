/*
 ****************************************************************************
 *							Audio PDM Demo
 *
 *Description	: Audio PDM使用范例。PDM Link是一种数字MIC接口。
 *Notes			: 本demo为开发测试范例，请不要修改该demo， 如有需求，请自行
 *				  复制再修改
 ****************************************************************************
 */
#include "audio_demo.h"

#include "includes.h"
#include "asm/includes.h"
#include "asm/gpio.h"
#include "asm/dac.h"
#include "media/includes.h"
#include "pdm_link.h"
#include "audio_config.h"

/* #define PLNK_DEMO_SR 		    16000//48000//44100 //16000/44100/48000 */
#define PLNK_DEMO_SCLK          2400000/*1M-4M,SCLK/SR需为整数且在1-4096范围*/
#define PLNK_DEMO_CH_NUM 	     1  /*支持的最大通道(max = 2)*/
#define PLNK_DEMO_IRQ_POINTS    256 /*采样中断点数*/

struct pdm_link_demo {
    audio_plnk_t plnk_mic;
    u8 mic_2_dac;
};

static struct pdm_link_demo *plnk_demo = NULL;
extern struct audio_dac_hdl dac_hdl;

/*
*********************************************************************
*                  AUDIO PDM OUTPUT
* Description: pdm mic数据回调
* Arguments  : data	mic数据
*			   len	数据点数
* Return	 : None.
* Note(s)    : 单声道数据格式
*			   L0 L1 L2 L3 L4 ...
*				双声道数据格式()
*			   L0 R0 L1 R1 L2 R2...
*********************************************************************
*/
static void audio_plnk_mic_output(void *data, u16 len)
{
    putchar('o');
    s16 *paddr = NULL;
    paddr = data;
    len = len * 2;
    u32 wlen = len * 2;
    if (plnk_demo->mic_2_dac) {
        wlen = audio_dac_write(&dac_hdl, paddr, len);
        if (wlen != len) {
            printf("pdm demo dac write err %d %d", wlen, len);
        }
    }
}

/*
*********************************************************************
*                  AUDIO PDM MIC OPEN
* Description: 打开pdm mic模块
* Arguments  : sample 		pdm mic采样率
*			   mic_2_dac 	mic数据（通过DAC）监听
* Return	 : None.
* Note(s)    : (1)sclk_fre:1M - 4M,sclk_fre / sr : 1 - 4096且须为整数
*              (2)打开pdm mic通道示例：
*				audio_plnk_demo_open(16000, 1);
*********************************************************************
*/
void audio_plnk_demo_open(u16 sample, u8 mic_2_dac)
{
    printf("pdm link demo\n");

    plnk_demo = zalloc(sizeof(struct pdm_link_demo));
    if (plnk_demo) {
        plnk_demo->plnk_mic.ch_num = PLNK_DEMO_CH_NUM;
        /*sclk_fre:1M - 4M,sclk_fre / sr ：1 —4096且须为整数*/
        plnk_demo->plnk_mic.sr = sample;
        plnk_demo->plnk_mic.sclk_fre = PLNK_DEMO_SCLK;
        if (plnk_demo->plnk_mic.sclk_fre % plnk_demo->plnk_mic.sr) {
            y_printf("[warn]SCLK/SR需为整数且在1-4096范围\n");
        }
#if (PLNK_CH_EN == PLNK_CH0_EN)
        plnk_demo->plnk_mic.ch0_mode = CH0MD_CH0_SCLK_RISING_EDGE;
        plnk_demo->plnk_mic.ch1_mode = CH1MD_CH0_SCLK_FALLING_EDGE;
#elif (PLNK_CH_EN == PLNK_CH1_EN)
        plnk_demo->plnk_mic.ch0_mode = CH0MD_CH1_SCLK_FALLING_EDGE;
        plnk_demo->plnk_mic.ch1_mode = CH1MD_CH1_SCLK_RISING_EDGE;
#else
        plnk_demo->plnk_mic.ch0_mode = CH0MD_CH0_SCLK_RISING_EDGE;
        plnk_demo->plnk_mic.ch1_mode = CH1MD_CH1_SCLK_RISING_EDGE;
#endif
        plnk_demo->plnk_mic.output = audio_plnk_mic_output;
        plnk_demo->plnk_mic.buf_len = PLNK_DEMO_IRQ_POINTS;
        plnk_demo->plnk_mic.buf = zalloc(plnk_demo->plnk_mic.buf_len * plnk_demo->plnk_mic.ch_num * 2 * 2);
        ASSERT(plnk_demo->plnk_mic.buf);
        plnk_demo->plnk_mic.sclk_io = TCFG_AUDIO_PLNK_SCLK_PIN;
        plnk_demo->plnk_mic.ch0_io = TCFG_AUDIO_PLNK_DAT0_PIN;
        /* JL_WL_AUD->CON0 |= BIT(0); */
        plnk_demo->mic_2_dac = mic_2_dac;
        if (plnk_demo->mic_2_dac) {
            extern void app_audio_state_switch(u8 state, s16 max_volume);
            app_audio_state_switch(APP_AUDIO_STATE_MUSIC, 16);
            audio_dac_set_volume(&dac_hdl, 16);
            audio_dac_set_sample_rate(&dac_hdl, sample);
            audio_dac_start(&dac_hdl);
        }

        audio_plnk_open(&plnk_demo->plnk_mic);
        audio_plnk_start(&plnk_demo->plnk_mic);
    }
}

/*
*********************************************************************
*                  AUDIO PDM MIC CLOSE
* Description: 关闭pdm mic模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_plnk_demo_close(void)
{
    if (plnk_demo) {
        audio_plnk_close();
        if (plnk_demo->mic_2_dac) {
            audio_dac_stop(&dac_hdl);
        }
        free(plnk_demo);
        plnk_demo = NULL;
    }
}

#if AUDIO_DEMO_LP_REG_ENABLE
static u8 plnk_demo_idle_query()
{
    return plnk_demo ? 0 : 1;
}

REGISTER_LP_TARGET(plnk_demo_lp_target) = {
    .name = "plnk_demo",
    .is_idle = plnk_demo_idle_query,
};
#endif/*AUDIO_DEMO_LP_REG_ENABLE*/
