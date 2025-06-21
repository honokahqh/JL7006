/*****************************************************************
>file name : smart_voice_core.c
>author : lichao
>create time : Mon 01 Nov 2021 11:35:15 AM CST
*****************************************************************/
#define LOG_TAG         "[Smart-Voice]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_WARN_ENABLE
#include "debug.h"
#include "includes.h"
#include "voice_mic_data.h"
#include "kws_event.h"
#include "smart_voice.h"
/*#include "nn_vad.h"*/
#include "media/jl_kws.h"
#include "audio_codec_clock.h"

#define SMART_VOICE_TEST_LISTEN_SOUND     0
#define SMART_VOICE_TEST_PRINT_PCM        0
#define SMART_VOICE_TEST_WRITE_FILE       0
#define SMART_VOICE_DEBUG_KWS_RESULT      0
#define AUDIO_NN_VAD_ENABLE             0

#define SMART_VOICE_SAMPLE_RATE           16000
#define SMART_VOICE_REC_MIC_SECS          8
#define SMART_VOICE_REC_DATA_LEN          (SMART_VOICE_SAMPLE_RATE * SMART_VOICE_REC_MIC_SECS * 2)

#define SMART_VOICE_KWS_FRAME_LEN     (320)

#if SMART_VOICE_TEST_WRITE_FILE
#define VOICE_DATA_BUFFER_SIZE     16 * 1024
#elif SMART_VOICE_TEST_PRINT_PCM
#define VOICE_DATA_BUFFER_SIZE     SMART_VOICE_REC_DATA_LEN
#else
#define VOICE_DATA_BUFFER_SIZE     2 * 1024
#endif

#if ((defined TCFG_AUDIO_DATA_EXPORT_ENABLE && TCFG_AUDIO_DATA_EXPORT_ENABLE) || SMART_VOICE_TEST_WRITE_FILE)
#define CONFIG_VAD_KWS_DETECT_ENABLE    0
#else
#define CONFIG_VAD_KWS_DETECT_ENABLE    1
#endif

extern u8 config_audio_kws_enable;
/*
 * Audio语音识别
 */
struct smart_voice_context {
    u8 kws_model;
    void *kws;
    void *mic;
    void *nn_vad;
#if SMART_VOICE_TEST_WRITE_FILE
    void *file;
#endif
#if SMART_VOICE_DEBUG_KWS_RESULT
    void *dump_hdl;
#endif
};

static struct smart_voice_context *this_sv = NULL;
static u8 volatile smart_voice_wakeup = 0;
#if SMART_VOICE_TEST_LISTEN_SOUND
#include "audio_config.h"
extern struct audio_dac_hdl dac_hdl;
#endif

static inline void smart_voice_data_listen_sound(void *data, int len)
{
#if SMART_VOICE_TEST_LISTEN_SOUND
    if (audio_dac_is_working(&dac_hdl)) {
        audio_dac_write(&dac_hdl, data, len);
    }
#endif
}

static inline void smart_voice_data_write_file(struct smart_voice_context *sv, void *data, int len)
{
#if SMART_VOICE_TEST_WRITE_FILE
    if (sv->file) {
        fwrite(sv->file, data, sizeof(data));
    }
#endif
}

static void smart_voice_kws_close(struct smart_voice_context *sv)
{
#if AUDIO_NN_VAD_ENABLE
    if (sv->nn_vad) {
        audio_nn_vad_close(sv->nn_vad);
        sv->nn_vad = NULL;
    }
#else
    if (sv->kws) {
        audio_kws_close(sv->kws);
        sv->kws = NULL;
    }
#endif
}

static void smart_voice_kws_open(struct smart_voice_context *sv, u8 model)
{
#if AUDIO_NN_VAD_ENABLE
    if (sv->nn_vad) {
        return;
    }
    sv->nn_vad = audio_nn_vad_open();
#else
    if (sv->kws) {
        if (sv->kws_model == model) {
            return;
        }
        smart_voice_kws_close(sv);
    }

    sv->kws_model = model;
    sv->kws = audio_kws_open(model, NULL);//kws_model_files[model]);
#endif
}

/*
 * 语音识别的KWS处理
 */
