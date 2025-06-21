/*
 ****************************************************************
 *							AUDIO ENCODE PROCESS
 * File  : audio_enc.c
 * By    :
 * Notes : 编码api实现
 *
 ****************************************************************
 */
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "classic/hci_lmp.h"
#include "aec_user.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_enc.h"
#include "app_config.h"

#if TCFG_AUDIO_DMS_DUT_ENABLE
#include "audio_dms_tool.h"
#endif/*TCFG_AUDIO_DMS_DUT_ENABLE*/
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif/*TCFG_AUDIO_ANC_ENABLE*/
#if TCFG_AUDIO_INPUT_IIS
#include "audio_link.h"
#include "Resample_api.h"
#endif/*TCFG_AUDIO_INPUT_IIS*/
#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
#include "phone_message/phone_message.h"
#endif/*TCFG_PHONE_MESSAGE_ENABLE*/

#if TCFG_SMART_VOICE_ENABLE
#include "smart_voice/smart_voice.h"
#include "media/jl_kws.h"
#if TCFG_CALL_KWS_SWITCH_ENABLE
#include "btstack/avctp_user.h"
#endif
#endif
#if TCFG_SIDETONE_ENABLE
#include "audio_sidetone.h"
#endif/*TCFG_SIDETONE_ENABLE*/
extern struct adc_platform_data adc_data;

struct audio_adc_hdl adc_hdl;
struct audio_encoder_task *encode_task = NULL;

struct esco_enc_hdl {
    struct audio_encoder encoder;
    u8 start;
    s16 output_frame[30];               //align 2Bytes
    int pcm_frame[60];                 	//align 2Bytes
};
struct esco_enc_hdl *esco_enc = NULL;

/*
 *mic电源管理
 *设置mic vdd对应port的状态
 */
void audio_mic_pwr_io(u32 gpio, u8 out_en)
{
    gpio_set_die(gpio, 1);
    gpio_set_pull_up(gpio, 0);
    gpio_set_pull_down(gpio, 0);
    gpio_direction_output(gpio, out_en);
}

void audio_mic_pwr_ctl(u8 state)
{
    u8 mic_busy = 0;
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_GPIO)
    switch (state) {
    case MIC_PWR_OFF:
        if (esco_enc) {
            mic_busy = esco_enc->start;
            if (mic_busy) {
                printf("phone mic is useing !!!!!\n");
            }
        }
#if TCFG_AUDIO_ANC_ENABLE
        if (anc_status_get()) {
            printf("anc mic is useing !!!!!\n");
            mic_busy = 1;
        }
#endif/*TCFG_AUDIO_ANC_ENABLE*/
        if (audio_adc_is_active()) {
            printf("audio adc is useing !!!!!\n");
            mic_busy = 1;
        }
        if (mic_busy) {
            return;
        }
        printf("mic close !!!\n");
    case MIC_PWR_INIT:
        /*mic供电IO配置：输出0*/
#if ((TCFG_AUDIO_MIC_PWR_PORT != NO_CONFIG_PORT))
        audio_mic_pwr_io(TCFG_AUDIO_MIC_PWR_PORT, 0);
#endif/*TCFG_AUDIO_MIC_PWR_PORT*/
#if ((TCFG_AUDIO_MIC1_PWR_PORT != NO_CONFIG_PORT))
        audio_mic_pwr_io(TCFG_AUDIO_MIC1_PWR_PORT, 0);
#endif/*TCFG_AUDIO_MIC1_PWR_PORT*/
#if ((TCFG_AUDIO_MIC2_PWR_PORT != NO_CONFIG_PORT))
        audio_mic_pwr_io(TCFG_AUDIO_MIC2_PWR_PORT, 0);
#endif/*TCFG_AUDIO_MIC2_PWR_PORT*/

        /*mic信号输入端口配置：
         *mic0:PA1
         *mic1:PB8*/
#if (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_L)
        audio_mic_pwr_io(IO_PORTA_01, 0);//MIC0
#endif/*TCFG_AUDIO_ADC_MIC_CHA*/

#if (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_R)
        audio_mic_pwr_io(IO_PORTB_08, 0);//MIC1
#endif/*TCFG_AUDIO_ADC_MIC_CHA*/
        break;

    case MIC_PWR_ON:
        /*mic供电IO配置：输出1*/
#if (TCFG_AUDIO_MIC_PWR_PORT != NO_CONFIG_PORT)
        audio_mic_pwr_io(TCFG_AUDIO_MIC_PWR_PORT, 1);
#endif/*TCFG_AUDIO_MIC_PWR_CTL*/
#if (TCFG_AUDIO_MIC1_PWR_PORT != NO_CONFIG_PORT)
        audio_mic_pwr_io(TCFG_AUDIO_MIC1_PWR_PORT, 1);
#endif/*TCFG_AUDIO_MIC1_PWR_PORT*/
#if (TCFG_AUDIO_MIC2_PWR_PORT != NO_CONFIG_PORT)
        audio_mic_pwr_io(TCFG_AUDIO_MIC2_PWR_PORT, 1);
#endif/*TCFG_AUDIO_MIC2_PWR_PORT*/
        break;

    case MIC_PWR_DOWN:
        break;
    }
