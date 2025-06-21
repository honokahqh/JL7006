/*
 ****************************************************************************
 *							Audio ADC Demo
 *
 *Description	: Audio ADC使用范例
 *Notes			: (1)本demo为开发测试范例，请不要修改该demo， 如有需求，请自行
 *				  复制再修改
 *				  (2)mic工作模式说明
 *				A.单端隔直(cap_mode)
 *				  可以选择mic供电方式：外部供电还是内部供电(mic_bias_inside = 1)
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

#define ADC_DEMO_CH_NUM        	1	/*支持的最大采样通道(max = 2)*/
#define ADC_DEMO_BUF_NUM        2	/*采样buf数*/
#define ADC_DEMO_IRQ_POINTS     256	/*采样中断点数*/
#define ADC_DEMO_BUFS_SIZE      (ADC_DEMO_CH_NUM * ADC_DEMO_BUF_NUM * ADC_DEMO_IRQ_POINTS)

struct adc_demo {
    u8 adc_2_dac;
    u8 mic_idx;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[ADC_DEMO_BUFS_SIZE];
};
static struct adc_demo *mic_demo = NULL;

/*
*********************************************************************
*                  AUDIO MIC OUTPUT
* Description: mic采样数据回调
* Arguments  : pric 私有参数
*			   data	mic数据
*			   len	数据长度
* Return	 : 0 成功 其他 失败
* Note(s)    : 单声道数据格式
*			   L0 L1 L2 L3 L4 ...
*				双声道数据格式()
*			   L0 R0 L1 R1 L2 R2...
*********************************************************************
*/
static void adc_mic_demo_output(void *priv, s16 *data, int len)
{
    putchar('O');
    int wlen = 0;
    if (mic_demo && mic_demo->adc_2_dac) {
        if (mic_demo->mic_idx == (LADC_CH_MIC_L | LADC_CH_MIC_R)) {
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
            /*输出两个mic的数据到DAC两个通道*/
            putchar('a');
            wlen = audio_dac_write(&dac_hdl, data, len * 2);
#else
            /*输出其中一个mic的数据到DAC一个通道*/
            putchar('b');
            s16 *mono_data = data;
            for (int i = 0; i < (len / 2); i++) {
                mono_data[i] = data[i * 2];			/*MIC_L*/
                //mono_data[i] = data[i * 2 + 1];		/*MIC_R*/
            }
            wlen = audio_dac_write(&dac_hdl, mono_data, len);
#endif/*TCFG_AUDIO_DAC_CONNECT_MODE*/
        } else {
            /*输出一个mic的数据到DAC一个通道*/
            putchar('c');
            wlen = audio_dac_write(&dac_hdl, data, len);
        }
    }
}

/*
*********************************************************************
*                  AUDIO ADC MIC OPEN
* Description: 打开mic通道
* Arguments  : mic_idx 		mic通道
*			   gain			mic增益
*			   sr			mic采样率
*			   mic_2_dac 	mic数据（通过DAC）监听
* Return	 : None.
* Note(s)    : (1)打开一个mic通道示例：
*				audio_adc_mic_demo_open(LADC_CH_MIC_L,10,16000,1);
*				或者
*				audio_adc_mic_demo_open(LADC_CH_MIC_R,10,16000,1);
*			   (2)打开两个mic通道示例：
*				audio_adc_mic_demo_open(LADC_CH_MIC_R|LADC_CH_MIC_L,10,16000,1);
*********************************************************************
*/
void audio_adc_mic_demo_open(u8 mic_idx, u8 gain, u16 sr, u8 mic_2_dac)
{
    ADC_DEMO_LOG("audio_adc_mic_demo open:%d,gain:%d,sr:%d,mic_2_dac:%d\n", mic_idx, gain, sr, mic_2_dac);
    mic_demo = zalloc(sizeof(struct adc_demo));
    if (mic_demo) {
        //step0:打开mic通道，并设置增益
        if (mic_idx & LADC_CH_MIC_L) {
            audio_adc_mic_open(&mic_demo->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
            audio_adc_mic_set_gain(&mic_demo->mic_ch, gain);
        }
        if (mic_idx & LADC_CH_MIC_R) {
            audio_adc_mic1_open(&mic_demo->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
            audio_adc_mic1_set_gain(&mic_demo->mic_ch, gain);
        }
        //step1:设置mic通道采样率
        audio_adc_mic_set_sample_rate(&mic_demo->mic_ch, sr);
        //step2:设置mic采样buf
        audio_adc_mic_set_buffs(&mic_demo->mic_ch, mic_demo->adc_buf, ADC_DEMO_IRQ_POINTS * 2, ADC_DEMO_BUF_NUM);
        //step3:设置mic采样输出回调函数
        mic_demo->adc_output.handler = adc_mic_demo_output;
        audio_adc_add_output_handler(&adc_hdl, &mic_demo->adc_output);
        //step4:启动mic通道采样
        audio_adc_mic_start(&mic_demo->mic_ch);

        /*监听配置（可选）*/
        mic_demo->adc_2_dac = mic_2_dac;
        mic_demo->mic_idx = mic_idx;
        if (mic_2_dac) {
            ADC_DEMO_LOG("max_sys_vol:%d\n", get_max_sys_vol());
            app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
            ADC_DEMO_LOG("cur_vol:%d\n", app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
            audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
            audio_dac_set_sample_rate(&dac_hdl, sr);
            audio_dac_start(&dac_hdl);
        }
    }
    ADC_DEMO_LOG("audio_adc_mic_demo start succ\n");
}

/*
*********************************************************************
*                  AUDIO ADC MIC CLOSE
* Description: 关闭mic采样模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_mic_demo_close(void)
{
    ADC_DEMO_LOG("audio_adc_mic_demo_close\n");
    if (mic_demo) {
        audio_adc_del_output_handler(&adc_hdl, &mic_demo->adc_output);
        audio_adc_mic_close(&mic_demo->mic_ch);
        free(mic_demo);
        mic_demo = NULL;
    }
}

#if AUDIO_DEMO_LP_REG_ENABLE
static u8 adc_demo_idle_query()
{
    return mic_demo ? 0 : 1;
}

REGISTER_LP_TARGET(adc_demo_lp_target) = {
    .name = "adc_demo",
    .is_idle = adc_demo_idle_query,
};
#endif/*AUDIO_DEMO_LP_REG_ENABLE*/
