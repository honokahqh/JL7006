#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_link.h"
#include "audio_iis.h"
#include "app_action.h"
#include "audio_dec/audio_dec_linein.h"
#include "asm/audio_src.h"
#if 1
#define ADC_LINEIN_LOG	printf
#else
#define ADC_LINEIN_LOG(...)
#endif


#if  TCFG_ADC_IIS_ENABLE
static  struct audio_dac_channel dac_ch;
extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;

#define ADC_LINEIN_CH_NUM        2	/*支持的最大采样通道(max = 2)*/
#define ADC_LINEIN_BUF_NUM        2	/*采样buf数*/
#define ADC_LINEIN_IRQ_POINTS     32//128/*采样中断点数*/
#define ADC_LINEIN_BUFS_SIZE      (ADC_LINEIN_CH_NUM * ADC_LINEIN_BUF_NUM * ADC_LINEIN_IRQ_POINTS * 2)
#define ADC_STORE_PCM_SIZE     (ADC_LINEIN_BUFS_SIZE * 6 * 4)
#define IIS_TX_BYTES             256  //128 //iis发送每次写多少字节
#define IIS_RX_BYTES             256//iis接收每次读多少字节
#define IIS_SALVE_PW_OFF_TIME    1 //10 //从机多少秒无iis时钟或开机多少秒没收到主机命令就关机
//主机要求从机adc 的配置
#define LINEIN_SAMPLE_RATE 44100
//打开通道数
#define LINEIN_CH_IDX  (LADC_CH_LINEIN_R|LADC_CH_LINEIN_L)
#define LINEIN_GAIN   4

#define  CMD_OPEN_LINEIN    0x12345678

//定义命令的长度为多少字节, 变量个数* 变量类型字节数
#define  CMD_LEN      4 * 4


//主机唤醒从机的io
#define IIS_SLAVE_PWON_PORT   IO_PORTC_02

struct adc_linein {
    u8 adc_2_dac;
    u8 linein_idx;
    u16 sr;
    struct audio_adc_output_hdl adc_output;
    struct adc_linein_ch linein_ch;
    s16 adc_buf[ADC_LINEIN_BUFS_SIZE];
    s16 temp_buf[ADC_LINEIN_IRQ_POINTS * 2 * 2];
    cbuffer_t cbuf;
    s16 *store_pcm_buf;
    u8 channel_num;
#if IIS_LINEIN_HW_SRC_EN	//iis从机打开硬件变采样
    struct audio_src_handle *hw_src;
    cbuffer_t hw_src_cbuf;
    OS_SEM  hw_src_sem;		//信号量
    s16 *hw_src_buf;
    u16 indata[256];
    int src_output_rate;
#endif
};
static struct adc_linein *linein = NULL;

static s16 temp_buf_data[IIS_RX_BYTES / 2] = {0};
static u32 temp_buf_cmd[CMD_LEN / 4]  = {0};

ALINK_PARM	alink_param = {
#if (IIS_ROLE == ROLE_S)
    .mclk_io = IO_PORTB_01,
    .sclk_io = IO_PORTB_01,
#else
    .mclk_io = IO_PORTC_04,
    .sclk_io = IO_PORTC_04,	//这个不知道为什么这样子配置，但是这样子它跑起来了
#endif

#if (IIS_ROLE == ROLE_M)
    .lrclk_io = IO_PORTC_03,
#else
    .lrclk_io = IO_PORTB_02,	//iis slave
#endif
    .ch_cfg[0].data_io = IO_PORTC_02,  //接收
    .ch_cfg[1].data_io = IO_PORTC_02,  //发送 //节约io可使用 和接收相同的io口
    .mode = ALINK_MD_IIS,
#if (IIS_ROLE == ROLE_M)
    .role = ALINK_ROLE_MASTER,
#else
    .role = ALINK_ROLE_SLAVE,
#endif
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = 1024 * 4,
    .sample_rate = 44100,
    .buf_mode = ALINK_BUF_CIRCLE,
};