#endif/*TCFG_AUDIO_MIC_PWR_CTL*/
}

void esco_enc_resume(void);



__attribute__((weak)) int audio_aec_output_read(s16 *buf, u16 len)
{
    return 0;
}

void esco_enc_resume(void)
{
    if (esco_enc) {
        audio_encoder_resume(&esco_enc->encoder);
    }
}

static int esco_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int rlen = 0;
    if (encoder == NULL) {
        printf("encoder NULL");
    }
    struct esco_enc_hdl *enc = container_of(encoder, struct esco_enc_hdl, encoder);

    if (enc == NULL) {
        printf("enc NULL");
    }

    while (1) {
#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
        rlen = phone_message_output_read(enc->pcm_frame, frame_len);
        if (rlen == frame_len) {
            break;
        } else if (rlen == 0) {
            return 0;
        }
#endif/*TCFG_PHONE_MESSAGE_ENABLE*/
        rlen = audio_aec_output_read(enc->pcm_frame, frame_len);
        if (rlen == frame_len) {
            /*esco编码读取数据正常*/
            break;
        } else if (rlen == 0) {
            /*esco编码读不到数，返回0*/
            return 0;
        } else {
            /*通话结束，aec已经释放*/
            printf("audio_enc end:%d\n", rlen);
            rlen = 0;
            break;
        }
    }

    *frame = enc->pcm_frame;
    return rlen;
}
static void esco_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input esco_enc_input = {
    .fget = esco_enc_pcm_get,
    .fput = esco_enc_pcm_put,
};

static int esco_enc_probe_handler(struct audio_encoder *encoder)
{
#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
    phone_message_call_api_start();
#endif/*TCFG_PHONE_MESSAGE_ENABLE*/
    return 0;
}

static int esco_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    lmp_private_send_esco_packet(NULL, frame, len);
    //printf("frame:%x,out:%d\n",frame, len);

    return len;
}

const static struct audio_enc_handler esco_enc_handler = {
    .enc_probe = esco_enc_probe_handler,
    .enc_output = esco_enc_output_handler,
};

static void esco_enc_event_handler(struct audio_encoder *encoder, int argc, int *argv)
{
    printf("esco_enc_event_handler:0x%x,%d\n", argv[0], argv[0]);
    switch (argv[0]) {
    case AUDIO_ENC_EVENT_END:
        puts("AUDIO_ENC_EVENT_END\n");
        break;
    }
}

/*
*********************************************************************
*                  PDM MIC API
* Description: PDM mic操作接口
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
/* #define ESCO_PLNK_SR 		    16000#<{(|16000/44100/48000|)}># */
#define ESCO_PLNK_SCLK          2400000 /*1M-4M,SCLK/SR需为整数且在1-4096范围*/
#define ESCO_PLNK_CH_NUM 	     1  /*支持的最大通道(max = 2)*/
#define ESCO_PLNK_IRQ_POINTS    256 /*采样中断点数*/
#include "pdm_link.h"
static audio_plnk_t *audio_plnk_mic = NULL;
static void audio_plnk_mic_output(void *buf, u16 len)
{
    s16 *mic0 = (s16 *)buf;
    if (esco_enc) {
#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
        int ret = phone_message_mic_write(mic0, len << 1);
        if (ret >= 0) {
            esco_enc_resume();
            return ;
        }
#endif/*TCFG_PHONE_MESSAGE_ENABLE*/
        audio_aec_inbuf(mic0, len << 1);
    }
}
static int esco_pdm_mic_en(u8 en, u16 sr, u16 gain)
{
    printf("esco_pdm_mic_en:%d\n", en);
    if (en) {
        audio_plnk_mic = zalloc(sizeof(audio_plnk_t));
        audio_mic_pwr_ctl(MIC_PWR_ON);
        if (audio_plnk_mic) {
            audio_plnk_mic->ch_num = ESCO_PLNK_CH_NUM;//PLNK_MIC_CH;
            /*SCLK:1M-4M,SCLK/SR需为整数且在1-4096范围*/
            audio_plnk_mic->sr = sr;
            audio_plnk_mic->sclk_fre = ESCO_PLNK_SCLK;
            if (audio_plnk_mic->sclk_fre % audio_plnk_mic->sr) {
                r_printf("[warn]SCLK/SR需为整数且在1-4096范围\n");
            }
#if (PLNK_CH_EN == PLNK_CH0_EN)
            audio_plnk_mic->ch0_mode = CH0MD_CH0_SCLK_RISING_EDGE;
            audio_plnk_mic->ch1_mode = CH1MD_CH0_SCLK_FALLING_EDGE;
#elif (PLNK_CH_EN == PLNK_CH1_EN)
            audio_plnk_mic->ch0_mode = CH0MD_CH1_SCLK_FALLING_EDGE;
            audio_plnk_mic->ch1_mode = CH1MD_CH1_SCLK_RISING_EDGE;
#else
            audio_plnk_mic->ch0_mode = CH0MD_CH0_SCLK_RISING_EDGE;
            audio_plnk_mic->ch1_mode = CH1MD_CH1_SCLK_RISING_EDGE;
#endif
            audio_plnk_mic->output = audio_plnk_mic_output;
            audio_plnk_mic->buf_len = ESCO_PLNK_IRQ_POINTS;
            audio_plnk_mic->buf = zalloc(audio_plnk_mic->buf_len * audio_plnk_mic->ch_num * 2 * 2);
            ASSERT(audio_plnk_mic->buf);
            audio_plnk_mic->sclk_io = TCFG_AUDIO_PLNK_SCLK_PIN;
            audio_plnk_mic->ch0_io = TCFG_AUDIO_PLNK_DAT0_PIN;
            audio_plnk_mic->ch1_io = TCFG_AUDIO_PLNK_DAT1_PIN;
            audio_plnk_open(audio_plnk_mic);
            audio_plnk_start(audio_plnk_mic);
        }
    } else {
        audio_plnk_close();
        audio_mic_pwr_ctl(MIC_PWR_OFF);
        if (audio_plnk_mic->buf) {
            free(audio_plnk_mic->buf);
        }
        if (audio_plnk_mic) {
            free(audio_plnk_mic);
            audio_plnk_mic = NULL;
        }
    }
    return 0;
}

