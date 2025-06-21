#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_anc_hearing_aid.h"
#include "audio_anc.h"
#include "howling_api.h"

#if ANC_HEARAID_HOWLING_DET && ANC_HEARAID_EN

#if 1
#define hear_aid_log	y_printf
#else
#define hear_aid_log(...)
#endif

extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;

#define HEARAID_ADC_CH_NUM         1	    /*支持的最大采样通道(max = 2)*/
#define HEARAID_ADC_BUF_NUM        2  	    /*采样buf数*/
#define HEARAID_ADC_IRQ_POINTS     160	    /*采样中断点数*/
#define HEARAID_ADC_SR 			   16000	/*采样率*/
#define HEARAID_ADC_BUFS_SIZE      (HEARAID_ADC_CH_NUM * HEARAID_ADC_BUF_NUM * HEARAID_ADC_IRQ_POINTS)
#define HEARAID_CBUFS_SIZE      HEARAID_ADC_BUFS_SIZE * 4

#define HEARAID_RESUME_TIMEOUT			   5000		/*触发啸叫抑制的恢复时间，ms*/

//ADC数据结构
struct hearing_adc_context {
    u8 mic_idx;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[HEARAID_ADC_BUFS_SIZE];
};

//啸叫抑制数据结构
struct anc_hearing_aid_handle {
    u8 en;
    u8 state;				//当前状态
    u8 resume_cnt;			//恢复累加值，用于判断没有连续啸叫信号
    u8 hold_cnt;			//继续压制的啸叫信号累加值
    u8 dither_cnt;			//啸叫标记累加值
    u8 dither_cnt_thr;		//啸叫标记消抖阈值，连续的啸叫信号才触发抑制
    u8 howl_thr;			//啸叫检测阈值， 越小越灵敏
    u16 gain;				//当前增益
    u16 hd_time_id;			//触发啸叫空挡定时器ID
    u16 hd_fade_time_id;	//增益恢复定时器ID
    u16 target_gain;		//啸叫之后的目标增益，建议写0
    audio_anc_t *anc_param;
    struct hearing_adc_context *adc;
    cbuffer_t cbuf;
    short temp_buf[160];
    u8 buf[HEARAID_CBUFS_SIZE];
    u8 howl_runbuf[0];
};

static struct anc_hearing_aid_handle *hearaid_hdl = NULL;

#define __this 			(hearaid_hdl)

extern void anc_core_ffdrc_normal_gain(s16 gain);
extern void anc_core_ffdrc_high_gain(s16 gain);

//ADC回调接口
static void audio_anc_hearing_aid_mic_output(void *priv, s16 *data, int len)
{
    int wlen = cbuf_write(&__this->cbuf, data, len);
    /* printf("wlen %d, len %d\n",wlen,len); */
    if (wlen < len) {
        putchar('D');
    }
    os_taskq_post_msg("anchearaid", 1, HEARAID_HOWL_MSG_RUN);
}

