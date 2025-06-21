/*
 ****************************************************************
 *							AUDIO ANC
 * File  : audio_anc.c
 * By    :
 * Notes : ref_mic = 参考mic
 *		   err_mic = 误差mic
 *
 ****************************************************************
 */
#include "system/includes.h"
#include "audio_anc.h"
#include "system/task.h"
#include "timer.h"
#include "asm/clock.h"
#include "asm/power/p33.h"
#include "online_db_deal.h"
#include "app_config.h"
#include "tone_player.h"
#include "audio_adc.h"
#include "audio_enc.h"
#include "audio_anc_coeff.h"
#include "btstack/avctp_user.h"
#include "app_main.h"
#if ANC_MUSIC_DYNAMIC_GAIN_EN
#include "amplitude_statistic.h"
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/*TCFG_USER_TWS_ENABLE*/
#if RCSP_ADV_EN
#include "le_rcsp_adv_module.h"
#endif/*RCSP_ADV_EN*/
#if ANC_HEARAID_HOWLING_DET && ANC_HEARAID_EN
#include "audio_anc_hearing_aid.h"
#endif/*ANC_HEARAID_HOWLING_DET*/
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".anc_user_bss")
#pragma data_seg(".anc_user_data")
#pragma const_seg(".anc_user_const")
#pragma code_seg(".anc_user_code")
#endif/*SUPPORT_MS_EXTENSIONS*/

#if INEAR_ANC_UI
extern u8 inear_tws_ancmode;
#endif/*INEAR_ANC_UI*/

#if TCFG_AUDIO_ANC_ENABLE

#if 1
#define user_anc_log	printf
#else
#define user_anc_log(...)
#endif/*log_en*/


/*************************ANC增益配置******************************/
#define ANC_DAC_GAIN			8			//ANC Speaker增益
#define ANC_REF_MIC_GAIN	 	8			//ANC参考Mic增益
#define ANC_ERR_MIC_GAIN	    9			//ANC误差Mic增益
/*****************************************************************/

static void anc_mix_out_audio_drc_thr(float thr);
static void anc_fade_in_timeout(void *arg);
static void anc_mode_switch_deal(u8 mode);
static void anc_timer_deal(void *priv);
static int anc_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size);
static anc_coeff_t *anc_cfg_test(u8 coeff_en, u8 gains_en);
static void anc_fade_in_timer_add(audio_anc_t *param);
void anc_dmic_io_init(audio_anc_t *param, u8 en);
u8 anc_btspp_train_again(u8 mode, u32 dat);
static void anc_tone_play_and_mode_switch(u8 index, u8 preemption, u8 cb_sel);
void anc_mode_enable_set(u8 mode_enable);
void audio_anc_post_msg_drc(void);
void audio_anc_post_msg_debug(void);
void audio_anc_music_dynamic_gain_process(void);
extern void audio_anc_dac_gain(u8 gain_l, u8 gain_r);
extern void audio_anc_mic_gain(char gain0, char gain1);
extern u8 bt_phone_dec_is_running();
extern u8 bt_media_is_running(void);
#define ESCO_MIC_DUMP_CNT	10
extern void esco_mic_dump_set(u16 dump_cnt);

typedef struct {
    u8 state;		/*ANC状态*/
    u8 mode_num;	/*模式总数*/
    u8 mode_enable;	/*使能的模式*/
    u8 anc_parse_seq;
    u8 suspend;		/*挂起*/
    u8 ui_mode_sel; /*ui菜单降噪模式选择*/
    u8 new_mode;	/*记录模式切换最新的目标模式*/
    u8 last_mode;	/*记录模式切换的上一个模式*/
    u8 dy_mic_ch;			/*动态MIC增益类型*/
    char mic_diff_gain;		/*动态MIC增益差值*/
    u16 ui_mode_sel_timer;
    u16 fade_in_timer;
    u16 fade_gain;
    u16 mic_resume_timer;	/*动态MIC增益恢复定时器id*/
    float drc_ratio;		/*动态MIC增益，对应DRC增益比例*/
    volatile u8 mode_switch_lock;/*且模式锁存，1表示正在切ANC模式*/
    volatile u8 sync_busy;
    audio_anc_t param;

#if ANC_MUSIC_DYNAMIC_GAIN_EN
    char loud_dB;					/*音乐动态增益-当前音乐幅值*/
    s16 loud_nowgain;				/*音乐动态增益-当前增益*/
    s16 loud_targetgain;			/*音乐动态增益-目标增益*/
    u16 loud_timeid;				/*音乐动态增益-定时器ID*/
    LOUDNESS_M_STRUCT loud_hdl;		/*音乐动态增益-操作句柄*/
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/

} anc_t;
static anc_t *anc_hdl = NULL;

const u8 anc_tone_tab[3] = {IDEX_TONE_ANC_OFF, IDEX_TONE_ANC_ON, IDEX_TONE_TRANSPARCNCY};

static void anc_task(void *p)
{
    int res;
    int anc_ret = 0;
    int msg[16];
    u8 cur_anc_mode, index;
    u32 pend_timeout = portMAX_DELAY;
    user_anc_log(">>>ANC TASK<<<\n");
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case ANC_MSG_TRAIN_OPEN:/*启动训练模式*/
                audio_mic_pwr_ctl(MIC_PWR_ON);
                anc_dmic_io_init(&anc_hdl->param, 1);
                user_anc_log("ANC_MSG_TRAIN_OPEN");
                audio_anc_train(&anc_hdl->param, 1);
                os_time_dly(1);
                anc_train_close();
                audio_mic_pwr_ctl(MIC_PWR_OFF);
                break;
            case ANC_MSG_TRAIN_CLOSE:/*训练关闭模式*/
                user_anc_log("ANC_MSG_TRAIN_CLOSE");
                anc_train_close();
                break;
            case ANC_MSG_RUN:
                cur_anc_mode = anc_hdl->param.mode;/*保存当前anc模式，防止切换过程anc_hdl->param.mode发生切换改变*/
                user_anc_log("ANC_MSG_RUN:%s \n", anc_mode_str[cur_anc_mode]);
#if (RCSP_ADV_EN && RCSP_ADV_ANC_VOICE)
                /*APP增益设置*/
                extern u16 rcsp_adv_anc_voice_value_get(u8 mode);
                anc_hdl->param.anc_fade_gain = rcsp_adv_anc_voice_value_get(cur_anc_mode);
#endif/*RCSP_ADV_EN && RCSP_ADV_ANC_VOICE*/
#if ANC_MUSIC_DYNAMIC_GAIN_EN
                audio_anc_music_dynamic_gain_reset(0);	/*音乐动态增益，切模式复位状态*/
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/
#if (TCFG_AUDIO_ADC_MIC_CHA == LADC_CH_PLNK)
                esco_mic_dump_set(ESCO_MIC_DUMP_CNT);
#endif/*LADC_CH_PLNK*/
                if (anc_hdl->state == ANC_STA_INIT) {
                    audio_mic_pwr_ctl(MIC_PWR_ON);
                    anc_dmic_io_init(&anc_hdl->param, 1);
#if ANC_MODE_SYSVDD_EN
                    clock_set_lowest_voltage(SYSVDD_VOL_SEL_105V);	//进入ANC时提高SYSVDD电压
#endif/*ANC_MODE_SYSVDD_EN*/
                    anc_ret = audio_anc_run(&anc_hdl->param, anc_hdl->state);
                    if (anc_ret == 0) {
                        anc_hdl->state = ANC_STA_OPEN;
                    } else {
                        /*
                         *-EPERM(-1):不支持ANC(非差分MIC，或混合馈使用4M的flash)
                         *-EINVAL(-22):参数错误(anc_coeff.bin参数为空, 或不匹配)
                         */
                        user_anc_log("audio_anc open Failed:%d\n", anc_ret);
                        anc_hdl->mode_switch_lock = 0;
                        anc_hdl->state = ANC_STA_INIT;
                        anc_hdl->param.mode = ANC_OFF;
                        audio_mic_pwr_ctl(MIC_PWR_OFF);
                        break;
                    }
                } else {
                    audio_anc_run(&anc_hdl->param, anc_hdl->state);
                    if (cur_anc_mode == ANC_OFF) {
                        anc_hdl->state = ANC_STA_INIT;
                        anc_dmic_io_init(&anc_hdl->param, 0);
                        audio_mic_pwr_ctl(MIC_PWR_OFF);
                    }
                }
                if (cur_anc_mode == ANC_OFF) {
#if ANC_MODE_SYSVDD_EN
                    clock_set_lowest_voltage(SYSVDD_VOL_SEL_084V);	//退出ANC恢复普通模式
#endif/*ANC_MODE_SYSVDD_EN*/
                    /*anc关闭，如果没有连接蓝牙，倒计时进入自动关机*/
                    extern u8 get_total_connect_dev(void);
                    if (get_total_connect_dev() == 0) {    //已经没有设备连接
                        sys_auto_shut_down_enable();
                    }
                }
#if TCFG_AUDIO_DYNAMIC_ADC_GAIN
                if (bt_phone_dec_is_running() && (anc_hdl->state == ANC_STA_OPEN)) {
                    if (anc_hdl->dy_mic_ch != MIC_NULL) {
                        audio_anc_drc_ctrl(&anc_hdl->param, anc_hdl->drc_ratio);
                    } else if (anc_hdl->drc_ratio != 1.0f) {	//开机之后先通话,再切ANC时
#if (!TCFG_AUDIO_DUAL_MIC_ENABLE && (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_R)) || \
	(TCFG_AUDIO_DUAL_MIC_ENABLE && (TCFG_AUDIO_DMS_MIC_MANAGE == DMS_MASTER_MIC0))
                        anc_dynamic_micgain_start(app_var.aec_mic1_gain);
#else
                        anc_dynamic_micgain_start(app_var.aec_mic_gain);
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
                    }
                }
#endif/*TCFG_AUDIO_DYNAMIC_ADC_GAIN*/
#if ANC_HEARAID_HOWLING_DET && ANC_HEARAID_EN
                if (anc_hdl->param.trans_mode_sel == ANC_TRANS_MODE_VOICE_ENHANCE) {
                    if (cur_anc_mode == ANC_TRANSPARENCY) {
                        audio_anc_hearing_aid_howldet_start(&anc_hdl->param);
                    } else {
                        audio_anc_hearing_aid_howldet_stop();
                    }
                }
#endif/*ANC_HEARAID_HOWLING_DET*/
                if (cur_anc_mode == ANC_ON && (anc_hdl->param.lfb_en && anc_hdl->param.rfb_en)) {
                    os_time_dly(40);	//针对头戴式FB，延时过滤哒哒声
                } else if (cur_anc_mode != ANC_OFF) {
                    os_time_dly(6);		//延时过滤哒哒声
                }
                anc_hdl->mode_switch_lock = 0;
                anc_fade_in_timer_add(&anc_hdl->param);
                anc_hdl->last_mode = cur_anc_mode;
                break;
            case ANC_MSG_MODE_SYNC:
                user_anc_log("anc_mode_sync:%d, mode_sel %d", msg[2], msg[3]);
#if ANC_HEARAID_EN
                if (msg[3] == ANC_TRANS_MODE_VOICE_ENHANCE) {
                    anc_hearaid_mode_switch(msg[2], 0);
                } else {
                    anc_mode_switch(msg[2], 0);
                }
#else
                anc_mode_switch(msg[2], 0);
#endif/*ANC_HEARAID_EN*/
                break;
#if TCFG_USER_TWS_ENABLE
            case ANC_MSG_TONE_SYNC:
                user_anc_log("anc_tone_sync_play:%d", msg[2]);
                if (anc_hdl->mode_switch_lock) {
                    user_anc_log("anc mode switch lock\n");
                    break;
                }
                /* anc_hdl->mode_switch_lock = 1; */
                if (msg[2] == SYNC_TONE_ANC_OFF) {
                    anc_hdl->param.trans_mode_sel = ANC_TRANS_MODE_NORMAL;
                    anc_hdl->param.mode = ANC_OFF;
                } else if (msg[2] == SYNC_TONE_ANC_ON) {
                    anc_hdl->param.trans_mode_sel = ANC_TRANS_MODE_NORMAL;
                    anc_hdl->param.mode = ANC_ON;
                } else if (msg[2] == SYNC_TONE_ANC_TRANS) {
                    anc_hdl->param.trans_mode_sel = ANC_TRANS_MODE_NORMAL;
                    anc_hdl->param.mode = ANC_TRANSPARENCY;
#if ANC_HEARAID_EN
                } else if (msg[2] == SYNC_TONE_ANC_HEAR_AID_ON) {
                    anc_hdl->param.mode = ANC_TRANSPARENCY;
                    anc_hdl->param.trans_mode_sel = ANC_TRANS_MODE_VOICE_ENHANCE;
                    index = IDEX_TONE_HEARAID_ON;
                } else if (msg[2] == SYNC_TONE_ANC_HEAR_AID_OFF) {
                    anc_hdl->param.mode = ANC_OFF;
                    anc_hdl->param.trans_mode_sel = ANC_TRANS_MODE_VOICE_ENHANCE;
                    index = IDEX_TONE_HEARAID_OFF;
#endif/*ANC_HEARAID_EN*/
                }
                if (anc_hdl->suspend) {
                    anc_hdl->param.tool_enablebit = 0;
                }
                anc_hdl->new_mode = anc_hdl->param.mode;	//确保准备切的新模式，与最后一个提示音播放的模式一致
                if (anc_hdl->param.trans_mode_sel == ANC_TRANS_MODE_VOICE_ENHANCE) {
                    anc_tone_play_and_mode_switch(index, ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
                } else {
                    anc_tone_play_and_mode_switch(anc_tone_tab[anc_hdl->param.mode - 1], ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
                }
                //anc_mode_switch_deal(anc_hdl->param.mode);
                anc_hdl->sync_busy = 0;
                break;
#endif/*TCFG_USER_TWS_ENABLE*/
            case ANC_MSG_FADE_END:
                break;
            case ANC_MSG_DRC_TIMER:
                audio_anc_drc_process();
                break;
            case ANC_MSG_DEBUG_OUTPUT:
                audio_anc_debug_data_output();
                break;
#if ANC_MUSIC_DYNAMIC_GAIN_EN
            case ANC_MSG_MUSIC_DYN_GAIN:
                audio_anc_music_dynamic_gain_process();
                break;
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/
            }
        } else {
            user_anc_log("res:%d,%d", res, msg[1]);
        }
    }
}

