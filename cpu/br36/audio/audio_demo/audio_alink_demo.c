/*
 ****************************************************************************
 *							Audio ALINK Demo
 *
 *Description	: Audio ALINK使用范例，AudioLink是一个通用的双声道音频接口，
 *				  连接信号有MCLK,SCLK,LRCK,DATA,支持IIS模式。
 *Notes			: 本demo为开发测试范例，请不要修改该demo， 如有需求，请自行
 *				  复制再修改
 ****************************************************************************
 */
#include "audio_demo.h"
#include "audio_link.h"
#include "media/includes.h"
#include "audio_config.h"

#define ALNK_DEMO_BUF_POINTS_NUM    256     //IIS中断点数
#define IIS_DEMO_SAMPLE_RATE        16000   //iis默认采样率
#define ALINK_TX_CH                 0       //IIS输出通道号
#define ALINK_RX_CH                 1       //IIS输入通道号

extern struct audio_dac_hdl dac_hdl;
extern void alink_isr_en(u8 en);
ALINK_PARM  *alink_demo_hdl = NULL;

ALINK_PARM	alink_demo = {
    .mclk_io = IO_PORTA_03,
    .sclk_io = IO_PORTA_02,
    .lrclk_io = IO_PORTA_00,
    .ch_cfg[0].data_io = IO_PORTA_04,
    .ch_cfg[1].data_io = IO_PORTB_09,
    /* .ch_cfg[2].data_io = IO_PORTB_09, */
    /* .mode = ALINK_MD_IIS_LALIGN, */
    .mode = ALINK_MD_IIS,
    /* .role = ALINK_ROLE_SLAVE, */
    .role = ALINK_ROLE_MASTER,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    /* .bitwide = ALINK_LEN_24BIT, */
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = ALNK_DEMO_BUF_POINTS_NUM * 2 * 2,
    .sample_rate = IIS_DEMO_SAMPLE_RATE,
    .buf_mode = ALINK_BUF_CIRCLE,
    /* .buf_mode = ALINK_BUF_DUAL, */
};