//MIC打开接口
void audio_anc_hearing_aid_mic_open(u8 mic_idx, u8 gain, u16 sr)
{
    hear_aid_log("audio_anc_hearing_aid_mic open:%d,gain:%d,sr:%d\n", mic_idx, gain, sr);
    __this->adc = zalloc(sizeof(struct hearing_adc_context));
    if (__this->adc) {
        if (mic_idx == A_MIC0) {
            audio_adc_mic_open(&__this->adc->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
            audio_adc_mic_set_gain(&__this->adc->mic_ch, gain);
        } else {
            audio_adc_mic1_open(&__this->adc->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
            audio_adc_mic1_set_gain(&__this->adc->mic_ch, gain);
        }
        audio_adc_mic_set_sample_rate(&__this->adc->mic_ch, sr);
        audio_adc_mic_set_buffs(&__this->adc->mic_ch, __this->adc->adc_buf, HEARAID_ADC_IRQ_POINTS * 2, HEARAID_ADC_BUF_NUM);
        __this->adc->adc_output.handler = audio_anc_hearing_aid_mic_output;
        audio_adc_add_output_handler(&adc_hdl, &__this->adc->adc_output);
        audio_adc_mic_start(&__this->adc->mic_ch);

        __this->adc->mic_idx = mic_idx;
    }
    hear_aid_log("audio_anc_hearing_aid_mic start succ\n");
}

//MIC关闭接口
void audio_anc_hearing_aid_mic_close(void)
{
    hear_aid_log("audio_anc_hearing_aid_mic_close\n");
    if (__this->adc) {
        audio_adc_del_output_handler(&adc_hdl, &__this->adc->adc_output);
        audio_adc_mic_close(&__this->adc->mic_ch);
        free(__this->adc);
        __this->adc = NULL;
    }
}

void audio_anc_hearing_aid_param_update(audio_anc_t *anc_param, u8 reset_en)
{
    u8 howl_reset = 0;
    if (!__this) {
        if (anc_param->gains.sw_howl_en && anc_param->mode == ANC_TRANSPARENCY) {	//在线更新启动
            audio_anc_hearing_aid_howldet_start(anc_param);
        }
        return;
    }
    if (reset_en && anc_param->mode == ANC_TRANSPARENCY && \
        ((__this->en != anc_param->gains.sw_howl_en) || \
         (__this->howl_thr != anc_param->gains.sw_howl_thr))) {
        howl_reset = 1;
    }
    __this->en = anc_param->gains.sw_howl_en;
    __this->dither_cnt_thr = anc_param->gains.sw_howl_dither_thr;
    __this->howl_thr = anc_param->gains.sw_howl_thr;
    __this->target_gain = (anc_param->gains.sw_howl_en) ? anc_param->gains.sw_howl_gain : 1024;

    hear_aid_log("targain %d, howl_thr %d, dither_cnt_thr %d\n", __this->target_gain, __this->howl_thr, __this->dither_cnt_thr);
    if (howl_reset) {
        //在线关闭 恢复增益
        if (!anc_param->gains.sw_howl_en) {
            __this->gain = 1024;
            __this->target_gain = 1024;
            anc_core_ffdrc_normal_gain(1024);	//恢复流程将norgain恢复正常
            anc_core_ffdrc_high_gain(anc_param->gains.drctrans_hgain);
            audio_anc_fade(16384, 1, 1);
        } else {
            howling_init(__this->howl_runbuf, 1, 1.0f, 1.0f, __this->howl_thr, HEARAID_ADC_SR);
            SetHowlingDetection(__this->howl_runbuf, 1);
        }
    }
}


//增益设置函数, 判断等于最小增益时，使用drc_norgain, 设置更快
void audio_anc_hearing_aid_ffgain(int gain)
{
//1024为底
#if 1
    int hgain;
    hgain = (__this->anc_param->gains.drctrans_hgain * gain) >> 10;	//等比例换算hgain
    if (gain == __this->target_gain) {
        /* if(1) { */
        anc_core_ffdrc_normal_gain(gain);	//迅速压低增益
        anc_core_ffdrc_high_gain(hgain);	//保证DRC工作正常
        audio_anc_fade(gain << 4, 1, 1);
    } else {
        anc_core_ffdrc_normal_gain(1024);	//恢复流程将norgain恢复正常
        anc_core_ffdrc_high_gain(__this->anc_param->gains.drctrans_hgain);
        audio_anc_fade(gain << 4, 1, 1);	//fadegain 缓慢恢复
    }
    g_printf("hgain %d, gain %d, SFC hgain %d, gain %d\n", hgain, gain, JL_ANC->CON17 >> 16, JL_ANC->CON16 >> 16);
#else
    audio_anc_fade(gain << 4, 1, 1);
#endif
}

//啸叫抑制当前增益获取接口
int audio_anc_hearing_aid_ffgain_get(void)
{
    return __this->gain;
}

//啸叫抑制增益恢复
void audio_anc_hearing_aid_gain_resume_fade(void *priv)
{
    if (__this->state == HEARAID_HOWL_STA_RESUME) {
        __this->gain += 20;
        if (__this->gain > 1024) {
            __this->gain = 1024;
            audio_anc_hearing_aid_ffgain(__this->gain);
            __this->state = HEARAID_HOWL_STA_NORMAL;
            sys_timer_del(__this->hd_fade_time_id);
            __this->hd_fade_time_id = 0;
            return;
        }
        hear_aid_log("anc hearing_aid resume_fade gain: %d\n", __this->gain);
        audio_anc_hearing_aid_ffgain(__this->gain);
    }
}

//啸叫抑制增益恢复定时器注册
void audio_anc_hearing_aid_gain_resume(void *priv)
{
    hear_aid_log("anc hearing_aid resume gain: %d\n", __this->gain);
    __this->hd_fade_time_id = sys_timer_add(NULL, audio_anc_hearing_aid_gain_resume_fade, 100);
    __this->hd_time_id = 0;
}

//啸叫抑制去抖函数,
void audio_anc_hearing_aid_dither_cnt_clear(void *priv)
{
    __this->dither_cnt = 0;
    hear_aid_log("clean dither_cnt!\n");
}

//啸叫抑制处理任务
static void audio_anc_hearing_aid_task(void *p)
{
    int res;
    int msg[16];
    u32 pend_timeout = portMAX_DELAY;
    int rlen, num;
    hear_aid_log(">>>audio_anc_hearing_aid_task<<<\n");
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case HEARAID_HOWL_MSG_START:
                hear_aid_log("HEARAID_HOWL_MSG_START\n");

                __this->state == HEARAID_HOWL_STA_NORMAL;
                audio_anc_hearing_aid_mic_open(msg[2], msg[3], HEARAID_ADC_SR);
                howling_init(__this->howl_runbuf, 1, 1.0f, 1.0f, __this->howl_thr, HEARAID_ADC_SR);
                SetHowlingDetection(__this->howl_runbuf, 1);
                break;
            case HEARAID_HOWL_MSG_STOP:
                hear_aid_log("HEARAID_HOWL_MSG_STOP\n");
                audio_anc_hearing_aid_mic_close();
                break;
            case HEARAID_HOWL_MSG_RUN:
                /* hear_aid_log("HEARAID_HOWL_MSG_RUN\n"); */
                if (__this->cbuf.data_len > (160 << 1)) {
                    rlen = cbuf_read(&__this->cbuf, __this->temp_buf, 160 << 1);
                    if (rlen != 0) {
                        howling_run(__this->howl_runbuf, __this->temp_buf, __this->temp_buf, (rlen >> 1));  //运行
                        float *freq = getHowlingFreq(__this->howl_runbuf, &num);
#if 1
                        if (num) {
                            if (__this->dither_cnt++ < __this->dither_cnt_thr) {
                                /* hear_aid_log("cnt: %d\n", __this->dither_cnt); */
                                if (__this->dither_cnt == 1) {
                                    sys_timeout_add(NULL, audio_anc_hearing_aid_dither_cnt_clear, 100);
                                }
                                break;
                            }
                            __this->dither_cnt = 0;
                            /* if(((++__this->hold_cnt) > 5) && (__this->state == HEARAID_HOWL_STA_HOLD)){ */
                            if (0) {
                                //降低到安全增益仍然发生啸叫，增益继续降低
                                __this->gain >>= 1;
                            } else {
                                __this->gain = __this->target_gain;
                            }
                            __this->state = HEARAID_HOWL_STA_HOLD;
                            __this->resume_cnt = 0;
                            audio_anc_hearing_aid_ffgain(__this->gain);
                            if (__this->hd_fade_time_id) {
                                sys_timer_del(__this->hd_fade_time_id);
                            }
                            if (!__this->hd_time_id) {
                                __this->hd_time_id = sys_timeout_add(NULL, audio_anc_hearing_aid_gain_resume, HEARAID_RESUME_TIMEOUT);
                            }
                            //降低增益
                            /* hear_aid_log("anc hearing_aid howling now!!! gain: %d\n", __this->gain); */
                        } else if (__this->state == HEARAID_HOWL_STA_HOLD) {
                            /* hear_aid_log("anc hearing_aid resume ready!!! gain: %d\n", __this->gain); */
                            if (++__this->resume_cnt > 10) {
                                __this->hold_cnt = 0;
                                __this->state = HEARAID_HOWL_STA_RESUME;
                            }
                        }
#else
                        if (num) {
                            hear_aid_log("anc hearing_aid howling now!!! gain: %d\n", __this->gain);
                        }
#endif
                    }
                }
                break;
            }
        }
    }
}