struct iis_iis_hdl {
    u8 tx_is_data;
    u32 tx_cmd;
    u8 rx_is_data;
    u32 r_cmd;
    u8 rcmd_flag;
    OS_SEM get_cmd_sem;
    u32 linein_sample_rate;
    u32 linein_ch_idx;
    u32 linein_gain;
    u32 rx_isr_cnt;
};
static struct iis_iis_hdl *iis_hdl = NULL;



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
static u16 tx_s_cnt = 0;
/*
*********************************************************************
*                  AUDIO linein OUTPUT
* Description: linein采样数据回调
* Arguments  : pric 私有参数
*			   data	linein数据
*			   len	数据长度
* Return	 : 0 成功 其他 失败
* Note(s)    : 单声道数据格式
*			   L0 L1 L2 L3 L4 ...
*				双声道数据格式()
*			   L0 R0 L1 R1 L2 R2...
*********************************************************************
*/
static void adc_linein_output(void *priv, s16 *data, int len)
{
    int wlen;

#if 0
    if (linein->channel_num == 1) { //单变双
        for (int i = 0; i < (len / 2); i++) {
            linein->temp_buf[i * 2] = data[i];
            linein->temp_buf[i * 2 + 1] = data[i];
        }
        len = len * 2;
    }
#endif

    if (linein->channel_num == 1) {
        wlen = cbuf_write(&linein->cbuf, linein->temp_buf, len);
    } else {
        wlen = cbuf_write(&linein->cbuf, data, len * linein->channel_num);
    }

#if IIS_LINEIN_HW_SRC_EN
    os_sem_post(&linein->hw_src_sem);
#endif

    if (wlen != len * linein->channel_num) {
        putchar('W');
        putchar('1');
    }

#if 0
    if (linein && linein->adc_2_dac) {
        if (linein->linein_idx == (LADC_CH_LINEIN_L | LADC_CH_LINEIN_R)) {
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
            wlen = audio_dac_write(&dac_hdl, data, len * 2);
#else
            s16 *mono_data = data;
            for (int i = 0; i < (len / 2); i++) {
                mono_data[i] = data[i * 2];			/*linein_L*/
            }
            wlen = audio_dac_write(&dac_hdl, mono_data, len);
#endif/*TCFG_AUDIO_DAC_CONNECT_MODE*/
        } else {
            wlen = audio_dac_write(&dac_hdl, data, len);
        }
    }
#endif
}

