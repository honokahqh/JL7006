/*****************************************************************
>file name : audio_codec_clock.c
>create time : Thu 03 Jun 2021 09:36:12 AM CST
>History :
 2021-11-19 : 增加后台叠加时钟的配置，用于满足一些后台运行的
              音频处理
*****************************************************************/
#define LOG_TAG         "[CODEC_CLK]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_WARN_ENABLE
#include "debug.h"
#include "app_config.h"
#include "system/includes.h"
#include "audio_codec_clock.h"
#include "audio_dec_eff.h"
#include "asm/dac.h"

#define AUDIO_CODING_ALL        (0xffffffff)
#define MAX_CODING_TYPE_NUM     4 //可根据模式的解码格式需求扩展
#define AUDIO_CODEC_BASE_CLK    SYS_24M

static LIST_HEAD(codec_clock_head);

struct audio_codec_clock {
    u32 coding_type;
    u32 clk;
    u8  background; //后台运行的时钟
};

struct audio_codec_clk_context {
    u8 mode;
    struct audio_codec_clock params;
    struct list_head entry;
};


const struct audio_codec_clock audio_clock[AUDIO_MAX_MODE][MAX_CODING_TYPE_NUM] = {
    {
        //A2DP_MODE
        {
            AUDIO_CODING_SBC,
#if TCFG_AUDIO_OUTPUT_IIS
            SYS_60M,
#else
#if defined(AUDIO_SPK_EQ_CONFIG) && AUDIO_SPK_EQ_CONFIG || defined(TCFG_AUDIO_MDRC_ENABLE) && TCFG_AUDIO_MDRC_ENABLE
            SYS_76M,
#elif (AUDIO_SURROUND_CONFIG && (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)) || (defined(TCFG_AUDIO_DAC_24BIT_MODE) && TCFG_AUDIO_DAC_24BIT_MODE)
            SYS_76M,
#elif  (TCFG_BT_MUSIC_EQ_ENABLE && ((EQ_SECTION_MAX >= 10) && AUDIO_OUT_EFFECT_ENABLE)) || (AUDIO_VBASS_CONFIG || AUDIO_SURROUND_CONFIG)
            SYS_60M,
#elif (TCFG_BT_MUSIC_EQ_ENABLE &&  TCFG_AUDIO_OUT_EQ_ENABLE && TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR && ((EQ_SECTION_MAX >= 10) || AUDIO_OUT_EFFECT_ENABLE)) && (TCFG_DRC_ENABLE || TCFG_AUDIO_ANC_ENABLE)
            SYS_60M,
#elif (TCFG_BT_MUSIC_EQ_ENABLE && ((EQ_SECTION_MAX >= 10) || AUDIO_OUT_EFFECT_ENABLE) || TCFG_DRC_ENABLE || TCFG_AUDIO_ANC_ENABLE || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
#if (TCFG_AUDIO_DAC_MIX_ENABLE)
            SYS_60M,
#else
            SYS_48M,
#endif
#elif (TCFG_AUDIO_DAC_MIX_ENABLE)
            SYS_48M,
#else
            SYS_32M,
#endif
#endif /*TCFG_AUDIO_OUTPUT_IIS*/
        },
        {
            AUDIO_CODING_AAC,
#if TCFG_AUDIO_OUTPUT_IIS
            SYS_76M,
#else
#if defined(AUDIO_SPK_EQ_CONFIG) && AUDIO_SPK_EQ_CONFIG || AUDIO_VBASS_CONFIG || defined(TCFG_AUDIO_MDRC_ENABLE) && TCFG_AUDIO_MDRC_ENABLE
            SYS_76M,
#elif (AUDIO_SURROUND_CONFIG && (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR))|| (defined(TCFG_AUDIO_DAC_24BIT_MODE) && TCFG_AUDIO_DAC_24BIT_MODE)
            SYS_76M,
#elif  (TCFG_BT_MUSIC_EQ_ENABLE && ((EQ_SECTION_MAX >= 10) || AUDIO_OUT_EFFECT_ENABLE))  || AUDIO_SURROUND_CONFIG || TCFG_DRC_ENABLE
#if (TCFG_AUDIO_ANC_ENABLE)
            SYS_76M,
#else
            SYS_60M,
#endif /*TCFG_AUDIO_ANC_ENABLE*/
#else
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
#if (TCFG_AUDIO_DAC_MIX_ENABLE)
            SYS_76M,
#else
#if TCFG_LOWEST_POWER_EN
            SYS_48M,
#else
            SYS_60M,
#endif
#endif
#elif (TCFG_AUDIO_DAC_MIX_ENABLE)
            SYS_60M,
#else
            SYS_48M,
#endif /*TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR*/
#endif
#endif /*TCFG_AUDIO_OUTPUT_IIS*/
        }
    },

    {
        //ESCO MODE
        {
            AUDIO_CODING_MSBC,
#if TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR
#if (TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
            SYS_76M,
#else /*TCFG_AUDIO_CVP_NS_MODE == CVP_DNS_MODE*/
            SYS_96M,
#endif /*TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE*/
#else
#if (TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
            BT_CALL_16k_ADVANCE_HZ,
#else
            CONFIG_BT_CALL_DNS_HZ,
#endif
#endif
        },
        {
            AUDIO_CODING_CVSD,
#if (TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
            BT_CALL_ADVANCE_HZ,
#else
            CONFIG_BT_CALL_DNS_HZ,
#endif
        },
    },

    {
        //TONE MODE
        {AUDIO_CODING_AAC | AUDIO_CODING_WAV | AUDIO_CODING_WTGV2 | AUDIO_CODING_MP3, 96 * 1000000L},
        {AUDIO_CODING_ALL, 96 * 1000000L},
    },

    {
        //FILE MODE
        {AUDIO_CODING_ALL, 120 * 1000000L},
    },

    {
        //PC MODE
        {AUDIO_CODING_ALL, 96 * 1000000L},
    },

    {
        //KWS MODE
        {AUDIO_CODING_ALL, SYS_48M},
    },

    {
        //USB MIC MODE
        {AUDIO_CODING_ALL, 96 * 1000000L},
    },

    {
        //SMART VOICE MODE
        {AUDIO_CODING_ALL, SYS_76M, 1},
    },

    {
        //HEARING AID MODE
        {AUDIO_CODING_ALL, SYS_76M, 1},
    },

    {
        //KEY TONE MODE
        {AUDIO_CODING_ALL, SYS_96M, 0},
    },

    //TODO
};

