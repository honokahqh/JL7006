/*****************************************************************
>file name : audio_link.c
>author :
>create time : Fri 7 Dec 2018 14:59:12 PM CST
*****************************************************************/
#include "includes.h"
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "asm/clock.h"
#include "asm/iis.h"
#include "audio_link.h"
#include "audio_syncts.h"
#include "audio_iis.h"
#include "sound/sound.h"

#if TCFG_AUDIO_INPUT_IIS || TCFG_AUDIO_OUTPUT_IIS || TCFG_ADC_IIS_ENABLE
/*#define ALINK_TEST_ENABLE*/

#define ALINK_DEBUG_INFO
#ifdef ALINK_DEBUG_INFO
#define alink_printf  printf
#else
#define alink_printf(...)
#endif

#define SAMPLE_16BIT_TO_16BIT       0
#define SAMPLE_16BIT_TO_24BIT       1

static void alink_info_dump();
static u32 *ALNK_BUF_ADR[] = {
    (u32 *)(&(JL_ALNK0->ADR0)),
    (u32 *)(&(JL_ALNK0->ADR1)),
    (u32 *)(&(JL_ALNK0->ADR2)),
    (u32 *)(&(JL_ALNK0->ADR3)),
};

static u32 *ALNK_HWPTR[] = {
    (u32 *)(&(JL_ALNK0->HWPTR0)),
    (u32 *)(&(JL_ALNK0->HWPTR1)),
    (u32 *)(&(JL_ALNK0->HWPTR2)),
    (u32 *)(&(JL_ALNK0->HWPTR3)),
};

static u32 *ALNK_SWPTR[] = {
    (u32 *)(&(JL_ALNK0->SWPTR0)),
    (u32 *)(&(JL_ALNK0->SWPTR1)),
    (u32 *)(&(JL_ALNK0->SWPTR2)),
    (u32 *)(&(JL_ALNK0->SWPTR3)),
};

static u32 *ALNK_SHN[] = {
    (u32 *)(&(JL_ALNK0->SHN0)),
    (u32 *)(&(JL_ALNK0->SHN1)),
    (u32 *)(&(JL_ALNK0->SHN2)),
    (u32 *)(&(JL_ALNK0->SHN3)),
};

static u32 PFI_ALNK0_DAT[] = {
    PFI_ALNK0_DAT0,
    PFI_ALNK0_DAT1,
    PFI_ALNK0_DAT2,
    PFI_ALNK0_DAT3,
};

static u32 FO_ALNK0_DAT[] = {
    FO_ALNK0_DAT0,
    FO_ALNK0_DAT1,
    FO_ALNK0_DAT2,
    FO_ALNK0_DAT3,
};


enum {
    MCLK_11M2896K = 0,
    MCLK_12M288K
};

enum {
    MCLK_EXTERNAL	= 0u,
    MCLK_SYS_CLK		,
    MCLK_OSC_CLK 		,
    MCLK_PLL_CLK		,
};

enum {
    MCLK_DIV_1		= 0u,
    MCLK_DIV_2			,
    MCLK_DIV_4			,
    MCLK_DIV_8			,
    MCLK_DIV_16			,
};

enum {
    MCLK_LRDIV_EX	= 0u,
    MCLK_LRDIV_64FS		,
    MCLK_LRDIV_128FS	,
    MCLK_LRDIV_192FS	,
    MCLK_LRDIV_256FS	,
    MCLK_LRDIV_384FS	,
    MCLK_LRDIV_512FS	,
    MCLK_LRDIV_768FS	,
};

struct audio_link_channel {
    u8 id;
    void (*irq_handler)(void *priv, void *addr, int len);
    void *private_data;
    struct list_head entry;
};

struct audio_link_driver {
    struct list_head head;
    ALINK_PARM *platform_data;
};

static struct audio_link_driver *g_alink_driver = NULL;
extern ALINK_PARM alink_param;

//==================================================
static void alink_clk_io_in_init(u8 gpio)
{
    gpio_set_direction(gpio, 1);
    gpio_set_pull_down(gpio, 0);
    gpio_set_pull_up(gpio, 1);
    gpio_set_die(gpio, 1);
}

static void *alink_addr(u8 ch)
{
    if (!g_alink_driver) {
        return NULL;
    }
    ALINK_PARM *pd = g_alink_driver->platform_data;
    u8 *buf_addr = NULL; //can be used
    u32 buf_index = 0;

    if (pd->buf_mode == ALINK_BUF_CIRCLE) {
        buf_addr = (s16 *)(*ALNK_BUF_ADR[ch] + *ALNK_SWPTR[ch] * 4);
        return buf_addr;
    }

    buf_addr = (u8 *)(pd->ch_cfg[ch].buf);

    u8 index_table[4] = {12, 13, 14, 15};
    u8 bit_index = index_table[ch];
    buf_index = (JL_ALNK0->CON0 & BIT(bit_index)) ? 0 : 1;
    buf_addr = buf_addr + ((pd->dma_len / 2) * buf_index);

    return buf_addr;
}

void alink_isr_en(u8 en)
{
    if (en) {
        bit_set_ie(IRQ_ALINK0_IDX);
    } else {
        bit_clr_ie(IRQ_ALINK0_IDX);
    }
}

___interrupt
static void alink_dma_irq_handler(void)
{
    u16 reg;
    s16 *buf_addr = NULL ;
    u8 ch = 0;
    u8 do_irq_handler = 0;
    struct audio_link_channel *hw_channel;

    reg = JL_ALNK0->CON2;
    reg >>= 4;
    /*printf("----reg : 0x%x ----\n", reg);*/
    for (ch = 0; ch < 4; ch++) {
        if (!(reg & BIT(ch))) {
            continue;
        }
        if (g_alink_driver) {
            list_for_each_entry(hw_channel, &g_alink_driver->head, entry) {
                if (hw_channel->id == ch) {
                    do_irq_handler = 1;
                    break;
                }
            }
        }
        if (do_irq_handler && hw_channel->irq_handler) {
            buf_addr = alink_addr(ch);
            hw_channel->irq_handler(hw_channel->private_data, buf_addr, g_alink_driver->platform_data->dma_len / 2);
        }
        ALINK_CLR_CHx_PND(ch);
    }
}

