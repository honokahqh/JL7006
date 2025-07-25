#include "key_event_deal.h"
#include "btstack/avctp_user.h"
#include "event.h"
#include "app_power_manage.h"
#include "app_main.h"
#include "tone_player.h"
#include "audio_config.h"
#include "user_cfg.h"
#include "system/timer.h"
#include "vol_sync.h"
#include "media/includes.h"
#include "pbg_user.h"
#if TCFG_EQ_ENABLE
#include "application/eq_config.h"
#endif/*TCFG_EQ_ENABLE*/
#include "earphone.h"
#include "le_trans_data.h"
#include "dual_pwm_driver.h"


#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif/*TCFG_AUDIO_ANC_ENABLE*/

#define LOG_TAG_CONST       KEY_EVENT_DEAL
#define LOG_TAG             "[KEY_EVENT_DEAL]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define POWER_OFF_CNT       10

#define OPEN_SIRI "{\"events\":[{\"openSIRI\":\"true\"}]}"

static u8 goto_poweroff_cnt = 0;
static u8 goto_poweroff_flag = 0;
u8 goto_reset_flag = 0;

extern u8 key_table[KEY_NUM_MAX][KEY_EVENT_MAX];
extern u8 get_max_sys_vol(void);
extern void sys_enter_soft_poweroff(void *priv);
extern bool get_tws_sibling_connect_state(void);
extern int bt_get_low_latency_mode();
extern void bt_set_low_latency_mode(int enable);
extern u8 tone_isCN;

u8 poweroff_sametime_flag = 0;

#if CONFIG_TWS_POWEROFF_SAME_TIME
enum {
    FROM_HOST_POWEROFF_CNT_ENOUGH = 1,
    FROM_TWS_POWEROFF_CNT_ENOUGH,
    POWEROFF_CNT_ENOUGH,
};

static u16 poweroff_sametime_timer_id = 0;
static void poweroff_sametime_timer(void *priv)
{
    log_info("poweroff_sametime_timer\n");
    int state = tws_api_get_tws_state();
    if (!(state & TWS_STA_SIBLING_CONNECTED)) {
        sys_enter_soft_poweroff(NULL);
        return;
    }
    if (priv == NULL || poweroff_sametime_flag == POWEROFF_CNT_ENOUGH) {
        poweroff_sametime_timer_id = sys_timeout_add((void *)1, poweroff_sametime_timer, 500);
    } else {
        poweroff_sametime_timer_id = 0;
    }
}

static void poweroff_sametime_timer_init(void)
{
    if (poweroff_sametime_timer_id) {
        return;
    }

    poweroff_sametime_timer_id = sys_timeout_add(NULL, poweroff_sametime_timer, 500);
}
#endif

void volume_up_down_direct(s8 value);

#if TCFG_USER_TWS_ENABLE
u8 replay_tone_flag = 1;
int max_tone_timer_hdl = 0;
static void max_tone_timer(void *priv)
{
    if (!tone_get_status()) {
        max_tone_timer_hdl = 0;
        replay_tone_flag = 1;
    } else {
        max_tone_timer_hdl = sys_timeout_add(NULL, max_tone_timer, TWS_SYNC_TIME_DO + 100);
    }
}
#endif

