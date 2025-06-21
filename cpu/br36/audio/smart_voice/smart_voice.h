/*****************************************************************
>file name : smart_voice.h
>create time : Thu 17 Jun 2021 02:07:32 PM CST
*****************************************************************/
#ifndef _SMART_VOICE_H_
#define _SMART_VOICE_H_
#include "media/includes.h"
#include "app_config.h"

#define SMART_VOICE_MSG_WAKE              0 /*唤醒*/
#define SMART_VOICE_MSG_STANDBY           1 /*待机*/
#define SMART_VOICE_MSG_DMA               2 /*语音数据DMA传输*/
#define SMART_VOICE_MSG_MIC_OPEN          3 /*MIC打开*/
#define SMART_VOICE_MSG_SWITCH_SOURCE     4 /*MIC切换*/
#define SMART_VOICE_MSG_MIC_CLOSE         5 /*MIC关闭*/

#if (TCFG_AUDIO_KWS_LANGUAGE_SEL == KWS_CH)
#include "tech_lib/jlsp_kws.h"
#define audio_kws_model_get_heap_size   jl_kws_model_get_heap_size
#define audio_kws_model_init            jl_kws_model_init
#define audio_kws_model_reset           jl_kws_model_reset
#define audio_kws_model_process         jl_kws_model_process
#define audio_kws_model_free            jl_kws_model_free
#else
#include "tech_lib/jlsp_india_english.h"
#define audio_kws_model_get_heap_size   jl_india_english_model_get_heap_size
#define audio_kws_model_init            jl_india_english_model_init
#define audio_kws_model_reset           jl_india_english_model_reset
#define audio_kws_model_process         jl_india_english_model_process
#define audio_kws_model_free            jl_india_english_model_free
#endif /*TCFG_AUDIO_KWS_LANGUAGE_SEL*/

/*
*********************************************************************
*       audio smart voice detect create
* Description: 创建智能语音检测引擎
* Arguments  : model         - KWS模型
*			   task_name     - VAD引擎task
*			   mic           - MIC的选择（低功耗MIC或主控MIC）
*			   buffer_size   - MIC的DMA数据缓冲大小
* Return	 : 0 - 创建成功，非0 - 失败.
* Note(s)    : None.
*********************************************************************
*/
int audio_smart_voice_detect_create(u8 model, const char *task_name, u8 mic, int buffer_size);

/*
*********************************************************************
*       audio smart voice detect open
* Description: 智能语音检测打开接口
* Arguments  : mic           - MIC的选择（低功耗MIC或主控MIC）
* Return	 : void.
* Note(s)    : None.
*********************************************************************
*/
void audio_smart_voice_detect_open(u8 mic);

/*
*********************************************************************
*       audio smart voice detect close
* Description: 智能语音检测关闭接口
* Arguments  : void.
* Return	 : void.
* Note(s)    : 关闭引擎的所有资源（无论使用哪个mic）.
*********************************************************************
*/
void audio_smart_voice_detect_close(void);

/*
*********************************************************************
*       audio smart voice detect init
* Description: 智能语音检测配置初始化
* Arguments  : void.
* Return	 : void.
* Note(s)    : None.
*********************************************************************
*/
int audio_smart_voice_detect_init(void);

/*
*********************************************************************
*       audio phone call kws start
* Description: 启动通话来电关键词识别
* Arguments  : void.
* Return	 : 0 - 成功，非0 - 失败.
* Note(s)    : 接口会将VAD的低功耗mic切换至通话使用的主控mic.
*********************************************************************
*/
int audio_phone_call_kws_start(void);

/*
*********************************************************************
*       audio phone call kws close
* Description: 关闭通话来电关键词识别
* Arguments  : void.
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : 关闭来电关键词识别，通常用于接通后或挂断/拒接后.
*********************************************************************
*/
int audio_phone_call_kws_close(void);

/*
*********************************************************************
*       smart voice idle query
* Description: 获取语音识别状态
* Arguments  : void.
* Return	 : 1 - 关闭，0 - 开启.
* Note(s)    : None.
*********************************************************************
*/
u8 smart_voice_idle_query(void);

#endif