void anc_param_gain_t_printf(u8 cmd)
{
    char *str[2] = {
        "ANC_CFG_READ",
        "ANC_CFG_WRITE"
    };
    anc_gain_param_t *gains = &anc_hdl->param.gains;
    user_anc_log("-------------anc_param anc_gain_t-%s--------------\n", str[cmd - 1]);
    user_anc_log("dac_gain %d\n", gains->dac_gain);
    user_anc_log("ref_mic_gain %d\n", gains->ref_mic_gain);
    user_anc_log("err_mic_gain %d\n", gains->err_mic_gain);
    user_anc_log("cmp_en %d\n",		  gains->cmp_en);
    user_anc_log("drc_en %d\n",  	  gains->drc_en);
    user_anc_log("ahs_en %d\n",		  gains->ahs_en);
    user_anc_log("dcc_sel %d\n",	  gains->dcc_sel);
    user_anc_log("gain_sign 0X%x\n",  gains->gain_sign);
    user_anc_log("noise_lvl 0X%x\n",  gains->noise_lvl);
    user_anc_log("fade_step 0X%x\n",  gains->fade_step);
    user_anc_log("alogm %d\n",		  gains->alogm);
    user_anc_log("trans_alogm %d\n",  gains->trans_alogm);

    user_anc_log("ancl_ffgain %d.%d\n", ((int)(gains->l_ffgain * 100.0)) / 100, ((int)(gains->l_ffgain * 100.0)) % 100);
    user_anc_log("ancl_fbgain %d.%d\n", ((int)(gains->l_fbgain * 100.0)) / 100, ((int)(gains->l_fbgain * 100.0)) % 100);
    user_anc_log("ancl_trans_gain %d.%d\n", ((int)(gains->l_transgain * 100.0)) / 100, ((int)(gains->l_transgain * 100.0)) % 100);
    user_anc_log("ancl_cmpgain %d.%d\n", ((int)(gains->l_cmpgain * 100.0)) / 100, ((int)(gains->l_cmpgain * 100.0)) % 100);
    user_anc_log("ancr_ffgain %d.%d\n", ((int)(gains->r_ffgain * 100.0)) / 100, ((int)(gains->r_ffgain * 100.0)) % 100);
    user_anc_log("ancr_fbgain %d.%d\n", ((int)(gains->r_fbgain * 100.0)) / 100, ((int)(gains->r_fbgain * 100.0)) % 100);
    user_anc_log("ancr_trans_gain %d.%d\n", ((int)(gains->r_transgain * 100.0)) / 100, ((int)(gains->r_transgain * 100.0)) % 100);
    user_anc_log("ancr_cmpgain %d.%d\n", ((int)(gains->r_cmpgain * 100.0)) / 100, ((int)(gains->r_cmpgain * 100.0)) % 100);

    user_anc_log("drcff_zero_det %d\n",    gains->drcff_zero_det);
    user_anc_log("drcff_dat_mode %d\n",    gains->drcff_dat_mode);
    user_anc_log("drcff_lpf_sel %d\n",     gains->drcff_lpf_sel);
    user_anc_log("drcfb_zero_det %d\n",    gains->drcfb_zero_det);
    user_anc_log("drcfb_dat_mode %d\n",    gains->drcfb_dat_mode);
    user_anc_log("drcfb_lpf_sel %d\n",     gains->drcfb_lpf_sel);

    user_anc_log("drcff_low_thr %d\n",     gains->drcff_lthr);
    user_anc_log("drcff_high_thr %d\n",    gains->drcff_hthr);
    user_anc_log("drcff_low_gain %d\n",    gains->drcff_lgain);
    user_anc_log("drcff_high_gain %d\n",   gains->drcff_hgain);
    user_anc_log("drcff_nor_gain %d\n",    gains->drcff_norgain);

    user_anc_log("drcfb_low_thr %d\n",     gains->drcfb_lthr);
    user_anc_log("drcfb_high_thr %d\n",    gains->drcfb_hthr);
    user_anc_log("drcfb_low_gain %d\n",    gains->drcfb_lgain);
    user_anc_log("drcfb_high_gain %d\n",   gains->drcfb_hgain);
    user_anc_log("drcfb_nor_gain %d\n",    gains->drcfb_norgain);

    user_anc_log("drctrans_low_thr %d\n",     gains->drctrans_lthr);
    user_anc_log("drctrans_high_thr %d\n",    gains->drctrans_hthr);
    user_anc_log("drctrans_low_gain %d\n",    gains->drctrans_lgain);
    user_anc_log("drctrans_high_gain %d\n",   gains->drctrans_hgain);
    user_anc_log("drctrans_nor_gain %d\n",    gains->drctrans_norgain);

    user_anc_log("ahs_dly %d\n",     gains->ahs_dly);
    user_anc_log("ahs_tap %d\n",     gains->ahs_tap);
    user_anc_log("ahs_wn_shift %d\n", gains->ahs_wn_shift);
    user_anc_log("ahs_wn_sub %d\n",  gains->ahs_wn_sub);
    user_anc_log("ahs_shift %d\n",   gains->ahs_shift);
    user_anc_log("ahs_u %d\n",       gains->ahs_u);
    user_anc_log("ahs_gain %d\n",    gains->ahs_gain);

    user_anc_log("audio_drc_thr %d/10\n", (int)(gains->audio_drc_thr * 10.0));

    user_anc_log("sw_howl_en %d\n", gains->sw_howl_en);
    user_anc_log("sw_howl_thr %d\n", gains->sw_howl_thr);
    user_anc_log("sw_howl_dither_thr %d\n", gains->sw_howl_dither_thr);
    user_anc_log("sw_howl_gain %d\n", gains->sw_howl_gain);
    user_anc_log("gain_sign2 %d\n", gains->gain_sign2);

    user_anc_log("l_transgain2 %d.%d\n", ((int)(gains->l_transgain2 * 100.0)) / 100, ((int)(gains->l_transgain2 * 100.0)) % 100);
    user_anc_log("r_transgain2 %d.%d\n", ((int)(gains->r_transgain2 * 100.0)) / 100, ((int)(gains->r_transgain2 * 100.0)) % 100);

}

/*ANC配置参数填充*/
void anc_param_fill(u8 cmd, anc_gain_t *cfg)
{
    if (cmd == ANC_CFG_READ) {
        cfg->gains = anc_hdl->param.gains;
    } else if (cmd == ANC_CFG_WRITE) {
        anc_hdl->param.gains = cfg->gains;
        //默认DRC设置为初始值
        anc_hdl->param.gains.drcff_zero_det = 0;
        anc_hdl->param.gains.drcff_dat_mode = 0;//hbit control l_gain, lbit control h_gain
        anc_hdl->param.gains.drcff_lpf_sel  = 0;
        anc_hdl->param.gains.drcfb_zero_det = 0;
        anc_hdl->param.gains.drcfb_dat_mode = 0;//hbit control l_gain, lbit control h_gain
        anc_hdl->param.gains.drcfb_lpf_sel  = 0;
        anc_hdl->param.gains.developer_mode = ANC_DEVELOPER_MODE_EN;
        audio_anc_param_normalize(&anc_hdl->param);
    }
    /* anc_param_gain_t_printf(cmd); */
}

static anc_coeff_t *anc_cfg_test(u8 coeff_en, u8 gains_en)
{
    int i, j;
    user_anc_log("%s start\n", __func__);
    if (coeff_en) {
        u16	enablebit;
        u16 head_offset = 0;
        u8 ch = 1;
        u8 *temp_dat;
        u16 temp_offset = 0;
        u16 anc_mode[2] = { ANC_FF_EN, ANC_FB_EN};
        u16 anc_id[2][5] = {
            {ANC_L_FF_IIR, ANC_L_FB_IIR, ANC_L_CMP_IIR, ANC_L_TRANS_IIR, ANC_L_FIR},
            {ANC_R_FF_IIR, ANC_R_FB_IIR, ANC_R_CMP_IIR, ANC_R_TRANS_IIR, ANC_R_FIR},
        };
        u16 anc_len[5] = {800, 800, 800, 800, 400};
        anc_param_head_t coeff_head;
        anc_hdl->param.enablebit = ANC_FF_EN | ANC_FB_EN;
        anc_hdl->param.ch  = ANC_L_CH;
        anc_hdl->param.write_coeff_size = anc_coeff_size_get(ANC_CFG_WRITE);
        anc_coeff_t *db_coeff = zalloc(anc_hdl->param.write_coeff_size);
        db_coeff->version = ANC_VERSION_BR36;
        db_coeff->cnt = 0;
        enablebit = anc_hdl->param.enablebit;
        for (i = 4; i > -1 ; i--) {
            db_coeff->cnt += (enablebit >> i);
            enablebit &= ~BIT(i);
        }
        if (anc_hdl->param.ch == (ANC_L_CH | ANC_R_CH)) {
            ch = 2;
        }
        db_coeff->cnt *= ch;
        temp_dat = db_coeff->dat + (db_coeff->cnt * 6);	//偏移到实际数据
        if (anc_hdl->param.ch & ANC_L_CH) {
            for (i = 0; i < 2; i++) {
                if (anc_hdl->param.enablebit & anc_mode[i]) {
                    coeff_head.id = anc_id[0][i];
                    coeff_head.offset = temp_offset;
                    coeff_head.len = anc_len[i];
                    temp_offset += coeff_head.len;
                    switch (coeff_head.id) {
                    case ANC_L_FF_IIR:
                    /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                    case ANC_L_FB_IIR:
                    /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                    case ANC_L_CMP_IIR:
                    /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                    case ANC_L_TRANS_IIR:
                        /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                        memcpy(temp_dat + coeff_head.offset, ancl_fb_coef_test, sizeof(ancl_fb_coef_test));
                        break;
                    case ANC_L_FIR:
                        memcpy(temp_dat + coeff_head.offset, fz_coef_test, sizeof(fz_coef_test));
                        break;
                    }
                    user_anc_log("%d,id:%d,offset:%d,len,%d\n", i, coeff_head.id, coeff_head.offset, coeff_head.len);
                    memcpy(db_coeff->dat + head_offset, (u8 *)&coeff_head, 6);
                    head_offset += 6;
                    put_buf(temp_dat + coeff_head.offset, 80);
                }
            }
            user_anc_log("db_coeff->dat head,cnt %d\n", db_coeff->cnt);
            put_buf(db_coeff->dat, db_coeff->cnt * 6);
        }
        if (anc_hdl->param.ch & ANC_R_CH) {
            for (i = 0; i < 5; i++) {
                if (anc_hdl->param.enablebit & anc_mode[i]) {
                    coeff_head.id = anc_id[1][i];
                    coeff_head.offset = temp_offset;
                    coeff_head.len = anc_len[i];
                    temp_offset += coeff_head.len;
                    switch (coeff_head.id) {
                    case ANC_R_FF_IIR:
                    /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                    case ANC_R_FB_IIR:
                    /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                    case ANC_R_CMP_IIR:
                    /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                    case ANC_R_TRANS_IIR:
                        /* ancl_fb_coef_test[0] = 1.000000e+00 + ancl_fb_coef_test[0]; */
                        memcpy(temp_dat + coeff_head.offset, ancl_fb_coef_test, sizeof(ancl_fb_coef_test));
                        break;
                    case ANC_R_FIR:
                        memcpy(temp_dat + coeff_head.offset, fz_coef_test, sizeof(fz_coef_test));
                        break;
                    }
                    user_anc_log("%d,id:%d,offset:%d,len,%d\n", i, coeff_head.id, coeff_head.offset, coeff_head.len);
                    memcpy(db_coeff->dat + head_offset, (u8 *)&coeff_head, 6);
                    head_offset += 6;
                    put_buf(temp_dat + coeff_head.offset, 80);
                }
            }
            user_anc_log("db_coeff->dat head,cnt %d\n", db_coeff->cnt);
            put_buf(db_coeff->dat, db_coeff->cnt * 6);
        }
        return db_coeff;
    }
    if (gains_en) {
    }
    return NULL;
}