/*
*********************************************************************
*                  IIS MIC API
* Description: iis mic操作接口
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
#if TCFG_AUDIO_INPUT_IIS
struct esco_iis_mic_hdl {
    u32 in_sample_rate;
    u32 out_sample_rate;
    u32 sr_cal_timer;
    u32 points_total;
    u32 iis_inbuf_idx;
    short iis_inbuf[3][256];
    cbuffer_t *iis_mic_cbuf;
    RS_STUCT_API *iis_sw_src_api;
    u8 *iis_sw_src_buf;
};
#define IIS_MIC_BUF_SIZE    (2*1024)
struct esco_iis_mic_hdl *esco_iis_mic = NULL;
extern ALINK_PARM  alink_param;
extern void alink_isr_en(u8 en);

s16 temp_iis_input_data[ALNK_BUF_POINTS_NUM * 3];
static void audio_iis_mic_output(s16 *data, u16 len)
{
    int wlen = 0;
    esco_iis_mic->points_total += len >> 1;
    // 双声道转单声道
    for (int i = 0; i < len / 2 / 2; i++) {
        data[i] = data[2 * i];
    }
    len >>= 1;

    if (esco_iis_mic->iis_mic_cbuf) {
        if (esco_iis_mic->iis_sw_src_api && esco_iis_mic->iis_sw_src_buf) {
            len = esco_iis_mic->iis_sw_src_api->run(esco_iis_mic->iis_sw_src_buf, data, len >> 1, temp_iis_input_data);
            len = len << 1;
        }

#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
        int ret = phone_message_mic_write(temp_iis_input_data, len);
        if (ret >= 0) {
            esco_enc_resume();
            return ;
        }
#endif/*TCFG_PHONE_MESSAGE_ENABLE*/

        wlen = cbuf_write(esco_iis_mic->iis_mic_cbuf, temp_iis_input_data, len);
        if (wlen != len) {
            printf("wlen:%d,len:%d\n", wlen, len);
        }
        printf("len %d\n", esco_iis_mic->iis_mic_cbuf->data_len);
        if (esco_iis_mic->iis_mic_cbuf->data_len >= 512) {
            cbuf_read(esco_iis_mic->iis_mic_cbuf, &esco_iis_mic->iis_inbuf[esco_iis_mic->iis_inbuf_idx][0], 512);
            audio_aec_inbuf(&esco_iis_mic->iis_inbuf[esco_iis_mic->iis_inbuf_idx][0], 512);
            if (++esco_iis_mic->iis_inbuf_idx > 2) {
                esco_iis_mic->iis_inbuf_idx = 0;
            }
        }
    }
}

void audio_iis_enc_sr_cal_timer(void *param)
{
    if (esco_iis_mic) {
        esco_iis_mic->in_sample_rate = esco_iis_mic->points_total >> 1;
        esco_iis_mic->points_total = 0;
        printf("e:%d\n", esco_iis_mic->in_sample_rate);
        if (esco_iis_mic->iis_sw_src_api && esco_iis_mic->iis_sw_src_buf) {
            esco_iis_mic->iis_sw_src_api->set_sr(esco_iis_mic->iis_sw_src_buf, esco_iis_mic->in_sample_rate);
        }
    }
}