static u8 volume_flag = 1;
void volume_up(u8 inc)
{
    u8 test_box_vol_up = 0x41;
    s8 cur_vol = 0;
    u8 call_status = get_call_status();

    if (tone_get_status() && volume_flag) {
        if (get_call_status() == BT_CALL_INCOMING) {
            volume_up_down_direct(1);
        }
        return;
    }

    /*打电话出去彩铃要可以调音量大小*/
    if ((call_status == BT_CALL_ACTIVE) || (call_status == BT_CALL_OUTGOING)) {
        cur_vol = app_audio_get_volume(APP_AUDIO_STATE_CALL);
    } else {
        cur_vol = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    }
    if (get_remote_test_flag()) {
        user_send_cmd_prepare(USER_CTRL_TEST_KEY, 1, &test_box_vol_up); //音量加
    }

    if (cur_vol >= app_audio_get_max_volume()) {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state()) {
            if (tws_api_get_role() == TWS_ROLE_MASTER && replay_tone_flag) {
                replay_tone_flag = 0;               //防止提示音被打断标志
                bt_tws_play_tone_at_same_time(SYNC_TONE_MAX_VOL, 400);
                max_tone_timer_hdl = sys_timeout_add(NULL, max_tone_timer, TWS_SYNC_TIME_DO + 100);  //同步在TWS_SYNC_TIME_DO之后才会播放提示音，所以timer需要在这个时间之后才去检测提示音状态
            }
        } else
#endif
        {
#if TCFG_MAX_VOL_PROMPT
            STATUS *p_tone = get_tone_config();
            tone_play_index(p_tone->max_vol, 0);
#endif
        }

        if (get_call_status() != BT_CALL_HANGUP) {
            /*本地音量最大，如果手机音量还没最大，继续加，以防显示不同步*/
            if (bt_user_priv_var.phone_vol < 15) {
                if (get_curr_channel_state() & HID_CH) {
                    user_send_cmd_prepare(USER_CTRL_HID_VOL_UP, 0, NULL);
                } else {
                    user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_UP, 0, NULL);
                }
            }
            return;
        }
#if BT_SUPPORT_MUSIC_VOL_SYNC
        opid_play_vol_sync_fun(&app_var.music_volume, 1);
        user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_INC, 0, NULL);
#endif/*BT_SUPPORT_MUSIC_VOL_SYNC*/
        return;
    }

#if BT_SUPPORT_MUSIC_VOL_SYNC
    opid_play_vol_sync_fun(&app_var.music_volume, 1);
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_var.music_volume, 1);
#else
    app_audio_volume_up(inc);
#endif/*BT_SUPPORT_MUSIC_VOL_SYNC*/
    log_info("vol+: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
    if (get_call_status() != BT_CALL_HANGUP) {
        if (get_curr_channel_state() & HID_CH) {
            user_send_cmd_prepare(USER_CTRL_HID_VOL_UP, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_UP, 0, NULL);
        }
    } else {
#if BT_SUPPORT_MUSIC_VOL_SYNC
        /* opid_play_vol_sync_fun(&app_var.music_volume, 1); */

#if TCFG_USER_TWS_ENABLE
        user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_INC, 0, NULL);     //使用HID调音量
        //user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_SEND_VOL, 0, NULL);
#else
        user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_INC, 0, NULL);
#endif/*TCFG_USER_TWS_ENABLE*/
#endif/*BT_SUPPORT_MUSIC_VOL_SYNC*/
    }
}
void volume_down(u8 dec)
{
    u8 test_box_vol_down = 0x42;
    if (tone_get_status() && volume_flag) {
        if (get_call_status() == BT_CALL_INCOMING) {
            volume_up_down_direct(-1);
        }
        return;
    }
    if (get_remote_test_flag()) {
        user_send_cmd_prepare(USER_CTRL_TEST_KEY, 1, &test_box_vol_down); //音量减
    }

    if (app_audio_get_volume(APP_AUDIO_CURRENT_STATE) <= 0) {
        if (get_call_status() != BT_CALL_HANGUP) {
            /*
             *本地音量最小，如果手机音量还没最小，继续减
             *注意：有些手机通话最小音量是1(GREE G0245D)
             */
            if (bt_user_priv_var.phone_vol > 1) {
                if (get_curr_channel_state() & HID_CH) {
                    user_send_cmd_prepare(USER_CTRL_HID_VOL_DOWN, 0, NULL);
                } else {
                    user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_DOWN, 0, NULL);

                }
            }
            return;
        }
#if BT_SUPPORT_MUSIC_VOL_SYNC
        opid_play_vol_sync_fun(&app_var.music_volume, 0);
        user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_DEC, 0, NULL);
#endif
        return;
    }

#if BT_SUPPORT_MUSIC_VOL_SYNC
    opid_play_vol_sync_fun(&app_var.music_volume, 0);
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_var.music_volume, 1);
#else
    app_audio_volume_down(dec);
