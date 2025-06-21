#ifndef _LINEIN_IIS_H_
#define _LINEIN_IIS_H_


/*
*********************************************************************
*                  IIS Master Open
* Description: 打开一个iis 主机接收 从机发过来的ADC数据
* Arguments  : None
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/

void  audio_adc_iis_master_open(void);


/*
*********************************************************************
*                  IIS Salve Open
* Description: 打开一个iis 从机发送ADC数据给 主机
* Arguments  : None
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/

void  audio_adc_iis_slave_open(void);

/*
*********************************************************************
*                  IIS Master Close
* Description: 关闭iis 主机
* Arguments  : None
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/

void  audio_adc_iis_master_close(void);

/*
*********************************************************************
*                  IIS Slave Close
* Description: 关闭iis 从机
* Arguments  : None
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/

void  audio_adc_iis_slave_close(void);

#endif/*_AUD_HEARING_AID_LOW_POWER_H_*/