static void alink_sr(u32 rate)
{
    if (!g_alink_driver) {
        return;
    }
    ALINK_PARM *pd = g_alink_driver->platform_data;
    alink_printf("ALINK_SR = %d\n", rate);
    u8 pll_target_frequency = clock_get_pll_target_frequency();
    switch (rate) {
    case ALINK_SR_48000:
        /*12.288Mhz 256fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_256FS);
        break ;

    case ALINK_SR_44100:
        /*11.289Mhz 256fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_1);
        if (pll_target_frequency == 192) {
            ALINK_LRDIV(MCLK_LRDIV_256FS);
        } else if (pll_target_frequency == 240) {
            ALINK_LRDIV(MCLK_LRDIV_128FS);
        }
        break ;

    case ALINK_SR_32000:
        /*12.288Mhz 384fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_384FS);
        break ;

    case ALINK_SR_24000:
        /*12.288Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_2);
        ALINK_LRDIV(MCLK_LRDIV_256FS);
        break ;

    case ALINK_SR_22050:
        /*11.289Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_1);
        if (pll_target_frequency == 192) {
            ALINK_LRDIV(MCLK_LRDIV_512FS);
        } else if (pll_target_frequency == 240) {
            ALINK_LRDIV(MCLK_LRDIV_256FS);
        }
        break ;

    case ALINK_SR_16000:
        /*12.288/2Mhz 384fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_2);
        ALINK_LRDIV(MCLK_LRDIV_384FS);
        break ;

    case ALINK_SR_12000:
        /*12.288/2Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_4);
        ALINK_LRDIV(MCLK_LRDIV_256FS);
        break ;

    case ALINK_SR_11025:
        /*11.289/2Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_4);
        if (pll_target_frequency == 192) {
            ALINK_LRDIV(MCLK_LRDIV_512FS);
        } else if (pll_target_frequency == 240) {
            ALINK_LRDIV(MCLK_LRDIV_256FS);
        }
        break ;

    case ALINK_SR_8000:
        /*12.288/4Mhz 384fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_4);
        ALINK_LRDIV(MCLK_LRDIV_384FS);
        break ;

    default:
        //44100
        /*11.289Mhz 256fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_256FS);
        break;
    }
    if (pd->role == ALINK_ROLE_SLAVE) {
        ALINK_LRDIV(MCLK_LRDIV_EX);
    }
}


void alink_channel_init(u8 ch_idx, u8 dir, void *priv, void (*handler)(void *priv, void *addr, int len))
{
    if (!g_alink_driver) {
        return;
    }
    struct audio_link_channel *alink_hw_channel = (struct audio_link_channel *)zalloc(sizeof(struct audio_link_channel));
    if (!alink_hw_channel) {
        return;
    }
    list_add(&alink_hw_channel->entry, &g_alink_driver->head);
    alink_hw_channel->private_data = priv;
    alink_hw_channel->irq_handler = handler;
    alink_hw_channel->id = ch_idx;

    ALINK_PARM *pd = g_alink_driver->platform_data;
    pd->ch_cfg[ch_idx].dir = dir;
    pd->ch_cfg[ch_idx].buf = malloc(pd->dma_len);

    printf(">>> alink_channel_init %x\n", pd->ch_cfg[ch_idx].buf);

    //===================================//
    //           ALNK工作模式            //
    //===================================//
    if (pd->mode > ALINK_MD_IIS_RALIGN) {
        ALINK_CHx_MODE_SEL((pd->mode - ALINK_MD_IIS_RALIGN), ch_idx);
    } else {
        ALINK_CHx_MODE_SEL(pd->mode, ch_idx);
    }
    //===================================//
    //           ALNK CH DMA BUF         //
    //===================================//

    *ALNK_BUF_ADR[ch_idx] = (u32)(pd->ch_cfg[ch_idx].buf);
    //===================================//
    //          ALNK CH DAT IO INIT      //
    //===================================//
    if (pd->ch_cfg[ch_idx].dir == ALINK_DIR_RX) {
        gpio_set_direction(pd->ch_cfg[ch_idx].data_io, 1);
        gpio_set_pull_down(pd->ch_cfg[ch_idx].data_io, 0);
        gpio_set_pull_up(pd->ch_cfg[ch_idx].data_io, 1);
        gpio_set_die(pd->ch_cfg[ch_idx].data_io, 1);
        gpio_set_fun_input_port(pd->ch_cfg[ch_idx].data_io, PFI_ALNK0_DAT[ch_idx], LOW_POWER_FREE);
        ALINK_CHx_DIR_RX_MODE(ch_idx);
        ALINK_CHx_IE(ch_idx, 1);
    } else {
        gpio_direction_output(pd->ch_cfg[ch_idx].data_io, 1);
        gpio_set_fun_output_port(pd->ch_cfg[ch_idx].data_io, FO_ALNK0_DAT[ch_idx], 1, 1, 0);
        ALINK_CHx_DIR_TX_MODE(ch_idx);
#if  TCFG_ADC_IIS_ENABLE
        ALINK_CHx_IE(ch_idx, 1);
#else
        ALINK_CHx_IE(ch_idx, 0);
#endif
    }
    r_printf("alink_ch %d init\n", ch_idx);
}

static void alink_info_dump()
{
    alink_printf("JL_ALNK0->CON0 = 0x%x", JL_ALNK0->CON0);
    alink_printf("JL_ALNK0->CON1 = 0x%x", JL_ALNK0->CON1);
    alink_printf("JL_ALNK0->CON2 = 0x%x", JL_ALNK0->CON2);
    alink_printf("JL_ALNK0->CON3 = 0x%x", JL_ALNK0->CON3);
    alink_printf("JL_ALNK0->LEN = 0x%x", JL_ALNK0->LEN);
    alink_printf("JL_ALNK0->ADR0 = 0x%x", JL_ALNK0->ADR0);
    alink_printf("JL_ALNK0->ADR1 = 0x%x", JL_ALNK0->ADR1);
    alink_printf("JL_ALNK0->ADR2 = 0x%x", JL_ALNK0->ADR2);
    alink_printf("JL_ALNK0->ADR3 = 0x%x", JL_ALNK0->ADR3);
    alink_printf("JL_ALNK0->PNS = 0x%x", JL_ALNK0->PNS);
    alink_printf("JL_ALNK0->HWPTR0 = 0x%x", JL_ALNK0->HWPTR0);
    alink_printf("JL_ALNK0->HWPTR1 = 0x%x", JL_ALNK0->HWPTR1);
    alink_printf("JL_ALNK0->HWPTR2 = 0x%x", JL_ALNK0->HWPTR2);
    alink_printf("JL_ALNK0->HWPTR3 = 0x%x", JL_ALNK0->HWPTR3);
    alink_printf("JL_ALNK0->SWPTR0 = 0x%x", JL_ALNK0->SWPTR0);
    alink_printf("JL_ALNK0->SWPTR1 = 0x%x", JL_ALNK0->SWPTR1);
    alink_printf("JL_ALNK0->SWPTR2 = 0x%x", JL_ALNK0->SWPTR2);
    alink_printf("JL_ALNK0->SWPTR3 = 0x%x", JL_ALNK0->SWPTR3);
    alink_printf("JL_ALNK0->SHN0 = 0x%x", JL_ALNK0->SHN0);
    alink_printf("JL_ALNK0->SHN1 = 0x%x", JL_ALNK0->SHN1);
    alink_printf("JL_ALNK0->SHN2 = 0x%x", JL_ALNK0->SHN2);
    alink_printf("JL_ALNK0->SHN3 = 0x%x", JL_ALNK0->SHN3);
    alink_printf("JL_PLL->PLL_CON3 = 0x%x", JL_PLL->PLL_CON3);
    alink_printf("JL_CLOCK->CLK_CON2 = 0x%x", JL_CLOCK->CLK_CON2);
    alink_printf("JL_CLOCK->CLK_CON1 = 0x%x", JL_CLOCK->CLK_CON1);
}

int alink_init(ALINK_PARM *parm)
{
    if (parm == NULL) {
        return -1;
    }

    if (!g_alink_driver) {
        g_alink_driver = (struct audio_link_driver *)zalloc(sizeof(struct audio_link_driver));
        INIT_LIST_HEAD(&g_alink_driver->head);
    }

    if (!list_empty(&g_alink_driver->head)) {
        return 0;
    }
    ALNK_CON_RESET();
    ALNK_HWPTR_RESET();
    ALNK_SWPTR_RESET();
    /*ALNK_SHN_RESET();*/
    ALNK_ADR_RESET();
    ALNK_PNS_RESET();
    ALINK_CLR_ALL_PND();
    g_alink_driver->platform_data = parm;

    ALINK_MSRC(MCLK_PLL_CLK);	/*MCLK source*/
    ALINK_PARM *pd = parm;

    //===================================//
    //        输出时钟配置               //
    //===================================//
    if (parm->role == ALINK_ROLE_MASTER) {
        //主机输出时钟
        if (parm->mclk_io != ALINK_CLK_OUPUT_DISABLE) {
            gpio_direction_output(parm->mclk_io, 1);
            gpio_set_fun_output_port(parm->mclk_io, FO_ALNK0_MCLK, 1, 1, 0);
            ALINK_MOE(1);				/*MCLK output to IO*/
        }
        if ((parm->sclk_io != ALINK_CLK_OUPUT_DISABLE) && (parm->lrclk_io != ALINK_CLK_OUPUT_DISABLE)) {
            gpio_direction_output(parm->lrclk_io, 1);
            gpio_set_fun_output_port(parm->lrclk_io, FO_ALNK0_LRCK, 1, 1, 0);
            gpio_direction_output(parm->sclk_io, 1);
            gpio_set_fun_output_port(parm->sclk_io, FO_ALNK0_SCLK, 1, 1, 0);
            ALINK_SOE(1);				/*SCLK/LRCK output to IO*/
        }
    } else {
        //从机输入时钟
        if (parm->mclk_io != ALINK_CLK_OUPUT_DISABLE) {
            alink_clk_io_in_init(parm->mclk_io);
            gpio_set_fun_input_port(parm->mclk_io, PFI_ALNK0_MCLK, LOW_POWER_FREE);
            ALINK_MOE(0);				/*MCLK input to IO*/
        }
        if ((parm->sclk_io != ALINK_CLK_OUPUT_DISABLE) && (parm->lrclk_io != ALINK_CLK_OUPUT_DISABLE)) {
            alink_clk_io_in_init(parm->lrclk_io);
            gpio_set_fun_input_port(parm->lrclk_io, PFI_ALNK0_LRCK, LOW_POWER_FREE);
            alink_clk_io_in_init(parm->sclk_io);
            gpio_set_fun_input_port(parm->sclk_io, PFI_ALNK0_SCLK, LOW_POWER_FREE);
            ALINK_SOE(0);				/*SCLK/LRCK output to IO*/
        }
    }
    //===================================//
    //        基本模式/扩展模式          //
    //===================================//
    ALINK_DSPE(pd->mode >> 2);

    //===================================//
    //         数据位宽16/32bit          //
    //===================================//
    //注意: 配置了24bit, 一定要选ALINK_FRAME_64SCLK, 因为sclk32 x 2才会有24bit;
    if (pd->bitwide == ALINK_LEN_24BIT) {
        ASSERT(pd->sclk_per_frame == ALINK_FRAME_64SCLK);
        ALINK_24BIT_MODE();
        //一个点需要4bytes, LR = 2, 双buf = 2
    } else {
        ALINK_16BIT_MODE();
        //一个点需要2bytes, LR = 2, 双buf = 2
    }
    //===================================//
    //             时钟边沿选择          //
    //===================================//
    if (pd->clk_mode == ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE) {
        SCLKINV(0);
    } else {
        SCLKINV(1);
    }
    //===================================//
    //            每帧数据sclk个数       //
    //===================================//
    if (pd->sclk_per_frame == ALINK_FRAME_64SCLK) {
        F32_EN(0);
    } else {
        F32_EN(1);
    }

    //===================================//
    //            设置数据采样率       	 //
    //===================================//
    alink_sr(pd->sample_rate);

    //===================================//
    //            设置DMA MODE       	 //
    //===================================//
    ALINK_DMA_MODE_SEL(pd->buf_mode);
    if (pd->buf_mode == ALINK_BUF_CIRCLE) {
        ALINK_OPNS_SET(128);
        ALINK_IPNS_SET(128);
        //一个点需要2bytes, LR = 2
        JL_ALNK0->LEN = pd->dma_len / 4; //点数
        request_irq(IRQ_ALINK0_IDX, 2, alink_dma_irq_handler, 0);
    } else {
        //一个点需要2bytes, LR = 2, 双buf = 2
        JL_ALNK0->LEN = pd->dma_len / 8; //点数
        request_irq(IRQ_ALINK0_IDX, 2, alink_dma_irq_handler, 0);
    }

    return 0;
}

void alink_channel_close(u8 ch_idx)
{
    if (!g_alink_driver) {
        return;
    }
    ALINK_PARM *pd = g_alink_driver->platform_data;
    struct audio_link_channel *hw_channel = NULL;
    u8 delete_channel = 0;
    list_for_each_entry(hw_channel, &g_alink_driver->head, entry) {
        if (hw_channel->id == ch_idx) {
            delete_channel = 1;
            break;
        }
    }

    if (delete_channel) {
        list_del(&hw_channel->entry);
        free(hw_channel);
    }

    gpio_set_pull_up(pd->ch_cfg[ch_idx].data_io, 0);
    gpio_set_pull_down(pd->ch_cfg[ch_idx].data_io, 0);
    gpio_set_direction(pd->ch_cfg[ch_idx].data_io, 1);
    gpio_set_die(pd->ch_cfg[ch_idx].data_io, 0);
    ALINK_CHx_IE(ch_idx, 0);
    ALINK_CHx_CLOSE(0, ch_idx);
    if (pd->ch_cfg[ch_idx].buf) {
        free(pd->ch_cfg[ch_idx].buf);
        pd->ch_cfg[ch_idx].buf = NULL;
    }
    r_printf("alink_ch %d closed\n", ch_idx);
}


int alink_start(void)
{
    if (g_alink_driver) {
        ALINK_EN(1);
        return 0;
    }
    return -1;
}

void alink_uninit(void)
{
    if (!g_alink_driver) {
        return;
    }
    ALINK_PARM *pd = g_alink_driver->platform_data;
    ALINK_EN(0);
    if ((JL_CLOCK->CLK_CON1 & 0x4) == 0) {
        PLL_CLK_F2_OE(0);
    }
    if ((JL_CLOCK->CLK_CON1 & 0x3) == 0) {
        PLL_CLK_F3_OE(0);
    }
    for (int i = 0; i < 4; i++) {
        if (pd->ch_cfg[i].buf) {
            alink_channel_close(i);
        }
    }
    free(g_alink_driver);
    g_alink_driver = NULL;
    ALNK_CON_RESET();
    alink_printf("audio_link_exit OK\n");
}

int alink_sr_set(u16 sr)
{
    if (g_alink_driver) {
        ALINK_EN(0);
        alink_sr(sr);
        ALINK_EN(1);
        return 0;
    } else {
        return -1;
    }
}

#define IIS_TX_MIN_PNS      (JL_ALNK0->LEN / 4)
struct audio_iis_sync_node {
    void *hdl;
    struct list_head entry;
};
/***********************************************************
 * i2s 使用接口
 *
 ***********************************************************/
int audio_iis_buffered_frames(struct audio_iis_hdl *iis)
{
    return (JL_ALNK0->LEN - *ALNK_SHN[iis->hw_ch] - 1) / (iis->bit_width == ALINK_LEN_24BIT ? 2 : 1);
}

int audio_iis_buffered_time(struct audio_iis_hdl *iis)
{
    if (!iis) {
        return 0;
    }

    int buffered_time = ((audio_iis_buffered_frames(iis) * 1000000) / iis->sample_rate) / 1000;

    return buffered_time;
}

int audio_iis_set_underrun_params(struct audio_iis_hdl *iis, int time, void *priv, void (*feedback)(void *))
{
    local_irq_disable();
    iis->underrun_time = time;
    iis->underrun_pns = 0;
    iis->underrun_data = priv;
    iis->underrun_feedback = feedback;
    local_irq_enable();

    return 0;
}

void audio_iis_syncts_update_frame(struct audio_iis_hdl *iis)
{
    struct audio_iis_sync_node *node;
    list_for_each_entry(node, &iis->sync_list, entry) {
        sound_pcm_enter_update_frame(node->hdl);
    }
}

void audio_iis_syncts_latch_trigger(struct audio_iis_hdl *iis)
{
    struct audio_iis_sync_node *node;

    list_for_each_entry(node, &iis->sync_list, entry) {
        sound_pcm_syncts_latch_trigger(node->hdl);
    }
}


void audio_iis_dma_update_to_syncts(struct audio_iis_hdl *iis, int frames)
{
    struct audio_iis_sync_node *node;
    u8 have_syncts = 0;

    list_for_each_entry(node, &iis->sync_list, entry) {
        sound_pcm_update_frame_num(node->hdl, frames);
        have_syncts = 1;
    }

    if (have_syncts) {
        u16 free_points = *ALNK_SHN[iis->hw_ch];
        int timeout = (1000000 / iis->sample_rate) * (clk_get("sys") / 1000000);
        while (free_points == *ALNK_SHN[iis->hw_ch] && (--timeout > 0));
    }
}

void audio_iis_add_syncts_handle(struct audio_iis_hdl *iis, void *syncts)
{
    struct audio_iis_sync_node *node = (struct audio_iis_sync_node *)zalloc(sizeof(struct audio_iis_sync_node));
    node->hdl = syncts;

    list_add(&node->entry, &iis->sync_list);

    if (iis->state == SOUND_PCM_STATE_RUNNING) {
        sound_pcm_syncts_latch_trigger(syncts);
    }
    ALINK_CHx_BTSYNC(iis->hw_ch);
}

void audio_iis_remove_syncts_handle(struct audio_iis_hdl *iis, void *syncts)
{
    struct audio_iis_sync_node *node;

    list_for_each_entry(node, &iis->sync_list, entry) {
        if (node->hdl == syncts) {
            goto remove_node;
        }
    }

    return;
remove_node:

    list_del(&node->entry);
    free(node);
}

static void audio_iis_tx_irq_handler(struct audio_iis_hdl *iis)
{
    if (iis->irq_trigger && iis->trigger_handler) {
        iis->trigger_handler(iis->trigger_data);
        iis->irq_trigger = 0;
    }

    if (iis->underrun_pns) {
        int unread_frames = JL_ALNK0->LEN - *ALNK_SHN[iis->hw_ch] - 1;
        if (unread_frames <= iis->underrun_pns) {
            //TODO
        } else {
            ALINK_OPNS_SET(iis->underrun_pns);
            ALINK_CLR_CHx_PND(iis->hw_ch);
            return;
        }
    }

    ALINK_CHx_IE(iis->hw_ch, 0);
    ALINK_CLR_CHx_PND(iis->hw_ch);
}


int audio_iis_pcm_tx_open(struct audio_iis_hdl *iis, u8 ch, int sample_rate)
{
    memset(iis, 0x0, sizeof(struct audio_iis_hdl));
    INIT_LIST_HEAD(&iis->sync_list);
    iis->state = SOUND_PCM_STATE_IDLE;
    iis->sample_rate = sample_rate;
    iis->bit_width = alink_param.bitwide;
    alink_init(&alink_param);
    alink_channel_init(ch, ALINK_DIR_TX, iis, audio_iis_tx_irq_handler);
    /*alink_start();*/
    iis->hw_ch = ch;
}

void audio_iis_pcm_tx_close(struct audio_iis_hdl *iis)
{
    alink_channel_close(iis->hw_ch);
    iis->state = SOUND_PCM_STATE_IDLE;
}

int audio_iis_pcm_sample_rate(struct audio_iis_hdl *iis)
{
    return iis->sample_rate;
}

int audio_iis_set_delay_time(struct audio_iis_hdl *iis, int prepared_time, int delay_time)
{
    iis->prepared_time = prepared_time;
    iis->delay_time = delay_time;
    return 0;
}

int audio_iis_pcm_remapping(struct audio_iis_hdl *iis, u8 mapping)
{
    if (!iis->input_mapping) {
        iis->input_mapping = mapping;
    }
    if ((iis->input_mapping & SOUND_CHMAP_RL) || (iis->input_mapping & SOUND_CHMAP_RR)) {
        printf("Not support this channel map : 0x%x\n", mapping);
    }
    return 0;
}

int audio_iis_pcm_channel_num(struct audio_iis_hdl *iis)
{
    int num = 0;
    for (int i = 0; i < 2; i++) {
        if (iis->input_mapping & BIT(i)) {
            num++;
        }
    }
    return num;
}

#if 0
const unsigned char sin44K[88] ALIGNED(4) = {
    0x00, 0x00, 0x45, 0x0E, 0x41, 0x1C, 0xAA, 0x29, 0x3B, 0x36, 0xB2, 0x41, 0xD5, 0x4B, 0x6E, 0x54,
    0x51, 0x5B, 0x5A, 0x60, 0x70, 0x63, 0x82, 0x64, 0x8A, 0x63, 0x8E, 0x60, 0x9D, 0x5B, 0xD1, 0x54,
    0x4D, 0x4C, 0x3D, 0x42, 0xD5, 0x36, 0x50, 0x2A, 0xF1, 0x1C, 0xFB, 0x0E, 0xB7, 0x00, 0x70, 0xF2,
    0x6E, 0xE4, 0xFD, 0xD6, 0x60, 0xCA, 0xD9, 0xBE, 0xA5, 0xB4, 0xF7, 0xAB, 0xFC, 0xA4, 0xDA, 0x9F,
    0xAB, 0x9C, 0x7F, 0x9B, 0x5E, 0x9C, 0x3F, 0x9F, 0x19, 0xA4, 0xCE, 0xAA, 0x3D, 0xB3, 0x3A, 0xBD,
    0x92, 0xC8, 0x0A, 0xD5, 0x60, 0xE2, 0x50, 0xF0
};

int read_44k_sine_data(void *buf, int bytes, int offset, u8 channel)
{
    s16 *sine = (s16 *)sin44K;
    s16 *data = (s16 *)buf;
    int frame_len = (bytes >> 1) / channel;
    int sin44k_frame_len = sizeof(sin44K) / 2;
    int i, j;

    offset = offset % sin44k_frame_len;

    for (i = 0; i < frame_len; i++) {
        for (j = 0; j < channel; j++) {
            *data++ = sine[offset];
        }
        if (++offset >= sin44k_frame_len) {
            offset = 0;
        }
    }

    return i * 2 * channel;
}
static int frames_offset = 0;
#endif

static int __audio_iis_pcm_write(s16 *dst, s16 *src, int frames, int remapping, convert_mode)
{
    /*
    int frame_bytes = read_44k_sine_data(dst, frames * 2 * 2, frames_offset, 2);
    frames_offset += (frame_bytes >> 1) / 2;
    return frame_bytes;
    */
    if (remapping == 1) {
        if (convert_mode == SAMPLE_16BIT_TO_24BIT) {
            int *dst_24bit = (int *)dst;
            for (int i = 0; i < frames; i++) {
                dst_24bit[i * 2] = src[i] << 8;
                dst_24bit[i * 2 + 1] = src[i] << 8;
            }
        } else {
            for (int i = 0; i < frames; i++) {
                dst[i * 2] = src[i];
                dst[i * 2 + 1] = src[i];
            }
        }
        return frames << 1;
    }

    if (remapping == 2) {
        if (convert_mode == SAMPLE_16BIT_TO_24BIT) {
            int samples = frames * 2;
            int *dst_24bit = (int *)dst;
            for (int i = 0; i < samples; i++) {
                dst_24bit[i] = src[i] << 8;
            }
        } else {
            memcpy(dst, src, (frames << 1) * 2);
        }
        return frames << 2;
    }


    return frames;
}

#define CVP_REF_SRC_ENABLE
#ifdef CVP_REF_SRC_ENABLE
#define CVP_REF_SRC_TASK_NAME "RefSrcTask"
#include "Resample_api.h"
#include "aec_user.h"

#define CVP_REF_SRC_FRAME_SIZE  512
typedef struct {
    volatile u8 state;
    volatile u8 busy;
    RS_STUCT_API *sw_src_api;
    u8 *sw_src_buf;
    u16 input_rate;
    u16 output_rate;
    s16 ref_tmp_buf[CVP_REF_SRC_FRAME_SIZE / 2];
    cbuffer_t cbuf;
    u8 ref_buf[CVP_REF_SRC_FRAME_SIZE * 3];
} aec_ref_src_t;
static aec_ref_src_t *aec_ref_src = NULL;

extern void audio_aec_ref_src_get_output_rate(u16 *input_rate, u16 *output_rate);
extern u8 bt_phone_dec_is_running();

extern struct audio_iis_hdl iis_hdl;
void audio_aec_ref_src_run(s16 *data, int len)
{
    u16 ref_len = 0;
    if (aec_ref_src) {
        if (iis_hdl.input_mapping != SOUND_CHMAP_MONO) {
            /*双变单*/
            for (int i = 0; i < (len >> 2); i++) {
                aec_ref_src->ref_tmp_buf[i] =  data[2 * i];
            }
            len >>= 1;
        }
        /* audio_aec_ref_src_get_output_rate(&aec_ref_src->input_rate, &aec_ref_src->output_rate); */
        /* printf("%d %d \n",input_rate,output_rate); */
        if (aec_ref_src->sw_src_api) {
            /* aec_ref_src->sw_src_api->set_sr(aec_ref_src->sw_src_buf, aec_ref_src->output_rate); */
            ref_len = aec_ref_src->sw_src_api->run(aec_ref_src->sw_src_buf, aec_ref_src->ref_tmp_buf, len >> 1, aec_ref_src->ref_tmp_buf);
            ref_len <<= 1;
        }
        /* printf("ref_len %d", ref_len); */
        audio_aec_refbuf(aec_ref_src->ref_tmp_buf, ref_len);
    }
}

static void audio_aec_ref_src_task(void *p)
{
    int res;
    int msg[16];
    while (1) {

        res = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));

        if (aec_ref_src && aec_ref_src->state) {
            s16 *data = (int)msg[1];
            int len = msg[2];
            int rlen = 0;

            aec_ref_src->busy = 1;
            if (cbuf_get_data_len(&aec_ref_src->cbuf) >= CVP_REF_SRC_FRAME_SIZE) {
                cbuf_read(&aec_ref_src->cbuf, aec_ref_src->ref_tmp_buf, CVP_REF_SRC_FRAME_SIZE);
                audio_aec_ref_src_run(aec_ref_src->ref_tmp_buf, CVP_REF_SRC_FRAME_SIZE);
            }
            aec_ref_src->busy = 0;
        }
    }
}

