#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "audio_config.h"
#include "app_config.h"
#include "audio_config.h"
#include "app_config.h"
#include "app_main.h"
#include "list.h"
#include "audio_dec/audio_dec.h"
#include "audio_codec_clock.h"
#include "clock_cfg.h"
#include "audio_dec_linein.h"

#if (TCFG_APP_LINEIN_EN && TCFG_LINEIN_PCM_DECODER)

struct pcm_dec_hdl *pcm_dec = NULL;	//pcm 解码句柄，外部想使用时extern

s16 two_ch_data[512 * 3];     //单->双  通道转换buf

extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;
extern struct audio_decoder_task decode_task;
extern struct audio_mixer mixer;
extern void audio_pcm_mono_to_dual(s16 *dual_pcm, s16 *mono_pcm, int points);

/* 函数声明 */
static int linein_pcm_dec_data_read(struct audio_decoder *decoder, void *buf, u32 len);
static int linein_pcm_dec_probe_handler(struct audio_decoder *decoder);
static int linein_pcm_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv);
static linein_pcm_dec_release(void);
static int __linein_pcm_dec_res_close(void);


static const struct audio_dec_input linein_pcm_dec_input = {
    .coding_type = AUDIO_CODING_PCM,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = linein_pcm_dec_data_read,
        }
    }
};

static const struct audio_dec_handler pcm_dec_handler = {
    .dec_probe  = linein_pcm_dec_probe_handler,
    .dec_output = linein_pcm_dec_output_handler,
};

static int linein_pcm_dec_probe_handler(struct audio_decoder *decoder)
{
    return 0;
}

static int linein_pcm_dec_data_read(struct audio_decoder *decoder, void *buf, u32 len)
{
    struct pcm_dec_hdl *dec = container_of(decoder, struct pcm_dec_hdl, decoder);
    int rlen = 0;
    rlen = cbuf_read(&dec->pcm_in_cbuf, buf, PCM_DEC_IN_SIZE);
    if (!rlen) {
        return -1;
    }
    return rlen;
}


static void linein_pcm_dec_set_output_channel(struct pcm_dec_hdl *dec)
{
    u8 dac_conn = audio_dac_get_channel(&dac_hdl);
    if (dac_conn == DAC_OUTPUT_LR) {
        dec->output_ch = 2;
    } else {
        dec->output_ch = 1;
    }
    printf("dec->output_ch = %d\n", dec->output_ch);
}


static int linein_pcm_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    struct pcm_dec_hdl *dec = container_of(decoder, struct pcm_dec_hdl, decoder);
    int wlen = 0;
    if (dec->input_data_channel_num == 2 && dec->output_ch == 2) { //输入双声道数据，需要的是双声道数据
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
    } else if (dec->input_data_channel_num == 2) {
        //双边单处理
    } else if (dec->input_data_channel_num == 1 && dec->output_ch == 2) {	//输入单声道数据，输出双声道数据
        //单变双处理
        linein_pcm_1to2_output_handler(dec, data, len);
    } else {
        //单声道数据，单声道输出
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
    }
    return wlen;
}


int linein_pcm_dec_data_write(s16 *data, int len)
{
    if (!pcm_dec) {
        printf("error: pcm_dec is NULL!\n");
        return 0;
    }
    if (pcm_dec->dec_put_res_flag == 1) {
        pcm_dec->last_dec_put_res_flag = 1;
        return len;
    }
    if (pcm_dec->last_dec_put_res_flag == 1) {
        pcm_dec->last_dec_put_res_flag = 0;
        cbuf_clear(&pcm_dec->pcm_in_cbuf);
    }
    int wlen = cbuf_write(&pcm_dec->pcm_in_cbuf, data, len);
    return wlen;
}


static int linein_pcm_1to2_output_handler(struct pcm_dec_hdl *dec, s16 *data, int len)
{
    int wlen = 0;
    if (!dec->two_ch_remain_len) {
        audio_pcm_mono_to_dual(two_ch_data, data, len >> 1);
        dec->two_ch_remain_addr = two_ch_data;
        dec->two_ch_remain_len = len << 1;
    }
    wlen = audio_mixer_ch_write(&dec->mix_ch, dec->two_ch_remain_addr, dec->two_ch_remain_len);
    dec->two_ch_remain_addr += wlen;
    dec->two_ch_remain_len -= wlen;
    return wlen >> 1;
}

static void linein_pcm_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if (!pcm_dec) {
            printf("pcm_dec handle err ");
            break;
        }
        if (pcm_dec->id != argv[1]) {
            printf("pcm_dec id err : 0x%x, 0x%x \n", pcm_dec->id, argv[1]);
            break;
        }
        /* __linein_pcm_dec_res_close(); */
        /* linein_pcm_dec_close(); */
        break;
    }
}

