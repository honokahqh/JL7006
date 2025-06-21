/*
 ****************************************************************
 *				   Audio ADC to DAC Low latency
 * File  : audio_ad2da_low_latency.c
 * By    :
 * Notes : 实现ADC采样低延时输出到DAC，用来实现监听等功能
 * Usage :
 ****************************************************************
 */

#include "generic/typedef.h"
#include "board_config.h"
#include "media/includes.h"
#include "audio_config.h"
#include "sound_device.h"
#include "audio_ad2da_low_latency.h"

#if TCFG_AD2DA_LOW_LATENCY_ENABLE

extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;
#define AD2DA_LOW_LATENCY_TASK_NAME     "ad2da_task"

struct __ad2da_low_latency_hdl {
    struct __ad2da_low_latency_param param;
    struct audio_dac_channel dac_ch;
    struct adc_mic_ch mic_ch;
    struct audio_adc_output_hdl adc_output;
    s16 *adc_dma_buf;
    OS_SEM sem;
    s16 *adc_data_addr;
    s32  adc_data_len;
    u8 task_release;
    u8 task_exit;
};

static struct __ad2da_low_latency_hdl *ad2da_low_latency_hdl = NULL;

static void adc_irq_output_handler(void *priv, s16 *data, int len)
{
    struct __ad2da_low_latency_hdl *hdl = (struct __ad2da_low_latency_hdl *)priv;
    if (hdl == NULL) {
        return;
    }
    hdl->adc_data_addr = data;
    hdl->adc_data_len = len;
    os_sem_set(&hdl->sem, 0);
    os_sem_post(&hdl->sem);
}

static void ad2da_low_latency_task_deal(void *p)
{
    int err = 0;
    int wlen = 0;
    struct __ad2da_low_latency_hdl *hdl = (struct __ad2da_low_latency_hdl *)p;
    while (1) {
        err = os_sem_pend(&hdl->sem, 0);
        if (err || hdl->task_release) {
            break;
        }

        // 算法处理

        // 写入DAC
        wlen = sound_pcm_dev_write(&hdl->dac_ch, hdl->adc_data_addr, hdl->adc_data_len);
    }
    hdl->task_exit = 1;
    while (1) {
        os_time_dly(100);
    }
}

int ad2da_low_latency_open(struct __ad2da_low_latency_param *param)
{
    int err = 0;
    struct __ad2da_low_latency_hdl *hdl = ad2da_low_latency_hdl;
    if (hdl != NULL) {
        printf("hdl != NULL!");
        goto __err;
    }

    if (param == NULL) {
        printf("param == NULL!");
        goto __err;
    }

    u32 adc_dma_size = param->adc_irq_points * param->adc_buf_num * 2;
    if (adc_dma_size == 0) {
        printf("adc_dma_size == NULL!");
        goto __err;
    }

    hdl = zalloc(sizeof(struct __ad2da_low_latency_hdl));
    if (hdl == NULL) {
        printf("hdl malloc error!");
        goto __err;
    }

    hdl->adc_dma_buf = zalloc(adc_dma_size);
    if (hdl->adc_dma_buf == NULL) {
        printf("hdl->adc_dma_buf malloc error!");
        goto __err;
    }

    memcpy(&hdl->param, param, sizeof(struct __ad2da_low_latency_param));

    os_sem_create(&hdl->sem, 0);
    err  = os_task_create(ad2da_low_latency_task_deal, (void *)hdl, 5, 768, 128, AD2DA_LOW_LATENCY_TASK_NAME);
    if (err != OS_NO_ERR) {
        printf("task create error!");
        goto __err;
    }

    // 算法初始化

    // DAC 初始化
    audio_dac_new_channel(&dac_hdl, &hdl->dac_ch);
    struct audio_dac_channel_attr attr;
    attr.delay_time = hdl->param.dac_delay;
    attr.protect_time = 8;
    attr.write_mode = WRITE_MODE_BLOCK;
    audio_dac_channel_set_attr(&hdl->dac_ch, &attr);
    sound_pcm_dev_start(&hdl->dac_ch, hdl->param.sample_rate, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));

    // ADC 初始化
    audio_adc_mic_open(&hdl->mic_ch, &hdl->param.mic_ch_sel, &adc_hdl);
    audio_adc_mic_set_gain(&hdl->mic_ch, hdl->param.mic_gain);
    audio_adc_mic_set_sample_rate(&hdl->mic_ch, hdl->param.sample_rate);
    audio_adc_mic_set_buffs(&hdl->mic_ch, hdl->adc_dma_buf, hdl->param.adc_irq_points * 2, hdl->param.adc_buf_num);
    hdl->adc_output.handler = adc_irq_output_handler;
    hdl->adc_output.priv = hdl;
    audio_adc_add_output_handler(&adc_hdl, &hdl->adc_output);
    audio_adc_mic_start(&hdl->mic_ch);

    ad2da_low_latency_hdl = hdl;
    printf("ad2da_low_latency_open success!");
    return 0;
__err:
    if (hdl) {
        if (hdl->adc_dma_buf) {
            free(hdl->adc_dma_buf);
        }
        free(hdl);
    }
    ad2da_low_latency_hdl = NULL;
    printf("ad2da_low_latency_open error!");
    return -1;
}


int ad2da_low_latency_close(void)
{
    int err = 0;
    struct __ad2da_low_latency_hdl *hdl = (struct __ad2da_low_latency_hdl *)ad2da_low_latency_hdl;

    if (hdl == NULL) {
        printf("ad2da_low_latency_close error! hdl == NULL");
        return -1;
    }

    // ADC 关闭
    audio_adc_mic_close(&hdl->mic_ch);
    audio_adc_del_output_handler(&adc_hdl, &hdl->adc_output);

    // DAC 关闭
    sound_pcm_dev_stop(&hdl->dac_ch);

    hdl->task_release = 1;
    os_sem_set(&hdl->sem, 0);
    os_sem_post(&hdl->sem);

    while (hdl->task_exit == 0) {
        os_time_dly(5);
    }

    err = task_kill(AD2DA_LOW_LATENCY_TASK_NAME);
    os_sem_del(&hdl->sem, 0);
    free(hdl->adc_dma_buf);
    free(hdl);
    ad2da_low_latency_hdl = NULL;

    // 算法关闭

    printf("ad2da_low_latency_close success!");
    return 0;
}

void ad2da_low_latency_test(void)
{
    void *test_hdl = NULL;
    struct __ad2da_low_latency_param test_param = {
        .mic_ch_sel        = TCFG_AD2DA_LOW_LATENCY_MIC_CHANNEL,
        .mic_gain          = 12,
        .sample_rate       = TCFG_AD2DA_LOW_LATENCY_SAMPLE_RATE,//采样率
        .adc_irq_points    = 128,//一次处理数据的数据单元， 单位点 4对齐(要配合mic起中断点数修改)
        .adc_buf_num       = 2,
        .dac_delay         = 6,//dac硬件混响延时， 单位ms 要大于 point_unit*2
    };
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
    ad2da_low_latency_open(&test_param);

    /* while (1) { */
    /* os_time_dly(200); */
    /* ad2da_low_latency_close(); */
    /* os_time_dly(200); */
    /* ad2da_low_latency_open(&test_param); */
    /* } */
}

#endif //TCFG_AD2DA_LOW_LATENCY_ENABLE
