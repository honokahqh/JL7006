#ifndef _CHARGE_CALIBRATION_H_
#define _CHARGE_CALIBRATION_H_

#include "typedef.h"

//电流校准的返回值
#define CALIBRATION_SUCC        0   //正常
#define CALIBRATION_VBG_ERR     1   //VBG档位不够导致错误
#define CALIBRATION_VBAT_ERR    2   //VBAT满电档位不够导致错误
#define CALIBRATION_MA_ERR      3   //电流设置太小,无法调整档位了
#define CALIBRATION_FAIL        4   //调整不到设置范围
#define CALIBRATION_CONTINUE    0xFF

typedef struct _calibration_result_ {
    u8 vbg_lev;
    u8 curr_lev;
    u8 vbat_lev;
    u8 hv_mode;
} calibration_result;

int charge_voltage_calc(int now_value, int step_value, int limit_max, int limit_min);
calibration_result charge_calibration_get_result(void);
void charge_calibration_set_current_limit(u32 max_current, u32 min_current);
u8 charge_calibration_report_current(u32 current);

//设置各个电压调整的最大档位及每次变化的档位,不需要改动
#define VBG_LEV_MAX     150 //@800mV
#define VBG_LEV_DIFF    10

#define VDDIO_LEV_MAX   70  //@3.0V
#define VDDIO_LEV_DIFF  2

#define VDC13_LEV_MAX   150 //@1.25V
#define VDC13_LEV_DIFF  5

#define EVDD_LEV_MAX    30  //@1.0V
#define EVDD_LEV_DIFF   4

#define RVDD_LEV_MAX    150 //@1.11V
#define RVDD_LEV_DIFF   7

#define DVDD_LEV_MAX    150 //@1.11V
#define DVDD_LEV_DIFF   7

#endif    //_CHARGE_CALIBRATION_H_