int audio_aec_ref_src_data_fill(void *p, s16 *data, int len)
{
    int ret = 0;
    if (aec_ref_src && aec_ref_src->state) {
        audio_aec_ref_start(1);
        if (0 == cbuf_write(&aec_ref_src->cbuf, data, len)) {
            cbuf_clear(&aec_ref_src->cbuf);
            printf("ref src cbuf wfail!!");
        }
        if (cbuf_get_data_len(&aec_ref_src->cbuf) >= CVP_REF_SRC_FRAME_SIZE) {
            ret = os_taskq_post_msg(CVP_REF_SRC_TASK_NAME, 2, (int)data, len);
        }
    }
    return ret;
}

int audio_aec_ref_src_open(u32 insr, u32 outsr)
{
    if (aec_ref_src) {
        printf("aec_ref_src alreadly open !!!");
        return -1;
    }
    aec_ref_src = zalloc(sizeof(aec_ref_src_t));
    if (aec_ref_src == NULL) {
        printf("aec_ref_src malloc fail !!!");
        return -1;
    }

    cbuf_init(&aec_ref_src->cbuf, aec_ref_src->ref_buf, sizeof(aec_ref_src->ref_buf));
    int err = os_task_create(audio_aec_ref_src_task, NULL, 4, 256, 128, CVP_REF_SRC_TASK_NAME);
    if (err != OS_NO_ERR) {
        printf("task create error!");
        free(aec_ref_src);
        aec_ref_src = NULL;
        return -1;
    }

    /* audio_aec_ref_src_get_output_rate(&aec_ref_src->input_rate, &aec_ref_src->output_rate); */
    aec_ref_src->input_rate = insr;
    aec_ref_src->output_rate = outsr;
    aec_ref_src->sw_src_api = get_rs16_context();
    printf("sw_src_api:0x%x\n", aec_ref_src->sw_src_api);
    ASSERT(aec_ref_src->sw_src_api);
    int sw_src_need_buf = aec_ref_src->sw_src_api->need_buf();
    printf("sw_src_buf:%d\n", sw_src_need_buf);
    aec_ref_src->sw_src_buf = zalloc(sw_src_need_buf);
    ASSERT(aec_ref_src->sw_src_buf, "sw_src_buf zalloc fail");
    RS_PARA_STRUCT rs_para_obj;
    rs_para_obj.nch = 1;
    if (insr == 44100) {
        rs_para_obj.new_insample = 44117;
    } else {
        rs_para_obj.new_insample = insr;
    }
    rs_para_obj.new_outsample = outsr;
    printf("sw src,ch = %d, in = %d,out = %d\n", rs_para_obj.nch, rs_para_obj.new_insample, rs_para_obj.new_outsample);
    aec_ref_src->sw_src_api->open(aec_ref_src->sw_src_buf, &rs_para_obj);

    aec_ref_src->state = 1;
    return 0;
}