int anc_coeff_fill(anc_coeff_t *db_coeff)
{
    int i = 0;
    u8 *temp_dat;
    if (db_coeff) {
        anc_param_head_t coeff_head[db_coeff->cnt];

        user_anc_log("%s cnt %d, coeff_size %d\n", __func__, db_coeff->cnt, anc_hdl->param.coeff_size);
        put_buf(db_coeff->dat, db_coeff->cnt * 6);
        temp_dat = db_coeff->dat + (db_coeff->cnt * 6);

        for (i = 0; i < db_coeff->cnt; i++) {
            memcpy(&coeff_head[i], db_coeff->dat + (i * 6), 6); //结构体为8字节，这里只有6字节
            user_anc_log("id:%d, offset:%d,len:%d\n", coeff_head[i].id, coeff_head[i].offset, coeff_head[i].len);
            switch (coeff_head[i].id) {
            case ANC_L_FF_IIR:
                user_anc_log("ANC_L_FF_IIR\n");
                anc_hdl->param.lff_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_L_FB_IIR:
                user_anc_log("ANC_L_FB_IIR\n");
                anc_hdl->param.lfb_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_L_CMP_IIR:
                user_anc_log("ANC_L_CMP_IIR\n");
                anc_hdl->param.lcmp_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_L_TRANS_IIR:
                user_anc_log("ANC_L_TRANS_IIR\n");
                anc_hdl->param.ltrans_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_L_FIR:
                user_anc_log("ANC_L_FIR\n");
                anc_hdl->param.lfir_coeff = (s32 *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_R_FF_IIR:
                user_anc_log("ANC_R_FF_IIR\n");
                anc_hdl->param.rff_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_R_FB_IIR:
                user_anc_log("ANC_R_FB_IIR\n");
                anc_hdl->param.rfb_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_R_CMP_IIR:
                user_anc_log("ANC_R_CMP_IIR\n");
                anc_hdl->param.rcmp_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_R_TRANS_IIR:
                user_anc_log("ANC_R_TRANS_IIR\n");
                anc_hdl->param.rtrans_coeff = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_R_FIR:
                user_anc_log("ANC_R_FIR\n");
                anc_hdl->param.rfir_coeff = (s32 *)(temp_dat + coeff_head[i].offset);
                break;
#if ANC_HEARAID_EN
            case ANC_L_TRANS2_IIR:
                user_anc_log("ANC_L_TRANS2_IIR\n");
                anc_hdl->param.ltrans_coeff2 = (double *)(temp_dat + coeff_head[i].offset);
                break;
            case ANC_R_TRANS2_IIR:
                user_anc_log("ANC_R_TRANS2_IIR\n");
                anc_hdl->param.rtrans_coeff2 = (double *)(temp_dat + coeff_head[i].offset);
                break;
#endif/*ANC_HEARAID_EN*/
            default:
                break;
            }
            //put_buf((u8 *)(temp_dat + coeff_head[i].offset), coeff_head[i].len);
        }
        return 0;
    }
    return 1;
}

/*ANC初始化*/
void anc_init(void)
{
    anc_hdl = zalloc(sizeof(anc_t));
    user_anc_log("anc_hdl size:%lu\n", sizeof(anc_t));
    ASSERT(anc_hdl);
    anc_debug_init(&anc_hdl->param);
#if TCFG_ANC_TOOL_DEBUG_ONLINE
    app_online_db_register_handle(DB_PKT_TYPE_ANC, anc_app_online_parse);
#endif/*TCFG_ANC_TOOL_DEBUG_ONLINE*/
    anc_hdl->param.post_msg_drc = audio_anc_post_msg_drc;
    anc_hdl->param.post_msg_debug = audio_anc_post_msg_debug;
    anc_hdl->mode_enable = ANC_MODE_ENABLE;
    anc_mode_enable_set(anc_hdl->mode_enable);
    anc_hdl->param.mode = ANC_OFF;
    anc_hdl->param.train_way = ANC_TRAIN_WAY;
    anc_hdl->param.anc_fade_en = ANC_FADE_EN;/*ANC淡入淡出，默认开*/
    anc_hdl->param.anc_fade_gain = 16384;/*ANC淡入淡出增益,16384 (0dB) max:32767*/
    anc_hdl->param.tool_enablebit = ANC_TRAIN_MODE;
    anc_hdl->param.online_busy = 0;		//在线调试繁忙标志位
    //36
    anc_hdl->param.fade_time_lvl = ANC_MODE_FADE_LVL;
    anc_hdl->param.enablebit = ANC_TRAIN_MODE;
    anc_hdl->param.ch = ANC_CH;
    anc_hdl->param.ff_yorder = ANC_MAX_ORDER;
    anc_hdl->param.trans_yorder = ANC_MAX_ORDER;
    anc_hdl->param.fb_yorder = ANC_MAX_ORDER;
    anc_hdl->param.lff_en = ((ANC_TRAIN_MODE & (ANC_HYBRID_EN | ANC_FF_EN)) && (ANC_CH & ANC_L_CH)) ? 1 : 0;
    anc_hdl->param.lfb_en = ((ANC_TRAIN_MODE & (ANC_HYBRID_EN | ANC_FB_EN)) && (ANC_CH & ANC_L_CH)) ? 1 : 0;
    anc_hdl->param.rff_en = ((ANC_TRAIN_MODE & (ANC_HYBRID_EN | ANC_FF_EN)) && (ANC_CH & ANC_R_CH)) ? 1 : 0;
    anc_hdl->param.rfb_en = ((ANC_TRAIN_MODE & (ANC_HYBRID_EN | ANC_FB_EN)) && (ANC_CH & ANC_R_CH)) ? 1 : 0;

    anc_hdl->param.mic_type[0] = (anc_hdl->param.lff_en) ? ANCL_FF_MIC : MIC_NULL;
    anc_hdl->param.mic_type[1] = (anc_hdl->param.lfb_en) ? ANCL_FB_MIC : MIC_NULL;
    anc_hdl->param.mic_type[2] = (anc_hdl->param.rff_en) ? ANCR_FF_MIC : MIC_NULL;
    anc_hdl->param.mic_type[3] = (anc_hdl->param.rfb_en) ? ANCR_FB_MIC : MIC_NULL;

    anc_hdl->param.debug_sel = 0;
    anc_hdl->param.gains.version = ANC_GAINS_VERSION;
    anc_hdl->param.gains.dac_gain = ANC_DAC_GAIN;
    anc_hdl->param.gains.ref_mic_gain = ANC_REF_MIC_GAIN;
    anc_hdl->param.gains.err_mic_gain = ANC_ERR_MIC_GAIN;
    anc_hdl->param.gains.cmp_en = ANC_CMP_EN;
    anc_hdl->param.gains.drc_en = ANC_DRC_EN;
    anc_hdl->param.gains.ahs_en = ANC_AHS_EN;
    anc_hdl->param.gains.dcc_sel = 4;
    anc_hdl->param.gains.gain_sign = 0;
    anc_hdl->param.gains.alogm = ANC_ALOGM6;
    anc_hdl->param.gains.trans_alogm = ANC_ALOGM4;
    anc_hdl->param.gains.l_ffgain = 1.0f;
    anc_hdl->param.gains.l_fbgain = 1.0f;
    anc_hdl->param.gains.l_cmpgain = 1.0f;
    anc_hdl->param.gains.l_transgain = 1.0f;
    anc_hdl->param.gains.r_ffgain = 1.0f;
    anc_hdl->param.gains.r_fbgain = 1.0f;
    anc_hdl->param.gains.r_cmpgain = 1.0f;
    anc_hdl->param.gains.r_transgain = 1.0f;

    anc_hdl->param.gains.drcff_zero_det = 0;
    anc_hdl->param.gains.drcff_dat_mode = 0;//hbit control l_gain, lbit control h_gain
    anc_hdl->param.gains.drcff_lpf_sel  = 0;
    anc_hdl->param.gains.drcfb_zero_det = 0;
    anc_hdl->param.gains.drcfb_dat_mode = 0;//hbit control l_gain, lbit control h_gain
    anc_hdl->param.gains.drcfb_lpf_sel  = 0;

    anc_hdl->param.gains.drcff_lthr = 0;
    anc_hdl->param.gains.drcff_hthr = 6000 ;
    anc_hdl->param.gains.drcff_lgain = 0;
    anc_hdl->param.gains.drcff_hgain = 512;
    anc_hdl->param.gains.drcff_norgain = 1024 ;

    anc_hdl->param.gains.drctrans_lthr = 0;
    anc_hdl->param.gains.drctrans_hthr = 32767 ;
    anc_hdl->param.gains.drctrans_lgain = 0;
    anc_hdl->param.gains.drctrans_hgain = 1024;
    anc_hdl->param.gains.drctrans_norgain = 1024 ;

    anc_hdl->param.gains.drcfb_lthr = 0;
    anc_hdl->param.gains.drcfb_hthr = 6000;
    anc_hdl->param.gains.drcfb_lgain = 0;
    anc_hdl->param.gains.drcfb_hgain = 512;
    anc_hdl->param.gains.drcfb_norgain = 1024;

    anc_hdl->param.gains.ahs_dly = 1;
    anc_hdl->param.gains.ahs_tap = 100;
    anc_hdl->param.gains.ahs_wn_shift = 9;
    anc_hdl->param.gains.ahs_wn_sub = 1;
    anc_hdl->param.gains.ahs_shift = 210;
    anc_hdl->param.gains.ahs_u = 4000;
    anc_hdl->param.gains.ahs_gain = -1024;
    anc_hdl->param.gains.audio_drc_thr = -6.0f;

#if ANC_COEFF_SAVE_ENABLE
    anc_db_init();
    /*读取ANC增益配置*/
    anc_gain_t *db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, &anc_hdl->param.gains_size);
    if (db_gain) {
        user_anc_log("anc_gain_db get succ,len:%d\n", anc_hdl->param.gains_size);
        if (audio_anc_gains_version_verify(&anc_hdl->param, db_gain)) {
            db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, &anc_hdl->param.gains_size);
        }
        anc_param_fill(ANC_CFG_WRITE, db_gain);
    } else {
        user_anc_log("anc_gain_db get failed");
        anc_param_gain_t_printf(ANC_CFG_READ);
    }
    /*读取ANC滤波器系数*/
    anc_coeff_t *db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
    if (!anc_coeff_fill(db_coeff)) {
        user_anc_log("anc_coeff_db get succ,len:%d", anc_hdl->param.coeff_size);
    } else {
        user_anc_log("anc_coeff_db get failed");
#if 0	//测试数据写入是否正常，或写入自己想要的滤波器数据
        anc_coeff_t *test_coeff = anc_cfg_test(1, 1);
        if (test_coeff) {
            int ret = anc_db_put(&anc_hdl->param, NULL, test_coeff);
            if (ret == 0) {
                user_anc_log("anc_db put test succ\n");
            } else {
                user_anc_log("anc_db put test err\n");
            }
        }
        anc_coeff_t *test_coeff1 = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
        anc_coeff_fill(test_coeff1);
#endif
    }
