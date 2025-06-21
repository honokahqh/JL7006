#include "app_config.h"
#include "user_cfg.h"
#include "fs.h"
#include "string.h"
#include "system/includes.h"
#include "vm.h"
#include "btcontroller_config.h"
#include "app_main.h"
#include "media/includes.h"
#include "audio_config.h"
#include "asm/pwm_led.h"
#include "aec_user.h"
#include "app_power_manage.h"

#if TCFG_CHARGE_CALIBRATION_ENABLE
#include "asm/charge_calibration.h"
#endif


#define LOG_TAG_CONST       USER_CFG
#define LOG_TAG             "[USER_CFG]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

void lp_winsize_init(struct lp_ws_t *lp);
void bt_max_pwr_set(u8 pwr, u8 pg_pwr, u8 iq_pwr, u8 ble_pwr);
void app_set_sys_vol(s16 vol_l, s16  vol_r);

BT_CONFIG bt_cfg = {
    .edr_name        = {'Y', 'L', '-', 'B', 'R', '3', '0'},
    .mac_addr        = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    .tws_local_addr  = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    .rf_power        = 10,
    .dac_analog_gain = 25,
    .mic_analog_gain = 7,
    .tws_device_indicate = 0x6688,
};


AUDIO_CONFIG audio_cfg = {
    .max_sys_vol    = SYS_MAX_VOL,
    .default_vol    = SYS_DEFAULT_VOL,
    .tone_vol       = SYS_DEFAULT_TONE_VOL,
};

//======================================================================================//
//                                 		BTIF配置项表                               		//
//	参数1: 配置项名字                                			   						//
//	参数2: 配置项需要多少个byte存储														//
//	说明: 配置项ID注册到该表后该配置项将读写于BTIF区域, 其它没有注册到该表       		//
//		  的配置项则默认读写于VM区域.													//
//======================================================================================//
const struct btif_item btif_table[] = {
// 	 	item id 		   	   len   	//
    {CFG_BT_MAC_ADDR, 			6 },
    {CFG_BT_FRE_OFFSET,   		6 },   //测试盒矫正频偏值
#if TCFG_CHARGE_CALIBRATION_ENABLE
    {VM_CHARGE_CALIBRATION,     sizeof(calibration_result) },   //充电电流校准
#endif
    //{CFG_DAC_DTB,   			2 },
    //{CFG_MC_BIAS,   			1 },
    {0, 						0 },   //reserved cfg
};

//============================= VM 区域空间最大值 ======================================//
const int vm_max_size_config = VM_MAX_SIZE_CONFIG; //该宏在app_cfg中配置
//======================================================================================//

struct lp_ws_t lp_winsize = {
    .lrc_ws_inc = 480,      //260
    .lrc_ws_init = 160,
    .bt_osc_ws_inc = 100,
    .bt_osc_ws_init = 140,
    .osc_change_mode = 0,
};
#if CONFIG_TWS_PAIR_BY_BOTH_SIDES
static u8 tws_pair_by_both_sides_en = 0;

void bt_set_pair_code_en(u8 en)
{
    tws_pair_by_both_sides_en = en;
}
#endif
u16 bt_get_tws_device_indicate(u8 *tws_device_indicate)
{
    u16 crc = 0;
#if CONFIG_TWS_DIFF_NAME_NOT_MATCH
    crc = CRC16(bt_get_local_name(), strlen(bt_get_local_name()));
#endif
#if CONFIG_TWS_PAIR_BY_BOTH_SIDES
    if (tws_pair_by_both_sides_en) {
        return bt_cfg.tws_device_indicate + crc;
    } else {
        return bt_cfg.tws_device_indicate + crc + 1;
    }
#else
    return bt_cfg.tws_device_indicate + crc;
#endif
}
const u8 *bt_get_mac_addr()
{
    return bt_cfg.mac_addr;
}