#endif/*BT_SUPPORT_MUSIC_VOL_SYNC*/
    log_info("vol-: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
    if (get_call_status() != BT_CALL_HANGUP) {
        if (get_curr_channel_state() & HID_CH) {
            user_send_cmd_prepare(USER_CTRL_HID_VOL_DOWN, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_DOWN, 0, NULL);
        }
    } else {
#if BT_SUPPORT_MUSIC_VOL_SYNC
        /* opid_play_vol_sync_fun(&app_var.music_volume, 0); */
        if (app_audio_get_volume(APP_AUDIO_CURRENT_STATE) == 0) {
            app_audio_volume_down(0);
        }

#if TCFG_USER_TWS_ENABLE
        user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_DEC, 0, NULL);
        //user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_SEND_VOL, 0, NULL);
#else
        user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_DEC, 0, NULL);
#endif

#endif
    }
}



#if ONE_KEY_CTL_DIFF_FUNC
static void lr_diff_otp_deal(u8 opt, char channel)
{
    log_info("lr_diff_otp_deal:%d", opt);
    switch (opt) {
    case ONE_KEY_CTL_NEXT_PREV:
        if (channel == 'L') {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        } else if (channel == 'R') {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        }
        break;
    case ONE_KEY_CTL_VOL_UP_DOWN:
        if (channel == 'L') {
            volume_up(1);
        } else if (channel == 'R') {
            volume_down(1);
        }
        break;
    default:
        break;
    }
}

void key_tws_lr_diff_deal(struct sys_event *event, u8 opt)
{
    u8 channel = 'U';
    if (get_bt_tws_connect_status()) {
        channel = tws_api_get_local_channel();
        if ('L' == channel) {
            channel = (u32)event->arg == KEY_EVENT_FROM_TWS ? 'R' : 'L';
        } else {
            channel = (u32)event->arg == KEY_EVENT_FROM_TWS ? 'L' : 'R';
        }
    }
    lr_diff_otp_deal(opt, channel);
}
#else
void key_tws_lr_diff_deal(struct sys_event *event, u8 opt)
{
}
#endif


void audio_aec_pitch_change_ctrl();
void audio_surround_voice_ctrl();
extern void start_streamer_test(void);

