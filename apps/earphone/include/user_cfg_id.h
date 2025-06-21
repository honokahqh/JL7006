#ifndef _USER_CFG_ID_H_
#define _USER_CFG_ID_H_

//=================================================================================//
//                            与APP CASE相关配置项[1 ~ 60]                         //
//=================================================================================//

#define 	CFG_EARTCH_ENABLE_ID			 1
#define 	CFG_PBG_MODE_INFO				 2

#define		CFG_MIC_ARRAY_DIFF_CMP_VALUE     3//麦克风补偿值
#define		CFG_MIC_ARRAY_TRIM_EN		     4//麦克风补偿使能

#define     TUYA_SYNC_KEY_INFO              20
#define     CFG_USER_TUYA_INFO_AUTH         21
#define     CFG_USER_TUYA_INFO_AUTH_BK      22
#define     CFG_USER_TUYA_INFO_SYS          23
#define     CFG_USER_TUYA_INFO_SYS_BK       24

#define     VM_LP_TOUCH_KEY0_ALOG_CFG       32
#define     VM_LP_TOUCH_KEY1_ALOG_CFG       33
#define     VM_LP_TOUCH_KEY2_ALOG_CFG       34
#define     VM_LP_TOUCH_KEY3_ALOG_CFG       35
#define     VM_LP_TOUCH_KEY4_ALOG_CFG       36


#define     CFG_SPK_EQ_SEG_SAVE             37
#define     CFG_SPK_EQ_GLOBAL_GAIN_SAVE     38

#define 	CFG_BCS_MAP_WEIGHT				 39
#define     CFG_RCSP_ADV_HIGH_LOW_VOL        40
#define     CFG_RCSP_ADV_EQ_MODE_SETTING     41
#define     CFG_RCSP_ADV_EQ_DATA_SETTING     42
#define     ADV_SEQ_RAND                     43
#define     CFG_RCSP_ADV_TIME_STAMP          44
#define     CFG_RCSP_ADV_WORK_SETTING        45
#define     CFG_RCSP_ADV_MIC_SETTING         46
#define     CFG_RCSP_ADV_LED_SETTING         47
#define     CFG_RCSP_ADV_KEY_SETTING         48

#define 	CFG_USB_BREAKPOINT0		    49
#define 	CFG_SD0_BREAKPOINT0		    50
#define 	CFG_SD1_BREAKPOINT0		    51

#define 	CFG_HAVE_MASS_STORAGE       52
#define     CFG_MUSIC_MODE              53

#define		LP_KEY_EARTCH_TRIM_VALUE	54

#define     CFG_RCSP_ADV_ANC_VOICE      55
#define     CFG_RCSP_ADV_ANC_VOICE_MODE 56
#define     CFG_RCSP_ADV_ANC_VOICE_KEY  57

#define     CFG_VOLUME_ENHANCEMENT_MODE        58
#define     CFG_DHA_FITTING_ID          59//辅听验配保存参数的id
#define     CFG_DMS_MALFUNC_STATE_ID          60//dms故障麦克风检测默认使用哪个mic的参数id


#endif /* #ifndef _USER_CFG_ID_H_ */