static int esco_iis_mic_en(u8 en, u16 sr, u16 gain)
{
    if (en) {
        if (esco_iis_mic) {
            printf("esco_iis_mic re-malloc error\n");
            return -1;
        }
        esco_iis_mic = zalloc(sizeof(struct esco_iis_mic_hdl));
        if (esco_iis_mic == NULL) {
            printf("esco iis mic zalloc failed\n");
            return -1;
        }

        if (esco_iis_mic->iis_mic_cbuf == NULL) {
            esco_iis_mic->iis_mic_cbuf = zalloc(sizeof(cbuffer_t) + IIS_MIC_BUF_SIZE);
            if (esco_iis_mic->iis_mic_cbuf) {
                cbuf_init(esco_iis_mic->iis_mic_cbuf, esco_iis_mic->iis_mic_cbuf + 1, IIS_MIC_BUF_SIZE);
            } else {
                printf("iis_mic_cbuf zalloc err !!!!!!!!!!!!!!\n");
                return -1;
            }
        }
        esco_iis_mic->iis_inbuf_idx = 0;
        esco_iis_mic->points_total = 0;

#if TCFG_IIS_MODE/*slave mode*/
        esco_iis_mic->in_sample_rate = TCFG_IIS_SAMPLE_RATE;
        esco_iis_mic->out_sample_rate = sr;
        esco_iis_mic->sr_cal_timer = sys_hi_timer_add(NULL, audio_iis_enc_sr_cal_timer, 1000);
#else /*master mode*/
        esco_iis_mic->in_sample_rate = 625 * 20;
        esco_iis_mic->out_sample_rate = 624 * 20;
#endif/*TCFG_IIS_MODE*/
        extern RS_STUCT_API *get_rs16_context();
        esco_iis_mic->iis_sw_src_api = get_rs16_context();
        printf("iis_sw_src_api:0x%x\n", esco_iis_mic->iis_sw_src_api);
        ASSERT(esco_iis_mic->iis_sw_src_api);
        int iis_sw_src_need_buf = esco_iis_mic->iis_sw_src_api->need_buf();
        printf("iis_sw_src_buf:%d\n", iis_sw_src_need_buf);
        esco_iis_mic->iis_sw_src_buf = zalloc(iis_sw_src_need_buf);
        ASSERT(esco_iis_mic->iis_sw_src_buf);
        RS_PARA_STRUCT rs_para_obj;
        rs_para_obj.nch = 1;
        rs_para_obj.new_insample = esco_iis_mic->in_sample_rate;
        rs_para_obj.new_outsample = esco_iis_mic->out_sample_rate;

        printf("sw src,in = %d,out = %d\n", rs_para_obj.new_insample, rs_para_obj.new_outsample);
        esco_iis_mic->iis_sw_src_api->open(esco_iis_mic->iis_sw_src_buf, &rs_para_obj);

        /*使用wm8978模块做iis输入*/
#if 0
        extern u8 WM8978_Init(u8 dacen, u8 adcen);
        WM8978_Init(0, 1);
#endif
        /* alink_param.sample_rate = sr; */
        alink_init(&alink_param);
        alink_channel_init(TCFG_IIS_INPUT_DATAPORT_SEL, ALINK_DIR_RX, NULL, audio_iis_mic_output);
        alink_start();
    } else {
        if (esco_iis_mic) {
            alink_isr_en(0);
            alink_channel_close(TCFG_IIS_INPUT_DATAPORT_SEL);

            if (esco_iis_mic->sr_cal_timer) {
                sys_hi_timer_del(esco_iis_mic->sr_cal_timer);
            }
            if (esco_iis_mic->iis_sw_src_api) {
                esco_iis_mic->iis_sw_src_api = NULL;
            }
            if (esco_iis_mic->iis_sw_src_buf) {
                free(esco_iis_mic->iis_sw_src_buf);
                esco_iis_mic->iis_sw_src_buf = NULL;
            }
            if (esco_iis_mic->iis_mic_cbuf) {
                free(esco_iis_mic->iis_mic_cbuf);
                esco_iis_mic->iis_mic_cbuf = NULL;
            }
            free(esco_iis_mic);
            esco_iis_mic = NULL;
        }
    }
    return 0;
}
#endif /*TCFG_AUDIO_INPUT_IIS */

/*
*********************************************************************
*                  ADC MIC API
* Description: adc mic操作接口
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
#define ESCO_ADC_BUF_NUM        3	//mic_adc采样buf个数

#if TCFG_SIDETONE_ENABLE
#define ESCO_ADC_IRQ_POINTS     32  //侧音减少采样长度，降低延时
#else
#define ESCO_ADC_IRQ_POINTS     256 //mic_adc采样长度（单位：点数）
#endif

#if TCFG_AUDIO_DUAL_MIC_ENABLE
#define ESCO_ADC_CH			    2	//双mic通话
#else
#define ESCO_ADC_CH			    1	//单mic通话
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
#define ESCO_ADC_BUFS_SIZE      (ESCO_ADC_BUF_NUM * ESCO_ADC_IRQ_POINTS * ESCO_ADC_CH)
struct esco_mic_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    int adc_dump_cnt;
    s16 adc_buf[ESCO_ADC_BUFS_SIZE];    //align 2Bytes
#if (ESCO_ADC_CH == 2)
    s16 tmp_buf[ESCO_ADC_IRQ_POINTS];
#endif/*ESCO_ADC_CH*/
};
static struct esco_mic_hdl *esco_mic = NULL;
void esco_mic_dump_set(u16 dump_cnt)
{
    printf("adc_dump_cnt:%d\n", dump_cnt);
    if (esco_mic) {
        esco_mic->adc_dump_cnt = dump_cnt;
    }
}

