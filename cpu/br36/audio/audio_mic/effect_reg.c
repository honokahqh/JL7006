#include "effect_reg.h"
#include "app_config.h"
/* #include "audio_enc/audio_enc.h" */
/**********************混响功能选配*********************************/
#if (TCFG_MIC_EFFECT_ENABLE)
#define MIC_EFFECT_CONFIG 	  	  0	\
								| BIT(MIC_EFFECT_CONFIG_EQ) \
								| BIT(MIC_EFFECT_CONFIG_NOISEGATE) \
								/* | BIT(MIC_EFFECT_CONFIG_PITCH) \ */
/* | BIT(MIC_EFFECT_CONFIG_HOWLING) \ */
/* | BIT(MIC_EFFECT_CONFIG_HOWLING_TRAP) \ */
/* | BIT(MIC_EFFECT_CONFIG_ENERGY_DETECT) \ */
/* | BIT(MIC_EFFECT_CONFIG_DVOL) \ */

/*********************************************************************/

const struct __mic_effect_parm effect_parm_default = {
    .effect_config = MIC_EFFECT_CONFIG,///混响通路支持哪些功能
    .effect_run = MIC_EFFECT_CONFIG,///混响打开之时， 默认打开的功能
    .sample_rate = MIC_EFFECT_SAMPLERATE,
};

const struct __mic_stream_parm effect_mic_stream_parm_default = {
    .mic_gain			= 12,
    .sample_rate 		= MIC_EFFECT_SAMPLERATE,//采样率
    .point_unit  		= 256,//一次处理数据的数据单元， 单位点 4对齐(要配合mic起中断点数修改)
    .dac_delay			= 12,//dac硬件混响延时， 单位ms 要大于 point_unit*2
};

const EF_REVERB_FIX_PARM effect_echo_fix_parm_default = {
    .sr = MIC_EFFECT_SAMPLERATE,		////采样率
    .max_ms = 200,				////所需要的最大延时，影响 need_buf 大小
};

const ECHO_PARM_SET  effect_echo_parm_default = {
    .delay = 200,
    .decayval = 45,
    .filt_enable = 1,
    .lpf_cutoff = 5000,
    .wetgain = 50,
    .drygain = 100,
};

const  PITCH_SHIFT_PARM effect_pitch_parm_default = {
    .sr = MIC_EFFECT_SAMPLERATE,
    .shiftv = 56,
    .effect_v = EFFECT_PITCH_SHIFT,
    .formant_shift = 0,
};

const NoiseGateParam effect_noisegate_parm_default = {
    .attackTime = 300,
    .releaseTime = 5,
    .threshold = -45000,
    .low_th_gain = 0,
    .sampleRate = MIC_EFFECT_SAMPLERATE,
    .channel = 1,
};

HOWLING_PARM_SET howling_param_default = {
    .threshold = 25,
    .sample_rate = MIC_EFFECT_SAMPLERATE,
    .channel = 1,
    .fade_time = 10,
    .notch_Q = 2.0f,
    .notch_gain = -20.0f,
};

#if 0
static const u16 r_dvol_table[] = {
    0	, //0
    93	, //1
    111	, //2
    132	, //3
    158	, //4
    189	, //5
    226	, //6
    270	, //7
    323	, //8
    386	, //9
    462	, //10
    552	, //11
    660	, //12
    789	, //13
    943	, //14
    1127, //15
    1347, //16
    1610, //17
    1925, //18
    2301, //19
    2751, //20
    3288, //21
    3930, //22
    4698, //23
    5616, //24
    6713, //25
    8025, //26
    9592, //27
    11466,//28
    15200,//29
    16000,//30
    16384 //31
};

audio_dig_vol_param effect_dvol_default_parm = {
#if (SOUNDCARD_ENABLE)
    .vol_start = 0,//30,
#else
    .vol_start = 30,
#endif
    .vol_max = ARRAY_SIZE(r_dvol_table) - 1,
    .ch_total = 1,
    .fade_en = 1,
    .fade_points_step = 2,
    .fade_gain_step = 2,
    .vol_list = (void *)r_dvol_table,
};

#endif


const struct __effect_dodge_parm dodge_parm = {
    .dodge_in_thread = 1000,//触发闪避的能量阈值
    .dodge_in_time_ms = 1,//能量值持续大于dodge_in_thread 就触发闪避
    .dodge_out_thread = 1000,//退出闪避的能量阈值
    .dodge_out_time_ms = 500,//能量值持续小于dodge_out_thread 就退出闪避
};



#endif//TCFG_MIC_EFFECT_ENABLE