#endif/*ANC_COEFF_SAVE_ENABLE*/

    /* sys_timer_add(NULL, anc_timer_deal, 5000); */

    audio_anc_max_yorder_verify(&anc_hdl->param, ANC_MAX_ORDER);
    anc_mix_out_audio_drc_thr(anc_hdl->param.gains.audio_drc_thr);
    task_create(anc_task, NULL, "anc");
    anc_hdl->state = ANC_STA_INIT;
    anc_hdl->dy_mic_ch = MIC_NULL;
    user_anc_log("anc_init ok");
}

void anc_train_open(u8 mode, u8 debug_sel)
{
    user_anc_log("ANC_Train_Open\n");
    local_irq_disable();
    if (anc_hdl && (anc_hdl->state == ANC_STA_INIT)) {
        /*防止重复打开训练模式*/
        if (anc_hdl->param.mode == ANC_TRAIN) {
            local_irq_enable();
            return;
        }

        /*anc工作，退出sniff*/
        user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);

        /*anc工作，关闭自动关机*/
        sys_auto_shut_down_disable();
        audio_mic_pwr_ctl(MIC_PWR_ON);

        anc_hdl->param.debug_sel = debug_sel;
        anc_hdl->param.mode = mode;
        /*训练的时候，申请buf用来保存训练参数*/
        local_irq_enable();
        os_taskq_post_msg("anc", 1, ANC_MSG_TRAIN_OPEN);
        user_anc_log("%s ok\n", __FUNCTION__);
        return;
    }
    local_irq_enable();
}

void anc_train_close(void)
{
    int ret = 0;
    if (anc_hdl == NULL) {
        return;
    }
    if (anc_hdl && (anc_hdl->param.mode == ANC_TRAIN)) {
        anc_hdl->param.train_para.train_busy = 0;
        anc_hdl->param.mode = ANC_OFF;
        anc_hdl->state = ANC_STA_INIT;
        audio_anc_train(&anc_hdl->param, 0);

#if TCFG_USER_TWS_ENABLE
        /*训练完毕，tws正常配置连接*/
        bt_tws_poweron();
#endif/*TCFG_USER_TWS_ENABLE*/
        user_anc_log("anc_train_close ok\n");
    }
}

/*查询当前ANC是否处于训练状态*/
int anc_train_open_query(void)
{
    if (anc_hdl) {
        if (anc_hdl->param.mode == ANC_TRAIN) {
            return 1;
        }
    }
    return 0;
}

extern void audio_dump();
static void anc_timer_deal(void *priv)
{
    u8 dac_again_l = JL_ADDA->DAA_CON1 & 0xF;
    u8 dac_again_r = (JL_ADDA->DAA_CON1 >> 8) & 0xF;
    u32 dac_dgain_l = JL_AUDIO->DAC_VL0 & 0xFFFF;
    u32 dac_dgain_r = (JL_AUDIO->DAC_VL0 >> 16) & 0xFFFF;
    u8 mic0_gain = (JL_ADDA->ADA_CON2 >> 24) & 0x1F;
    u8 mic1_gain = (JL_ADDA->ADA_CON1 >> 24) & 0x1F;
    u32 anc_drcffgain = (JL_ANC->CON16 >> 16) & 0xFFFF;
    u32 anc_drcfbgain = (JL_ANC->CON19 >> 16) & 0xFFFF;
    user_anc_log("MIC_G:%d,%d,DAC_AG:%d,%d,DAC_DG:%d,%d,ANC_NFFG:%d ,ANC_NFBG :%d\n", mic0_gain, mic1_gain, dac_again_l, dac_again_r, dac_dgain_l, dac_dgain_r, (short)anc_drcffgain, (short)anc_drcfbgain);
    mem_stats();
    /* audio_dump(); */
}

static void anc_tone_play_cb(void *priv)
{
    u8 next_mode = (u8)priv;
    printf("anc_tone_play_cb,anc_mode:%d,%d,new_mode:%d\n", next_mode, anc_hdl->param.mode, anc_hdl->new_mode);
    /*
     *当最新的目标ANC模式和即将切过去的模式一致，才进行模式切换实现。
     *否则表示，模式切换操作（比如快速按键切模式）new_mode领先于ANC模式切换实现next_mode
     *则直接在切模式的时候，实现最新的目标模式
     */
    if (anc_hdl->param.mode == next_mode) {
        anc_mode_switch_deal(next_mode);
    } else {
        anc_hdl->mode_switch_lock = 0;
    }
}

/*
*********************************************************************
*                  Audio ANC Tone Play And Mode Switch
* Description: ANC提示音播放和模式切换
* Arguments  : index		提示音索引
*			   preemption	提示音抢断播放标识
*			   cb_sel	 	切模式方式选择
* Return	 : None.
* Note(s)    : 通过cb_sel选择是在播放提示音切模式，还是播提示音同时切
*			   模式
*********************************************************************
*/
static void anc_tone_play_and_mode_switch(u8 index, u8 preemption, u8 cb_sel)
{
    if (!bt_media_is_running() && !bt_phone_dec_is_running()) {	//后台没有输出，强制打断播放
        preemption = 1;
    }
    if (cb_sel) {
        /*ANC打开情况下，播提示音的同时，anc效果淡出*/
        if (anc_hdl->state == ANC_STA_OPEN) {
            audio_anc_fade(0, anc_hdl->param.anc_fade_en, 1);
        }
        tone_play_index_with_callback(index, preemption, anc_tone_play_cb, (void *)anc_hdl->param.mode);
    } else {
        tone_play_index(index, preemption);
        anc_mode_switch_deal(anc_hdl->param.mode);
    }
}

static void anc_tone_stop(void)
{
    if (anc_hdl && anc_hdl->mode_switch_lock) {
        tone_play_stop();
    }
}

/*
*********************************************************************
*                  anc_fade_in_timer
* Description: ANC增益淡入函数
* Arguments  : None.
* Return	 : None.
* Note(s)    :通过定时器的控制，使ANC的淡入增益一点一点叠加
*********************************************************************
*/
static void anc_fade_in_timer(void *arg)
{
    anc_hdl->fade_gain += (anc_hdl->param.anc_fade_gain / anc_hdl->param.fade_time_lvl);
    if (anc_hdl->fade_gain > anc_hdl->param.anc_fade_gain) {
        anc_hdl->fade_gain = anc_hdl->param.anc_fade_gain;
        usr_timer_del(anc_hdl->fade_in_timer);
    }
    audio_anc_fade(anc_hdl->fade_gain, anc_hdl->param.anc_fade_en, 1);
}

static void anc_fade_in_timer_add(audio_anc_t *param)
{
    u32 alogm, dly;
    if (param->anc_fade_en) {
        anc_hdl->param.fade_lock = 0;	//解锁淡入淡出增益
        alogm = (param->mode == ANC_TRANSPARENCY) ? param->gains.trans_alogm : param->gains.alogm;
        dly = audio_anc_fade_dly_get(param->anc_fade_gain, alogm);
        anc_hdl->fade_gain = 0;
        if (param->mode == ANC_ON) {		//可自定义模式，当前仅在ANC模式下使用
            user_anc_log("anc_fade_time_lvl  %d\n", param->fade_time_lvl);
            anc_hdl->fade_gain = (param->anc_fade_gain / param->fade_time_lvl);
            user_anc_log("gain:%d->%d\n", (JL_ANC->CON7 >> 9) & 0xFFFF, anc_hdl->fade_gain);
            audio_anc_fade(anc_hdl->fade_gain, param->anc_fade_en, 1);
            if (param->fade_time_lvl > 1) {
                anc_hdl->fade_in_timer = usr_timer_add((void *)0, anc_fade_in_timer, dly, 1);
            }
        } else if (param->mode != ANC_OFF) {
            audio_anc_fade(param->anc_fade_gain, param->anc_fade_en, 1);
        }
    }
}
static void anc_fade_out_timeout(void *arg)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_RUN);
}

static void anc_fade(u32 gain)
{
    u32	alogm, dly;
    if (anc_hdl) {
        alogm = (anc_hdl->last_mode == ANC_TRANSPARENCY) ? anc_hdl->param.gains.trans_alogm : anc_hdl->param.gains.alogm;
        dly = audio_anc_fade_dly_get(anc_hdl->param.anc_fade_gain, alogm);
        user_anc_log("anc_fade:%d,dly:%d", gain, dly);
        audio_anc_fade(gain, anc_hdl->param.anc_fade_en, 1);
        if (anc_hdl->param.anc_fade_en) {
            usr_timeout_del(anc_hdl->fade_in_timer);
            usr_timeout_add((void *)0, anc_fade_out_timeout, dly, 1);
        } else {/*不淡入淡出，则直接切模式*/
            os_taskq_post_msg("anc", 1, ANC_MSG_RUN);
        }
    }
}

/*ANC淡入淡出增益设置接口*/
void audio_anc_fade_gain_set(int gain)
{
    anc_hdl->param.anc_fade_gain = gain;
    if (anc_hdl->param.mode != ANC_OFF) {
        audio_anc_fade(gain, anc_hdl->param.anc_fade_en, 1);
    }
}
/*ANC淡入淡出增益接口*/
int audio_anc_fade_gain_get(void)
{
    return anc_hdl->param.anc_fade_gain;
}

/*
 *mode:降噪/通透/关闭
 *tone_play:切换模式的时候，是否播放提示音
 */
static void anc_mode_switch_deal(u8 mode)
{
    user_anc_log("anc switch,state:%s", anc_state_str[anc_hdl->state]);
    /* anc_hdl->mode_switch_lock = 1; */
    if (anc_hdl->state == ANC_STA_OPEN) {
        user_anc_log("anc open now,switch mode:%d", mode);
        anc_fade(0);//切模式，先fade_out
    } else if (anc_hdl->state == ANC_STA_INIT) {
        if (anc_hdl->param.mode != ANC_OFF) {
            /*anc工作，关闭自动关机*/
            sys_auto_shut_down_disable();
            os_taskq_post_msg("anc", 1, ANC_MSG_RUN);
        } else {
            user_anc_log("anc close now,new mode is ANC_OFF\n");
            anc_hdl->mode_switch_lock = 0;
        }
    } else {
        user_anc_log("anc state err:%d\n", anc_hdl->state);
        anc_hdl->mode_switch_lock = 0;
    }
}

void anc_gain_app_value_set(int app_value)
{
    static int anc_gain_app_value = -1;
    if (-1 == app_value && -1 != anc_gain_app_value) {
        /* anc_hdl->param.anc_ff_gain = anc_gain_app_value; */
    }
    anc_gain_app_value = app_value;
}

