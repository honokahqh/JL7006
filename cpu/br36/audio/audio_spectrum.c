#include "audio_spectrum.h"
/*
 * 频谱计算的32个频点 单位Hz(分别对应)
48k:				    44.1k				    32k				        16k:		    //采样率
123.686951				121.196915				111.772774				91.4053421		//频点0
247.373901				242.393829				223.545547				182.810684		//频点1
371.060852				363.590729				335.318298				274.216003		//频点....
494.747803				484.787659				447.091095				365.621368
618.434753				605.984619				558.863892				457.026733
742.121704				727.181458				670.636597				548.432007
865.808655				848.378418				782.409363				639.837341
989.495605				969.575317				894.18219				731.242737
1123.8075				1098.13269				1006.1601				822.648071
1276.69922				1244.33203				1129.08752				914.053467
1450.39148				1409.99524				1267.03369				1005.64532
1647.71411				1597.71399				1421.83325				1105.05334
1871.8822				1810.4248				1595.54553				1214.28784
2126.54761				2051.45435				1790.48108				1334.32031
2415.85986				2324.57349				2009.23303				1466.2179
2744.53247				2634.0542				2254.71069				1611.15356
3117.92041				2984.7373				2530.17969				1770.41614
3542.10693				3382.10791				2839.30444				1945.42188
4024.00317				3832.3833				3186.1958				2137.72681
4571.46045				4342.60547				3575.46948				2349.04126
5193.39795				4920.75439				4012.30127				2581.24365
5899.94922				5575.87598				4502.50439				2836.39966
6702.62451				6318.21729				5052.59717				3116.77808
7614.50195				7159.3877				5669.89648				3424.87158
8650.43945				8112.54932				6362.61621				3763.42041
9827.3125				9192.6084				7139.9668				4135.43457
11164.2969				10416.4609				8012.29297				4544.22266
12683.1748				11803.248				8991.19238				4993.41895
14408.6953				13374.666				10089.6924				5487.01855
16368.9668				15155.2939				11322.3975				6029.41016
18595.9316				17172.9805				12705.7109				6625.41797
21125.8672				19459.3008				14258.0303				7280.34033
*/




#if AUDIO_SPECTRUM_CONFIG

extern struct audio_dac_hdl dac_hdl;
spectrum_fft_hdl *spec_hdl;
/*----------------------------------------------------------------------------*/
/**@brief   频响输出例子
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_get_demo(void *p)
{
    spectrum_fft_hdl *hdl = p;
    if (hdl) {
        u8 db_num = audio_spectrum_fft_get_num(hdl);//获取频谱个数
        short *db_data = audio_spectrum_fft_get_val(hdl);//获取存储频谱值得地址
        if (!db_data) {
            return;
        }
        for (int i = 0; i < db_num; i++) {
            //输出db_num个 db值
            printf("db_data db[%d] %d\n", i, db_data[i]);
        }
    }
}
static u16 timer_test;
/*----------------------------------------------------------------------------*/
/**@brief   打开频响统计
   @param   sr:采样率
   @return  hdl:句柄
   @note
*/
/*----------------------------------------------------------------------------*/
spectrum_fft_hdl *spectrum_open_demo(u32 sr)
{
    u8 ch_num;
#if TCFG_APP_FM_EMITTER_EN
    ch_num = 2;
#else
    u8 dac_connect_mode = audio_dac_get_channel(&dac_hdl);
    if (dac_connect_mode == DAC_OUTPUT_LR) {
        ch_num =  2;
    } else {
        ch_num =  1;
    }
#endif//TCFG_APP_FM_EMITTER_EN

    u8 channel = ch_num;

    spectrum_fft_hdl *hdl = NULL;
    spectrum_fft_open_parm parm = {0};
    parm.sr = sr;
    parm.channel = channel;
    parm.attackFactor = 0.9;
    parm.releaseFactor = 0.9;
    parm.mode = 2;
    hdl = audio_spectrum_fft_open(&parm);

    /* timer_test = sys_timer_add(hdl, spectrum_get_demo, 500);//频谱值获取测试 */

    /* clock_add(SPECTRUM_CLK); */
    return hdl;
}
/*
 *频谱计算处理
 * */
void spectrum_run_demo(spectrum_fft_hdl *hdl, s16 *data, int len)
{
    audio_spectrum_fft_run(hdl, data, len);
}
/*----------------------------------------------------------------------------*/
/**@brief   关闭频响统计
   @param  hdl:句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_close_demo(spectrum_fft_hdl *hdl)
{
    if (timer_test) {
        sys_timer_del(timer_test);
        timer_test = 0;
    }
    audio_spectrum_fft_close(hdl);
    /* clock_remove(SPECTRUM_CLK); */
}


/*----------------------------------------------------------------------------*/
/**@brief  切换频响计算
   @param  en:0 不做频响计算， 1 使能频响计算（通话模式，需关闭频响计算）
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void spectrum_switch_demo(u8 en)
{
    if (spec_hdl) {
        audio_spectrum_fft_switch(spec_hdl, en);
    }
}


#else
void spectrum_switch_demo(u8 en)
{
}
#endif