//助听器啸叫抑制启动接口
void audio_anc_hearing_aid_howldet_start(audio_anc_t *param)
{
    if (!param->gains.sw_howl_en) {
        return;
    }
    u8 mic_type, micgain;
    int howling_buflen = get_howling_buf(HEARAID_ADC_SR);
    hear_aid_log("hearing aid hdl len %d, howling_buflen %d\n", sizeof(struct anc_hearing_aid_handle), howling_buflen);
    __this = malloc(sizeof(struct anc_hearing_aid_handle) + howling_buflen);
    if (!__this) {
        ASSERT(0);
    }
    __this->anc_param = param;
    mic_type = (param->ch & ANC_L_CH) ? param->mic_type[0] : param->mic_type[1];
    cbuf_init(&__this->cbuf, __this->buf, HEARAID_CBUFS_SIZE);
    __this->hold_cnt = 0;
    __this->dither_cnt = 0;
    __this->hd_time_id = 0;
    __this->hd_fade_time_id = 0;
    audio_anc_hearing_aid_param_update(param, 0);
    task_create(audio_anc_hearing_aid_task, NULL, "anchearaid");
    micgain = (ANCL_FF_MIC == A_MIC0) ? param->gains.ref_mic_gain : param->gains.err_mic_gain;
    os_taskq_post_msg("anchearaid", 3, HEARAID_HOWL_MSG_START, mic_type, micgain);
}

//助听器啸叫抑制停止接口
void audio_anc_hearing_aid_howldet_stop(void)
{
    if (__this) {
        /* os_taskq_post_msg("anchearaid", 1, HEARAID_HOWL_MSG_STOP); */
        hear_aid_log("HEARAID_HOWL_MSG_STOP\n");
        audio_anc_hearing_aid_mic_close();
        if (__this->hd_time_id) {
            sys_timer_del(__this->hd_time_id);
        }
        if (__this->hd_fade_time_id) {
            sys_timer_del(__this->hd_fade_time_id);
        }
        task_kill("anchearaid");
        free(__this);
        __this = NULL;
    }
}

static u8 ANC_HEARAID_HOWL_idle_query(void)
{
    return (__this) ? 0 : 1;
}

REGISTER_LP_TARGET(ANC_HEARAID_HOWL_lp_target) = {
    .name = "ANC_HEARAID_HOWL",
    .is_idle = ANC_HEARAID_HOWL_idle_query,
};

#endif/*ANC_HEARAID_HOWLING_DET*/
