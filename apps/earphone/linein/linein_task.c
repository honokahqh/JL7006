#include "system/includes.h"
#include "app_action.h"
#include "app_main.h"
#include "earphone.h"
#include "key_event_deal.h"
#include "audio_config.h"
#include "bt_background.h"
#include "default_event_handler.h"
#include "app_online_cfg.h"
#include "app_task.h"
#include "linein_iis.h"
#include "audio_dec/audio_dec_linein.h"
#include "tone_player.h"

#if TCFG_APP_LINEIN_EN

extern u8 key_table[KEY_NUM_MAX][KEY_EVENT_MAX];
u8 poweron_tone_play_flag = 0;

volatile u32 dig_vol = 16384;

extern struct audio_dac_hdl dac_hdl;
//减少音量
static void inc_iis_linein_vol(void)
{
    dig_vol *= 2;
    if (dig_vol > 16384) {
        dig_vol = 16384;
    }
    printf("vol++, dig_vol = %d\n", dig_vol);
    audio_dac_set_digital_vol(&dac_hdl, dig_vol);
}

static void sub_iis_linein_vol(void)
{
    dig_vol /= 2;
    if (dig_vol < 1) {
        dig_vol = 1;
    }
    printf("vol--, dig_vol = %d\n", dig_vol);
    audio_dac_set_digital_vol(&dac_hdl, dig_vol);
}


static int app_linein_key_event_handler(struct sys_event *event)
{
    struct key_event *key = &event->u.key;
    int key_event = key_table[key->value][key->event];
    int key_value = key->value;

    printf("\n--------- ---- ---- key_value = %d, key_event = %d\n\n", key_value, key_event);
    /* printf("---  KEY_ANC_SWITCH = %d\n\n", KEY_ANC_SWITCH); */

    if (key_value == TCFG_ADKEY_VALUE0) {	//ad按键1被按下
        switch (key_event) {
        case KEY_ANC_SWITCH:
            anc_mode_next();  //anc模式切换
            break;
        case KEY_MODE_SWITCH:
            //通过linein det检测来切入模式
            /* app_task_switch_next();		//模式切换 */
            break;
        case KEY_POWEROFF:
        case KEY_POWEROFF_HOLD:
            app_earphone_key_event_handler(event);
            break;
        }
    } else if (key_value == TCFG_ADKEY_VALUE1) { //按键vol-被按下
        switch (key_event) {
        case KEY_VOL_DOWN:
            sub_iis_linein_vol();
            break;
        }
    } else if (key_value == TCFG_ADKEY_VALUE2) {  //按键vol+被按下
        switch (key_event) {
        case KEY_VOL_UP:
            inc_iis_linein_vol();
            break;
        default:
            break;
        }
    }
    return false;

}

/*
 * 系统事件处理函数
 */
static int event_handler(struct application *app, struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        /*
         * 普通按键消息处理
         */
#if (IIS_ROLE == ROLE_M)
        return app_linein_key_event_handler(event);
#endif
        break;
    case SYS_BT_EVENT:
        /*
         * 蓝牙事件处理
         */
        break;
    case SYS_DEVICE_EVENT:
        /*
         * 系统设备事件处理
         */
#if TCFG_ANC_BOX_ENABLE  //加上这句话，支持串口调试ANC
        if ((u32)event->arg == DEVICE_EVENT_FROM_ANC) {
            return app_ancbox_event_handler(&event->u.ancbox);
        }
#endif
        break;
    default:
        break;
    }
#if (IIS_ROLE == ROLE_M)
    default_event_handler(event);
#endif
    return false;
}

static linein_task_init(void)
{
    sys_key_event_enable();		//按键使能
    tone_table_init();
}

void exit_linein_task()
{
    printf(">>> exit linein task!\n");
#if (IIS_ROLE == ROLE_M)
    audio_adc_iis_master_close();
#else
    audio_adc_iis_slave_close();
#endif
#if (TCFG_LINEIN_PCM_DECODER && (IIS_ROLE == ROLE_M))
    linein_pcm_dec_close();
#endif
}

void poweron_tone_play_callback(void *priv)
{
    if (app_get_curr_task() == APP_LINEIN_TASK) {
        tone_play_index(IDEX_TONE_LINEIN, 1);
    } else if (app_get_curr_task() == APP_BT_TASK) {
        tone_play_index(IDEX_TONE_BT_MODE, 1);
    }
}

//************************************************
//			tone 播放结束回调函数
//************************************************
static void linein_tone_play_end_callback(void *priv)
{
    struct pcm_dec_hdl *dec_hdl = (struct pcm_dec_hdl *)priv;
    if (dec_hdl) {
        dec_hdl->dec_put_res_flag = 0;
    }
    printf("-- tone play end\n");
}


static int state_machine(struct application *app, enum app_state state,
                         struct intent *it)
{
    int err = 0;
#if (IIS_ROLE == ROLE_M)
    u32 linein_det_voltage = adc_get_voltage(LINEIN_DET_CH);
    if (linein_det_voltage >= 2000) {
        //当前没有音频线插入
        return 0;
    }
#endif
    switch (state) {
    case APP_STA_CREATE:
#if (IIS_ROLE == ROLE_M)
        printf(">>> Linein Task!\n");
        linein_task_init();
#endif
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_LINEIN_MAIN:
#if (TCFG_LINEIN_PCM_DECODER && (IIS_ROLE == ROLE_M))
            audio_linein_pcm_dec_open(TCFG_IIS_SAMPLE_RATE, 2);
#endif
#if (IIS_ROLE == ROLE_M)
            if (poweron_tone_play_flag == 0) {
                poweron_tone_play_flag = 1;
                tone_play_index_with_callback(IDEX_TONE_POWER_ON, 1, poweron_tone_play_callback, NULL);
            } else {
                extern struct pcm_dec_hdl *pcm_dec;
                if (pcm_dec) {
                    pcm_dec->dec_put_res_flag = 1;
                }
                tone_play_index_with_callback(IDEX_TONE_LINEIN, 1, linein_tone_play_end_callback, (void *)pcm_dec);
            }
            audio_adc_iis_master_open();
#else
            audio_adc_iis_slave_open();
#endif

#if (IIS_ROLE == ROLE_M)
            /* audio_dac_set_digital_vol(&dac_hdl, dig_vol); */
#endif
            break;
        }
        break;
    case APP_STA_PAUSE:
        break;
    case APP_STA_RESUME:
        break;
    case APP_STA_STOP:
        break;
    case APP_STA_DESTROY:
#if (IIS_ROLE == ROLE_M)
        exit_linein_task();
#endif
        break;
    }
    return err;
}

static const struct application_operation app_linein_ops = {
    .state_machine  = state_machine,
    .event_handler 	= event_handler,
};


/*
 * 注册linein模式
 */
REGISTER_APPLICATION(app_linein) = {
    .name 	= "linein",
    .action	= ACTION_LINEIN_MAIN,
    .ops 	= &app_linein_ops,
    .state  = APP_STA_DESTROY,
};

// 在linein模式下不进入 lowpower
static u8 iis_linein_idle_query()
{
    if (get_curr_task() == APP_LINEIN_TASK) {
        return 0;
    } else {
        return 1;
    }
}

REGISTER_LP_TARGET(iis_linein_lp_target) = {
    .name = "linein",
    .is_idle = iis_linein_idle_query,
};

#endif