void bt_update_mac_addr(u8 *addr)
{
    memcpy(bt_cfg.mac_addr, addr, 6);
}

static u8 bt_mac_addr_for_testbox[6] = {0};
void bt_get_vm_mac_addr(u8 *addr)
{
#if 0
    //中断不能调用syscfg_read;
    int ret = 0;

    ret = syscfg_read(CFG_BT_MAC_ADDR, addr, 6);
    if ((ret != 6)) {
        syscfg_write(CFG_BT_MAC_ADDR, addr, 6);
    }
#else

    memcpy(addr, bt_mac_addr_for_testbox, 6);
#endif
}

void bt_get_tws_local_addr(u8 *addr)
{
    memcpy(addr, bt_cfg.tws_local_addr, 6);
}

const char *sdk_version_info_get(void)
{
    extern u32 __VERSION_BEGIN;
    char *version_str = ((char *)&__VERSION_BEGIN) + 4;

    return version_str;
}

const char *bt_get_local_name()
{
    return (const char *)(bt_cfg.edr_name);
}

const char *bt_get_pin_code()
{
    return "0000";
}

extern STATUS_CONFIG status_config;
extern struct charge_platform_data charge_data;
/* extern struct dac_platform_data dac_data; */
extern struct adkey_platform_data adkey_data;
extern struct led_platform_data pwm_led_data;
extern struct adc_platform_data adc_data;

extern u8 key_table[KEY_EVENT_MAX][KEY_NUM_MAX];

u8 get_max_sys_vol(void)
{
    return (audio_cfg.max_sys_vol);
}

u8 get_tone_vol(void)
{
    if (!audio_cfg.tone_vol) {
        return (get_max_sys_vol());
    }
    if (audio_cfg.tone_vol > get_max_sys_vol()) {
        return (get_max_sys_vol());
    }

    return (audio_cfg.tone_vol);
}

#define USE_CONFIG_BIN_FILE                  0

#define USE_CONFIG_STATUS_SETTING            1                          //状态设置，包括灯状态和提示音
#define USE_CONFIG_AUDIO_SETTING             USE_CONFIG_BIN_FILE        //音频设置
#define USE_CONFIG_CHARGE_SETTING            USE_CONFIG_BIN_FILE        //充电设置
#define USE_CONFIG_KEY_SETTING               USE_CONFIG_BIN_FILE        //按键消息设置
#define USE_CONFIG_MIC_TYPE_SETTING          USE_CONFIG_BIN_FILE        //MIC类型设置
#define USE_CONFIG_LOWPOWER_V_SETTING        USE_CONFIG_BIN_FILE        //低电提示设置
#define USE_CONFIG_AUTO_OFF_SETTING          USE_CONFIG_BIN_FILE        //自动关机时间设置
#define USE_CONFIG_COMBINE_VOL_SETTING       1					        //联合音量读配置

#define USE_CONFIG_DEBUG_INFO				 0							//打印配置信息用于调试和测试，打开该宏编译后会多出4K

#if USE_CONFIG_DEBUG_INFO
#if USE_CONFIG_STATUS_SETTING
static char *status_c[] = {
    "charge_start",
    "charge_full",
    "power_on",
    "power_off",
    "lowpower",
    "max_volume",
    "phone_in",
    "phone_out",
    "phone_activ",
    "bt_init_ok",
    "bt_connect_ok",
    "bt_disconnect",
    "tws_connect_ok",
    "tws_disconnect"
};
static char *led_c[] = {
    "null",
    "red and blue led all off",
    "red and blue led all on",
    "blue led on",
    "blue led off",
    "blue led flash slowly",
    "blue led flash quickly",
    "blue led flashing twice in 5 seconds",
    "blue led flashing once in 5 seconds",
    "red led on",
    "red led off",
    "red led flash slowly",
    "red led flash quickly",
    "red led flashing twice in 5 seconds",
    "red led flashing once in 5 seconds",
    "red and blue led alternate breathing(quickly)",
    "red and blue led alternate breathing(slowly)",
    "blue led breathing",
    "red led breathing",
    "Alternate breathing",
    "null",
    "red led flashing three times",
    "blue led flashing three times",
};
static char *tone_c[] = {
    "digit 0",
    "digit 1",
    "digit 2",
    "digit 3",
    "digit 4",
    "digit 5",
    "digit 6",
    "digit 7",
    "digit 8",
    "digit 9",
    "BT mode",
    "connected",
    "disconnected",
    "tws_connect_ok",
    "tws_disconnect",
    "low_power",
    "power_off",
    "power_on",
    "ringing",
    "max volume",
    "null"
};
#endif