void audio_aec_ref_src_close()
{
    if (aec_ref_src) {
        aec_ref_src->state = 0;
        while (aec_ref_src->busy) {
            putchar('w');
            os_time_dly(1);
        }
        int err = os_task_del(CVP_REF_SRC_TASK_NAME);
        if (err) {
            log_i("kill task %s: err=%d\n", CVP_REF_SRC_TASK_NAME, err);
        }
        if (aec_ref_src->sw_src_api) {
            aec_ref_src->sw_src_api = NULL;
        }
        if (aec_ref_src->sw_src_buf) {
            free(aec_ref_src->sw_src_buf);
            aec_ref_src->sw_src_buf = NULL;
        }
        free(aec_ref_src);
        aec_ref_src = NULL;
    }
}
#endif/*CVP_REF_SRC_ENABLE*/

int audio_iis_pcm_write(struct audio_iis_hdl *iis, void *data, int len)
{
    if (iis->state != SOUND_PCM_STATE_RUNNING) {
        return 0;
    }
    int remapping_ch = 2;
    int frames = 0;
    if (iis->input_mapping == SOUND_CHMAP_MONO) {
        frames = len >> 1;
        remapping_ch = 1;
    } else if (iis->input_mapping & SOUND_CHMAP_FR) {
        frames = len >> 2;
        remapping_ch = 2;
    } else {

    }

    u8 convert_mode = iis->bit_width == ALINK_LEN_24BIT ? SAMPLE_16BIT_TO_24BIT : SAMPLE_16BIT_TO_16BIT;
    u8 multiple = convert_mode == SAMPLE_16BIT_TO_24BIT ? 2 : 1;

    s16 *ref_data = (s16 *)data;
    int swp = *ALNK_SWPTR[iis->hw_ch];
    int free_frames = (*ALNK_SHN[iis->hw_ch] - iis->reserved_frames);

    /*printf("free : %d, %d\n", *ALNK_SHN[iis->hw_ch], free_frames);*/
    if (free_frames <= 0) {
        return 0;
    }

    if (free_frames > (frames * multiple)) {
        free_frames = (frames * multiple);
    }

    frames = 0;
    if (swp + free_frames > JL_ALNK0->LEN) {
        frames = (JL_ALNK0->LEN - swp) / multiple;
        __audio_iis_pcm_write((s16 *)(*ALNK_BUF_ADR[iis->hw_ch] + swp * 2 * 2), (s16 *)data, frames, remapping_ch, convert_mode);
        free_frames -= frames * multiple;
        data = (s16 *)data + frames * remapping_ch;
        swp = 0;
    }

    int write_frames = free_frames / multiple;
    __audio_iis_pcm_write((s16 *)(*ALNK_BUF_ADR[iis->hw_ch] + swp * 2 * 2), (s16 *)data, write_frames, remapping_ch, convert_mode);
    frames += write_frames;

    audio_iis_syncts_update_frame(iis);
    *ALNK_SHN[iis->hw_ch] = frames * multiple;
    __asm_csync();
    audio_iis_dma_update_to_syncts(iis, frames);

    /*printf("frames : %d\n", frames);*/
    /*printf("CLK CON2 : %d, %d\n", JL_CLOCK->CLK_CON2 & 0x3, (JL_CLOCK->CLK_CON2 >> 2) & 0x3);*/
#ifdef CVP_REF_SRC_ENABLE
    if (bt_phone_dec_is_running()) {
        audio_aec_ref_src_data_fill(iis, ref_data, (frames << 1) * remapping_ch);
    }
#endif
    return (frames << 1) * remapping_ch;
}