/*adc_mic采样输出接口*/
static void adc_mic_output_handler(void *priv, s16 *data, int len)
{
    if (esco_mic) {
#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
        int ret = phone_message_mic_write(data, len);
        if (ret >= 0) {
            esco_enc_resume();
            return ;
        }
#endif/*TCFG_PHONE_MESSAGE_ENABLE*/
        if (esco_mic->adc_dump_cnt) {
            esco_mic->adc_dump_cnt--;
            //printf("[%d]",esco_mic->adc_dump_cnt);
            memset(data, 0, len);
            return;
        }

#if (ESCO_ADC_CH == 2)/*DualMic*/
        s16 *mic0_data = data;
        s16 *mic1_data = esco_mic->tmp_buf;
        s16 *mic1_data_pos = data + (len / 2);
        //printf("mic_data:%x,%x,%d\n",data,mic1_data_pos,len);
        for (u16 i = 0; i < (len >> 1); i++) {
            mic0_data[i] = data[i * 2];
            mic1_data[i] = data[i * 2 + 1];
        }
        memcpy(mic1_data_pos, mic1_data, len);

#if 0 /*debug*/
        static u16 mic_cnt = 0;
        if (mic_cnt++ > 300) {
            putchar('1');
            audio_aec_inbuf(mic1_data_pos, len);
            if (mic_cnt > 600) {
                mic_cnt = 0;
            }
        } else {
            putchar('0');
            audio_aec_inbuf(mic0_data, len);
        }
        return;
#endif/*debug end*/
#if (TCFG_AUDIO_DMS_MIC_MANAGE == DMS_MASTER_MIC0)
        audio_aec_inbuf_ref(mic1_data_pos, len);
        audio_aec_inbuf(data, len);
#else
        audio_aec_inbuf_ref(data, len);
        audio_aec_inbuf(mic1_data_pos, len);
#endif/*TCFG_AUDIO_DMS_MIC_MANAGE*/
#if TCFG_SIDETONE_ENABLE
        audio_sidetone_inbuf(data, len);
#endif/*TCFG_SIDETONE_ENABLE*/

#else/*SingleMic*/
        audio_aec_inbuf(data, len);
#if TCFG_SIDETONE_ENABLE
        audio_sidetone_inbuf(data, len);
#endif/*TCFG_SIDETONE_ENABLE*/
#endif/*ESCO_ADC_CH*/
    }
}
extern void dmic_io_mux_ctl(u8 en, u8 sclk_sel);
static int esco_mic_en(u8 en, u16 sr, u16 mic0_gain, u16 mic1_gain)
{
    printf("esco_mic_en:%d\n", en);
    if (en) {
#if TCFG_SIDETONE_ENABLE
        audio_sidetone_open();
#endif/*TCFG_SIDETONE_ENABLE*/
        if (esco_mic) {
            printf("esco_mic re-malloc error\n");
            return -1;
        }
        esco_mic = zalloc(sizeof(struct esco_mic_hdl));
        if (esco_mic == NULL) {
            printf("esco mic zalloc failed\n");
            return -1;
        }
        audio_mic_pwr_ctl(MIC_PWR_ON);
#if TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_DYNAMIC_ADC_GAIN
#if (!TCFG_AUDIO_DUAL_MIC_ENABLE && (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_R)) || \
	(TCFG_AUDIO_DUAL_MIC_ENABLE && (TCFG_AUDIO_DMS_MIC_MANAGE == DMS_MASTER_MIC0))
        anc_dynamic_micgain_start(mic1_gain);
#else
        anc_dynamic_micgain_start(mic0_gain);
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
#endif/*TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_DYNAMIC_ADC_GAIN*/

#if (TCFG_AUDIO_ADC_MIC_CHA == LADC_CH_PLNK)
        printf("pink_2_adc open\n");
        audio_plnk2adc_open(PLNK_CH0_EN, 3000000L);
        adc_data.adc0_sel = AUDIO_ADC_SEL_DMIC0;
        dmic_io_mux_ctl(1, 0);
        audio_adc_mic_open(&esco_mic->mic_ch, TCFG_AUDIO_ADC_MIC_CHA, &adc_hdl);
#else
        esco_mic_dump_set(3);/*处理开mic后在短时间起3次中断影响aec的问题*/
#if (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_L)
        printf("adc_mic0 open,gain:%d\n", mic0_gain);
        audio_adc_mic_open(&esco_mic->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_gain(&esco_mic->mic_ch, mic0_gain);
#endif/*TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_L*/
#if (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_R)
        printf("adc_mic1 open,gain:%d\n", mic1_gain);
        audio_adc_mic1_open(&esco_mic->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic1_set_gain(&esco_mic->mic_ch, mic1_gain);
#endif/*(TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_R)*/
#endif/*TCFG_AUDIO_ADC_MIC_CHA*/
        audio_adc_mic_set_sample_rate(&esco_mic->mic_ch, sr);
        audio_adc_mic_set_buffs(&esco_mic->mic_ch, esco_mic->adc_buf,
                                ESCO_ADC_IRQ_POINTS * 2, ESCO_ADC_BUF_NUM);
        esco_mic->adc_output.handler = adc_mic_output_handler;
        audio_adc_add_output_handler(&adc_hdl, &esco_mic->adc_output);
        audio_adc_mic_start(&esco_mic->mic_ch);
    } else {
        if (esco_mic) {
#if TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_DYNAMIC_ADC_GAIN
            anc_dynamic_micgain_stop();
#endif/*TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_DYNAMIC_ADC_GAIN*/
            audio_adc_mic_close(&esco_mic->mic_ch);
            audio_adc_del_output_handler(&adc_hdl, &esco_mic->adc_output);
#if (TCFG_AUDIO_ADC_MIC_CH == LADC_CH_PLNK)
            audio_plnk2adc_close();
            dmic_io_mux_ctl(0, 0);
#endif/*TCFG_AUDIO_ADC_MIC_CH == LADC_CH_PLNK*/

            audio_mic_pwr_ctl(MIC_PWR_OFF);
            free(esco_mic);
            esco_mic = NULL;
#if TCFG_SIDETONE_ENABLE
            audio_sidetone_close();
#endif/*TCFG_SIDETONE_ENABLE*/
        }
    }
    return 0;
}

int esco_enc_mic_gain_set(u8 gain)
{
    app_var.aec_mic_gain = gain;
    if (esco_mic) {
        audio_adc_mic_set_gain(&esco_mic->mic_ch, app_var.aec_mic_gain);
    }
    return 0;
}

int esco_enc_mic1_gain_set(u8 gain)
{
    app_var.aec_mic1_gain = gain;
    if (esco_mic) {
        audio_adc_mic1_set_gain(&esco_mic->mic_ch, app_var.aec_mic1_gain);
    }
    return 0;
}

struct __encoder_task {
    u8 init_ok;
    atomic_t used;
    OS_MUTEX mutex;
};

static struct __encoder_task enc_task = {0};

int audio_encoder_task_open(void)
{
    local_irq_disable();
    if (enc_task.init_ok == 0) {
        atomic_set(&enc_task.used, 0);
        os_mutex_create(&enc_task.mutex);
        enc_task.init_ok = 1;
    }
    local_irq_enable();

    os_mutex_pend(&enc_task.mutex, 0);
    if (!encode_task) {
        encode_task = zalloc(sizeof(*encode_task));
        audio_encoder_task_create(encode_task, "audio_enc");
    }
    atomic_inc_return(&enc_task.used);
    os_mutex_post(&enc_task.mutex);
    return 0;
}

void audio_encoder_task_close(void)
{
    os_mutex_pend(&enc_task.mutex, 0);
    if (encode_task) {
        if (atomic_dec_and_test(&enc_task.used)) {
            audio_encoder_task_del(encode_task);
            //local_irq_disable();
            free(encode_task);
            encode_task = NULL;
            //local_irq_enable();
        }
    }
    os_mutex_post(&enc_task.mutex);
}


/*
*********************************************************************
*                  SCO/ESCO Encode Open
* Description: 蓝牙通话编码初始化
* Arguments  : coding_type	SCO/ESCO编码类型
*			   frame_len	SCO/ESCO数据包帧长
* Return	 : 0 成功 其他 失败
* Note(s)    : 蓝牙通话开始时调用
*********************************************************************
*/
int esco_enc_open(u32 coding_type, u8 frame_len)
{
    int err;
    struct audio_fmt fmt;

    printf("esco_enc_open: %d,frame_len:%d\n", coding_type, frame_len);

    fmt.channel = 1;
    fmt.frame_len = frame_len;
    if (coding_type == AUDIO_CODING_MSBC) {
        fmt.sample_rate = 16000;
        fmt.coding_type = AUDIO_CODING_MSBC;
    } else if (coding_type == AUDIO_CODING_CVSD) {
        fmt.sample_rate = 8000;
        fmt.coding_type = AUDIO_CODING_CVSD;
    } else {
        /*Unsupoport eSCO Air Mode*/
    }
    audio_encoder_task_open();
    /* if (!encode_task) { */
    /* encode_task = zalloc(sizeof(*encode_task)); */
    /* audio_encoder_task_create(encode_task, "audio_enc"); */
    /* } */
    if (!esco_enc) {
        esco_enc = zalloc(sizeof(*esco_enc));
    }

    audio_encoder_open(&esco_enc->encoder, &esco_enc_input, encode_task);
    audio_encoder_set_handler(&esco_enc->encoder, &esco_enc_handler);
    audio_encoder_set_fmt(&esco_enc->encoder, &fmt);
    audio_encoder_set_event_handler(&esco_enc->encoder, esco_enc_event_handler, 0);
    audio_encoder_set_output_buffs(&esco_enc->encoder, esco_enc->output_frame,
                                   sizeof(esco_enc->output_frame), 1);
    audio_encoder_start(&esco_enc->encoder);

#if TCFG_AUDIO_ANC_ENABLE && (!TCFG_AUDIO_DYNAMIC_ADC_GAIN)
    app_var.aec_mic_gain = anc_mic_gain_get();
#endif/*TCFG_AUDIO_ANC_ENABLE && (!TCFG_AUDIO_DYNAMIC_ADC_GAIN)*/
#if TCFG_AUDIO_ANC_ENABLE && (ANC_HEARAID_EN)
    anc_hearaid_mode_suspend(1);
#endif/*TCFG_AUDIO_ANC_ENABLE && (ANC_HEARAID_EN)*/

    printf("esco sample_rate: %d,mic_gain:%d\n", fmt.sample_rate, app_var.aec_mic_gain);

    /*pdm mic*/
#if (TCFG_AUDIO_ADC_MIC_CHA == PLNK_MIC)
    esco_pdm_mic_en(1, fmt.sample_rate, app_var.aec_mic_gain);
    return 0;
#endif/*TCFG_AUDIO_ADC_MIC_CHA == PLNK_MIC*/

    /*iis mic*/
#if (TCFG_AUDIO_ADC_MIC_CHA == ALNK_MIC)
    esco_iis_mic_en(1, fmt.sample_rate, app_var.aec_mic_gain);
    return 0;
#endif/*TCFG_AUDIO_ADC_MIC_CHA == ALNK_MIC*/


    /*adc mic*/
    esco_mic_en(1, fmt.sample_rate, app_var.aec_mic_gain, app_var.aec_mic1_gain);

    esco_enc->start = 1;
    return 0;
}

int esco_adc_mic_en()
{
    if (esco_enc && esco_enc->start && esco_mic) {
        /* audio_adc_mic_start(&esco_mic->mic_ch); */
        return 0;
    }
    return -1;
}

/*
*********************************************************************
*                  SCO/ESCO Encode Close
* Description: 蓝牙通话编码关闭
* Arguments  : None.
* Return	 : 0 成功 其他 失败
* Note(s)    : 蓝牙通话结束时调用
*********************************************************************
*/
void esco_enc_close(void)
{
    printf("esco_enc_close\n");
    if (!esco_enc) {
        printf("esco_enc NULL\n");
        return;
    }
    esco_enc->start = 0;
#if (TCFG_AUDIO_ADC_MIC_CHA == PLNK_MIC)
    esco_pdm_mic_en(0, 0, 0);
#elif (TCFG_AUDIO_ADC_MIC_CHA == ALNK_MIC)
    esco_iis_mic_en(0, 0, 0);
#else
    esco_mic_en(0, 0, 0, 0);
#endif/*TCFG_AUDIO_ADC_MIC_CH*/

#if TCFG_AUDIO_ANC_ENABLE && (ANC_HEARAID_EN)
    anc_hearaid_mode_suspend(0);
#endif/*TCFG_AUDIO_ANC_ENABLE && (ANC_HEARAID_EN)*/
    audio_encoder_close(&esco_enc->encoder);

    local_irq_disable();
    free(esco_enc);
    esco_enc = NULL;
    local_irq_enable();

    audio_encoder_task_close();
    /* if (encode_task) { */
    /* audio_encoder_task_del(encode_task); */
    /* free(encode_task); */
    /* encode_task = NULL; */
    /* } */
    printf("esco_enc_close ok\n");
}

int esco_enc_get_fmt(struct audio_fmt *pfmt)
{
    if (!esco_enc) {
        return false;
    }
    memcpy(pfmt, &esco_enc->encoder.fmt, sizeof(struct audio_fmt));
    return true;
}

/*
*********************************************************************
*                  Audio Encode Init
* Description: Audio编码数据结构初始化
* Arguments  : None.
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_enc_init(void)
{
    printf("audio_enc_init\n");
    audio_adc_init(&adc_hdl, &adc_data);

#if TCFG_AUDIO_DMS_DUT_ENABLE
    dms_spp_init();
#endif /*TCFG_AUDIO_CVP_DUT_ENABLE*/
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))

