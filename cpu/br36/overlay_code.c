#include "asm/includes.h"
#include "media/includes.h"
#include "overlay_code.h"
#include "board_config.h"
#include "system/bank_switch.h"


struct code_type {
    u32 type;
    u32 dst;
    u32 src;
    u32 size;
    u32 index;
};

////用于区分 overlay code 和 bss
/* u32 aec_bss_id sec(.aec_bss_id) = 0xffffffff; */
/* u32 fm_bss_id sec(.fm_bss_id) = 0xffffffff; */
/* u32 mp3_bss_id sec(.mp3_bss_id) = 0xffffffff; */
/* u32 wma_bss_id sec(.wma_bss_id) = 0xffffffff; */
/* u32 wav_bss_id sec(.wav_bss_id) = 0xffffffff; */

/* u32 wav_bss_id sec(.wav_bss_id) = 0xffffffff;
u32 ape_bss_id sec(.ape_bss_id) = 0xffffffff;
u32 flac_bss_id sec(.flac_bss_id) = 0xffffffff;
u32 m4a_bss_id sec(.m4a_bss_id) = 0xffffffff;
u32 amr_bss_id sec(.amr_bss_id) = 0xffffffff;
u32 dts_bss_id sec(.dts_bss_id) = 0xffffffff;
 */
extern int aec_addr, aec_begin, aec_size;
extern int wav_addr, wav_begin, wav_size;
extern int ape_addr, ape_begin, ape_size;
extern int flac_addr, flac_begin, flac_size;
/* #if TCFG_DEC_M4A_ENABLE */
/* extern int m4a_addr, m4a_begin, m4a_size; */
/* #elif TCFG_BT_SUPPORT_AAC */
extern int aac_addr, aac_begin, aac_size;
#define m4a_addr aac_addr
#define m4a_begin aac_begin
#define m4a_size aac_size
/* #endif */
extern int amr_addr, amr_begin, amr_size;
extern int dts_addr, dts_begin, dts_size;
extern int fm_addr,  fm_begin,  fm_size;

#ifdef CONFIG_MP3_WMA_LIB_SPECIAL
extern int mp3_addr, mp3_begin, mp3_size;
extern int wma_addr, wma_begin, wma_size;
#endif

const struct code_type  ctype[] = {
    {OVERLAY_AEC, (u32) &aec_addr, (u32) &aec_begin, (u32) &aec_size, 1},
#if TCFG_DEC_WAV_ENABLE
    {OVERLAY_WAV, (u32) &wav_addr, (u32) &wav_begin, (u32) &wav_size },
#endif
#if TCFG_DEC_APE_ENABLE
    {OVERLAY_APE, (u32) &ape_addr, (u32) &ape_begin, (u32) &ape_size },
#endif
#if TCFG_DEC_FLAC_ENABLE
    /* {OVERLAY_FLAC, (u32) &flac_addr, (u32) &flac_begin, (u32) &flac_size}, */
#endif
#if (((defined TCFG_DEC_M4A_ENABLE) && TCFG_DEC_M4A_ENABLE) || TCFG_BT_SUPPORT_AAC)
    {OVERLAY_M4A, (u32) &m4a_addr, (u32) &m4a_begin, (u32) &m4a_size, 2},
#endif
#if TCFG_DEC_AMR_ENABLE
    {OVERLAY_AMR, (u32) &amr_addr, (u32) &amr_begin, (u32) &amr_size },
#endif
#if TCFG_DEC_DTS_ENABLE
    {OVERLAY_DTS, (u32) &dts_addr, (u32) &dts_begin, (u32) &dts_size },
#endif
#if TCFG_FM_ENABLE
    {OVERLAY_FM, (u32) &fm_addr, (u32) &fm_begin, (u32) &fm_size },
#endif
#ifdef CONFIG_MP3_WMA_LIB_SPECIAL
    {OVERLAY_MP3, (u32) &mp3_addr, (u32) &mp3_begin, (u32) &mp3_size },
    {OVERLAY_WMA, (u32) &wma_addr, (u32) &wma_begin, (u32) &wma_size },
#endif
};

struct audio_overlay_type {
    u32 atype;
    u32 otype;
};

const struct audio_overlay_type  aotype[] = {
    //{AUDIO_CODING_MSBC, OVERLAY_AEC },
    //{AUDIO_CODING_CVSD, OVERLAY_AEC },
#if (((defined TCFG_DEC_M4A_ENABLE) && TCFG_DEC_M4A_ENABLE) || TCFG_BT_SUPPORT_AAC)
    {AUDIO_CODING_AAC, OVERLAY_M4A },
#endif
};


void overlay_load_code(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(ctype); i++) {
        if (type == ctype[i].type) {
            if (ctype[i].dst != 0 && (ctype[i].size > sizeof(int))) {

                load_overlay_code(ctype[i].index);

                /* memcpy((void *)ctype[i].dst, (void *)ctype[i].src, (int)ctype[i].size); */
            }
            break;
        }
    }
}

void audio_overlay_load_code(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(aotype); i++) {
        if (type == aotype[i].atype) {
            overlay_load_code(aotype[i].otype);
        }
    }
}