static void audio_iis_dma_fifo_start(struct audio_iis_hdl *iis)
{
    if (iis->state != SOUND_PCM_STATE_PREPARED) {
        return;
    }

    if (iis->prepared_frames) {
        *ALNK_SHN[iis->hw_ch] = iis->prepared_frames;
        __asm_csync();
    }

    local_irq_disable();
    iis->state = SOUND_PCM_STATE_RUNNING;
    ALINK_CHx_IE(iis->hw_ch, 1);
    ALINK_CLR_CHx_PND(iis->hw_ch);
    local_irq_enable();
    audio_iis_syncts_latch_trigger(iis);
}

static int audio_iis_fifo_set_delay(struct audio_iis_hdl *iis)
{
    iis->prepared_frames = (iis->prepared_time * iis->sample_rate / 1000) * (iis->bit_width == ALINK_LEN_24BIT ? 2 : 1);
    int delay_frames = ((iis->delay_time * iis->sample_rate) / 1000 / 2 * 2) * (iis->bit_width == ALINK_LEN_24BIT ? 2 : 1);
    if (iis->prepared_frames >= JL_ALNK0->LEN) {
        iis->prepared_frames = JL_ALNK0->LEN / 2;
    }
    if (delay_frames < JL_ALNK0->LEN) {
        iis->reserved_frames = JL_ALNK0->LEN - delay_frames;
    } else {
        iis->reserved_frames = 0;
    }
    return 0;
}