// KEY_RESET音频播放完成回调函数
static void key_reset_tone_play_end_callback(void *priv)
{
    if (get_tws_phone_connect_state()) {
        goto_reset_flag = 1;
        user_send_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
    } else {
        user_send_cmd_prepare(USER_CTRL_DEL_ALL_REMOTE_INFO, 0, NULL);

        if (get_tws_sibling_connect_state()) {
            extern void tws_conn_remove_pairs(void);
            tws_conn_remove_pairs();
        } else {
            bt_tws_remove_pairs();
        }
        extern u8 goto_reboot_flag;
        goto_reboot_flag = 1;
    }
    
}
int app_earphone_key_event_handler(struct sys_event *event)
{
    static VolUp_count = 0;
    int ret = false;
    struct key_event *key = &event->u.key;

    u8 key_event;

    if (key->type == KEY_DRIVER_TYPE_VOICE) {
        /* 语音消息 */
        ret = jl_kws_voice_event_handle(event);
        if (ret == true) {
            return ret;
        }
    }
#if TCFG_APP_MUSIC_EN
    if (event->arg == DEVICE_EVENT_FROM_CUSTOM) {
        log_e("is music mode msg\n");
        return false;
    }
#endif

#if (TCFG_EAR_DETECT_ENABLE && TCFG_EAR_DETECT_CTL_KEY)
    extern int cmd_key_msg_handle(struct sys_event * event);
    if (key->event == KEY_EVENT_USER) {
        cmd_key_msg_handle(event);
        return ret;
    }
#endif

#if TCFG_USER_TWS_ENABLE
    if (pbg_user_key_vaild(&key_event, event)) {
        ;
    } else
#endif
    {
        key_event = key_table[key->value][key->event];
    }

    void bt_sniff_ready_clean(void);
    bt_sniff_ready_clean();

    EARPHONE_CUSTOM_EARPHONE_KEY_REMAP(event, key, &key_event);
#if RCSP_ADV_EN
    extern void set_key_event_by_rcsp_info(struct sys_event * event, u8 * key_event);
    set_key_event_by_rcsp_info(event, &key_event);
#endif

    log_info("key_event:%d %d %d\n", key_event, key->value, key->event);

#if LL_SYNC_EN
    extern void ll_sync_led_switch(void);
    if (key->value == 0 && key->event == KEY_EVENT_CLICK) {
        log_info("ll_sync_led_switch\n");
        ll_sync_led_switch();
        return 0;
    }
#endif

    switch (key_event) {
#if TCFG_APP_LINEIN_EN
    case KEY_MODE_SWITCH:
        app_task_switch_next();
        break;
#endif
    case  KEY_MUSIC_PP:
        /* void test_esco_role_switch(u8 flag); */
        /* if (tws_api_get_role() == TWS_ROLE_MASTER) { */
        /* test_esco_role_switch(1); */
        /* } */
        /* break; */
#ifdef CONFIG_BOARD_AC700N_TEST
        tone_play_index(IDEX_TONE_BT_MODE, 0);
        mem_stats();
#endif
        /* tone_play_index(IDEX_TONE_NORMAL,1);
        break; */

        /*start_streamer_test();
        break;*/

#if TCFG_AUDIO_BASS_BOOST_TEST
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            //log_info("from tws\n");
            break;
        } else {
            //log_info("from key\n");
        }
        extern void audio_bass_ctrl();
        audio_bass_ctrl();
        break;
#endif/*TCFG_AUDIO_BASS_BOOST_TEST*/


        if ((get_call_status() == BT_CALL_ACTIVE) ||
            (get_call_status() == BT_CALL_INCOMING)) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
        break;

    case  KEY_POWEROFF:
        goto_poweroff_cnt = 0;
        poweroff_sametime_flag = 0;
        goto_poweroff_flag = 0;

        if ((BT_STATUS_CONNECTING == get_bt_connect_status()) ||
            (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) ||
            (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) {
            /* if (get_call_status() != BT_CALL_HANGUP) {
                log_info("call hangup\n");
                user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
                goto_poweroff_flag = 0;
                break;
            } */
            if ((get_call_status() == BT_CALL_INCOMING) ||
                (get_call_status() == BT_CALL_OUTGOING)) {
                log_info("key call reject\n");
                user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
                goto_poweroff_flag = 0;
                break;
            } else if (get_call_status() == BT_CALL_ACTIVE) {
                log_info("key call hangup\n");
                user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
                goto_poweroff_flag = 0;
                break;
            }
        }

        goto_poweroff_flag = 1;
        user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
        break;
    case  KEY_POWEROFF_HOLD:
        log_info("poweroff flag:%d cnt:%d\n", goto_poweroff_flag, goto_poweroff_cnt);

        if (goto_poweroff_flag) {
            goto_poweroff_cnt++;
            if (goto_poweroff_cnt == POWER_OFF_CNT) {
                if (get_tws_sibling_connect_state()) {
                    if ((u32)event->arg != KEY_EVENT_FROM_TWS) {
                        tws_api_sync_call_by_uuid('T', SYNC_CMD_POWER_OFF_TOGETHER, TWS_SYNC_TIME_DO);
                    } else {
                        goto_poweroff_cnt--;
                    }
                } else {
                    sys_enter_soft_poweroff(NULL);
                }
            }
        }
        break;

	case KEY_RESET:
        tone_play_index(IDEX_TONE_CLEAR_ALL, 1);
        sys_timeout_add(NULL, key_reset_tone_play_end_callback, 2000);
        //tone_play_index_with_callback(IDEX_TONE_CLEAR_ALL, 1, key_reset_tone_play_end_callback, NULL);
        break;

    case  KEY_MUSIC_PREV:
        //user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        //============ for test ==============//
#if TCFG_AUDIO_ANC_ENABLE
        if (get_bt_connect_status() == BT_STATUS_TAKEING_PHONE) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else {
            if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
            } else {
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
            }
        }
#else
        if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        }
