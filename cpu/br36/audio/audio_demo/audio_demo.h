/*
 ****************************************************************************
 *							Audio Demo Declaration
 *
 *Description	: Audio Demo函数声明集合
 *Notes			: None.
 ****************************************************************************
 */
#ifndef __AUDIO_DEMO_H_
#define __AUDIO_DEMO_H_

#include "generic/typedef.h"

/*audio demo低功耗注册查询，防止调用demo的时候，进入低功耗，影响测试*/
#define AUDIO_DEMO_LP_REG_ENABLE	0

/*dac_io声道选择*/
#define DAC_IO_L        0x1
#define DAC_IO_R        0x2
#define DAC_IO_LR       0x3

/**********************************************************************
*
*					Audio DAC Demo APIs
*
**********************************************************************/




/**********************************************************************
*
*					Audio ADC Demo APIs
*
**********************************************************************/

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
void audio_adc_mic_demo_open(u8 mic_idx, u8 gain, u16 sr, u8 mic_2_dac);

/*
*********************************************************************
*                  AUDIO ADC MIC CLOSE
* Description: 关闭mic采样模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_mic_demo_close(void);


/**********************************************************************
*
*					Audio AudioLink(IIS) APIs
*
**********************************************************************/
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
void audio_link_demo_open(u8 iis_rx, u8 iis_tx, u16 sr, u8 iis_rx_2_dac);

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
void audio_link_demo_close(u8 iis_rx, u8 iis_tx);


/**********************************************************************
*
*					Audio PDM APIs
*
**********************************************************************/

/*
*********************************************************************
*                  AUDIO PDM MIC OPEN
* Description: 打开pdm mic模块
* Arguments  : sample 		pdm mic采样率
*			   mic_2_dac 	mic数据（通过DAC）监听
* Return	 : None.
* Note(s)    : (1)打开pdm mic通道示例：
*				audio_plnk_demo_open(16000, 1);
*********************************************************************
*/
void audio_plnk_demo_open(u16 sample, u8 mic_2_dac);

/*
*********************************************************************
*                  AUDIO PDM MIC CLOSE
* Description: 关闭pdm mic模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_plnk_demo_close(void);

/*
*********************************************************************
*                  hw_fft_demo_real
* Description: 实数数据 FFT 运算 demo
* Arguments  : None.
* Return     : None.
* Note(s)    : None.
*********************************************************************
*/
extern void hw_fft_demo_real();

/*
*********************************************************************
*                  hw_fft_demo_complex
* Description: 复数数据 FFT 运算 demo
* Arguments  : None.
* Return     : None.
* Note(s)    : None.
*********************************************************************
*/
extern void hw_fft_demo_complex();

/*
*********************************************************************
*                  audio_dac_io_open
* Description: 打开dac_io模块
* Arguments  : None.
* Return     : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_dac_io_open(void);

/*
*********************************************************************
*                  audio_dac_io_set
* Description: 设置dac_io的输出以及峰峰值
* Arguments  : ch          通道设置 (DAC_IO_L / DAC_IO_R / DAC_IO_LR)
* Return     : value       io输出   (1, 0)
* Note(s)    : None.
*********************************************************************
*/
void audio_dac_io_set(u8 ch, u8 value);

/*
*********************************************************************
*                  audio_dac_io_test
* Description: dac_io使用示例
* Arguments  : None.
* Return     : None.
* Note(s)    : 在app_main()中，audio_dec_init()之后调用即可测试该模块
*********************************************************************
*/
void audio_dac_io_test(void);

#endif/*__AUDIO_DEMO_H_*/
