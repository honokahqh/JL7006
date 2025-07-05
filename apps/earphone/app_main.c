#include "system/includes.h"
/*#include "btcontroller_config.h"*/
#include "btstack/btstack_task.h"
#include "app_config.h"
#include "app_action.h"
#include "asm/pwm_led.h"
#include "tone_player.h"
#include "ui_manage.h"
#include "gpio.h"
#include "app_main.h"
#include "asm/charge.h"
#include "update.h"
#include "app_power_manage.h"
#include "audio_config.h"
#include "app_charge.h"
#include "bt_profile_cfg.h"
#include "dev_manager/dev_manager.h"
#include "update_loader_download.h"
#include "app_task.h"

#ifndef CONFIG_MEDIA_NEW_ENABLE
#ifndef CONFIG_MEDIA_DEVELOP_ENABLE
#include "audio_dec_server.h"
#endif
#endif

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#define LOG_TAG_CONST       APP
#define LOG_TAG             "[APP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"




/*任务列表 */
const struct task_info task_info_table[] = {
    {"app_core",            1,     768,   256  },
    {"sys_event",           7,     256,   0    },
    {"systimer",		    7,	   128,   0	   },
    {"btctrler",            4,     512,   384  },
    {"btencry",             1,     512,   128  },
    {"tws",                 5,     512,   128  },
#if (BT_FOR_APP_EN)
    {"btstack",             3,     1024,  256  },
#else
    {"btstack",             3,     768,   256  },
#endif
    {"audio_dec",           5,     800,   128  },
    /*
     *为了防止dac buf太大，通话一开始一直解码，
     *导致编码输入数据需要很大的缓存，这里提高编码的优先级
     */
    {"audio_enc",           6,     768,   128  },
    {"aec",					2,	   768,   128  },
#if TCFG_AUDIO_HEARING_AID_ENABLE
    {"HearingAid",			6,	   768,   0  },
#endif/*TCFG_AUDIO_HEARING_AID_ENABLE*/

#ifndef CONFIG_256K_FLASH
    {"aec_dbg",				3,	   128,   128  },
    {"update",				1,	   256,   0    },
    {"tws_ota",				2,	   256,   0    },
    {"tws_ota_msg",			2,	   256,   128  },
    {"dw_update",		 	2,	   256,   128  },
    {"rcsp_task",		    2,	   640,	  128  },
#if RCSP_ADV_EN
    {"file_bs",             1,     768,   0   },
#endif
    {"aud_capture",         4,     512,   256  },
    {"data_export",         5,     512,   256  },
    {"anc",                 3,     512,   128  },
    {"anchearaid",        3,     512,   128  },
#endif

#if TCFG_GX8002_NPU_ENABLE
    {"gx8002",              2,     256,   64   },
#endif /* #if TCFG_GX8002_NPU_ENABLE */
#if TCFG_GX8002_ENC_ENABLE
    {"gx8002_enc",          2,     128,   64   },
#endif /* #if TCFG_GX8002_ENC_ENABLE */


#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
    {"kws",                 2,     256,   64   },
#endif /* #if TCFG_KWS_VOICE_RECOGNITION_ENABLE */
    {"usb_msd",           	1,     512,   128   },
#if AI_APP_PROTOCOL
    {"app_proto",           2,     768,   64   },
#endif
#if (TCFG_SPI_LCD_ENABLE||TCFG_SIMPLE_LCD_ENABLE)
    {"ui",           	    2,     768,   256  },
#else
    {"ui",                  3,     384 - 64,   128  },
#endif
#if (TCFG_DEV_MANAGER_ENABLE)
    {"dev_mg",           	3,     512,   512  },
#endif
    {"audio_vad",           1,     512,   128  },
#if TCFG_KEY_TONE_EN
    {"key_tone",            4,     256,   32},
#endif
#if (TCFG_WIRELESS_MIC_ENABLE)
    {"wl_mic_enc",        2,     768,   128  },
#endif
#if (TUYA_DEMO_EN)
    {"tuya",                1,     640,   256},
#endif
    {"dac_task",           2,     256,   128  },
#if (USER_UART_UPDATE_ENABLE)
    {"uart_update",         1,    256,   128  },
#endif
#if TCFG_SIDETONE_ENABLE
    {"sidetone",            3,    256,   0    },
#endif/*TCFG_SIDETONE_ENABLE*/
#if (TCFG_APP_LINEIN_EN && IIS_LINEIN_HW_SRC_EN && (IIS_ROLE == ROLE_S))
    {"iis_linein_hw_src",   1,    512,   128},
#endif
    {0, 0},
};


APP_VAR app_var;

/*
 * 2ms timer中断回调函数
 */
void timer_2ms_handler()
{

}

void app_var_init(void)
{
    memset((u8 *)&bt_user_priv_var, 0, sizeof(BT_USER_PRIV_VAR));
    app_var.play_poweron_tone = 1;

}



