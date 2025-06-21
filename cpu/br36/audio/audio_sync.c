
#include "board_config.h"
#include "asm/dac.h"
#include "media/includes.h"
#include "effectrs_sync.h"
#include "asm/gpio.h"
#include "audio_config.h"

#if 0

extern struct audio_dac_hdl dac_hdl;
extern int bt_media_sync_master(u8 type);
extern u8 bt_media_device_online(u8 dev);
extern void *bt_media_sync_open(void);
extern void bt_media_sync_close(void *);
extern int a2dp_media_get_total_buffer_size();
extern int bt_send_audio_sync_data(void *, void *buf, u32 len);
extern void bt_media_sync_set_handler(void *, void *priv,
                                      void (*event_handler)(void *, int *, int));
extern int bt_audio_sync_nettime_select(u8 basetime);


const static struct audio_tws_conn_ops tws_conn_ops = {
    .open = bt_media_sync_open,
    .set_handler = bt_media_sync_set_handler,
    .close = bt_media_sync_close,
    .master = bt_media_sync_master,
    .online = bt_media_device_online,
    .send = bt_send_audio_sync_data,
};

void *a2dp_audio_sync_open(u8 channel, u32 sample_rate, u32 output_rate, void **resample_ch)
{
    //br36_sh55_2020_1106_1909.tar.gz运行有问题，临时关闭
    /* return NULL; */

    struct audio_wireless_sync_info sync_param;
    void *sync_resample = audio_sync_resample_open();

#if defined(TCFG_AUDIO_DAC_24BIT_MODE) && TCFG_AUDIO_DAC_24BIT_MODE
    sync_param.channel = channel | BIT(7); //使用最高位，控制使能24bit sync src
#else
    sync_param.channel = channel;
#endif
    sync_param.tws_ops = &tws_conn_ops;
    sync_param.sample_rate = sample_rate;
    sync_param.output_rate = output_rate;

    sync_param.protocol = WL_PROTOCOL_RTP;
    sync_param.reset_enable = 0;//内部支持可复位


    sync_param.dev = sync_resample;
    sync_param.target = AUDIO_SYNC_TARGET_DAC;

    bt_audio_sync_nettime_select(0);

    audio_dac_sync_resample_enable(&dac_hdl, sync_resample);

    *resample_ch = sync_resample;

    return audio_wireless_sync_open(&sync_param);
}

void a2dp_audio_sync_close(void *audio_sync, void *resample_ch)
{
    audio_wireless_sync_close(audio_sync);
    audio_dac_sync_resample_disable(&dac_hdl, resample_ch);
    audio_sync_resample_close(resample_ch);
}

void *esco_audio_sync_open(u8 channel, u16 sample_rate, u16 output_rate, void **resample_ch)

{

    struct audio_wireless_sync_info sync_param;

    void *sync_resample = audio_sync_resample_open();
    int esco_buffer_size = 60 * 50;

    sync_param.channel = channel;
    sync_param.sample_rate = sample_rate;
    sync_param.output_rate = output_rate;
    sync_param.tws_ops = &tws_conn_ops;
    sync_param.protocol = WL_PROTOCOL_SCO;
    sync_param.reset_enable = 0;

    sync_param.dev = sync_resample;
    sync_param.target = AUDIO_SYNC_TARGET_DAC;

    bt_audio_sync_nettime_select(0);

    audio_dac_sync_resample_enable(&dac_hdl, sync_resample);

    *resample_ch = sync_resample;

    return audio_wireless_sync_open(&sync_param);
}

void esco_audio_sync_close(void *audio_sync, void *resample_ch)
{
    audio_wireless_sync_close(audio_sync);
    audio_dac_sync_resample_disable(&dac_hdl, resample_ch);
    audio_sync_resample_close(resample_ch);
}

void *local_file_dec_sync_open(u8 channel, u16 sample_rate, u16 output_rate, void **resample_ch)
{
    //br36_sh55_2020_1106_1909.tar.gz运行有问题，临时关闭
    /* return NULL; */

    struct audio_wireless_sync_info sync_param;
    void *sync_resample = audio_sync_resample_open();

    sync_param.channel = channel;
    sync_param.tws_ops = &tws_conn_ops;
    sync_param.sample_rate = sample_rate;
    sync_param.output_rate = output_rate;
    sync_param.protocol = WL_PROTOCOL_FILE;
    sync_param.reset_enable = 0;//内部支持可复位


    sync_param.dev = sync_resample;
    sync_param.target = AUDIO_SYNC_TARGET_DAC;

    bt_audio_sync_nettime_select(1);

    audio_dac_sync_resample_enable(&dac_hdl, sync_resample);

    *resample_ch = sync_resample;

    return audio_wireless_sync_open(&sync_param);
}


void local_file_dec_sync_close(void *audio_sync, void *resample_ch)
{
    audio_wireless_sync_close(audio_sync);
    audio_dac_sync_resample_disable(&dac_hdl, resample_ch);
    audio_sync_resample_close(resample_ch);
}



#endif
