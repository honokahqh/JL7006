#ifndef _APP_ANCBOX_H_
#define _APP_ANCBOX_H_

#include "typedef.h"
#include "system/event.h"

extern int app_ancbox_event_handler(struct ancbox_event *anc_dev);
extern void app_ancbox_module_deal(u8 *buf, u8 len);
extern u8 ancbox_get_status(void);
extern void ancbox_clear_status(void);

#endif    //_APP_CHARGESTORE_H_