static u32 audio_codec_background_clock(void)
{
    struct audio_codec_clk_context *ctx;
    u32 clk = 0;

    if (list_empty(&codec_clock_head)) {
        return 0;
    }
    list_for_each_entry(ctx, &codec_clock_head, entry) {
        if (ctx->params.background) {
            clk += ctx->params.clk;
        }
    }

    return clk;
}

static void audio_codec_background_clock_add(u32 clk)
{
    struct audio_codec_clk_context *ctx;

    if (list_empty(&codec_clock_head)) {
        return;
    }
    list_for_each_entry(ctx, &codec_clock_head, entry) {
        ctx->params.clk += clk;
    }
}

static void audio_codec_background_clock_del(u32 clk)
{
    struct audio_codec_clk_context *ctx;

    if (list_empty(&codec_clock_head)) {
        return;
    }

    list_for_each_entry(ctx, &codec_clock_head, entry) {
        ctx->params.clk -= clk;
    }
}

static bool audio_codec_mode_exist(u8 mode)
{
    struct audio_codec_clk_context *ctx;

    if (list_empty(&codec_clock_head)) {
        return false;
    }

    list_for_each_entry(ctx, &codec_clock_head, entry) {
        if (ctx->mode == mode) {
            return true;
        }
    }

    return false;
}