#define TWS_ANC_SYNC_TIMEOUT	400 //ms
void anc_mode_switch(u8 mode, u8 tone_play)
{
    if (anc_hdl == NULL) {
        return;
    }
    /*模式切换超出范围*/
    if ((mode > ANC_BYPASS) || (mode < ANC_OFF)) {
        user_anc_log("anc mode switch err:%d", mode);
        return;
    }
    /*模式切换同一个*/
    if ((anc_hdl->param.mode == mode) || (anc_hdl->new_mode == mode)) {
        user_anc_log("anc mode switch err:same mode");
        return;
    }

    anc_hdl->new_mode = mode;/*记录最新的目标ANC模式*/
    anc_hdl->param.trans_mode_sel = ANC_TRANS_MODE_NORMAL;

    if (anc_hdl->suspend) {
        anc_hdl->param.tool_enablebit = 0;
    }

    /*
     *ANC模式提示音播放规则
     *(1)根据应用选择是否播放提示音：tone_play
     *(2)tws连接的时候，主机发起模式提示音同步播放
     *(3)单机的时候，直接播放模式提示音
     */
    if (tone_play) {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state()) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                user_anc_log("[tws_master]anc_tone_sync_play");
                anc_hdl->sync_busy = 1;
                if (anc_hdl->new_mode == ANC_ON) {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_ON, TWS_ANC_SYNC_TIMEOUT);
                } else if (anc_hdl->new_mode == ANC_OFF) {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_OFF, TWS_ANC_SYNC_TIMEOUT);
                } else {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_TRANS, TWS_ANC_SYNC_TIMEOUT);
                }
            }
            return;
        } else {
            user_anc_log("anc_tone_play");
            anc_hdl->param.mode = mode;
            anc_tone_play_and_mode_switch(anc_tone_tab[mode - 1], ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
        }
#else
        anc_hdl->param.mode = mode;
        anc_tone_play_and_mode_switch(anc_tone_tab[mode - 1], ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
#endif/*TCFG_USER_TWS_ENABLE*/
    } else {
        anc_hdl->param.mode = mode;
        anc_mode_switch_deal(mode);
    }
}

#if ANC_HEARAID_EN
void anc_hearaid_mode_switch(u8 mode, u8 tone_play)
{
    u8 index;
    if (anc_hdl == NULL) {
        return;
    }
    /*模式切换超出范围*/
    if ((mode > ANC_BYPASS) || (mode < ANC_OFF)) {
        user_anc_log("anc mode switch err:%d", mode);
        return;
    }
    /*模式切换同一个*/
    if (anc_hdl->param.mode == mode) {
        user_anc_log("anc mode switch err:same mode");
        return;
    }

    anc_hdl->new_mode = mode;/*记录最新的目标ANC模式*/
#if ANC_TONE_END_MODE_SW
    anc_tone_stop();
#endif/*ANC_TONE_END_MODE_SW*/
    anc_hdl->param.trans_mode_sel = ANC_TRANS_MODE_VOICE_ENHANCE;
    anc_hdl->param.mode = mode;

    if (anc_hdl->suspend) {
        anc_hdl->param.tool_enablebit = 0;
    }

    /* anc_gain_app_value_set(-1); */		//没有处理好bypass与ANC_ON的关系
    /*
     *ANC模式提示音播放规则
     *(1)根据应用选择是否播放提示音：tone_play
     *(2)tws连接的时候，主机发起模式提示音同步播放
     *(3)单机的时候，直接播放模式提示音
     */
    if (tone_play) {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state()) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                user_anc_log("[tws_master]anc_tone_sync_play");
                anc_hdl->sync_busy = 1;
                if (anc_hdl->param.mode == ANC_TRANSPARENCY) {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_HEAR_AID_ON, TWS_ANC_SYNC_TIMEOUT);
                } else {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_HEAR_AID_OFF, TWS_ANC_SYNC_TIMEOUT);
                }
            }
            return;
        } else {
            user_anc_log("anc_tone_play");
            index = (anc_hdl->param.mode == ANC_TRANSPARENCY) ? IDEX_TONE_HEARAID_ON : IDEX_TONE_HEARAID_OFF;
            anc_tone_play_and_mode_switch(index, ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
        }
#else
        index = (anc_hdl->param.mode == ANC_TRANSPARENCY) ? IDEX_TONE_HEARAID_ON : IDEX_TONE_HEARAID_OFF;
        anc_tone_play_and_mode_switch(index, ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
#endif/*TCFG_USER_TWS_ENABLE*/
    } else {
        anc_mode_switch_deal(mode);
    }
}

/*ANC辅听器模式与通话互斥*/
void anc_hearaid_mode_suspend(u8 suspend)
{
    if (anc_hdl->param.trans_mode_sel == ANC_TRANS_MODE_VOICE_ENHANCE) {
        if (suspend && anc_hdl->param.mode == ANC_TRANSPARENCY) {
            audio_anc_hearing_aid_howldet_stop();
            audio_anc_fade(0, anc_hdl->param.anc_fade_en, 1);
        } else {
            audio_anc_fade(16384, anc_hdl->param.anc_fade_en, 1);
            audio_anc_hearing_aid_howldet_start(&anc_hdl->param);
        }
    }
}

#endif/*ANC_HEARAID_EN*/

static void anc_ui_mode_sel_timer(void *priv)
{
    if (anc_hdl->ui_mode_sel && (anc_hdl->ui_mode_sel != anc_hdl->param.mode)) {
        /*
         *提示音不打断
         *tws提示音同步播放完成
         */
        if ((tone_get_status() == 0) && (anc_hdl->sync_busy == 0)) {
            user_anc_log("anc_ui_mode_sel_timer:%d,sync_busy:%d", anc_hdl->ui_mode_sel, anc_hdl->sync_busy);
            anc_mode_switch(anc_hdl->ui_mode_sel, 1);
            sys_timer_del(anc_hdl->ui_mode_sel_timer);
            anc_hdl->ui_mode_sel_timer = 0;
        }
    }
}

int anc_mode_change_tool(u8 dat)
{
    if (dat < 0 || dat > ANC_HYBRID_EN) {
        return -1;
    }
    anc_hdl->param.tool_enablebit = dat;
    audio_anc_tool_en_ctrl(&anc_hdl->param, dat);
    return 0;
}

/*ANC通过ui菜单选择anc模式,处理快速切换的情景*/
void anc_ui_mode_sel(u8 mode, u8 tone_play)
{
    /*
     *timer存在表示上个模式还没有完成切换
     *提示音不打断
     *tws提示音同步播放完成
     */
    if ((anc_hdl->ui_mode_sel_timer == 0) && (tone_get_status() == 0) && (anc_hdl->sync_busy == 0)) {
        user_anc_log("anc_ui_mode_sel[ok]:%d", mode);
        anc_mode_switch(mode, tone_play);
    } else {
        user_anc_log("anc_ui_mode_sel[dly]:%d,timer:%d,sync_busy:%d", mode, anc_hdl->ui_mode_sel_timer, anc_hdl->sync_busy);
        anc_hdl->ui_mode_sel = mode;
        if (anc_hdl->ui_mode_sel_timer == 0) {
            anc_hdl->ui_mode_sel_timer = sys_timer_add(NULL, anc_ui_mode_sel_timer, 50);
        }
    }
}

/*tws同步播放模式提示音，并且同步进入anc模式*/
void anc_tone_sync_play(int tone_name)
{
#if TCFG_USER_TWS_ENABLE
    if (anc_hdl) {
        user_anc_log("anc_tone_sync_play:%d", tone_name);
        os_taskq_post_msg("anc", 2, ANC_MSG_TONE_SYNC, tone_name);
    }
#endif/*TCFG_USER_TWS_ENABLE*/
}

/*ANC模式同步(tws模式)*/
void anc_mode_sync(u8 mode, u8 mode_sel)
{
    if (anc_hdl) {
        os_taskq_post_msg("anc", 3, ANC_MSG_MODE_SYNC, mode, mode_sel);
    }
}

/*ANC挂起*/
void anc_suspend(void)
{
    if (anc_hdl) {
        user_anc_log("anc_suspend\n");
        anc_hdl->suspend = 1;
        anc_hdl->param.tool_enablebit = 0;	//使能设置为0
        if (anc_hdl->param.anc_fade_en) {	  //挂起ANC增益淡出
            audio_anc_fade(0, anc_hdl->param.anc_fade_en, 1);
        }
        audio_anc_tool_en_ctrl(&anc_hdl->param, 0);
    }
}

/*ANC恢复*/
void anc_resume(void)
{
    if (anc_hdl) {
        user_anc_log("anc_resume\n");
        anc_hdl->suspend = 0;
        anc_hdl->param.tool_enablebit = anc_hdl->param.enablebit;	//使能恢复
        audio_anc_tool_en_ctrl(&anc_hdl->param, anc_hdl->param.enablebit);
        if (anc_hdl->param.anc_fade_en) {	  //恢复ANC增益淡入
            audio_anc_fade(anc_hdl->param.anc_fade_gain, anc_hdl->param.anc_fade_en, 1);
        }
    }
}

