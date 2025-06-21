/*
 ****************************************************************************
 *							Audio ADC Demo
 *
 *Description	: Audio ADC使用范例
 *Notes			: (1)本demo为开发测试范例，请不要修改该demo， 如有需求，请自行
 *				  复制再修改
 *				  (2)linein工作模式说明
 *				A.单端隔直(cap_mode)
 *				  可以选择linein供电方式：外部供电还是内部供电(linein_bias_inside = 1)
 *				B.单端省电容(capless_mode)
 *				C.差分模式(cap_diff_mode)
 ****************************************************************************
 */
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_demo.h"

#if 1
#define ADC_DEMO_LOG	printf
#else
#define ADC_DEMO_LOG(...)
#endif

extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;

#define ADC_DEMO_CH_NUM        	2	/*支持的最大采样通道(max = 2)*/
#define ADC_DEMO_BUF_NUM        2	/*采样buf数*/
#define ADC_DEMO_IRQ_POINTS     256	/*采样中断点数*/
#define ADC_DEMO_BUFS_SIZE      (ADC_DEMO_CH_NUM * ADC_DEMO_BUF_NUM * ADC_DEMO_IRQ_POINTS)

struct adc_demo {
    u8 adc_2_dac;
    u8 linein_idx;
    struct audio_adc_output_hdl adc_output;
    struct adc_linein_ch linein_ch;
    s16 adc_buf[ADC_DEMO_BUFS_SIZE];
};
static struct adc_demo *linein_demo = NULL;

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
static void adc_linein_demo_output(void *priv, s16 *data, int len)
{
    /* putchar('O'); */
    printf("> %d %d\n", data[0], data[1]);
    int wlen = 0;
    if (linein_demo && linein_demo->adc_2_dac) {
        if (linein_demo->linein_idx == (LADC_CH_LINEIN_L | LADC_CH_LINEIN_R)) {
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
            /*输出两个linein的数据到DAC两个通道*/
            putchar('a');
            wlen = audio_dac_write(&dac_hdl, data, len * 2);
#else
            /*输出其中一个linein的数据到DAC一个通道*/
            putchar('b');
            s16 *mono_data = data;
            for (int i = 0; i < (len / 2); i++) {
                mono_data[i] = data[i * 2];			/*linein_L*/
                //mono_data[i] = data[i * 2 + 1];		/*linein_R*/
            }
            wlen = audio_dac_write(&dac_hdl, mono_data, len);
#endif/*TCFG_AUDIO_DAC_CONNECT_MODE*/
        } else {
            /*输出一个linein的数据到DAC一个通道*/
            putchar('c');
            wlen = audio_dac_write(&dac_hdl, data, len);
        }
    }
}

/*
*********************************************************************
*                  AUDIO ADC linein OPEN
* Description: 打开linein通道
* Arguments  : linein_idx 		linein通道
*			   gain			linein增益
*			   sr			linein采样率
*			   linein_2_dac 	linein数据（通过DAC）监听
* Return	 : None.
* Note(s)    : (1)打开一个linein通道示例：
*				audio_adc_linein_demo_open(LADC_CH_LINEIN_L,10,16000,1);
*				或者
*				audio_adc_linein_demo_open(LADC_CH_LINEIN_R,10,16000,1);
*			   (2)打开两个linein通道示例：
*				audio_adc_linein_demo_open(LADC_CH_LINEIN_R|LADC_CH_LINEIN_L,10,16000,1);
*********************************************************************
*/
void audio_adc_linein_demo_open(u8 linein_idx, u8 gain, u16 sr, u8 linein_2_dac)
{
    ADC_DEMO_LOG("audio_adc_linein_demo open:%d,gain:%d,sr:%d,linein_2_dac:%d\n", linein_idx, gain, sr, linein_2_dac);
    linein_demo = zalloc(sizeof(struct adc_demo));
    if (linein_demo) {
        //step0:打开linein通道，并设置增益
        audio_adc_linein_open(&linein_demo->linein_ch, linein_idx, &adc_hdl);
        audio_adc_linein_set_gain(&linein_demo->linein_ch, linein_idx, gain);
        //step1:设置linein通道采样率
        audio_adc_linein_set_sample_rate(&linein_demo->linein_ch, sr);
        //step2:设置linein采样buf
        audio_adc_linein_set_buffs(&linein_demo->linein_ch, \
                                   linein_demo->adc_buf, \
                                   ADC_DEMO_IRQ_POINTS * 2, \
                                   ADC_DEMO_BUF_NUM);
        //step3:设置linein采样输出回调函数
        linein_demo->adc_output.handler = adc_linein_demo_output;
        audio_adc_linein_add_output_handler(&linein_demo->linein_ch, &linein_demo->adc_output);
        //step4:启动linein通道采样
        audio_adc_linein_start(&linein_demo->linein_ch);

        /*监听配置（可选）*/
        linein_demo->adc_2_dac = linein_2_dac;
        linein_demo->linein_idx = linein_idx;
        if (linein_2_dac) {
            ADC_DEMO_LOG("max_sys_vol:%d\n", get_max_sys_vol());
            app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
            ADC_DEMO_LOG("cur_vol:%d\n", app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
            audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
            audio_dac_set_sample_rate(&dac_hdl, sr);
            audio_dac_start(&dac_hdl);
        }
    }
    ADC_DEMO_LOG("audio_adc_linein_demo start succ\n");
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
void audio_adc_linein_demo_close(void)
{
    ADC_DEMO_LOG("audio_adc_linein_demo_close\n");
    if (linein_demo) {
        audio_adc_linein_del_output_handler(&linein_demo->linein_ch, &linein_demo->adc_output);
        audio_adc_linein_close(&linein_demo->linein_ch);
        free(linein_demo);
        linein_demo = NULL;
    }
}

#if AUDIO_DEMO_LP_REG_ENABLE
static u8 linein_demo_idle_query()
{
    return linein_demo ? 0 : 1;
}

REGISTER_LP_TARGET(linein_demo_lp_target) = {
    .name = "linein_demo",
    .is_idle = linein_demo_idle_query,
};
#endif/*AUDIO_DEMO_LP_REG_ENABLE*/
