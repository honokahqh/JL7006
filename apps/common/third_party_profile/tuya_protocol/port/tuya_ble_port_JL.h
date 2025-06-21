#ifndef TUYA_BLE_PORT_JL_H_
#define TUYA_BLE_PORT_JL_H_
#include "os/os_type.h"

struct ble_task_param {
    char *ble_task_name;
    OS_SEM ble_sem;
};

enum {
    BLE_TASK_MSG_MSG_COMES,
    BLE_TASK_MSG_INFO_SYNC_AUTH,
    BLE_TASK_MSG_INFO_SYNC_SYS,
    BLE_TASK_PAIR_INFO_SYNC_SYS,
};


#endif //