/*ANC信息保存*/
void anc_info_save()
{
    if (anc_hdl) {
        anc_info_t anc_info;
        int ret = syscfg_read(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret == sizeof(anc_info)) {
#if INEAR_ANC_UI
            if (anc_info.mode == anc_hdl->param.mode && anc_info.inear_tws_mode == inear_tws_ancmode) {
#else
            if (anc_info.mode == anc_hdl->param.mode) {
#endif/*INEAR_ANC_UI*/
                user_anc_log("anc info.mode == cur_anc_mode");
                return;
            }
        } else {
            user_anc_log("read anc_info err");
        }

        user_anc_log("save anc_info");
        anc_info.mode = anc_hdl->param.mode;
#if INEAR_ANC_UI
        anc_info.inear_tws_mode = inear_tws_ancmode;
#endif/*INEAR_ANC_UI*/
        ret = syscfg_write(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret != sizeof(anc_info)) {
            user_anc_log("anc info save err!\n");
        }

    }
}

/*系统上电的时候，根据配置决定是否进入上次的模式*/
void anc_poweron(void)
{
    if (anc_hdl) {
#if ANC_INFO_SAVE_ENABLE
        anc_info_t anc_info;
        int ret = syscfg_read(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret == sizeof(anc_info)) {
            user_anc_log("read anc_info succ,state:%s,mode:%s", anc_state_str[anc_hdl->state], anc_mode_str[anc_info.mode]);
#if INEAR_ANC_UI
            inear_tws_ancmode = anc_info.inear_tws_mode;
#endif/*INEAR_ANC_UI*/
            if ((anc_hdl->state == ANC_STA_INIT) && (anc_info.mode != ANC_OFF)) {
                anc_mode_switch(anc_info.mode, 0);
            }
        } else {
            user_anc_log("read anc_info err");
        }
#endif/*ANC_INFO_SAVE_ENABLE*/
    }
}

/*ANC poweroff*/
void anc_poweroff(void)
{
    if (anc_hdl) {
        user_anc_log("anc_cur_state:%s\n", anc_state_str[anc_hdl->state]);
#if ANC_INFO_SAVE_ENABLE
        anc_info_save();
#endif/*ANC_INFO_SAVE_ENABLE*/
        if (anc_hdl->state == ANC_STA_OPEN) {
            user_anc_log("anc_poweroff\n");
            /*close anc module when fade_out timeout*/
            anc_hdl->param.mode = ANC_OFF;
            anc_fade(0);
        }
    }
}

/*模式切换测试demo*/
#define ANC_MODE_NUM	3 /*ANC模式循环切换*/
static const u8 anc_mode_switch_tab[ANC_MODE_NUM] = {
    ANC_OFF,
    ANC_TRANSPARENCY,
    ANC_ON,
};
void anc_mode_next(void)
{
    if (anc_hdl) {
        if (anc_train_open_query()) {
            return;
        }
        u8 next_mode = 0;
        local_irq_disable();
        anc_hdl->param.anc_fade_en = ANC_FADE_EN;	//防止被其他地方清0
        for (u8 i = 0; i < ANC_MODE_NUM; i++) {
            if (anc_mode_switch_tab[i] == anc_hdl->param.mode) {
                next_mode = i + 1;
                if (next_mode >= ANC_MODE_NUM) {
                    next_mode = 0;
                }
                if ((anc_hdl->mode_enable & BIT(anc_mode_switch_tab[next_mode])) == 0) {
                    user_anc_log("anc_mode_filt,next:%d,en:%d", next_mode, anc_hdl->mode_enable);
                    next_mode++;
                    if (next_mode >= ANC_MODE_NUM) {
                        next_mode = 0;
                    }
                }
                //g_printf("fine out anc mode:%d,next:%d,i:%d",anc_hdl->param.mode,next_mode,i);
                break;
            }
        }
        local_irq_enable();
        //user_anc_log("anc_next_mode:%d old:%d,new:%d", next_mode, anc_hdl->param.mode, anc_mode_switch_tab[next_mode]);
        u8 new_mode = anc_mode_switch_tab[next_mode];
        user_anc_log("new_mode:%s", anc_mode_str[new_mode]);
        anc_mode_switch(anc_mode_switch_tab[next_mode], 1);
    }
}

/*设置ANC支持切换的模式*/
void anc_mode_enable_set(u8 mode_enable)
{
    if (anc_hdl) {
        anc_hdl->mode_enable = mode_enable;
        u8 mode_cnt = 0;
        for (u8 i = 1; i < 4; i++) {
            if (mode_enable & BIT(i)) {
                mode_cnt++;
                user_anc_log("%s Select", anc_mode_str[i]);
            }
        }
        user_anc_log("anc_mode_enable_set:%d", mode_cnt);
        anc_hdl->mode_num = mode_cnt;
    }
}

/*获取anc状态，0:空闲，l:忙*/
u8 anc_status_get(void)
{
    u8 status = 0;
    if (anc_hdl) {
        if (anc_hdl->state == ANC_STA_OPEN) {
            status = 1;
        }
    }
    return status;
}

/*获取anc当前模式*/
u8 anc_mode_get(void)
{
    if (anc_hdl) {
        //user_anc_log("anc_mode_get:%s", anc_mode_str[anc_hdl->param.mode]);
        return anc_hdl->param.mode;
    }
    return 0;
}

/*获取anc当前模式选项*/
u8 anc_mode_sel_get(void)
{
    if (anc_hdl) {
        //user_anc_log("anc_mode_get:%s", anc_mode_str[anc_hdl->param.mode]);
        return anc_hdl->param.trans_mode_sel;
    }
    return 0;
}

/*获取anc模式，dac左右声道的增益*/
u8 anc_dac_gain_get(u8 ch)
{
    u8 gain = 0;
    if (anc_hdl) {
        gain = anc_hdl->param.gains.dac_gain;
    }
    return gain;
}

/*获取anc模式，ref_mic的增益*/
u8 anc_mic_gain_get(void)
{
    u8 gain = 0;
    if (anc_hdl) {
        gain = anc_hdl->param.gains.ref_mic_gain;
    }
    return gain;
}

void anc_coeff_size_set(u8 mode, int len)
{
    if (mode == ANC_CFG_READ) {
        anc_hdl->param.coeff_size = len;
        user_anc_log("ANC_CFG_READ_SET coeff_size %d\n", anc_hdl->param.coeff_size);
    } else {
        anc_hdl->param.write_coeff_size = len;
        user_anc_log("ANC_CFG_WRITE_SET coeff_size %d\n", anc_hdl->param.write_coeff_size);
    }
}

int anc_coeff_size_get(u8 mode)
{
    if (mode == ANC_CFG_READ) {
        user_anc_log("ANC_CFG_READ_GET coeff_size %d\n", anc_hdl->param.coeff_size);
        return anc_hdl->param.coeff_size;
    } else {
        //根据配置生成数据长度大小
        anc_hdl->param.write_coeff_size = anc_coeff_size_count(&anc_hdl->param);
        user_anc_log("ANC_CFG_WRITE_GET coeff_size %d\n", anc_hdl->param.write_coeff_size);
        return anc_hdl->param.write_coeff_size;
    }
}

/*anc coeff读接口*/
int *anc_coeff_read(void)
{
#if ANC_BOX_READ_COEFF
    int *coeff = anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
    if (coeff) {
        coeff = (int *)((u8 *)coeff);
    }
    user_anc_log("anc_coeff_read:0x%x", (u32)coeff);
    return coeff;
#else
    return NULL;
#endif/*ANC_BOX_READ_COEFF*/
}

/*anc coeff写接口*/
int anc_coeff_write(int *coeff, u16 len)
{
    int ret = 0;
    u8 single_type = 0;
    u16 config_len = anc_coeff_size_count(&anc_hdl->param);
    anc_coeff_t *db_coeff = (anc_coeff_t *)coeff;
    anc_coeff_t *tmp_coeff = NULL;

    user_anc_log("anc_coeff_write:0x%x, len:%d, config_len %d", (u32)coeff, len, config_len);
    if (db_coeff->cnt != 1) {
        ret = anc_db_put(&anc_hdl->param, NULL, (anc_coeff_t *)coeff);
    } else {	//保存对应的单项滤波器
        db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &anc_hdl->param.write_coeff_size);
        tmp_coeff = malloc(anc_hdl->param.write_coeff_size);
        if (tmp_coeff) {
            memcpy(tmp_coeff, db_coeff, anc_hdl->param.coeff_size);
        }
        anc_coeff_single_fill((anc_coeff_t *)coeff, tmp_coeff);
        ret = anc_db_put(&anc_hdl->param, NULL, tmp_coeff);
        if (tmp_coeff) {
            free(tmp_coeff);
        }
    }
    if (ret) {
        user_anc_log("anc_coeff_write err:%d", ret);
        return -1;
    }
    db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
    anc_coeff_fill(db_coeff);
    if (anc_hdl->param.mode != ANC_OFF) {		//实时更新填入使用
        anc_coeff_online_update(&anc_hdl->param, 1);
    }
    return 0;
}

int *anc_debug_ctr(u8 free_en)
{
    if (anc_hdl && anc_hdl->param.debug_buf) {
        if (free_en) {
            free(anc_hdl->param.debug_buf);
            anc_hdl->param.debug_buf = NULL;
        }
        return anc_hdl->param.debug_buf;
    } else {
        return NULL;
    }
}

static u8 ANC_idle_query(void)
{
    if (anc_hdl) {
        /*ANC训练模式，不进入低功耗*/
        if (anc_train_open_query()) {
            return 0;
        }
    }
    return 1;
}

static enum LOW_POWER_LEVEL ANC_level_query(void)
{
    if (anc_hdl == NULL) {
        return LOW_POWER_MODE_SLEEP;
    }
    /*根据anc的状态选择sleep等级*/
    if (anc_status_get() || anc_hdl->mode_switch_lock) {
        /*anc打开，进入轻量级低功耗*/
        return LOW_POWER_MODE_LIGHT_SLEEP;
    }
    /*anc关闭，进入最优低功耗*/
    return LOW_POWER_MODE_SLEEP;
}

REGISTER_LP_TARGET(ANC_lp_target) = {
    .name       = "ANC",
    .level      = ANC_level_query,
    .is_idle    = ANC_idle_query,
};

void chargestore_uart_data_deal(u8 *data, u8 len)
{
    /* anc_uart_process(data, len); */
}

#if TCFG_ANC_TOOL_DEBUG_ONLINE
//回复包
void anc_ci_send_packet(u32 id, u8 *packet, int size)
{
    if (DB_PKT_TYPE_ANC == id) {
        app_online_db_ack(anc_hdl->anc_parse_seq, packet, size);
        return;
    }
    /* y_printf("anc_app_spp_tx\n"); */
    ci_send_packet(id, packet, size);
}

//接收函数
static int anc_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
    /* y_printf("anc_app_spp_rx\n"); */
    anc_hdl->anc_parse_seq  = ext_data[1];
    anc_spp_rx_packet(packet, size);
    return 0;
}
//发送函数
void anc_btspp_packet_tx(u8 *packet, int size)
{
    app_online_db_send(DB_PKT_TYPE_ANC, packet, size);
}
#endif/*TCFG_ANC_TOOL_DEBUG_ONLINE*/

u8 anc_btspp_train_again(u8 mode, u32 dat)
{
    /* tws_api_detach(TWS_DETACH_BY_POWEROFF); */
    int ret = 0;
    user_anc_log("anc_btspp_train_again\n");
    audio_anc_close();
    anc_hdl->state = ANC_STA_INIT;
    anc_train_open(mode, (u8)dat);
    return 1;
}


void anc_api_set_callback(void (*callback)(u8, u8), void (*pow_callback)(anc_ack_msg_t *, u8))
{
    if (anc_hdl == NULL) {
        return;
    }
    anc_hdl->param.train_callback = callback;
    anc_hdl->param.pow_callback = pow_callback;
    user_anc_log("ANC_TRAIN_CALLBACK\n");
}

void anc_api_set_fade_en(u8 en)
{
    if (anc_hdl == NULL) {
        return;
    }
    anc_hdl->param.anc_fade_en = en;
    user_anc_log("ANC_SET_FADE_EN: %d\n", en);
}

anc_train_para_t *anc_api_get_train_param(void)
{
    if (anc_hdl == NULL) {
        return NULL;
    }
    return &anc_hdl->param.train_para;
}

u8 anc_api_get_train_step(void)
{
    if (anc_hdl == NULL) {
        return 0;
    }
    return anc_hdl->param.train_para.ret_step;
}

void anc_api_set_trans_aim_pow(u32 pow)
{
    if (anc_hdl == NULL) {
        return;
    }
    /* anc_hdl->param.trans_aim_pow = pow; */
}
/*ANC在线调试标志设置*/
void anc_online_busy_set(u8 en)
{
    anc_hdl->param.online_busy = en;
}

/*ANC在线调试标志获取*/
u8 anc_online_busy_get(void)
{
    return anc_hdl->param.online_busy;
}

/*ANC配置在线读取接口*/
int anc_cfg_online_deal(u8 cmd, anc_gain_t *cfg)
{
    /*同步在线更新配置*/
    int ret = 0;
    anc_param_fill(cmd, cfg);
    if (cmd == ANC_CFG_WRITE) {
        /*实时更新ANC配置*/
        audio_anc_reset(0);
        anc_mix_out_audio_drc_thr(anc_hdl->param.gains.audio_drc_thr);
        audio_anc_gain_sign(&anc_hdl->param);
        audio_anc_dac_gain(anc_hdl->param.gains.dac_gain, anc_hdl->param.gains.dac_gain);
        audio_anc_mic_gain(anc_hdl->param.gains.ref_mic_gain, anc_hdl->param.gains.err_mic_gain);
        audio_anc_drc_ctrl(&anc_hdl->param, 1.0);
        if (anc_hdl->param.mode == ANC_TRANSPARENCY) {
            audio_anc_dcc_set(1);
        } else {
            audio_anc_dcc_set(anc_hdl->param.gains.dcc_sel);
        }
        audio_anc_max_yorder_verify(&anc_hdl->param, ANC_MAX_ORDER);
        anc_coeff_online_update(&anc_hdl->param, 0);
#if ANC_HEARAID_EN
        audio_anc_hearing_aid_param_update(&anc_hdl->param, 1);
#endif/*ANC_HEARAID_EN*/
        if (anc_hdl->param.mode == ANC_ON) {
            audio_anc_cmp_en(&anc_hdl->param, anc_hdl->param.gains.cmp_en);
        }
        audio_anc_reset(1);
        /*仅修改增益配置*/
        ret = anc_db_put(&anc_hdl->param, cfg, NULL);
        if (ret) {
            user_anc_log("ret %d\n", ret);
            anc_hdl->param.lff_coeff = NULL;
            anc_hdl->param.rff_coeff = NULL;
            anc_hdl->param.lfb_coeff = NULL;
            anc_hdl->param.rfb_coeff = NULL;
        };
    }
    return 0;
}