static short const tsin_441k[441] = {
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

static short const sin0_16k[16] = {
    0x0000, 0x30fd, 0x5a83, 0x7641, 0x7fff, 0x7642, 0x5a82, 0x30fc, 0x0000, 0xcf04, 0xa57d, 0x89be, 0x8000, 0x89be, 0xa57e, 0xcf05,
};
static u16 tx1_s_cnt = 0;
static int get_sine0_data(u16 *s_cnt, s16 *data, u16 points, u8 ch)
{
    while (points--) {
        if (*s_cnt >= 16) {
            *s_cnt = 0;
        }
        *data++ = sin0_16k[*s_cnt] / 2;
        if (ch == 2) {
            *data++ = sin0_16k[*s_cnt] / 2;
        }
        (*s_cnt)++;
    }
    return 0;
}

#define IIS_SRC_ENABLE
#ifdef IIS_SRC_ENABLE
#include "Resample_api.h"
static RS_STUCT_API *sw_src_api = NULL;
static u8 *sw_src_buf = NULL;

static int sw_src_init(u8 nch, u16 insample, u16 outsample)
{
    sw_src_api = get_rs16_context();
    g_printf("sw_src_api:0x%x\n", sw_src_api);
    ASSERT(sw_src_api);
    u32 sw_src_need_buf = sw_src_api->need_buf();
    g_printf("sw_src_buf:%d\n", sw_src_need_buf);
    sw_src_buf = malloc(sw_src_need_buf);
    ASSERT(sw_src_buf);
    RS_PARA_STRUCT rs_para_obj;
    rs_para_obj.nch = nch;

    rs_para_obj.new_insample = insample;//48000;
    rs_para_obj.new_outsample = outsample;//16000;
    printf("sw src,in = %d,out = %d\n", rs_para_obj.new_insample, rs_para_obj.new_outsample);
    sw_src_api->open(sw_src_buf, &rs_para_obj);
    return 0;
}

static int sw_src_run(s16 *indata, s16 *outdata, u16 len)
{
    int outlen = 0;
    if (sw_src_api && sw_src_buf) {
        outlen = sw_src_api->run(sw_src_buf, indata, len >> 1, outdata);
        /* ASSERT(outlen <= (sizeof(outdata) >> 1));  */
        outlen = outlen << 1;
        /* printf("%d\n",outlen); */
    }
    return outlen;
}

static void sw_src_exit(void)
{
    if (sw_src_buf) {
        free(sw_src_buf);
        sw_src_buf = NULL;
        sw_src_api = NULL;
    }
}
#endif /*IIS_SRC_ENABLE*/

/*
*********************************************************************
*                  AUDIO IIS RX OUTPUT
* Description: iis rx 通道采样数据回调
* Arguments  : buf	iis rx通道数据
*			   len	数据长度
* Return	 : NONE
* Note(s)    : 双声道数据格式()
*			   L0 R0 L1 R1 L2 R2...
*********************************************************************
*/
u8 iis_mic_2_dac = 0;
static void audio_iis_mic_output(void *buf, u16 len)
{
    putchar('r');
    int wlen = 0;
    s16 *data = buf;
#if TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF

#else /*DAC_OUTPUT_MONO_L || DAC_OUTPUT_MONO_R || DAC_OUTPUT_MONO_LR_DIFF*/
    // 双声道转单声道
    for (int i = 0; i < len / 2 / 2; i++) {
        data[i] = data[2 * i];
    }
    len >>= 1;
#endif /*TCFG_AUDIO_DAC_CONNECT_MODE*/
#ifdef IIS_SRC_ENABLE
    len = sw_src_run(data, data, len);
#endif /*IIS_SRC_ENABLE*/
    if (iis_mic_2_dac) {
        wlen = audio_dac_write(&dac_hdl, data, len);
        if (wlen != len) {
            printf("iis_rx demo dac write err %d %d", wlen, len);
        }
    }

}

/*
*********************************************************************
*                  AUDIO IIS TX IRQ
* Description: iis TX 通道输出采样数据回调
* Arguments  : buf	iis tx通道数据
*			   len	数据长度
* Return	 : NONE
* Note(s)    : 双声道数据格式()
*			   L0 R0 L1 R1 L2 R2...
*********************************************************************
*/
static void audio_iis_tx_isr(void *buf, u16 len)
{
    putchar('t');
    //44100
    if (alink_demo.sample_rate == 44100) {
        get_sine_data(&tx_s_cnt, buf, len / 2 / 2, 2);
    } else if (alink_demo.sample_rate == 16000) {
        //16000
        get_sine0_data(&tx1_s_cnt, buf, len / 2 / 2, 2);
    }
}

/*
*********************************************************************
*                  AUDIO IIS OUTPUT/INPUT OPEN
* Description: 打开iis通道
* Arguments  : iis_rx 		是否打开iis输入
*			   iis_tx		是否打开iis输出
*			   sr			iis采样率
*			   iis_rx_2_dac iis_rx数据（通过DAC）监听
* Return	 : None.
* Note(s)    : (1)打开一个iis rx通道示例：
*				audio_link_demo_open(1,0,44100,1);
*			   (2)打开一个iis tx通道示例：
*               audio_link_demo_open(0,1,44100,0);
*			   (3)同时打开iis tx 和 rx 通道示例：
*				audio_link_demo_open(1,1,44100,1);
*********************************************************************
*/
void audio_link_demo_open(u8 iis_rx, u8 iis_tx, u16 sr, u8 iis_rx_2_dac)
{
    printf("audio_link_test_demo\n");
    iis_mic_2_dac = iis_rx_2_dac;
    alink_demo_hdl = &alink_demo;
    /*使用wm8978模块做iis输入*/
#if 0
    extern u8 WM8978_Init(u8 dacen, u8 adcen);
    WM8978_Init(0, 1);
#endif
#ifdef IIS_SRC_ENABLE
    u16 insample = 625 * 20;
    u16 outsample = 624 * 20;
#if TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF
    sw_src_init(2, insample, outsample);
#else /*DAC_OUTPUT_MONO_L || DAC_OUTPUT_MONO_R || DAC_OUTPUT_MONO_LR_DIFF*/
    sw_src_init(1, insample, outsample);
#endif /*TCFG_AUDIO_DAC_CONNECT_MODE*/
#endif /*IIS_SRC_ENABLE*/
    alink_demo.sample_rate = sr;
    alink_init(alink_demo_hdl);
    if (iis_tx) {
        alink_channel_init(ALINK_TX_CH, ALINK_DIR_TX, NULL, audio_iis_tx_isr);
        /* alink_channel_init(2, ALINK_DIR_TX, handle_tx1); */
    }
    if (iis_rx) {
        alink_channel_init(ALINK_RX_CH, ALINK_DIR_RX, NULL, audio_iis_mic_output);
    }
    /* alink_isr_en(1); */
    alink_start();
    /*iis rx通道数据是否输出到dac监听*/
    if (iis_rx && iis_mic_2_dac) {
        extern void app_audio_state_switch(u8 state, s16 max_volume);
        app_audio_state_switch(APP_AUDIO_STATE_MUSIC, 16);
        audio_dac_set_volume(&dac_hdl, 16);
        audio_dac_set_sample_rate(&dac_hdl, sr);
        audio_dac_start(&dac_hdl);
    }
}

/*
*********************************************************************
*                  AUDIO IIS OUTPUT/INPUT OPEN
* Description: 打开iis通道
* Arguments  : iis_rx 		是否关闭iis输入
*			   iis_tx		是否关闭iis输出
* Return	 : None.
* Note(s)    : (1)关闭一个iis rx通道示例：
*				audio_link_demo_open(1,0);
*			   (2)关闭一个iis tx通道示例：
*               audio_link_demo_open(0,1);
*			   (3)同时关闭iis tx 和 rx 通道示例：
*				audio_link_demo_open(1,1);
*********************************************************************
*/
void audio_link_demo_close(u8 iis_rx, u8 iis_tx)
{
    if (iis_tx) {
        alink_channel_close(ALINK_TX_CH);
    }
    if (iis_rx) {
        alink_channel_close(ALINK_RX_CH);
    }
    if (iis_rx && iis_tx) {
        alink_isr_en(0);
        alink_uninit();
    }
    if (alink_demo_hdl) {
        alink_demo_hdl = NULL;
    }
#ifdef IIS_SRC_ENABLE
    sw_src_exit();
#endif /*IIS_SRC_ENABLE*/
}

static u8 alink_demo_idle_query()
{
    return alink_demo_hdl ? 0 : 1;
}

REGISTER_LP_TARGET(alink_demo_lp_target) = {
    .name = "alink_demo",
    .is_idle = alink_demo_idle_query,
};