int audio_codec_clock_set(u8 mode, u32 coding_type, u8 preemption)
{
    printf("%s->mode = %d\n", __func__, mode);
    struct audio_codec_clk_context *ctx = (struct audio_codec_clk_context *)zalloc(sizeof(struct audio_codec_clk_context));
    u32 new_clk = 0;
    u32 background_clk = 0;

    if (mode >= AUDIO_MAX_MODE) {
        log_error("Not support this mode : %d", mode);
        return -EINVAL;
    }

    if (audio_codec_mode_exist(mode)) {
        return 0;
    }
    const struct audio_codec_clock *params = audio_clock[mode];
    printf("%s->params clk:%d\n", __func__, params->clk);
    int i = 0;

    for (i = 0; i < MAX_CODING_TYPE_NUM; i++) {
        printf("%s->params[%d].coding_type : 0x%x\n", __func__, i, params[i].coding_type);
        if (params[i].coding_type & coding_type) {
            goto match_clock;
        }
    }

    log_error("Not found right coding type : %d", coding_type);
    return -EINVAL;

match_clock:
    if (params[i].background) {
        /*如果是后台运行的，则需要处理最终时钟为前台+后台时钟总和*/
        audio_codec_background_clock_add(params[i].clk);
        new_clk = params[i].clk;
        struct audio_codec_clk_context *first = list_empty(&codec_clock_head) ? NULL : list_first_entry(&codec_clock_head, struct audio_codec_clk_context, entry);
        if (first) {
            new_clk = first->params.clk;
        } else {
            /*后台运行时钟比较接近负载极限，容易导致其他跑不过来，因此多加12M*/
            new_clk += 12 * 1000000L;
        }
        goto clock_set;
    }

    background_clk = audio_codec_background_clock();
    new_clk = (params[i].clk ? params[i].clk : clk_get("sys")) + background_clk;
    if (!preemption) {
        struct audio_codec_clk_context *ctx1;
        if (!list_empty(&codec_clock_head)) {
            ctx1 = list_first_entry(&codec_clock_head, struct audio_codec_clk_context, entry);
            if (ctx1 && !ctx1->params.background) {
                if (new_clk <= ctx1->params.clk) {
                    /*叠加解码的时钟不可小于在播放的音频时钟*/
                    new_clk = ctx1->params.clk + 12 * 1000000;
                } else {
                    new_clk += 32 * 1000000;
                }
            }
        }
    }

clock_set:

#if AUDIO_SPECTRUM_CONFIG
    new_clk += 12 * 1000000;
#endif

// #if (TCFG_BD_NUM == 2) && (TCFG_BT_SUPPORT_AAC==1)
//     if ((mode == AUDIO_A2DP_MODE) && (get_total_connect_dev() > 1)) {
//         new_clk += 12 * 1000000;
//     }
// #endif

#if (TCFG_BD_NUM == 2) || (TCFG_BT_SUPPORT_AAC==1)
    if (mode == AUDIO_A2DP_MODE) {
        new_clk += 12 * 1000000;
    }
#endif

    ctx->mode = mode;
    ctx->params.coding_type = coding_type;
    ctx->params.background = params[i].background;
    ctx->params.clk = ctx->params.background ? params[i].clk : new_clk;
    clk_set("sys", new_clk);
    if (ctx->params.background) {
        list_add_tail(&ctx->entry, &codec_clock_head);
    } else {
        list_add(&ctx->entry, &codec_clock_head);
    }

    return 0;
}

void audio_codec_clock_del(u8 mode)
{
    struct audio_codec_clk_context *ctx;
    int next_clk = 0;
    u32 remove_clk = 0;

    list_for_each_entry(ctx, &codec_clock_head, entry) {
        if (ctx->mode == mode) {
            goto clock_del;
        }
    }

    return;
clock_del:
    list_del(&ctx->entry);
    if (ctx->params.background) {
        audio_codec_background_clock_del(ctx->params.clk);
        remove_clk = ctx->params.clk;
    }
    free(ctx);

    if (!list_empty(&codec_clock_head)) {
        ctx = list_first_entry(&codec_clock_head, struct audio_codec_clk_context, entry);
        if (ctx) {
            next_clk = ctx->params.clk;
        }
    } else {
        u32 current_clk = clk_get("sys");
        next_clk = current_clk - remove_clk;
    }

    if (!next_clk || next_clk < AUDIO_CODEC_BASE_CLK) {
        next_clk = list_empty(&codec_clock_head) ? clk_get("sys") : AUDIO_CODEC_BASE_CLK;
    }
    clk_set("sys", next_clk);
}

void audio_codec_clock_check(void)
{
    struct audio_codec_clk_context *ctx;

    if (!list_empty(&codec_clock_head)) {
        ctx = list_first_entry(&codec_clock_head, struct audio_codec_clk_context, entry);
        if (!ctx) {
            return;
        }

        u32 current_clk = clk_get("sys");
        u32 expect_clk = ctx->params.background ? (ctx->params.clk + 12 * 1000000L) : ctx->params.clk;
        if ((current_clk < 128 * 1000000L) && current_clk < expect_clk) {
            /*log_error("Audio codec clock error : %dM, %dM.", current_clk / 1000000, expect_clk / 1000000);*/
            putchar('K');
            clk_set("sys", expect_clk);
        }
    }
}