#define PCM_ENC2TWS_OUTBUF_LEN				(4 * 1024)
struct pcm2tws_enc_hdl {
    struct audio_encoder encoder;
    OS_SEM pcm_frame_sem;
    s16 output_frame[30];               //align 4Bytes
    int pcm_frame[60];                 //align 4Bytes
    u8 output_buf[PCM_ENC2TWS_OUTBUF_LEN];
    cbuffer_t output_cbuf;
    void (*resume)(void);
    u32 status : 3;
    u32 reserved: 29;
};
struct pcm2tws_enc_hdl *pcm2tws_enc = NULL;

extern void local_tws_start(u32 coding_type, u32 rate);
extern void local_tws_stop(void);
extern int local_tws_resolve(u32 coding_type, u32 rate);
extern int local_tws_output(s16 *data, int len);

void pcm2tws_enc_close();
void pcm2tws_enc_resume(void);

int pcm2tws_enc_output(void *priv, s16 *data, int len)
{
    if (!pcm2tws_enc) {
        return 0;
    }
    u16 wlen = cbuf_write(&pcm2tws_enc->output_cbuf, data, len);
    os_sem_post(&pcm2tws_enc->pcm_frame_sem);
    if (!wlen) {
        /* putchar(','); */
    }
    /* printf("wl:%d ", wlen); */
    pcm2tws_enc_resume();
    return wlen;
}