static void handle_tx(void *priv, void *buf, u16 len)
{
    len = IIS_TX_BYTES; //每次写多少个字节

    u16 s_points =  JL_ALNK0->SHN1;
    if (s_points < len / 4) {
        len = s_points * 4;
    }

    if (iis_hdl->tx_is_data) {
        int rlen = 0;

#if IIS_LINEIN_HW_SRC_EN
        static u8 flag = 0;
        int cbuf_len = 0;
        cbuf_len = cbuf_get_data_len(&linein->hw_src_cbuf);
        //开始时先缓存一定的数量
        if (flag == 0 && cbuf_len < IIS_TX_BYTES) {
            memset(buf, 0, len);
            return;
        }
        flag = 1;
        if (cbuf_len >= len) {
            rlen = cbuf_read(&linein->hw_src_cbuf, buf, len);
#else
        if (cbuf_get_data_size(&linein->cbuf) > len) { //大于两次中断再开始读
            rlen = cbuf_read(&linein->cbuf, buf, len);
#endif
        } else {
            putchar('R');
            memset(buf, 0, len);
            rlen = len;
        }
        if (rlen != len) {
            putchar('r');
        }
    } else { //是命令
        u32 *buf_u32 = buf;
        buf_u32[0] = iis_hdl->tx_cmd;
        buf_u32[1] = LINEIN_SAMPLE_RATE;
        buf_u32[2] = LINEIN_CH_IDX;
        buf_u32[3] = LINEIN_GAIN;
    }
    JL_ALNK0->SHN1 = len / 4;
}


//主机进入的函数
static void handle_rx(void *priv, void *buf, u16 len)
{
    len = IIS_RX_BYTES; //每次读多少个字节
    int wlen = 0;
    s16 *data = buf;
    u16 s_points =  JL_ALNK0->SHN0;
    if (s_points < len / 4) {
        len = s_points * 4;
    }

#if 1 //把数据拷出来
    memcpy(temp_buf_data, data, len);
    data = temp_buf_data;
#endif

    JL_ALNK0->SHN0 = len / 4;

    if (iis_hdl->rx_is_data) { //收到的是数据

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF)

#else /*DAC_OUTPUT_MONO_L || DAC_OUTPUT_MONO_R || DAC_OUTPUT_MONO_LR_DIFF*/
        // 双声道转单声道
        for (int i = 0; i < len / 2 / 2; i++) {
            data[i] = data[2 * i];
        }
        len >>= 1;
#endif /*TCFG_AUDIO_DAC_CONNECT_MODE*/

        while (len) {
#if TCFG_LINEIN_PCM_DECODER
            wlen = linein_pcm_dec_data_write(data, len);
            if (wlen != len) {
                putchar('l');
            }
#else
            wlen = sound_pcm_dev_write(&dac_ch, data, len);
#endif
            len -= wlen;
        }
    } else { //收到的是命令
        u8 *temp = memchr(data, CMD_OPEN_LINEIN, len);
        u32 *data_u32 = data;
        if (temp) {
            memcpy(temp_buf_cmd, temp, CMD_LEN);
            if (temp_buf_cmd[2] == LADC_CH_LINEIN_L || temp_buf_cmd[2] == LADC_CH_LINEIN_R || temp_buf_cmd[2] == (LADC_CH_LINEIN_R | LADC_CH_LINEIN_L)) { //确保数据正确
                iis_hdl->linein_sample_rate = temp_buf_cmd[1];
                iis_hdl->linein_ch_idx = temp_buf_cmd[2];
                iis_hdl->linein_gain = temp_buf_cmd[3];
                iis_hdl->rcmd_flag = 1;
                os_sem_set(&iis_hdl->get_cmd_sem, 0);
                os_sem_post(&iis_hdl->get_cmd_sem);
            }
        } else { //从机开机长时间没收到命令
            iis_hdl->rx_isr_cnt++;
            u32 iis_isr_target_cnt = IIS_SALVE_PW_OFF_TIME * alink_param.sample_rate / (IIS_RX_BYTES / 4);
            if (iis_hdl->rx_isr_cnt > iis_isr_target_cnt) {
                iis_hdl->rx_isr_cnt = 0;
#if 0
                void audio_link_close(void);
                audio_link_close();
                sys_enter_soft_poweroff(NULL);
#endif
            }
        }
    }
}

u32 iis_receive_cmd(u32 cmd) //判断是否读到了某个命令
{
    iis_hdl->r_cmd = cmd;
    if (iis_hdl->rcmd_flag) {
        iis_hdl->rcmd_flag = 0;
        return 1;
    } else {
        return 0;
    }
}

void audio_link_tx_open(u8 is_data, u32 tx_cmd) //0是命令，1是数据
{
    iis_hdl ->tx_cmd = tx_cmd;
    iis_hdl ->tx_is_data = is_data;
    alink_init(&alink_param);
    alink_channel_init(1, ALINK_DIR_TX, NULL, handle_tx);
    alink_start();
}

void audio_iis_send_cmd(u32 tx_cmd)
{
    iis_hdl ->tx_cmd = tx_cmd;
    iis_hdl ->tx_is_data = 0;
}



void audio_iis_send_data_en()
{
    iis_hdl ->tx_is_data = 1;
}