#endif/*TCFG_AUDIO_ANC_ENABLE*/
        //====================================//
        break;
    case  KEY_MUSIC_NEXT:
#if ( (defined CONFIG_BOARD_AC6933B_LIGHTING) || (defined CONFIG_BOARD_AC6953B_LIGHTING))
        if (get_call_status() == BT_CALL_INCOMING) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
            break;
        }
#endif
        //user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);

        if (get_call_status() == BT_CALL_INCOMING) { // 若呼入则保持并接听
            user_send_cmd_prepare(USER_CTRL_HFP_THREE_WAY_ANSWER2, 0, NULL);
        } else if (get_call_status() == BT_CALL_ACTIVE) { // 若三方通话则保持并切换
            user_send_cmd_prepare(USER_CTRL_HFP_THREE_WAY_ANSWER2, 0, NULL);
        } else { // 根据左右声道 上下首切换
            if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
            }
            log_info("Music next or prev");
            key_tws_lr_diff_deal(event, ONE_KEY_CTL_NEXT_PREV);
            // char channel = bt_tws_get_local_channel();
            // if (channel == 'R' || channel == 'U') {
            //     user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
            // } else {
            //     user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
            // }
        }
        break;
    case  KEY_VOL_UP:
        // VolUp_count++;
        // if (VolUp_count > 10) {
        //     VolUp_count = 0;
        // }
        // set_elecGlass_lightTransmittrance(VolUp_count * 10);
        // log_info("VolUp_count: %d", VolUp_count);
        // break;
        u8 call_status = get_call_status();
        log_info("KEY_VOL_UP: call_status = %d\n", call_status);
        if (call_status == BT_CALL_INCOMING) { // 若呼入则接听
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
            break;
        } else if((call_status == BT_CALL_OUTGOING) ||
                (call_status == BT_CALL_ALERT)) { // 若打出或alert则挂断
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
            break;
        } else {
            log_info("Vol up or Down");
            key_tws_lr_diff_deal(event, ONE_KEY_CTL_VOL_UP_DOWN);
            break;
        }
        break;
    case  KEY_VOL_DOWN:
        volume_down(1);
        break;
    case  KEY_CALL_LAST_NO:
//充电仓不支持 双击回播电话,也不支持 双击配对和取消配对
#if (!TCFG_CHARGESTORE_ENABLE)||(CONFIG_DEVELOPER_MODE==1)
#if TCFG_USER_TWS_ENABLE
        if (bt_tws_start_search_sibling()) {
            tone_play_index(IDEX_TONE_NORMAL, 1);
            break;
        }
#endif
#endif
        if (bt_user_priv_var.last_call_type ==  BT_STATUS_PHONE_INCOME) {
            user_send_cmd_prepare(USER_CTRL_DIAL_NUMBER, bt_user_priv_var.income_phone_len,
                                  bt_user_priv_var.income_phone_num);
        } else {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_LAST_NO, 0, NULL);
        }
        break;
    case  KEY_CALL_HANG_UP:

        break;
    case  KEY_CALL_ANSWER:

        break;
    case  KEY_OPEN_SIRI:
        USER_BLE_SEND_DATA(OPEN_SIRI, sizeof(OPEN_SIRI), true);
        // user_send_cmd_prepare(USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL);
        break;
    case  KEY_EQ_MODE:
#if(TCFG_EQ_ENABLE == 1)
        eq_mode_sw();