#if USE_CONFIG_KEY_SETTING
static char *key_s[] = {
    "short_msg",
    "long_msg",
    "hold_msg",
    "up_msg",
    "double_msg",
    "triple_msg",
};
static char *key_c[] = {
    "power_on",
    "power_off",
    "poweroff_hold",
    "music_pp",
    "music_prev",
    "music_next",
    "vol_up",
    "vol_down",
    "call_last_no",
    "call_hang_up",
    "call_answer",
    "open_siri",
    "photograph",
    "null"
};

#endif

#if USE_CONFIG_CHARGE_SETTING
static char *charge_c[] = {
    "charge_config_enable",
    "poweron_charge_enable",
    "full_voltage",
    "full_current",
    "charge_current",
};
#endif
#endif

__BANK_INIT_ENTRY
void cfg_file_parse(u8 idx)
{
    u8 tmp[128] = {0};
    int ret = 0;

    memset(tmp, 0x00, sizeof(tmp));

    /*************************************************************************/
    /*                      CFG READ IN cfg_tools.bin                        */
    /*************************************************************************/

#if USE_CONFIG_DEBUG_INFO
    log_info("*******************************************************");
    log_info("**************cfg file info list begin*****************");
    log_info("*******************************************************");
#endif

    //-----------------------------CFG_COMBINE_VOL----------------------------------//
#if (SYS_VOL_TYPE == VOL_TYPE_AD)
    audio_combined_vol_init(USE_CONFIG_COMBINE_VOL_SETTING);
#endif/*SYS_VOL_TYPE*/

    //-----------------------------CFG_BT_NAME--------------------------------------//
    ret = syscfg_read(CFG_BT_NAME, tmp, 32);
    if (ret < 0) {
        log_info("read bt name err");
    } else if (ret >= LOCAL_NAME_LEN) {
        memset(bt_cfg.edr_name, 0x00, LOCAL_NAME_LEN);
        memcpy(bt_cfg.edr_name, tmp, LOCAL_NAME_LEN);
        bt_cfg.edr_name[LOCAL_NAME_LEN - 1] = 0;
    } else {
        memset(bt_cfg.edr_name, 0x00, LOCAL_NAME_LEN);
        memcpy(bt_cfg.edr_name, tmp, ret);
    }
    /* g_printf("bt name config:%s", bt_cfg.edr_name); */
    log_info("bt name config:%s", bt_cfg.edr_name);

    //-----------------------------CFG_TWS_PAIR_CODE_ID----------------------------//
    ret = syscfg_read(CFG_TWS_PAIR_CODE_ID, &bt_cfg.tws_device_indicate, 2);
    if (ret < 0) {
        log_debug("read pair code err");
        bt_cfg.tws_device_indicate = 0x8888;
    }
    /* g_printf("tws pair code config:"); */
    log_info("tws pair code config:");
    log_info_hexdump(&bt_cfg.tws_device_indicate, 2);

    //-----------------------------CFG_BT_RF_POWER_ID----------------------------//
    ret = syscfg_read(CFG_BT_RF_POWER_ID, &app_var.rf_power, 1);
    if (ret < 0) {
        log_debug("read rf err");
        app_var.rf_power = 10;
    }
    bt_max_pwr_set(app_var.rf_power, 5, 8, 9);//last is ble tx_pwer(0~9)
    /* g_printf("rf config:%d\n", app_var.rf_power); */
    log_info("rf config:%d\n", app_var.rf_power);

    //-----------------------------CFG_AEC_ID------------------------------------//
    log_info("aec config:");
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#if ((defined TCFG_AUDIO_DMS_SEL) && (TCFG_AUDIO_DMS_SEL == DMS_FLEXIBLE))
    log_info("dms_flexible config:");/*双mic话务耳机降噪*/
#if(TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
    log_info("dms_flexible config:");/*双mic话务耳机降噪*/
    DMS_FLEXIBLE_CONFIG aec;
    ret = syscfg_read(CFG_DMS_FLEXIBLE_ID, &aec, sizeof(aec));
#else /*DMS_DNS*/
    log_info("dms_dns_flexible config:");/*双mic话务耳机神经网络降噪*/
    DMS_FLEXIBLE_CONFIG aec;
    ret = syscfg_read(CFG_DMS_DNS_FLEXIBLE_ID, &aec, sizeof(aec));
#endif/*TCFG_AUDIO_CVP_NS_MODE*/
#else /*TCFG_AUDIO_DMS_SEL == DMS_NORMAL*/
#if(TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
    log_info("dms_ans config:");/*双mic传统降噪*/
    AEC_DMS_CONFIG aec;
    ret = syscfg_read(CFG_DMS_ID, &aec, sizeof(aec));
#else
    log_info("dms_dns config:");/*双mic神经网络降噪*/
    AEC_DMS_CONFIG aec;
    ret = syscfg_read(CFG_DMS_DNS_ID, &aec, sizeof(aec));
#endif/*TCFG_AUDIO_CVP_NS_MODE*/
    app_var.enc_degradation = aec.target_signal_degradation;
#endif/*TCFG_AUDIO_DMS_SEL*/
#else/*single mic*/
    AEC_CONFIG aec;
#if(TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
    log_info("sms_ans config:");
    ret = syscfg_read(CFG_AEC_ID, &aec, sizeof(aec));
#else
    log_info("sms_dns config:");
    ret = syscfg_read(CFG_SMS_DNS_ID, &aec, sizeof(aec));
#endif/*TCFG_AUDIO_CVP_NS_MODE*/
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
    //log_info("ret:%d,aec_cfg size:%d\n",ret,sizeof(aec));
    if (ret == sizeof(aec)) {
        log_info("aec cfg read succ\n");
        log_info_hexdump(&aec, sizeof(aec));
        app_var.aec_mic_gain = aec.mic_again;
        app_var.aec_mic1_gain = aec.mic_again;
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#if ((defined TCFG_AUDIO_DMS_SEL) && (TCFG_AUDIO_DMS_SEL == DMS_FLEXIBLE))
        app_var.aec_mic1_gain = aec.mic1_again;
#endif/*TCFG_AUDIO_DMS_SEL*/
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
        app_var.aec_dac_gain = aec.dac_again;
#if AEC_READ_CONFIG
#ifdef CONFIG_MEDIA_ORIGIN_ENABLE
        aec_cfg_fill(&aec);
#endif/*CONFIG_MEDIA_ORIGIN_ENABLE*/
#endif/*AEC_READ_CONFIG*/
    } else {
        log_info("aec cfg read err,use default value\n");
        app_var.aec_mic_gain = 6;
        app_var.aec_mic1_gain = 6;
        app_var.aec_dac_gain = 12;/*初始化默认值要注意不能大于芯片支持最大模拟音量*/
    }
    log_info("aec_cfg mic_gain:%d mic1_gain:%d dac_gain:%d", app_var.aec_mic_gain, app_var.aec_mic1_gain, app_var.aec_dac_gain);

#if TCFG_AUDIO_MIC_ARRAY_TRIM_ENABLE
    ret = syscfg_read(CFG_MIC_ARRAY_TRIM_EN, (u8 *)(&app_var.audio_mic_array_trim_en), sizeof(app_var.audio_mic_array_trim_en));
    if (ret != sizeof(app_var.audio_mic_array_trim_en)) {
        log_info("audio_mic_array_trim_en vm read fail %d, %d!!!", ret, sizeof(app_var.audio_mic_array_trim_en));
    }
    ret = syscfg_read(CFG_MIC_ARRAY_DIFF_CMP_VALUE, (u8 *)(&app_var.audio_mic_array_diff_cmp), sizeof(app_var.audio_mic_array_diff_cmp));
    if (ret != sizeof(app_var.audio_mic_array_diff_cmp)) {
        log_info("audio_mic_array_diff_cmp vm read fail  %d, %d!!!", ret, sizeof(app_var.audio_mic_array_diff_cmp));
    }
    log_info("mic array trim en: %d\n", app_var.audio_mic_array_trim_en);
    log_info("mic_array_diff_cmp:");
    put_float(app_var.audio_mic_array_diff_cmp);
#endif

    //-----------------------------CFG_MIC_TYPE_ID------------------------------------//
#if USE_CONFIG_MIC_TYPE_SETTING
    log_info("mic_type_config:");
    extern int read_mic_type_config(void);
    read_mic_type_config();

#endif

#if TCFG_MC_BIAS_AUTO_ADJUST
    ret = syscfg_read(CFG_MC_BIAS, &adc_data.mic_bias_res, 1);
    log_info("mic_bias_res:%d", adc_data.mic_bias_res);
    if (ret != 1) {
        log_info("mic_bias_adjust NULL");
    }
    u8 mic_ldo_idx;
    ret = syscfg_read(CFG_MIC_LDO_VSEL, &mic_ldo_idx, 1);
    if (ret == 1) {
        adc_data.mic_ldo_vsel = mic_ldo_idx & 0x3;
        log_info("mic_ldo_vsel:%d,%d\n", adc_data.mic_ldo_vsel, mic_ldo_idx);
    } else {
        log_info("mic_ldo_vsel_adjust NULL\n");
    }
#endif

#if USE_CONFIG_STATUS_SETTING
    /* g_printf("status_config:"); */
    log_info("status_config:");
    STATUS_CONFIG *status = (STATUS_CONFIG *)tmp;
    ret = syscfg_read(CFG_UI_TONE_STATUS_ID, status, sizeof(STATUS_CONFIG));
    if (ret > 0) {
        memcpy((u8 *)&status_config, (u8 *)status, sizeof(STATUS_CONFIG));
#if USE_CONFIG_DEBUG_INFO
        log_info("status syncing : %s", status->sw ? "on" : "off");
        u8 *temp_led = &(status->led);
        u8 *temp_tone = &(status->tone);
        for (u8 status_num = 0; status_num < sizeof(status_c) / sizeof(int); status_num++) {
            if (temp_led[status_num] == 0xff) {
                temp_led[status_num] = 0x00;
            }
            if (temp_tone[status_num] == 0xff) {
                temp_tone[status_num] = sizeof(tone_c) / sizeof(int) - 1;
            }
            if ((temp_led[status_num] >= (sizeof(led_c) / sizeof(int))) || (temp_tone[status_num] >= (sizeof(tone_c) / sizeof(int)))) {
                log_error("status config error\n");
                break;
            } else {
                log_info("%s -> led : %s -> tone : %s", status_c[status_num], led_c[temp_led[status_num]], tone_c[temp_tone[status_num]]);
                if (status_num == (sizeof(status_c) / sizeof(int) - 1)) {
                    printf("\n");
                }
            }
        }
#else
        log_info_hexdump(&status_config, sizeof(STATUS_CONFIG));
#endif
    }
#endif

#if USE_CONFIG_AUDIO_SETTING
    /* g_printf("app audio_config:"); */
    log_info("app audio_config:");
    ret = syscfg_read(CFG_AUDIO_ID, (u8 *)&audio_cfg, sizeof(AUDIO_CONFIG));
    if (ret > 0) {
        log_info_hexdump((u8 *)&audio_cfg, sizeof(AUDIO_CONFIG));
#else
    {
#endif
        /* dac_data.max_ana_vol = audio_cfg.max_sys_vol; */
        /* if (dac_data.max_ana_vol > 30) { */
        /* dac_data.max_ana_vol = 30; */
        /* } */

        u8 default_volume = audio_cfg.default_vol;
        s8 music_volume = -1;
#if SYS_DEFAULT_VOL
        default_volume = audio_cfg.default_vol;
#else
        syscfg_read(CFG_SYS_VOL, &default_volume, 1);
        ret = syscfg_read(CFG_MUSIC_VOL, &music_volume, 1);
        if (ret < 0) {
            music_volume = -1;
        }
#endif
        if (default_volume > audio_cfg.max_sys_vol) {
            default_volume = audio_cfg.max_sys_vol;
        }
        if (default_volume == 0) {
            default_volume = audio_cfg.max_sys_vol / 2;
        }

        app_var.music_volume = music_volume == -1 ? default_volume : music_volume;
        app_var.wtone_volume = audio_cfg.tone_vol;
#if TCFG_CALL_USE_DIGITAL_VOLUME
        app_var.call_volume = 15;
#else
        app_var.call_volume = app_var.aec_dac_gain;
#endif
        app_var.opid_play_vol_sync = default_volume * 127 / audio_cfg.max_sys_vol;

        log_info("max vol:%d default vol:%d tone vol:%d", audio_cfg.max_sys_vol, default_volume, audio_cfg.tone_vol);
    }

#if (USE_CONFIG_CHARGE_SETTING) && (TCFG_CHARGE_ENABLE)
    /* g_printf("app charge config:"); */
    printf("\n");
    log_info("charge config:");
    CHARGE_CONFIG *charge = (CHARGE_CONFIG *)tmp;
    ret = syscfg_read(CFG_CHARGE_ID, charge, sizeof(CHARGE_CONFIG));
    if (ret > 0) {
        memcpy((u8 *)&charge_data, (u8 *)charge, sizeof(CHARGE_CONFIG));
#if USE_CONFIG_DEBUG_INFO
        u8 *temp_charge = charge;
        for (u8 index = 0; index < sizeof(CHARGE_CONFIG); index++) {
            log_info("%s: %d", charge_c[index], temp_charge[index]);
        }
#else
        log_info_hexdump(charge, sizeof(CHARGE_CONFIG));
        log_info("sw:%d poweron_en:%d full_v:%d full_mA:%d charge_mA:%d",
                 charge->sw, charge->poweron_en, charge->full_v, charge->full_c, charge->charge_c);
#endif
    }
#endif

#if (USE_CONFIG_KEY_SETTING) && (TCFG_ADKEY_ENABLE || TCFG_IOKEY_ENABLE)
    /* g_printf("app key config:"); */
    printf("\n");
    log_info("valid key num: %d, key msg config:", KEY_NUM);
    KEY_OP *key_msg = (KEY_OP *)tmp;
    ret = syscfg_read(CFG_KEY_MSG_ID, key_msg, sizeof(KEY_OP) * KEY_NUM_MAX);
    if (ret > 0) {
        memcpy(key_table, key_msg, sizeof(KEY_OP) * KEY_NUM);
#if USE_CONFIG_DEBUG_INFO
        u8 *temp_key = key_msg;
        for (u8 key_num = 0; key_num < KEY_NUM; key_num++) {
            log_info("KEY_%d :", key_num);
            for (u8 key_s_num = 0; key_s_num < sizeof(key_s) / sizeof(int); key_s_num++) {
                if (temp_key[key_s_num + (key_num * 6)] == 0xff) {
                    temp_key[key_s_num + (key_num * 6)] = sizeof(key_c) / sizeof(int) - 1;
                }
                if (temp_key[key_s_num + (key_num * 6)] >= (sizeof(key_c) / sizeof(int))) {
                    log_error("key config error\n");
                    break;
                } else {
                    log_info("%s -> %s", key_s[key_s_num], key_c[temp_key[key_s_num + (key_num * 6)]]);
                    if (key_s_num == (sizeof(key_s) / sizeof(int) - 1)) {
                        printf("\n");
                    }
                }
            }
        }
#else
        log_info_hexdump(key_msg, sizeof(KEY_OP) * KEY_NUM);
#endif
    }

    log_info("key_msg:");
    log_info_hexdump((u8 *)key_table, KEY_EVENT_MAX * KEY_NUM_MAX);
#endif


#if USE_CONFIG_LOWPOWER_V_SETTING
    /* g_printf("auto low power config:"); */
    log_info("lowpower voltage config:");
    AUTO_LOWPOWER_V_CONFIG auto_lowpower;
    ret = syscfg_read(CFG_LOWPOWER_V_ID, &auto_lowpower, sizeof(AUTO_LOWPOWER_V_CONFIG));
    if (ret > 0) {
        app_var.warning_tone_v = auto_lowpower.warning_tone_v;
        app_var.poweroff_tone_v = auto_lowpower.poweroff_tone_v;
    }
    log_info("warning_tone_v:%d0mV poweroff_tone_v:%d0mV", app_var.warning_tone_v, app_var.poweroff_tone_v);
#else
    app_var.warning_tone_v = LOW_POWER_WARN_VAL;
    app_var.poweroff_tone_v = LOW_POWER_OFF_VAL;
    log_info("warning_tone_v:%d0mv poweroff_tone_v:%d0mV", app_var.warning_tone_v, app_var.poweroff_tone_v);
#endif

#if USE_CONFIG_AUTO_OFF_SETTING
    /* g_printf("auto off time config:"); */
    log_info("auto off time config:");
    AUTO_OFF_TIME_CONFIG auto_off_time;
    ret = syscfg_read(CFG_AUTO_OFF_TIME_ID, &auto_off_time, sizeof(AUTO_OFF_TIME_CONFIG));
    if (ret > 0) {
        app_var.auto_off_time = auto_off_time.auto_off_time * 60;
    }
    log_info("auto_off_time:%d minutes", app_var.auto_off_time);
#else
    app_var.auto_off_time =  TCFG_AUTO_SHUT_DOWN_TIME;
    log_info("auto_off_time:%d minutes", app_var.auto_off_time);
#endif

    /*************************************************************************/
    /*                      CFG READ IN VM                                   */
    /*************************************************************************/
    u8 mac_buf[6];
    u8 mac_buf_tmp[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    u8 mac_buf_tmp2[6] = {0, 0, 0, 0, 0, 0};
#if TCFG_USER_TWS_ENABLE
    int len = syscfg_read(CFG_TWS_LOCAL_ADDR, bt_cfg.tws_local_addr, 6);
    if (len != 6) {
        get_random_number(bt_cfg.tws_local_addr, 6);
        syscfg_write(CFG_TWS_LOCAL_ADDR, bt_cfg.tws_local_addr, 6);
    }
    log_debug("tws_local_mac:");
    log_info_hexdump(bt_cfg.tws_local_addr, sizeof(bt_cfg.tws_local_addr));

    ret = syscfg_read(CFG_TWS_COMMON_ADDR, mac_buf, 6);
    if (ret != 6 || !memcmp(mac_buf, mac_buf_tmp, 6))
#endif
        do {
            ret = syscfg_read(CFG_BT_MAC_ADDR, mac_buf, 6);
            if ((ret != 6) || !memcmp(mac_buf, mac_buf_tmp, 6) || !memcmp(mac_buf, mac_buf_tmp2, 6)) {
                get_random_number(mac_buf, 6);
                syscfg_write(CFG_BT_MAC_ADDR, mac_buf, 6);
            }
        } while (0);


    syscfg_read(CFG_BT_MAC_ADDR, bt_mac_addr_for_testbox, 6);
    if (!memcmp(bt_mac_addr_for_testbox, mac_buf_tmp, 6)) {
        get_random_number(bt_mac_addr_for_testbox, 6);
        syscfg_write(CFG_BT_MAC_ADDR, bt_mac_addr_for_testbox, 6);
        log_info(">>>init mac addr!!!\n");
    }

    log_info("mac:");
    log_info_hexdump(mac_buf, sizeof(mac_buf));
    memcpy(bt_cfg.mac_addr, mac_buf, 6);

#if (CONFIG_BT_MODE != BT_NORMAL)
    const u8 dut_name[]  = "AC693x_DUT";
    const u8 dut_addr[6] = {0x12, 0x34, 0x56, 0x56, 0x34, 0x12};
    memcpy(bt_cfg.edr_name, dut_name, sizeof(dut_name));
    memcpy(bt_cfg.mac_addr, dut_addr, 6);
#endif

    /*************************************************************************/
    /*                      CFG READ IN isd_config.ini                       */
    /*************************************************************************/
    LRC_CONFIG lrc_cfg;
    ret = syscfg_read(CFG_LRC_ID, &lrc_cfg, sizeof(LRC_CONFIG));
    if (ret > 0) {
        log_info("lrc parameter config:");
        log_info_hexdump(&lrc_cfg, sizeof(LRC_CONFIG));
        lp_winsize.lrc_ws_inc      = lrc_cfg.lrc_ws_inc;
        lp_winsize.lrc_ws_init     = lrc_cfg.lrc_ws_init;
        lp_winsize.bt_osc_ws_inc   = lrc_cfg.btosc_ws_inc;
        lp_winsize.bt_osc_ws_init  = lrc_cfg.btosc_ws_init;
        lp_winsize.osc_change_mode = lrc_cfg.lrc_change_mode;
#if USE_CONFIG_DEBUG_INFO
        log_info("lrc_ws_inc : %d", lp_winsize.lrc_ws_inc);
        log_info("lrc_ws_init : %d", lp_winsize.lrc_ws_init);
        log_info("bt_osc_ws_inc : %d", lp_winsize.bt_osc_ws_inc);
        log_info("bt_osc_ws_init : %d", lp_winsize.bt_osc_ws_init);
        log_info("osc_change_mode : %s", lp_winsize.osc_change_mode ? "on" : "off");
#endif
    }
    /* printf("%d %d %d ",lp_winsize.lrc_ws_inc,lp_winsize.lrc_ws_init,lp_winsize.osc_change_mode); */
    lp_winsize_init(&lp_winsize);

#if USE_CONFIG_DEBUG_INFO
    log_info("*******************************************************");
    log_info("***************cfg file info list end******************");
    log_info("*******************************************************");
#endif
}

extern void lmp_hci_write_local_name(const char *name);
int bt_modify_name(u8 *new_name)
{
    u8 new_len = strlen(new_name);

    if (new_len >= LOCAL_NAME_LEN) {
        new_name[LOCAL_NAME_LEN - 1] = 0;
    }

    if (strcmp(new_name, bt_cfg.edr_name)) {
        syscfg_write(CFG_BT_NAME, new_name, LOCAL_NAME_LEN);
        memcpy(bt_cfg.edr_name, new_name, LOCAL_NAME_LEN);
        lmp_hci_write_local_name(bt_get_local_name());
        log_info("mdy_name sucess\n");
        return 1;
    }
    return 0;
}


