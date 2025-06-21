#ifndef AUDIO_HEARING_AID_H
#define AUDIO_HEARING_AID_H

#include "anc.h"
//助听器啸叫抑制信号量
enum {
    HEARAID_HOWL_MSG_START = 0,
    HEARAID_HOWL_MSG_STOP,
    HEARAID_HOWL_MSG_RUN,
};

//助听器啸叫检测状态
enum {
    HEARAID_HOWL_STA_NORMAL = 0,	//正常
    HEARAID_HOWL_STA_RESUME,		//恢复
    HEARAID_HOWL_STA_HOLD,			//压制
};

void audio_anc_hearing_aid_howldet_start(audio_anc_t *param);

void audio_anc_hearing_aid_howldet_stop(void);

void audio_anc_hearing_aid_param_update(audio_anc_t *anc_param, u8 reset_en);

#endif/*AUDIO_HEARING_AID_H*/