/*蓝牙手动训练更新增益接口*/
void anc_cfg_btspp_update(u8 id, int data)
{
    anc_gain_t *cfg;
    cfg = zalloc(sizeof(anc_gain_t));
    anc_cfg_online_deal(ANC_CFG_READ, cfg);
    switch (id) {
    case ANC_REF_MIC_GAIN_SET:
        cfg->gains.ref_mic_gain = data;
        break;
    case ANC_ERR_MIC_GAIN_SET:
        cfg->gains.err_mic_gain = data;
        break;
    case ANC_DAC_GAIN_SET:
        cfg->gains.dac_gain = data;
        break;
    case ANC_SAMPLE_RATE_SET:
        cfg->gains.alogm = data;
        break;
    case ANC_TRANS_SAMPLE_RATE_SET:
        cfg->gains.trans_alogm = data;
        break;
    case ANC_CMP_EN_SET:
        cfg->gains.cmp_en = data;
        break;
    case ANC_DRC_EN_SET:
        cfg->gains.drc_en = data;
        break;
    case ANC_AHS_EN_SET:
        cfg->gains.ahs_en = data;
        break;
    case ANC_DCC_SEL_SET:
        cfg->gains.dcc_sel = data;
        break;
    case ANC_GAIN_SIGN_SET:
        cfg->gains.gain_sign = data;
        break;
    case ANC_NOISE_LVL_SET:
        cfg->gains.noise_lvl = data;
        break;
    case ANC_FFGAIN_SET:
        cfg->gains.l_ffgain = *((float *)&data);
        break;
    case ANC_FBGAIN_SET:
        cfg->gains.l_fbgain = *((float *)&data);
        break;
    case ANC_TRANS_GAIN_SET:
        cfg->gains.l_transgain = *((float *)&data);
        break;
    case ANC_CMP_GAIN_SET:
        cfg->gains.l_cmpgain = *((float *)&data);
        break;
    case ANC_RFF_GAIN_SET:
        cfg->gains.r_ffgain = *((float *)&data);
        break;
    case ANC_RFB_GAIN_SET:
        cfg->gains.r_fbgain = *((float *)&data);
        break;
    case ANC_RTRANS_GAIN_SET:
        cfg->gains.r_transgain = *((float *)&data);
        break;
    case ANC_RCMP_GAIN_SET:
        cfg->gains.r_cmpgain = *((float *)&data);
        break;
    case ANC_DRCFF_ZERO_DET_SET:
        cfg->gains.drcff_zero_det = data;
        break;
    case ANC_DRCFF_DAT_MODE_SET:
        cfg->gains.drcff_dat_mode = data;
        break;
    case ANC_DRCFF_LPF_SEL_SET:
        cfg->gains.drcff_lpf_sel = data;
        break;
    case ANC_DRCFB_ZERO_DET_SET:
        cfg->gains.drcfb_zero_det = data;
        break;
    case ANC_DRCFB_DAT_MODE_SET:
        cfg->gains.drcfb_dat_mode = data;
        break;
    case ANC_DRCFB_LPF_SEL_SET:
        cfg->gains.drcfb_lpf_sel = data;
        break;
    case ANC_DRCFF_LTHR_SET:
        cfg->gains.drcff_lthr = data;
        break;
    case ANC_DRCFF_HTHR_SET:
        cfg->gains.drcff_hthr = data;
        break;
    case ANC_DRCFF_LGAIN_SET:
        cfg->gains.drcff_lgain = data;
        break;
    case ANC_DRCFF_HGAIN_SET:
        cfg->gains.drcff_hgain = data;
        break;
    case ANC_DRCFF_NORGAIN_SET:
        cfg->gains.drcff_norgain = data;
        break;
    case ANC_DRCFB_LTHR_SET:
        cfg->gains.drcfb_lthr = data;
        break;
    case ANC_DRCFB_HTHR_SET:
        cfg->gains.drcfb_hthr = data;
        break;
    case ANC_DRCFB_LGAIN_SET:
        cfg->gains.drcfb_lgain = data;
        break;
    case ANC_DRCFB_HGAIN_SET:
        cfg->gains.drcfb_hgain = data;
        break;
    case ANC_DRCFB_NORGAIN_SET:
        cfg->gains.drcfb_norgain = data;
        break;
    case ANC_DRCTRANS_LTHR_SET:
        cfg->gains.drctrans_lthr = data;
        break;
    case ANC_DRCTRANS_HTHR_SET:
        cfg->gains.drctrans_hthr = data;
        break;
    case ANC_DRCTRANS_LGAIN_SET:
        cfg->gains.drctrans_lgain = data;
        break;
    case ANC_DRCTRANS_HGAIN_SET:
        cfg->gains.drctrans_hgain = data;
        break;
    case ANC_DRCTRANS_NORGAIN_SET:
        cfg->gains.drctrans_norgain = data;
        break;
    case ANC_AHS_DLY_SET:
        cfg->gains.ahs_dly = data;
        break;
    case ANC_AHS_TAP_SET:
        cfg->gains.ahs_tap = data;
        break;
    case ANC_AHS_WN_SHIFT_SET:
        cfg->gains.ahs_wn_shift = data;
        break;
    case ANC_AHS_WN_SUB_SET:
        cfg->gains.ahs_wn_sub = data;
        break;
    case ANC_AHS_SHIFT_SET:
        cfg->gains.ahs_shift = data;
        break;
    case ANC_AHS_U_SET:
        cfg->gains.ahs_u = data;
        break;
    case ANC_AHS_GAIN_SET:
        cfg->gains.ahs_gain = data;
        break;
    case ANC_FADE_STEP_SET:
        cfg->gains.fade_step = data;
        break;
    case ANC_AUDIO_DRC_THR_SET:
        cfg->gains.audio_drc_thr = *((float *)&data);
        break;
    case ANC_GAIN_SIGN2_SET:
        cfg->gains.gain_sign2 = data;
        break;
    case ANC_LTRANS2_GAIN_SET:
        cfg->gains.l_transgain2 =  *((float *)&data);
        break;
    case ANC_RTRANS2_GAIN_SET:
        cfg->gains.r_transgain2 = *((float *)&data);
        break;
    case ANC_HEARAID_HOWLDET_EN_SET:
        cfg->gains.sw_howl_en = data;
        break;
    case ANC_HEARAID_HOWLDET_THR_SET:
        cfg->gains.sw_howl_thr = data;
        break;
    case ANC_HEARAID_HOWLDET_GAIN_SET:
        cfg->gains.sw_howl_dither_thr = data;
        break;
    case ANC_HEARAID_HOWLDET_DITHER_THR_SET:
        cfg->gains.sw_howl_gain = data;
        break;
    default:
        break;
    }
    anc_cfg_online_deal(ANC_CFG_WRITE, cfg);
    free(cfg);
}

/*蓝牙手动训练读取增益接口*/
int anc_cfg_btspp_read(u8 id, int *data)
{
    int ret = 0;
    anc_gain_t *cfg;
    cfg = zalloc(sizeof(anc_gain_t));
    anc_cfg_online_deal(ANC_CFG_READ, cfg);
    switch (id) {
    case ANC_FFGAIN_SET:
        *data = *((int *)&cfg->gains.l_ffgain);
        break;
    case ANC_FBGAIN_SET:
        *data = *((int *)&cfg->gains.l_fbgain);
        break;
    case ANC_TRANS_GAIN_SET:
        *data = *((int *)&cfg->gains.l_transgain);
        break;
    case ANC_CMP_GAIN_SET:
        *data = *((int *)&cfg->gains.l_cmpgain);
        break;
    case ANC_RFF_GAIN_SET:
        *data = *((int *)&cfg->gains.r_ffgain);
        break;
    case ANC_RFB_GAIN_SET:
        *data = *((int *)&cfg->gains.r_fbgain);
        break;
    case ANC_RTRANS_GAIN_SET:
        *data = *((int *)&cfg->gains.r_transgain);
        break;
    case ANC_RCMP_GAIN_SET:
        *data = *((int *)&cfg->gains.r_cmpgain);
        break;
    default:
        ret = -1;
        break;
    }
    free(cfg);
    return ret;
}


/*ANC数字MIC IO配置*/
static atomic_t dmic_mux_ref;
#define DMIC_SCLK_FROM_PLNK		0
#define DMIC_SCLK_FROM_ANC		1
void dmic_io_mux_ctl(u8 en, u8 sclk_sel)
{
    user_anc_log("dmic_io_mux,en:%d,sclk:%d,ref:%d\n", en, sclk_sel, atomic_read(&dmic_mux_ref));
    if (en) {
        if (atomic_read(&dmic_mux_ref)) {
            user_anc_log("DMIC_IO_MUX open now\n");
            if (sclk_sel == DMIC_SCLK_FROM_ANC) {
                user_anc_log("plink_sclk -> anc_sclk\n");
                gpio_set_fun_output_port(TCFG_AUDIO_PLNK_SCLK_PIN, FO_ANC_MICCK, 0, 1, LOW_POWER_KEEP);
            }
            atomic_inc_return(&dmic_mux_ref);
            return;
        }
        if (sclk_sel == DMIC_SCLK_FROM_ANC) {
            gpio_set_fun_output_port(TCFG_AUDIO_PLNK_SCLK_PIN, FO_ANC_MICCK, 0, 1, LOW_POWER_KEEP);
        } else {
            gpio_set_fun_output_port(TCFG_AUDIO_PLNK_SCLK_PIN, FO_PLNK_SCLK, 0, 1, LOW_POWER_KEEP);
        }
        gpio_set_direction(TCFG_AUDIO_PLNK_SCLK_PIN, 0);
        gpio_set_die(TCFG_AUDIO_PLNK_SCLK_PIN, 0);
        gpio_direction_output(TCFG_AUDIO_PLNK_SCLK_PIN, 1);
#if TCFG_AUDIO_PLNK_DAT0_PIN != NO_CONFIG_PORT
        //anc data0 port init
        gpio_set_pull_down(TCFG_AUDIO_PLNK_DAT0_PIN, 0);
        gpio_set_pull_up(TCFG_AUDIO_PLNK_DAT0_PIN, 1);
        gpio_set_direction(TCFG_AUDIO_PLNK_DAT0_PIN, 1);
        gpio_set_die(TCFG_AUDIO_PLNK_DAT0_PIN, 1);
        gpio_set_fun_input_port(TCFG_AUDIO_PLNK_DAT0_PIN, PFI_PLNK_DAT0, LOW_POWER_KEEP);
#endif/*TCFG_AUDIO_PLNK_DAT0_PIN != NO_CONFIG_PORT*/

#if TCFG_AUDIO_PLNK_DAT1_PIN != NO_CONFIG_PORT
        //anc data1 port init
        gpio_set_pull_down(TCFG_AUDIO_PLNK_DAT1_PIN, 0);
        gpio_set_pull_up(TCFG_AUDIO_PLNK_DAT1_PIN, 1);
        gpio_set_direction(TCFG_AUDIO_PLNK_DAT1_PIN, 1);
        gpio_set_die(TCFG_AUDIO_PLNK_DAT1_PIN, 1);
        gpio_set_fun_input_port(TCFG_AUDIO_PLNK_DAT1_PIN, PFI_PLNK_DAT1, LOW_POWER_KEEP);
#endif/*TCFG_AUDIO_PLNK_DAT1_PIN != NO_CONFIG_PORT*/
        atomic_inc_return(&dmic_mux_ref);
    } else {
        if (atomic_read(&dmic_mux_ref)) {
            atomic_dec_return(&dmic_mux_ref);
            if (atomic_read(&dmic_mux_ref)) {
                if (sclk_sel == DMIC_SCLK_FROM_ANC) {
                    user_anc_log("anc close now,anc_sclk->plnk_sclk\n");
                    gpio_set_fun_output_port(TCFG_AUDIO_PLNK_SCLK_PIN, FO_PLNK_SCLK, 0, 1, LOW_POWER_KEEP);
                } else {
                    user_anc_log("plnk close,anc_plnk open\n");
                }
            } else {
                user_anc_log("dmic all close,disable plnk io_mapping output\n");
                gpio_disable_fun_output_port(TCFG_AUDIO_PLNK_SCLK_PIN);
#if TCFG_AUDIO_PLNK_DAT0_PIN != NO_CONFIG_PORT
                gpio_disable_fun_input_port(TCFG_AUDIO_PLNK_DAT0_PIN);
#endif/*TCFG_AUDIO_PLNK_DAT0_PIN != NO_CONFIG_PORT*/
#if TCFG_AUDIO_PLNK_DAT1_PIN != NO_CONFIG_PORT
                gpio_disable_fun_input_port(TCFG_AUDIO_PLNK_DAT1_PIN);
#endif/*TCFG_AUDIO_PLNK_DAT1_PIN != NO_CONFIG_PORT*/
            }
        } else {
            user_anc_log("dmic_mux_ref NULL\n");
        }
    }
}
void anc_dmic_io_init(audio_anc_t *param, u8 en)
{
    if (en) {
        int i;
        for (i = 0; i < 4; i++) {
            if ((param->mic_type[i] > A_MIC1) && (param->mic_type[i] != MIC_NULL)) {
                user_anc_log("anc_dmic_io_init %d:%d\n", i, param->mic_type[i]);
                dmic_io_mux_ctl(1, DMIC_SCLK_FROM_ANC);
                break;
            }
        }
    } else {
        dmic_io_mux_ctl(0, DMIC_SCLK_FROM_ANC);
    }
}

