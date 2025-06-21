#ifndef _APP_CHARGE_CALIBRATION_H_
#define _APP_CHARGE_CALIBRATION_H_

#include "typedef.h"
#include "system/event.h"

extern int app_charge_calibration_event_handler(struct charge_calibration_event *dev);
extern int app_charge_calibration_data_handler(u8 *buf, u8 len);

#endif    //_APP_CHARGE_CALIBRATION_H_