void pcm2tws_enc_set_resume_handler(void (*resume)(void))
{
    if (pcm2tws_enc) {
        pcm2tws_enc->resume = resume;
    }
}

static void pcm2tws_enc_need_data(void)
{
    if (pcm2tws_enc && pcm2tws_enc->resume) {
        pcm2tws_enc->resume();
    }
}

static int pcm2tws_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int rlen = 0;
    if (encoder == NULL) {
        r_printf("encoder NULL");
    }
    struct pcm2tws_enc_hdl *enc = container_of(encoder, struct pcm2tws_enc_hdl, encoder);

    if (enc == NULL) {
        r_printf("enc NULL");
    }
    os_sem_set(&pcm2tws_enc->pcm_frame_sem, 0);
    /* printf("l:%d", frame_len); */

    do {
        rlen = cbuf_read(&pcm2tws_enc->output_cbuf, enc->pcm_frame, frame_len);
        if (rlen == frame_len) {
            break;
        }
        if (rlen == -EINVAL) {
            return 0;
        }
        if (!pcm2tws_enc->status) {
            return 0;
        }
        pcm2tws_enc_need_data();
        os_sem_pend(&pcm2tws_enc->pcm_frame_sem, 2);
    } while (1);

    *frame = enc->pcm_frame;
    return rlen;
}
static void pcm2tws_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input pcm2tws_enc_input = {
    .fget = pcm2tws_enc_pcm_get,
    .fput = pcm2tws_enc_pcm_put,
};