static int smart_voice_data_handler(struct smart_voice_context *sv)
{
    int result = 0;
    if (!config_audio_kws_enable) {
        return 0;
    }

#if SMART_VOICE_TEST_PRINT_PCM
    putchar('*');
    return 1;
#endif
    if (!sv->mic) {
        return -EINVAL;
    }

    s16 data[SMART_VOICE_KWS_FRAME_LEN / 2];
    int rlen = voice_mic_data_read(sv->mic, data, SMART_VOICE_KWS_FRAME_LEN);
    if (rlen < SMART_VOICE_KWS_FRAME_LEN) {
        return -EINVAL;
    }

    if (sv->nn_vad) {
#if AUDIO_NN_VAD_ENABLE
        audio_nn_vad_data_handler(sv->nn_vad, data, sizeof(data));
#endif
    }

    if (sv->kws) {
        /*putchar('*');*/
        smart_voice_data_listen_sound(data, sizeof(data));
        smart_voice_data_write_file(sv, data, sizeof(data));
#if (CONFIG_VAD_KWS_DETECT_ENABLE)
        result = audio_kws_detect_handler(sv->kws, (void *)data, sizeof(data));
        if (result > 1) {
            printf("result : %d\n", result);
        }
        smart_voice_kws_event_handler(sv->kws_model, result);
#endif
#if SMART_VOICE_DEBUG_KWS_RESULT
        smart_voice_kws_dump_result_add(sv->dump_hdl, sv->kws_model, result);
#endif
    }


    return 0;
}


/*
 * 智能唤醒和语音识别处理任务
 */
static void smart_voice_core_task(void *arg)
{
    int msg[16];
    int res;
    u8 pend_taskq = 1;
    struct smart_voice_context *sv = (struct smart_voice_context *)arg;
    int err = 0;
    smart_voice_wakeup = 0;

    while (config_audio_kws_enable) {
        if (pend_taskq) {
            res = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        } else {
            res = os_taskq_accept(ARRAY_SIZE(msg), msg);
        }

        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case SMART_VOICE_MSG_MIC_OPEN:
                sv->mic = voice_mic_data_open(msg[2], msg[3], msg[4]);
                if (!sv->mic) {
                    printf("VAD mic open failed.\n");
                }
                smart_voice_kws_open(sv, msg[5]);
                break;
            case SMART_VOICE_MSG_SWITCH_SOURCE:
                /*os_taskq_flush();*/
                if (sv->mic) {
                    voice_mic_data_clear(sv->mic);
                    voice_mic_data_switch_source(sv->mic, msg[2], msg[3], msg[4]);
                }
                smart_voice_kws_open(sv, msg[5]);
                break;
            case SMART_VOICE_MSG_MIC_CLOSE:
                smart_voice_kws_close(sv);
                voice_mic_data_close(sv->mic);
                sv->mic = NULL;
                os_sem_post((OS_SEM *)msg[2]);
                break;
            case SMART_VOICE_MSG_WAKE:
                /*putchar('W');*/
#if SMART_VOICE_TEST_WRITE_FILE
                sv->file = fopen("storage/sd0/C/AudioVAD/vad***.raw", "w+");
                if (!sv->file) {
                    printf("Open file failed, can not test.\n");
                }
#endif
                pend_taskq = 0;
                smart_voice_wakeup = 1;
                break;
            case SMART_VOICE_MSG_STANDBY:
                pend_taskq = 1;
                smart_voice_wakeup = 0;
                if (sv->mic) {
#if SMART_VOICE_TEST_PRINT_PCM
                    voice_mic_data_dump(sv->mic);
#endif
                    voice_mic_data_clear(sv->mic);
                }
#if SMART_VOICE_TEST_WRITE_FILE
                if (sv->file) {
                    fclose(sv->file);
                    sv->file = NULL;
                }
#endif
                break;
            case SMART_VOICE_MSG_DMA:
                pend_taskq = 0;
                msg[0] = (int)audio_codec_clock_check;
                msg[1] = 0;
                os_taskq_post_type("app_core", Q_CALLBACK, 2, msg);
                /*audio_codec_clock_check();*/
                break;
            }
        }

        if (smart_voice_wakeup) {
            err = smart_voice_data_handler(sv);
            if (err) {
                pend_taskq = 1;
            }
        }
    }
}

int audio_smart_voice_detect_create(u8 model, const char *task_name, u8 mic, int buffer_size)
{
    int err = 0;

    if (!config_audio_kws_enable) {
        return 0;
    }

    if (this_sv) {
        os_taskq_post_msg("audio_vad",
                          5,
                          SMART_VOICE_MSG_SWITCH_SOURCE,
                          buffer_size,
                          SMART_VOICE_SAMPLE_RATE,
                          mic,
                          model);
        return 0;
    }

    struct smart_voice_context *sv = (struct smart_voice_context *)zalloc(sizeof(struct smart_voice_context));
    if (!sv) {
        goto __err;
    }

    err = task_create(smart_voice_core_task, sv, task_name);
    if (err != OS_NO_ERR) {
        goto __err;
    }

    audio_codec_clock_set(SMART_VOICE_MODE, AUDIO_CODING_PCM, 0);//TODO:先将VAD后台时钟设置放到app_core流程调用避免设置时钟临界的情况，待clk_set完善后再放到task动态设置

    os_taskq_post_msg("audio_vad",
                      5,
                      SMART_VOICE_MSG_MIC_OPEN,
                      mic,
                      buffer_size,
                      SMART_VOICE_SAMPLE_RATE,
                      model);

#if SMART_VOICE_DEBUG_KWS_RESULT
    if (!sv->dump_hdl) {
        sv->dump_hdl = smart_voice_kws_dump_open(2000);
    }
#endif
    this_sv = sv;
    return 0;
__err:
    if (sv) {
        free(sv);
    }
    return err;
}