int audio_iis_trigger_interrupt(struct audio_iis_hdl *iis, int time_ms, void *priv, void (*callback)(void *))
{
    if (iis->state != SOUND_PCM_STATE_RUNNING) {
        return -EINVAL;
    }

    int irq_frames = time_ms * iis->sample_rate / 1000 * (iis->bit_width == ALINK_LEN_24BIT ? 2 : 1);
    int pns = audio_iis_buffered_frames(iis) - irq_frames;
    if (pns < irq_frames) {
        callback(priv);
        return -EINVAL;
    }
    local_irq_disable();
    if (pns > ALINK_OPNS()) {
        ALINK_OPNS_SET(pns);
    }

    iis->irq_trigger = 1;
    iis->trigger_handler = callback;
    iis->trigger_data = priv;
    ALINK_CHx_IE(iis->hw_ch, 1);
    local_irq_enable();

    return 0;
}

int audio_iis_dma_start(struct audio_iis_hdl *iis)
{
    if (iis->state == SOUND_PCM_STATE_RUNNING) {
        return 0;
    }

    audio_iis_fifo_set_delay(iis);
    iis->underrun_pns = (iis->underrun_time ? (iis->underrun_time * iis->sample_rate / 1000) : 0) * (iis->bit_width == ALINK_LEN_24BIT ? 2 : 1);
    ALINK_OPNS_SET((iis->underrun_pns ? iis->underrun_pns : IIS_TX_MIN_PNS));
    iis->state = SOUND_PCM_STATE_PREPARED;
    /*printf("DMA : %d, %d, %d\n", *ALNK_SHN[iis->hw_ch], *ALNK_SWPTR[iis->hw_ch], *ALNK_HWPTR[iis->hw_ch]);*/
    audio_iis_dma_fifo_start(iis);
    alink_start();
    /*printf("[iis dma start]... %d, free : %d, prepared_frames : %d, reserved_frames : %d\n", JL_ALNK0->LEN, *ALNK_SHN[iis->hw_ch], iis->prepared_frames, iis->reserved_frames);*/
}