extern void mix_out_drc_threadhold_update(float threadhold);
static void anc_mix_out_audio_drc_thr(float thr)
{
    thr = (thr > 0.0) ? 0.0 : thr;
    thr = (thr < -6.0) ? -6.0 : thr;
    mix_out_drc_threadhold_update(thr);
}
#if TCFG_AUDIO_DYNAMIC_ADC_GAIN
void anc_dynamic_micgain_start(u8 audio_mic_gain)
{
    int anc_mic_gain, i;
    float ffgain;
    float multiple = 1.259;

    if (!anc_status_get()) {
        return;
    }
    anc_hdl->drc_ratio = 1.0;
    ffgain = (float)(anc_hdl->param.mode == ANC_TRANSPARENCY ? anc_hdl->param.gains.drctrans_norgain : anc_hdl->param.gains.drcff_norgain);
    //查询遍历当前的通话共用MIC
    if (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_L) {
        for (i = 0; i < 4; i += 2) {
            if (anc_hdl->param.mic_type[i] == A_MIC0) {
                anc_hdl->dy_mic_ch = A_MIC0;
                anc_mic_gain = anc_hdl->param.gains.ref_mic_gain;
            }
        }
    }
    if (TCFG_AUDIO_ADC_MIC_CHA & LADC_CH_MIC_R) {
        for (i = 0; i < 4; i += 2) {
            if (anc_hdl->param.mic_type[i] == A_MIC1) {
                anc_hdl->dy_mic_ch = A_MIC1;
                anc_mic_gain = anc_hdl->param.gains.err_mic_gain;
            }
        }
    }
    if (anc_hdl->dy_mic_ch == MIC_NULL) {
        user_anc_log("There is no common MIC\n");
        return;
    }
    if (anc_hdl->mic_resume_timer) {
        sys_timer_del(anc_hdl->mic_resume_timer);
    }
    anc_hdl->mic_diff_gain = audio_mic_gain - anc_mic_gain;
    if (anc_hdl->mic_diff_gain > 0) {
        for (i = 0; i < anc_hdl->mic_diff_gain; i++) {
            ffgain /= multiple;
            anc_hdl->drc_ratio *= multiple;
        }
    } else {
        for (i = 0; i > anc_hdl->mic_diff_gain; i--) {
            ffgain *= multiple;
            anc_hdl->drc_ratio /= multiple;
        }
    }
    audio_anc_drc_ctrl(&anc_hdl->param, anc_hdl->drc_ratio);
    if (ANC_CH == (ANC_L_CH | ANC_R_CH)) {	//需保持两个MIC增益一致
        audio_anc_mic_gain(app_var.aec_mic_gain, app_var.aec_mic_gain);
    }
    user_anc_log("dynamic mic:audio_g %d,anc_g %d, diff_g %d, ff_gain %d, \n", audio_mic_gain, anc_mic_gain, anc_hdl->mic_diff_gain, (s16)ffgain);
}

void anc_dynamic_micgain_resume_timer(void *priv)
{
    float multiple = 1.259;
    if (anc_hdl->mic_diff_gain > 0) {
        anc_hdl->mic_diff_gain--;
        anc_hdl->drc_ratio /= multiple;
        if (!anc_hdl->mic_diff_gain) {
            anc_hdl->drc_ratio = 1.0;
        }
        audio_anc_drc_ctrl(&anc_hdl->param, anc_hdl->drc_ratio);
        if (ANC_CH == (ANC_L_CH | ANC_R_CH)) {	//需保持两个MIC增益一致
            audio_anc_mic_gain(anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain, anc_hdl->param.gains.err_mic_gain + anc_hdl->mic_diff_gain);
            /* user_anc_log("mic0 gain / mic1 gain %d\n", anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain); */
        } else if (anc_hdl->dy_mic_ch == A_MIC0) {
            audio_anc_mic_gain(anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain, anc_hdl->param.gains.err_mic_gain);
            /* user_anc_log("mic0 gain %d\n", anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain); */
        } else {
            audio_anc_mic_gain(anc_hdl->param.gains.ref_mic_gain, anc_hdl->param.gains.err_mic_gain + anc_hdl->mic_diff_gain);
            /* user_anc_log("mic1 gain %d\n", anc_hdl->param.gains.err_mic_gain + anc_hdl->mic_diff_gain); */
        }
    } else if (anc_hdl->mic_diff_gain < 0) {
        anc_hdl->mic_diff_gain++;
        anc_hdl->drc_ratio *= multiple;
        if (!anc_hdl->mic_diff_gain) {
            anc_hdl->drc_ratio = 1.0;
        }
        audio_anc_drc_ctrl(&anc_hdl->param, anc_hdl->drc_ratio);
        if (ANC_CH == ANC_L_CH | ANC_R_CH) {	//需保持两个MIC增益一致
            audio_anc_mic_gain(anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain, anc_hdl->param.gains.err_mic_gain + anc_hdl->mic_diff_gain);
            /* user_anc_log("mic0 gain / mic1 gain %d\n", anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain); */
        } else if (anc_hdl->dy_mic_ch == A_MIC0) {
            audio_anc_mic_gain(anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain, anc_hdl->param.gains.err_mic_gain);
            /* user_anc_log("mic0 gain %d\n", anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain); */
        } else {
            audio_anc_mic_gain(anc_hdl->param.gains.ref_mic_gain, anc_hdl->param.gains.err_mic_gain + anc_hdl->mic_diff_gain);
            /* user_anc_log("mic1 gain %d\n", anc_hdl->param.gains.ref_mic_gain + anc_hdl->mic_diff_gain); */
        }
    } else {
        sys_timer_del(anc_hdl->mic_resume_timer);
        anc_hdl->mic_resume_timer = 0;
    }
}

void anc_dynamic_micgain_stop(void)
{
    if ((anc_hdl->dy_mic_ch != MIC_NULL) && anc_status_get()) {
        anc_hdl->mic_resume_timer = sys_timer_add(NULL, anc_dynamic_micgain_resume_timer, 10);
    }
}
#endif/*TCFG_AUDIO_DYNAMIC_ADC_GAIN*/

void audio_anc_post_msg_drc(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_DRC_TIMER);
}

void audio_anc_post_msg_debug(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_DEBUG_OUTPUT);
}

#if ANC_MUSIC_DYNAMIC_GAIN_EN

#define ANC_MUSIC_DYNAMIC_GAIN_NORGAIN				1024	/*正常默认增益*/
#define ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL		6		/*音乐响度取样间隔*/

#if ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB
void audio_anc_music_dyn_gain_resume_timer(void *priv)
{
    if (anc_hdl->loud_nowgain < anc_hdl->loud_targetgain) {
        anc_hdl->loud_nowgain += 5;      ///+5耗时194 * 10ms
        if (anc_hdl->loud_nowgain >= anc_hdl->loud_targetgain) {
            anc_hdl->loud_nowgain = anc_hdl->loud_targetgain;
            sys_timer_del(anc_hdl->loud_timeid);
            anc_hdl->loud_timeid = 0;
        }
    } else if (anc_hdl->loud_nowgain > anc_hdl->loud_targetgain) {
        anc_hdl->loud_nowgain -= 5;      ///-5耗时194 * 10ms
        if (anc_hdl->loud_nowgain <= anc_hdl->loud_targetgain) {
            anc_hdl->loud_nowgain = anc_hdl->loud_targetgain;
            sys_timer_del(anc_hdl->loud_timeid);
            anc_hdl->loud_timeid = 0;
        }
    }
    audio_anc_norgain(anc_hdl->param.gains.drcff_norgain, anc_hdl->loud_nowgain);
    /* user_anc_log("fb resume anc_hdl->loud_nowgain %d\n", anc_hdl->loud_nowgain); */
}

#endif/*ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB*/

void audio_anc_music_dynamic_gain_process(void)
{
    if (anc_hdl->loud_nowgain != anc_hdl->loud_targetgain) {
//        user_anc_log("low_nowgain %d, targergain %d\n", anc_hdl->loud_nowgain, anc_hdl->loud_targetgain);
#if ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB
        if (!anc_hdl->loud_timeid) {
            anc_hdl->loud_timeid = sys_timer_add(NULL, audio_anc_music_dyn_gain_resume_timer, 10);
        }
#else
        anc_hdl->loud_nowgain = anc_hdl->loud_targetgain;
        audio_anc_fade(anc_hdl->loud_targetgain << 4, anc_hdl->param.anc_fade_en, 1);
#endif/*ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB*/
    }
}

void audio_anc_music_dynamic_gain_reset(u8 effect_reset_en)
{
    if (anc_hdl->loud_targetgain != ANC_MUSIC_DYNAMIC_GAIN_NORGAIN) {
        /* user_anc_log("audio_anc_music_dynamic_gain_reset\n"); */
        anc_hdl->loud_targetgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
        if (anc_hdl->loud_timeid) {
            sys_timer_del(anc_hdl->loud_timeid);
            anc_hdl->loud_timeid = 0;
        }
        if (effect_reset_en && anc_hdl->param.mode == ANC_ON) {
            os_taskq_post_msg("anc", 1, ANC_MSG_MUSIC_DYN_GAIN);
        } else {
            anc_hdl->loud_nowgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
            anc_hdl->loud_dB = 1;	//清除上次保留的dB值, 避免切模式后不触发的情况
            anc_hdl->loud_hdl.peak_val = ANC_MUSIC_DYNAMIC_GAIN_THR;
        }
    }
}

void audio_anc_music_dynamic_gain_dB_get(int dB)
{
    float mult = 1.122f;
    float target_mult = 1.0f;
    int cnt, i, gain;
#if ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL > 1 //区间变化，避免频繁操作
    if (dB > ANC_MUSIC_DYNAMIC_GAIN_THR) {
        int temp_dB = dB / ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL;
        dB = temp_dB * ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL;
    }
#endif/*ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL*/
    if (anc_hdl->loud_dB != dB) {
        anc_hdl->loud_dB = dB;
        if (dB > ANC_MUSIC_DYNAMIC_GAIN_THR) {
            cnt = dB - ANC_MUSIC_DYNAMIC_GAIN_THR;
            for (i = 0; i < cnt; i++) {
                target_mult *= mult;
            }
            gain = (int)((float)ANC_MUSIC_DYNAMIC_GAIN_NORGAIN / target_mult);
//            user_anc_log("cnt %d, dB %d, gain %d\n", cnt, dB, gain);
            anc_hdl->loud_targetgain = gain;
            os_taskq_post_msg("anc", 1, ANC_MSG_MUSIC_DYN_GAIN);
        } else {
            audio_anc_music_dynamic_gain_reset(1);
        }
    }
}

void audio_anc_music_dynamic_gain_det(s16 *data, int len)
{
    /* putchar('l'); */
    if (anc_hdl->param.mode == ANC_ON) {
        loudness_meter_short(&anc_hdl->loud_hdl, data, len >> 1);
        audio_anc_music_dynamic_gain_dB_get(anc_hdl->loud_hdl.peak_val);
    }
}


void audio_anc_music_dynamic_gain_init(int sr)
{
    /* user_anc_log("audio_anc_music_dynamic_gain_init %d\n", sr); */
    anc_hdl->loud_nowgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
    anc_hdl->loud_targetgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
    anc_hdl->loud_dB = 1;     //清除上次保留的dB值
    loudness_meter_init(&anc_hdl->loud_hdl, sr, 50, 0);
    anc_hdl->loud_hdl.peak_val = ANC_MUSIC_DYNAMIC_GAIN_THR;
}
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/

#else

/*获取anc状态，0:空闲，l:忙*/
u8 anc_status_get(void)
{
    return 0;
}

#endif/*TCFG_AUDIO_ANC_ENABLE*/