void audio_smart_voice_detect_close(void)
{
    if (config_audio_kws_enable && this_sv) {
#if SMART_VOICE_DEBUG_KWS_RESULT
        smart_voice_kws_dump_close(this_sv->dump_hdl);
#endif
        OS_SEM *sem = (OS_SEM *)malloc(sizeof(OS_SEM));
        os_sem_create(sem, 0);
        os_taskq_post_msg("audio_vad", 2, SMART_VOICE_MSG_MIC_CLOSE, (int)sem);
        os_sem_pend(sem, 0);
        free(sem);
        audio_codec_clock_del(SMART_VOICE_MODE);
        task_kill("audio_vad");
        free(this_sv);
        this_sv = NULL;
    }
}

static void __audio_smart_voice_detect_open(u8 mic, u8 model)
{
#if SMART_VOICE_TEST_WRITE_FILE
    extern void force_set_sd_online(char *sdx);
    force_set_sd_online("sd0");
    void *mnt = mount("sd0", "storage/sd0", "fat", 3, NULL);
    if (!mnt) {
        printf("sd0 mount fat failed.\n");
    }
#endif
    audio_smart_voice_detect_create(model, "audio_vad", mic, VOICE_DATA_BUFFER_SIZE);
}

void audio_smart_voice_detect_open(u8 model)
{
    return __audio_smart_voice_detect_open(VOICE_MCU_MIC, model);
}

extern void get_task_state(void *parm);
static void system_dump_os_state(void *arg)
{
    /*get_task_state(NULL); */
}

int audio_smart_voice_detect_init(void)
{
    /*sys_timer_add(NULL, system_dump_os_state, 2000);*/
    __audio_smart_voice_detect_open(VOICE_MCU_MIC, JL_KWS_COMMAND_KEYWORD);
    return 0;
}

/*
 * 来电KWS关键词识别
 */
int audio_phone_call_kws_start(void)
{
    if (this_sv) {
        os_taskq_post_msg("audio_vad",
                          5,
                          SMART_VOICE_MSG_SWITCH_SOURCE,
                          VOICE_DATA_BUFFER_SIZE,
                          SMART_VOICE_SAMPLE_RATE,
                          VOICE_MCU_MIC,
                          JL_KWS_CALL_KEYWORD);
        return 0;
    }

    __audio_smart_voice_detect_open(VOICE_MCU_MIC, JL_KWS_CALL_KEYWORD);
    return 0;
}

/*
 * 来电KWS关闭（接通或拒接）
 */
int audio_phone_call_kws_close(void)
{
    if (this_sv) {
        os_taskq_post_msg("audio_vad",
                          5,
                          SMART_VOICE_MSG_SWITCH_SOURCE,
                          VOICE_DATA_BUFFER_SIZE,
                          SMART_VOICE_SAMPLE_RATE,
                          VOICE_MCU_MIC,
                          JL_KWS_COMMAND_KEYWORD);
    }

    return 0;
}

/*
 * 获取KWS状态
 * return: 1 关闭 0 开启
 */
u8 smart_voice_idle_query(void)
{
    return !smart_voice_wakeup;
}

static enum LOW_POWER_LEVEL smart_voice_level_query(void)
{
    return LOW_POWER_MODE_SLEEP;
}

REGISTER_LP_TARGET(smart_voice_lp_target) = {
    .name = "smart_voice",
    .level = smart_voice_level_query,
    .is_idle = smart_voice_idle_query,
};

int p11_dvad_trigger_test(void)
{
    /*audio_vad_m2p_event_post(M2P_VAD_CMD_TEST);*/
    return 0;
}

void audio_vad_test(void)
{
#if SMART_VOICE_TEST_LISTEN_SOUND
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
    audio_dac_set_sample_rate(&dac_hdl, SMART_VOICE_SAMPLE_RATE);
    audio_dac_set_volume(&dac_hdl, 15);
    audio_dac_start(&dac_hdl);
    audio_vad_m2p_event_post(M2P_VAD_CMD_TEST);
    while (1) {
        os_time_dly(10);
    }
#endif
}

static struct kws_multi_keyword_model kws_model_api = {
    .mem_dump = audio_kws_model_get_heap_size,
    .init = audio_kws_model_init,
    .reset = audio_kws_model_reset,
    .process = audio_kws_model_process,
    .free = audio_kws_model_free,
};

void get_kws_api(struct kws_multi_keyword_model *api)
{
    memcpy(api, &kws_model_api, sizeof(struct kws_multi_keyword_model));
}