static int linein_pcm_dec_start(void)
{
    printf("linein_pcm_dec_start:\n");
    struct pcm_dec_hdl *dec = pcm_dec;
    struct audio_fmt f = {0};
    struct audio_mixer *p_mixer = &mixer;

    audio_decoder_open(&dec->decoder, &linein_pcm_dec_input, &decode_task);
    audio_decoder_set_handler(&dec->decoder, &pcm_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, linein_pcm_dec_event_handler, dec->id);

    linein_pcm_dec_set_output_channel(dec);

    //设置解码格式
    f.coding_type = AUDIO_CODING_PCM;
    f.channel = dec->channel;
    f.sample_rate = dec->sample_rate;
    audio_decoder_set_fmt(&dec->decoder, &f);

    //设置叠加功能
    audio_mixer_ch_open(&dec->mix_ch, p_mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, dec->sample_rate);
    audio_mixer_ch_set_resume_handler(&dec->mix_ch, (void *)&dec->decoder, (void (*)(void *))audio_decoder_resume);

    // 设置音频输出音量
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
    dec->start = 1;
    // 开始解码
    audio_decoder_start(&dec->decoder);
    printf("audio_decoder_start~\n");
    return 0;
}


/* 解码资源等待回调 */
static int linein_pcm_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    printf("pcm_dec_wait_res_handler, event:%d\n", event);
    if (event == AUDIO_RES_GET) {
        printf("-------------------- get res !!");
        // 启动解码
        err = linein_pcm_dec_start();
        pcm_dec->dec_put_res_flag = 0;
    } else if (event == AUDIO_RES_PUT) {
        // 被打断
        printf("-------------------- put res !!");
        pcm_dec->dec_put_res_flag = 1;
        __linein_pcm_dec_res_close();
    }
    return err;
}

/*
 * 打开pcm 解码
 * 参数 input_data_channel_num 意义是输入进来的数据声道数，为1代表单声道数据，为2代表双声道数据
 * 返回值为0表示打开成功
 */
int audio_linein_pcm_dec_open(u16 sample_rate, u8 input_data_channel_num)
{
    struct pcm_dec_hdl *dec;
    if (pcm_dec) {
        printf("pcm decoder already open !!!\n");
        return -1;
    }
    if (input_data_channel_num != 1 && input_data_channel_num != 2) {
        printf("param Error!\n");
        return -1;
    }

    dec = zalloc(sizeof(*dec));
    if (dec) {
        printf("pcm dec open!\n");
    }
    dec->p_in_cbuf_buf = zalloc(PCM_DEC_IN_CBUF_SIZE);
    if (!dec->p_in_cbuf_buf) {
        printf("pcm dec cbuf malloc failer!\n");
        free(dec);
        return -1;
    }
    cbuf_init(&dec->pcm_in_cbuf, dec->p_in_cbuf_buf, PCM_DEC_IN_CBUF_SIZE);

    pcm_dec = dec;

    dec->id = rand32();
    dec->sample_rate = sample_rate;
    dec->coding_type = AUDIO_CODING_PCM;
    if (input_data_channel_num == 1) { //如果输入进来的数据是单声道数据
        dec->input_data_channel_num = 1;
        dec->channel = 2;
    } else if (input_data_channel_num == 2) {	//如果输入进来的数据是双声道数据
        dec->input_data_channel_num = 2;
        dec->channel = 1;
    }
    dec->wait.protect = 0;
    dec->wait.priority = 1;
    dec->wait.preemption = 1;
    dec->wait.handler = linein_pcm_wait_res_handler;

    audio_decoder_task_add_wait(&decode_task, &dec->wait);
    return 0;
}

static int __linein_pcm_dec_res_close(void)
{
    pcm_dec->start = 0;
    audio_decoder_close(&pcm_dec->decoder);
    audio_mixer_ch_close(&pcm_dec->mix_ch);
}

void linein_pcm_dec_close(void)
{
    if (pcm_dec && pcm_dec->start) {
        printf("linein dec close!\n");
        __linein_pcm_dec_res_close();
        linein_pcm_dec_release();
    }
}

static linein_pcm_dec_release(void)
{
    if (pcm_dec) {
        audio_decoder_task_del_wait(&decode_task, &pcm_dec->wait);
        audio_codec_clock_del(AUDIO_PCM_MODE);
        if (pcm_dec->hw_src) {
            audio_hw_src_stop(pcm_dec->hw_src);
            audio_hw_src_close(pcm_dec->hw_src);
            free(pcm_dec->hw_src);
            pcm_dec->hw_src = NULL;
        }
        free(pcm_dec->p_in_cbuf_buf);
        local_irq_disable();
        free(pcm_dec);
        pcm_dec = NULL;
        local_irq_enable();
    }
}

#endif