int audio_iis_dma_stop(struct audio_iis_hdl *iis)
{
    if (iis->state != SOUND_PCM_STATE_RUNNING) {
        return 0;
    }
    /*audio_iis_buffered_frames_fade_out(iis, JL_ALNK0->LEN - *ALNK_SHN[iis->hw_ch] - 1);*/
    ALINK_CHx_IE(iis->hw_ch, 0);
    ALINK_CLR_CHx_PND(iis->hw_ch);

    iis->input_mapping = 0;
    iis->state = SOUND_PCM_STATE_SUSPENDED;
    return 0;
}

//===============================================//
//			     for APP use demo                //
//===============================================//

#ifdef ALINK_TEST_ENABLE

short const tsin_441k[441] = {
    0x0000, 0x122d, 0x23fb, 0x350f, 0x450f, 0x53aa, 0x6092, 0x6b85, 0x744b, 0x7ab5, 0x7ea2, 0x7fff, 0x7ec3, 0x7af6, 0x74ab, 0x6c03,
    0x612a, 0x545a, 0x45d4, 0x35e3, 0x24db, 0x1314, 0x00e9, 0xeeba, 0xdce5, 0xcbc6, 0xbbb6, 0xad08, 0xa008, 0x94fa, 0x8c18, 0x858f,
    0x8181, 0x8003, 0x811d, 0x84ca, 0x8af5, 0x9380, 0x9e3e, 0xaaf7, 0xb969, 0xc94a, 0xda46, 0xec06, 0xfe2d, 0x105e, 0x223a, 0x3365,
    0x4385, 0x5246, 0x5f5d, 0x6a85, 0x7384, 0x7a2d, 0x7e5b, 0x7ffa, 0x7f01, 0x7b75, 0x7568, 0x6cfb, 0x6258, 0x55b7, 0x4759, 0x3789,
    0x2699, 0x14e1, 0x02bc, 0xf089, 0xdea7, 0xcd71, 0xbd42, 0xae6d, 0xa13f, 0x95fd, 0x8ce1, 0x861a, 0x81cb, 0x800b, 0x80e3, 0x844e,
    0x8a3c, 0x928c, 0x9d13, 0xa99c, 0xb7e6, 0xc7a5, 0xd889, 0xea39, 0xfc5a, 0x0e8f, 0x2077, 0x31b8, 0x41f6, 0x50de, 0x5e23, 0x697f,
    0x72b8, 0x799e, 0x7e0d, 0x7fee, 0x7f37, 0x7bed, 0x761f, 0x6ded, 0x6380, 0x570f, 0x48db, 0x392c, 0x2855, 0x16ad, 0x048f, 0xf259,
    0xe06b, 0xcf20, 0xbed2, 0xafd7, 0xa27c, 0x9705, 0x8db0, 0x86ab, 0x821c, 0x801a, 0x80b0, 0x83da, 0x8988, 0x919c, 0x9bee, 0xa846,
    0xb666, 0xc603, 0xd6ce, 0xe86e, 0xfa88, 0x0cbf, 0x1eb3, 0x3008, 0x4064, 0x4f73, 0x5ce4, 0x6874, 0x71e6, 0x790a, 0x7db9, 0x7fdc,
    0x7f68, 0x7c5e, 0x76d0, 0x6ed9, 0x64a3, 0x5863, 0x4a59, 0x3acc, 0x2a0f, 0x1878, 0x0661, 0xf42a, 0xe230, 0xd0d0, 0xc066, 0xb145,
    0xa3bd, 0x9813, 0x8e85, 0x8743, 0x8274, 0x8030, 0x8083, 0x836b, 0x88da, 0x90b3, 0x9acd, 0xa6f5, 0xb4ea, 0xc465, 0xd515, 0xe6a3,
    0xf8b6, 0x0aee, 0x1ced, 0x2e56, 0x3ecf, 0x4e02, 0x5ba1, 0x6764, 0x710e, 0x786f, 0x7d5e, 0x7fc3, 0x7f91, 0x7cc9, 0x777a, 0x6fc0,
    0x65c1, 0x59b3, 0x4bd3, 0x3c6a, 0x2bc7, 0x1a41, 0x0833, 0xf5fb, 0xe3f6, 0xd283, 0xc1fc, 0xb2b7, 0xa503, 0x9926, 0x8f60, 0x87e1,
    0x82d2, 0x804c, 0x805d, 0x8303, 0x8833, 0x8fcf, 0x99b2, 0xa5a8, 0xb372, 0xc2c9, 0xd35e, 0xe4da, 0xf6e4, 0x091c, 0x1b26, 0x2ca2,
    0x3d37, 0x4c8e, 0x5a58, 0x664e, 0x7031, 0x77cd, 0x7cfd, 0x7fa3, 0x7fb4, 0x7d2e, 0x781f, 0x70a0, 0x66da, 0x5afd, 0x4d49, 0x3e04,
    0x2d7d, 0x1c0a, 0x0a05, 0xf7cd, 0xe5bf, 0xd439, 0xc396, 0xb42d, 0xa64d, 0x9a3f, 0x9040, 0x8886, 0x8337, 0x806f, 0x803d, 0x82a2,
    0x8791, 0x8ef2, 0x989c, 0xa45f, 0xb1fe, 0xc131, 0xd1aa, 0xe313, 0xf512, 0x074a, 0x195d, 0x2aeb, 0x3b9b, 0x4b16, 0x590b, 0x6533,
    0x6f4d, 0x7726, 0x7c95, 0x7f7d, 0x7fd0, 0x7d8c, 0x78bd, 0x717b, 0x67ed, 0x5c43, 0x4ebb, 0x3f9a, 0x2f30, 0x1dd0, 0x0bd6, 0xf99f,
    0xe788, 0xd5f1, 0xc534, 0xb5a7, 0xa79d, 0x9b5d, 0x9127, 0x8930, 0x83a2, 0x8098, 0x8024, 0x8247, 0x86f6, 0x8e1a, 0x978c, 0xa31c,
    0xb08d, 0xbf9c, 0xcff8, 0xe14d, 0xf341, 0x0578, 0x1792, 0x2932, 0x39fd, 0x499a, 0x57ba, 0x6412, 0x6e64, 0x7678, 0x7c26, 0x7f50,
    0x7fe6, 0x7de4, 0x7955, 0x7250, 0x68fb, 0x5d84, 0x5029, 0x412e, 0x30e0, 0x1f95, 0x0da7, 0xfb71, 0xe953, 0xd7ab, 0xc6d4, 0xb725,
    0xa8f1, 0x9c80, 0x9213, 0x89e1, 0x8413, 0x80c9, 0x8012, 0x81f3, 0x8662, 0x8d48, 0x9681, 0xa1dd, 0xaf22, 0xbe0a, 0xce48, 0xdf89,
    0xf171, 0x03a6, 0x15c7, 0x2777, 0x385b, 0x481a, 0x5664, 0x62ed, 0x6d74, 0x75c4, 0x7bb2, 0x7f1d, 0x7ff5, 0x7e35, 0x79e6, 0x731f,
    0x6a03, 0x5ec1, 0x5193, 0x42be, 0x328f, 0x2159, 0x0f77, 0xfd44, 0xeb1f, 0xd967, 0xc877, 0xb8a7, 0xaa49, 0x9da8, 0x9305, 0x8a98,
    0x848b, 0x80ff, 0x8006, 0x81a5, 0x85d3, 0x8c7c, 0x957b, 0xa0a3, 0xadba, 0xbc7b, 0xcc9b, 0xddc6, 0xefa2, 0x01d3, 0x13fa, 0x25ba,
    0x36b6, 0x4697, 0x5509, 0x61c2, 0x6c80, 0x750b, 0x7b36, 0x7ee3, 0x7ffd, 0x7e7f, 0x7a71, 0x73e8, 0x6b06, 0x5ff8, 0x52f8, 0x444a,
    0x343a, 0x231b, 0x1146, 0xff17, 0xecec, 0xdb25, 0xca1d, 0xba2c, 0xaba6, 0x9ed6, 0x93fd, 0x8b55, 0x850a, 0x813d, 0x8001, 0x815e,
    0x854b, 0x8bb5, 0x947b, 0x9f6e, 0xac56, 0xbaf1, 0xcaf1, 0xdc05, 0xedd3
};
static u16 tx_s_cnt = 0;
static int get_sine_data(u16 *s_cnt, s16 *data, u16 points, u8 ch)
{
    while (points--) {
        if (*s_cnt >= 441) {
            *s_cnt = 0;
        }
        *data++ = tsin_441k[*s_cnt];
        if (ch == 2) {
            *data++ = tsin_441k[*s_cnt];
        }
        (*s_cnt)++;
    }
    return 0;
}