static int pcm2tws_enc_probe_handler(struct audio_encoder *encoder)
{
    return 0;
}

static int pcm2tws_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    struct pcm2tws_enc_hdl *enc = container_of(encoder, struct pcm2tws_enc_hdl, encoder);
    local_tws_resolve(encoder->fmt.coding_type, encoder->fmt.sample_rate | (encoder->fmt.channel << 16));
    int ret = local_tws_output(frame, len);
    if (!ret) {
        /* putchar('L'); */
    } else {
        /* printf("w:%d ", len);	 */
    }
    return ret;
}

const static struct audio_enc_handler pcm2tws_enc_handler = {
    .enc_probe = pcm2tws_enc_probe_handler,
    .enc_output = pcm2tws_enc_output_handler,
};



int pcm2tws_enc_open(u32 codec_type, u32 info)
{
    int err;
    struct audio_fmt fmt;
    u16 rate = info & 0x0000ffff;
    u16 channel = (info >> 16) & 0x0f;

    printf("pcm2tws_enc_open: %d\n", codec_type);

    fmt.channel = channel;
    fmt.sample_rate = rate;
    fmt.coding_type = codec_type;

    audio_encoder_task_open();
    /* if (!encode_task) { */
    /* encode_task = zalloc(sizeof(*encode_task)); */
    /* audio_encoder_task_create(encode_task, "audio_enc"); */
    /* } */
    if (pcm2tws_enc) {
        pcm2tws_enc_close();
    }
    pcm2tws_enc = zalloc(sizeof(*pcm2tws_enc));

    os_sem_create(&pcm2tws_enc->pcm_frame_sem, 0);

    cbuf_init(&pcm2tws_enc->output_cbuf, pcm2tws_enc->output_buf, PCM_ENC2TWS_OUTBUF_LEN);

    audio_encoder_open(&pcm2tws_enc->encoder, &pcm2tws_enc_input, encode_task);
    audio_encoder_set_handler(&pcm2tws_enc->encoder, &pcm2tws_enc_handler);
    audio_encoder_set_fmt(&pcm2tws_enc->encoder, &fmt);
    audio_encoder_set_output_buffs(&pcm2tws_enc->encoder, pcm2tws_enc->output_frame,
                                   sizeof(pcm2tws_enc->output_frame), 1);

    local_tws_start(pcm2tws_enc->encoder.fmt.coding_type, pcm2tws_enc->encoder.fmt.sample_rate | (pcm2tws_enc->encoder.fmt.channel << 16));

    pcm2tws_enc->status = 1;

    audio_encoder_start(&pcm2tws_enc->encoder);

    printf("sample_rate: %d\n", fmt.sample_rate);

    return 0;
}

void pcm2tws_enc_close()
{
    if (!pcm2tws_enc) {
        return;
    }
    pcm2tws_enc->status = 0;
    printf("pcm2tws_enc_close");
    local_tws_stop();
    audio_encoder_close(&pcm2tws_enc->encoder);
    free(pcm2tws_enc);
    pcm2tws_enc = NULL;
    audio_encoder_task_close();
    /* if (encode_task) { */
    /* audio_encoder_task_del(encode_task); */
    /* free(encode_task); */
    /* encode_task = NULL; */
    /* } */
}

void pcm2tws_enc_resume(void)
{
    if (pcm2tws_enc && pcm2tws_enc->status) {
        audio_encoder_resume(&pcm2tws_enc->encoder);
    }
}

#endif

//////////////////////////////////////////////////////////////////////////////



extern struct audio_dac_hdl dac_hdl;
void audio_dac_open_test()
{
    audio_dac_start(&dac_hdl);
    JL_AUDIO->DAC_CON |= BIT(6);
    JL_AUDIO->DAC_CON |= BIT(5);
    SFR(JL_ADDA->DAA_CON1,  4,  4,  0);         // RG_SEL_11v[3:0]
    SFR(JL_ADDA->DAA_CON1,  0,  4,  10);         // LG_SEL_11v[3:0]  ///////                    spk gain

}