void app_earphone_play_voice_file(const char *name);

void clr_wdt(void);

void check_power_on_key(void)
{
    u32 delay_10ms_cnt = 0;

    while (1) {
        clr_wdt();
        os_time_dly(1);

        extern u8 get_power_on_status(void);
        if (get_power_on_status()) {
            log_info("+");
            delay_10ms_cnt++;
#if(TCFG_APP_LINEIN_EN && TCFG_ADC_IIS_ENABLE && IIS_ROLE == ROLE_S)
            if (delay_10ms_cnt > 4) {
#else
            if (delay_10ms_cnt > 70) {
#endif
                return;
            }
        } else {
            log_info("-");
            delay_10ms_cnt = 0;
            log_info("enter softpoweroff\n");
            power_set_soft_poweroff();
        }
    }
}

extern int cpu_reset_by_soft();
extern int audio_dec_init();
extern int audio_enc_init();



__attribute__((weak))
u8 get_charge_online_flag(void)
{
    return 0;
}

/*充电拔出,CPU软件复位, 不检测按键，直接开机*/
static void app_poweron_check(int update)
{
#if (CONFIG_BT_MODE == BT_NORMAL)
    if (!update && cpu_reset_by_soft()) {
        app_var.play_poweron_tone = 0;
        return;
    }

#if TCFG_CHARGE_OFF_POWERON_NE
    if (is_ldo5v_wakeup()) {
        app_var.play_poweron_tone = 1;
        return;
    }
#endif
//#ifdef CONFIG_RELEASE_ENABLE
#if TCFG_POWER_ON_NEED_KEY
    check_power_on_key();
#endif
//#endif

#endif
}

extern u32 timer_get_ms(void);
void app_main()
{
    int update = 0;
    u32 addr = 0, size = 0;
    struct intent it;


    log_info("app_main\n");
    app_var.start_time = timer_get_ms();

#if (defined(CONFIG_MEDIA_NEW_ENABLE) || (defined(CONFIG_MEDIA_DEVELOP_ENABLE)))
    /*解码器*/
    audio_enc_init();
    audio_dec_init();
#endif

#ifdef BT_DUT_INTERFERE
    void audio_demo(void);
    audio_demo();
#endif
#ifdef BT_DUT_ADC_INTERFERE
#include "asm/audio_adc.h"
    audio_adc_mic_demo_open(LADC_CH_MIC_L, 10, 16000, 0);
#endif

    if (!UPDATE_SUPPORT_DEV_IS_NULL()) {
        update = update_result_deal();
    }

    app_var_init();

#if TCFG_MC_BIAS_AUTO_ADJUST
    mc_trim_init(update);
#endif/*TCFG_MC_BIAS_AUTO_ADJUST*/

    if (get_charge_online_flag()) {

#if(TCFG_SYS_LVD_EN == 1)
        vbat_check_init();
#endif

        init_intent(&it);
        it.name = "idle";
        it.action = ACTION_IDLE_MAIN;
        start_app(&it);
    } else {
        check_power_on_voltage();

        app_poweron_check(update);

        ui_manage_init();
        ui_update_status(STATUS_POWERON);

#if TCFG_WIRELESS_MIC_ENABLE
        extern void wireless_mic_main_run(void);
        wireless_mic_main_run();
#endif

#if  TCFG_ENTER_PC_MODE
        init_intent(&it);
        it.name = "pc";
        it.action = ACTION_PC_MAIN;
        start_app(&it);
#elif TCFG_ENTER_HEARING_AID_MODE
        init_intent(&it);
        it.name = "hearing_aid";
        it.action = ACTION_HEARING_AID_MAIN;
        start_app(&it);
#elif TCFG_ADC_IIS_ENABLE
        init_intent(&it);
        it.name = "linein";
        it.action = ACTION_LINEIN_MAIN;
        start_app(&it);
        app_curr_task = APP_LINEIN_TASK;
#else
        init_intent(&it);
        it.name = "earphone";
        it.action = ACTION_EARPHONE_MAIN;
        start_app(&it);
#endif
    }

#if (TCFG_USB_CDC_BACKGROUND_RUN && !TCFG_PC_ENABLE)
    usb_cdc_background_run();
#endif
}

int __attribute__((weak)) eSystemConfirmStopStatus(void)
{
    /* 系统进入在未来时间里，无任务超时唤醒，可根据用户选择系统停止，或者系统定时唤醒(100ms)，或自己指定唤醒时间 */
    //1:Endless Sleep
    //0:100 ms wakeup
    //other: x ms wakeup
    if (get_charge_full_flag()) {
        /* log_i("Endless Sleep"); */
        power_set_soft_poweroff();
        return 1;
    } else {
        /* log_i("100 ms wakeup"); */
        return 0;
    }
}

__attribute__((used)) int *__errno()
{
    static int err;
    return &err;
}