void audio_link_rx_open(u8 is_data, u32 cmd)
{
    iis_hdl->r_cmd = cmd;
    iis_hdl->rx_is_data = is_data;
    if (is_data) {
        audio_dac_new_channel(&dac_hdl, &dac_ch);
        struct audio_dac_channel_attr attr;
        attr.delay_time = 6;
        attr.protect_time = 8;
        attr.write_mode = WRITE_MODE_BLOCK;
        audio_dac_channel_set_attr(&dac_ch, &attr);
        JL_AUDIO->DAC_VL0 = 16384 | 16384 << 16;
        sound_pcm_dev_start(&dac_ch, 44100, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
    }
    alink_init(&alink_param);
    alink_channel_init(0, ALINK_DIR_RX, NULL, handle_rx); //通道0接收
    alink_start();
}


void audio_link_close(void)
{
    alink_channel_close(0);  //关闭接收
    alink_channel_close(1);  //关闭发送
    alink_isr_en(0);
    alink_uninit();
}

void iis_clk_check(void)
{
    u8 check_cnt = 100;
    u8 last_state = gpio_read(alink_param.lrclk_io);
    while (check_cnt--) {
        if (gpio_read(alink_param.lrclk_io) != last_state) {
            /* printf("enter linein_iis.c @@@@@@@@@@@@@@@@@@@@@@@@@@@@@%d\n", __LINE__); */
            return;
        };
    }
    printf("-- Slave chip Enter soft poweroff!");
    /* 从机秒关机，防止linein线拔插过快, 从机在关机过程 */
    extern void dac_power_off(void);
    dac_power_off();    // 关机前先关dac
    power_set_soft_poweroff();
    /* sys_enter_soft_poweroff(NULL); */
}

#if IIS_LINEIN_HW_SRC_EN
//***************************************************
// 			添加iis 硬件src任务
//**************************************************
static void hw_src_task(void *priv)
{
    u32 k = 0;
    u32 data_len = 0;
    printf("======================================= hw_src_task\n");
    while (1) {
        os_sem_pend(&linein->hw_src_sem, 0);
        data_len = cbuf_read(&linein->cbuf, linein->indata, cbuf_get_data_len(&linein->cbuf));
        audio_src_resample_write(linein->hw_src, linein->indata, data_len);
    }
}

//**************************************************
//			硬件 src 变采样定时器回调函数
//**************************************************
static void hw_src_timer_cb(void *priv)
{
    int cbuf_len = cbuf_get_data_len(&linein->hw_src_cbuf);
    if (cbuf_len < ADC_STORE_PCM_SIZE / 4) {
        //数据较少，需要增大数据量
        linein->src_output_rate += 10;
        audio_hw_src_set_rate(linein->hw_src, 44100, linein->src_output_rate);
    } else if (cbuf_len > (ADC_STORE_PCM_SIZE * 3 / 4)) {
        //数据太多了，需要减小数据量
        linein->src_output_rate -= 10;
        audio_hw_src_set_rate(linein->hw_src, 44100, linein->src_output_rate);
    }
}

//*******************************************
//				变采样输出回调
//*******************************************
static int iis_linein_src_output(struct adc_linein *hdl, s16 *data, u16 len)
{
    //写到iis tx 里的cbuf
    int wlen = cbuf_write(&linein->hw_src_cbuf, data, len);
    if (wlen != len) {
        putchar('u');
    }
    return len;
}

static void iis_linein_slave_hw_src_init(struct adc_linein *hdl, int insample, int outsample)
{
    if (!hdl) {
        return;
    }
    printf("=================== open hw src!!");
    hdl->hw_src = zalloc(sizeof(struct audio_src_handle));
    if (hdl->hw_src) {
        u8 channel = 2;
        u8 output_channel = 2;
        audio_hw_src_open(hdl->hw_src, channel, output_channel);
        audio_hw_src_set_rate(hdl->hw_src, insample, outsample);
        audio_src_set_output_handler(hdl->hw_src, hdl, (int (*)(void *, void *, int))iis_linein_src_output);
        printf("=================== open hw src success!\n");
    }
}
#endif
/*
*********************************************************************
*                  AUDIO LINEIN IIS OPEN
* Description: 打开linein通道
* Arguments  : linein_idx 		linein通道
*			   gain			linein增益
*			   sr			linein采样率
*			   linein_2_dac 	linein数据（通过DAC）监听
* Return	 : None.
* Note(s)    : (1)打开一个linein通道示例：
*				audio_linein_iis_open(LADC_CH_LINEIN_L,10,16000,1);
*				或者
*				audio_linein_iis_open(LADC_CH_LINEIN_R,10,16000,1);
*			   (2)打开两个linein通道示例：
*				audio_linein_iis_open(LADC_CH_LINEIN_R|LADC_CH_LINEIN_L,10,16000,1);
*********************************************************************
*/
void audio_linein_iis_open(u8 linein_idx, u8 gain, u16 sr, u8 linein_2_dac)
{
    linein_2_dac = 0;
    ADC_LINEIN_LOG("audio_adc_linein open:%d,gain:%d,sr:%d,linein_2_dac:%d\n", linein_idx, gain, sr, linein_2_dac);
    linein = zalloc(sizeof(struct adc_linein));
    if (linein) {
        linein->sr = sr;
        linein->store_pcm_buf = zalloc(ADC_STORE_PCM_SIZE);
        if (!linein->store_pcm_buf) {
            return;
        }

        if (linein_idx & LADC_CH_MIC_L) {
            linein->channel_num++;
        }
        if (linein_idx & LADC_CH_MIC_R) {
            linein->channel_num++;
        }

#if IIS_LINEIN_HW_SRC_EN
        //打开了iis从机变采样
        linein->src_output_rate = 44100;
        iis_linein_slave_hw_src_init(linein, 44100, linein->src_output_rate);
        linein->hw_src_buf = zalloc(ADC_STORE_PCM_SIZE);
        cbuf_init(&linein->hw_src_cbuf, linein->hw_src_buf, ADC_STORE_PCM_SIZE);
        os_sem_create(&linein->hw_src_sem, 0);

        //创建任务
        task_create(hw_src_task, NULL, "iis_linein_hw_src");
        sys_timer_add(NULL, hw_src_timer_cb, 500);
#endif

        cbuf_init(&linein->cbuf, linein->store_pcm_buf, ADC_STORE_PCM_SIZE);

        //step0:打开linein通道，并设置增益
        audio_adc_linein_open(&linein->linein_ch, linein_idx, &adc_hdl);

        audio_adc_linein_set_gain(&linein->linein_ch, linein_idx, gain);
        //step1:设置linein通道采样率
        audio_adc_linein_set_sample_rate(&linein->linein_ch, sr);
        //step2:设置linein采样buf
        audio_adc_linein_set_buffs(&linein->linein_ch, \
                                   linein->adc_buf, \
                                   ADC_LINEIN_IRQ_POINTS * 2, \
                                   ADC_LINEIN_BUF_NUM);
        //step3:设置linein采样输出回调函数
        linein->adc_output.handler = adc_linein_output;
        audio_adc_linein_add_output_handler(&linein->linein_ch, &linein->adc_output);
        //step4:启动linein通道采样
        audio_adc_linein_start(&linein->linein_ch);
        audio_link_tx_open(1, 0); //打开iis发送
        /*监听配置（可选）*/
        linein->adc_2_dac = linein_2_dac;
        linein->linein_idx = linein_idx;
        if (linein_2_dac) {
            ADC_LINEIN_LOG("max_sys_vol:%d\n", get_max_sys_vol());
            app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
            ADC_LINEIN_LOG("cur_vol:%d\n", app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
            audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
            audio_dac_set_sample_rate(&dac_hdl, sr);
            audio_dac_start(&dac_hdl);
        }
    }
    ADC_LINEIN_LOG("audio_adc_linein start succ\n");
}

/*
*********************************************************************
*                  AUDIO ADC linein CLOSE
* Description: 关闭linein采样模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_linein_iis_close(void)
{
    ADC_LINEIN_LOG("audio_linein_iis_close\n");
    if (linein) {
        linein->channel_num = 0;
        free(linein->store_pcm_buf);

        audio_adc_linein_del_output_handler(&linein->linein_ch, &linein->adc_output);
        audio_adc_linein_close(&linein->linein_ch);
        free(linein);
        linein = NULL;
    }
}


void pull_io_awaken_slave(void)
{
#if 1
    //拉高io 打开从机
    printf("\n\n -- Pull io to awaken slave chip!!");
    /* gpio_set_direction(IIS_SLAVE_PWON_PORT, 1); */
    /* gpio_set_pull_up(LINEIN_DET_PORT, 1); */
    /* gpio_set_pull_down(LINEIN_DET_PORT, 0); */
    gpio_set_direction(IIS_SLAVE_PWON_PORT, 0);
    gpio_set_output_value(IIS_SLAVE_PWON_PORT, 0);
    os_time_dly(40);
    gpio_set_output_value(IIS_SLAVE_PWON_PORT, 1);
#endif
}

void audio_adc_iis_master_open(void)
{
    printf("****************iis master open*******************\n");
    pull_io_awaken_slave();
    if (!iis_hdl) {
        iis_hdl = zalloc(sizeof(struct iis_iis_hdl));
    }
    audio_link_tx_open(0, CMD_OPEN_LINEIN); //发命令
    os_time_dly(10); //从机软关机之后重新打开需要发送较久的命令
    audio_link_close(); //关掉发送
    audio_link_rx_open(1, 0); //接收播到dac
}


void  audio_adc_iis_master_close(void)
{
    if (iis_hdl) {
        free(iis_hdl);
        iis_hdl = NULL;
    }
    audio_link_close();
}



void  audio_adc_iis_slave_open(void)
{
    printf("****************iis slave open*******************\n");
    clk_set("sys", 48 * 1000000);		//这句话得加，否则从机hw_src跑不过来, 依旧有可能会出现po声
    u32 cur_clock = clk_get("sys");
    printf("-- slave clk : %d\n", cur_clock);
#if 1
    printf("\n\n-- slave chip, awaken io port is : IO_PORTC_02 \n");
    sys_timer_add(NULL, iis_clk_check, IIS_SALVE_PW_OFF_TIME * 1000); //每10s检测一次是否有iis时钟，没有进入软关机
#endif
    if (!iis_hdl) {
        iis_hdl = zalloc(sizeof(struct iis_iis_hdl));
    }
    os_sem_create(&iis_hdl->get_cmd_sem, 0);
    audio_link_rx_open(0, CMD_OPEN_LINEIN);
    os_sem_pend(&iis_hdl->get_cmd_sem, 0);  //等待接收命令，如果没有收到主机的命令，则pend住，如果发现功能不正常，这句代码注释掉
    printf("\nreceive cmd!\n");
    audio_link_close(); //关掉接收

    /* audio_linein_iis_open(iis_hdl->linein_ch_idx, iis_hdl->linein_gain, iis_hdl->linein_sample_rate, 0); //打开linein发送 */
    audio_linein_iis_open((LADC_CH_LINEIN_R | LADC_CH_LINEIN_L), iis_hdl->linein_gain, iis_hdl->linein_sample_rate, 0); //打开linein发送
}


void  audio_adc_iis_slave_close(void)
{
    audio_linein_iis_close();
    if (iis_hdl) {
        os_sem_del(&iis_hdl->get_cmd_sem, 0);
        free(iis_hdl);
        iis_hdl = NULL;
    }
    audio_link_close(); //关掉发送
}



#if 1
static u8 linein_idle_query()
{
    return iis_hdl ? 0 : 1;
}

REGISTER_LP_TARGET(linein_lp_target) = {
    .name = "linein",
    .is_idle = linein_idle_query,
};

#endif
#endif