extern struct audio_decoder_task decode_task;
void handle_tx(void *priv, void *buf, u16 len)
{
    /* putchar('s'); */
    /* audio_decoder_resume_all(&decode_task); */
    /* putchar('t'); */
    /* return; */

#if 1
    get_sine_data(&tx_s_cnt, buf, len / 2 / 2, 2);
#else
    s16 *data_tx = (s16 *)buf;
    /* s16 *data = (s16 *)data_buf; */
    for (int i = 0; i < len / sizeof(s16); i++) {
        /* data_tx[i] = 0xFFFF; */
        data_tx[i] = 0x5a5a;
    }
#endif
}


void handle_rx(void *buf, u16 len)
{
}

ALINK_PARM	test_alink = {
    .mclk_io = IO_PORTA_01,
    .sclk_io = IO_PORTA_02,
    .lrclk_io = IO_PORTA_00,
    .ch_cfg[1].data_io = IO_PORTB_09,
    /* .ch_cfg[0].data_io = IO_PORTA_04, */
    /* .ch_cfg[2].data_io = IO_PORTB_09, */
    /* .mode = ALINK_MD_IIS_LALIGN, */
    .mode = ALINK_MD_IIS,
    /* .role = ALINK_ROLE_SLAVE, */
    .role = ALINK_ROLE_MASTER,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    /* .bitwide = ALINK_LEN_24BIT, */
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    /* .dma_len = ALNK_BUF_POINTS_NUM * 2 * 2 , */
    .dma_len = 4 * 1024,
    .sample_rate = 44100,
    /* .sample_rate = TCFG_IIS_SAMPLE_RATE, */
    .buf_mode = ALINK_BUF_CIRCLE,
    /* .buf_mode = ALINK_BUF_DUAL, */
};



int audio_link_ch_start(u8 ch_idx);
void audio_link_test(void)
{
    alink_init(&test_alink);
    alink_channel_init(1, ALINK_DIR_TX, NULL, handle_tx);
    /* alink_channel_init(1, ALINK_DIR_RX, handle_rx); */
    /* alink_channel_init(2, ALINK_DIR_TX, handle_tx1); */
    alink_start();

    alink_info_dump();
}


static u8 alink_init_cnt = 0;
void audio_link_init(void)
{
    if (alink_init_cnt == 0) {
        alink_init(&test_alink);
        alink_start();
    }
    alink_init_cnt++;
}

void audio_link_uninit(void)
{
    alink_init_cnt--;
    if (alink_init_cnt == 0) {
        alink_uninit();
    }
}

#endif

#endif /*TCFG_AUDIO_INPUT_IIS || TCFG_AUDIO_OUTPUT_IIS*/