#endif
        break;
    case  KEY_HID_CONTROL:
        log_info("get_curr_channel_state:%x\n", get_curr_channel_state());
        if (get_curr_channel_state() & HID_CH) {
            log_info("KEY_HID_CONTROL\n");
            user_send_cmd_prepare(USER_CTRL_HID_IOS, 0, NULL);
        }
        break;

    case KEY_DOUBLE_CLICK:
        log_info("KEY_DOUBLE_CLICK");
        key_tws_lr_diff_deal(event, ONE_KEY_CTL_NEXT_PREV);
        break;

    case KEY_THIRD_CLICK:
        log_info("KEY_THIRD_CLICK");
        key_tws_lr_diff_deal(event, ONE_KEY_CTL_NEXT_PREV);
        break;
    case KEY_LOW_LANTECY:
        bt_set_low_latency_mode(!bt_get_low_latency_mode());
        break;
#if TCFG_EARTCH_EVENT_HANDLE_ENABLE
        extern void eartch_event_deal_enable_cfg_save(u8 en);
    case KEY_EARTCH_ENABLE:
        eartch_event_deal_enable_cfg_save(1);
        break;
    case KEY_EARTCH_DISABLE:
        eartch_event_deal_enable_cfg_save(0);
        break;
#endif /* #if TCFG_EARTCH_EVENT_HANDLE_ENABLE */

#if TCFG_AUDIO_ANC_ENABLE
    case KEY_ANC_SWITCH:
#if RCSP_ADV_EN && RCSP_ADV_KEY_SET_ENABLE
        update_anc_voice_key_opt();
#else
        anc_mode_next();
#endif
        break;
#endif/*TCFG_AUDIO_ANC_ENABLE*/
    case  KEY_NULL:
#if TCFG_USER_TWS_ENABLE
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            break;
        }
#endif
        /* goto_poweroff_cnt = 0; */
        /* goto_poweroff_flag = 0; */
        break;
    case KEY_MUSIC_EFF:
#if TCFG_USER_TWS_ENABLE
#if AUDIO_SURROUND_CONFIG
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            //log_info("from tws\n");
            break;
        } else {
            //log_info("from key\n");
        }
        audio_surround_voice_ctrl();
#endif /* AUDIO_SURROUND_CONFIG */
#endif /*TCFG_USER_TWS_ENABLE*/
        break;

    case KEY_PHONE_PITCH:
#if TCFG_USER_TWS_ENABLE
#if AEC_PITCHSHIFTER_CONFIG
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            //log_info("from tws\n");
            break;
        } else {
            //log_info("from key\n");
        }
        audio_aec_pitch_change_ctrl();
#endif /* AEC_PITCHSHIFTER_CONFIG */
#endif /* TCFG_USER_TWS_ENABLE */
        break;

#if TCFG_AUDIO_HEARING_AID_ENABLE
    /*辅听功能测试demo开关*/
    case KEY_HEARING_AID_TOGGLE:
        printf("KEY_HEARING_AID_TOGGLE\n");
        extern void audio_hearing_aid_demo(void);
        audio_hearing_aid_demo();
        break;
#endif/*TCFG_AUDIO_HEARING_AID_ENABLE*/

#if TCFG_MIC_EFFECT_ENABLE
    case KEY_REVERB_OPEN:
        printf("KEY_REVERB_OPEN:%d\n", KEY_REVERB_OPEN);
        if (get_call_status() == BT_CALL_ACTIVE || get_call_status() == BT_CALL_INCOMING || get_call_status() == BT_CALL_OUTGOING) {
            break;
        }
        if (mic_effect_get_status()) {
            mic_effect_stop();
        } else {
            mic_effect_start();
        }
        break;
#endif/*TCFG_MIC_EFFECT_ENABLE*/
    case KEY_TONE_SWITCH:
        tone_isCN = !tone_isCN;
        printf("KEY_TONE_SWITCH:%d\n", tone_isCN);
        syscfg_write(CFG_USER_TONE, &tone_isCN, 1);
        tone_play_index(IDEX_TONE_TONE_SWITCH, 1);
        tone_cn_sync_to_sibling();
        break;
    }
    return ret;
}

void adsp_volume_up(void)
{
    volume_flag	= 0;
    volume_up(1);
    volume_flag	= 1;
}

void adsp_volume_down(void)
{
    volume_flag = 0;
    volume_down(1);
    volume_flag = 1;
}

