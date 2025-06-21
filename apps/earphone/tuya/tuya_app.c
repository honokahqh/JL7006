#include "app_config.h"
#include "earphone.h"
#include "app_main.h"
#include "3th_profile_api.h"
#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "bt_tws.h"

#include "update_tws.h"
#include "update_tws_new.h"

#include "tuya_ble_app_demo.h"
#include "application/audio_eq.h"
#include "tone_player.h"
#include "user_cfg.h"
#include "key_event_deal.h"
#include "app_power_manage.h"

#if TUYA_DEMO_EN

#define TUYA_TRIPLE_LENGTH      0x3E
#define LOG_TAG             "[tuya_app]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

u8 key_table_l[KEY_NUM_MAX][KEY_EVENT_MAX] = {
    // SHORT           LONG              HOLD              UP              DOUBLE           TRIPLE
    {KEY_VOL_DOWN,   KEY_VOL_UP,  KEY_POWEROFF_HOLD,  KEY_NULL,     KEY_MUSIC_NEXT,     KEY_MUSIC_PREV},   //KEY_0
    {KEY_MUSIC_NEXT, KEY_VOL_UP,    KEY_VOL_UP,         KEY_NULL,     KEY_OPEN_SIRI,        KEY_NULL},   //KEY_1
    {KEY_MUSIC_PREV, KEY_VOL_DOWN,  KEY_VOL_DOWN,       KEY_NULL,     KEY_HID_CONTROL,      KEY_NULL},   //KEY_2
};
u8 key_table_r[KEY_NUM_MAX][KEY_EVENT_MAX] = {
    // SHORT           LONG              HOLD              UP              DOUBLE           TRIPLE
    {KEY_VOL_DOWN,   KEY_VOL_UP,  KEY_POWEROFF_HOLD,  KEY_NULL,     KEY_MUSIC_NEXT,     KEY_MUSIC_PREV},   //KEY_0
    {KEY_MUSIC_NEXT, KEY_VOL_UP,    KEY_VOL_UP,         KEY_NULL,     KEY_OPEN_SIRI,        KEY_NULL},   //KEY_1
    {KEY_MUSIC_PREV, KEY_VOL_DOWN,  KEY_VOL_DOWN,       KEY_NULL,     KEY_HID_CONTROL,      KEY_NULL},   //KEY_2
};
extern void ble_app_disconnect(void);
extern void tuya_eq_data_deal(char *eq_info_data);
extern void tuya_set_music_volume(int volume);
extern void tuya_change_bt_name(char *name, u8 name_len);
extern u8 tuya_key_event_swith(u8 event);

extern struct ble_task_param ble_task;
extern tuya_ble_parameters_settings_t tuya_ble_current_para;
static u16 find_device_timer = 0;

#define TUYA_KEY_SYNC_VM        0
#define TUYA_BT_NAME_SYNC_VM    1
#define TUYA_EQ_INFO_SYNC_VM    2

static struct VM_INFO_MODIFY {
    char eq_info[11];
    u8 key_recored[6][6];
    char bt_name[LOCAL_NAME_LEN];
} vm_info_modify;

int tuya_earphone_state_set_page_scan_enable()
{
    return 0;
}

int tuya_earphone_state_get_connect_mac_addr()
{
    return 0;
}

int tuya_earphone_state_cancel_page_scan()
{
    return 0;
}

int tuya_earphone_state_tws_init(int paired)
{
    return 0;
}

int tuya_earphone_state_tws_connected(int first_pair, u8 *comm_addr)
{
    if (first_pair) {
        extern void ble_module_enable(u8 en);
        extern void bt_update_mac_addr(u8 * addr);
        extern void lib_make_ble_address(u8 * ble_address, u8 * edr_address);
        /* bt_ble_adv_enable(0); */
        u8 tmp_ble_addr[6] = {0};
        lib_make_ble_address(tmp_ble_addr, comm_addr);
        le_controller_set_mac(tmp_ble_addr);//将ble广播地址改成公共地址
        bt_update_mac_addr(comm_addr);
        /* bt_ble_adv_enable(1); */


        /*新的连接，公共地址改变了，要重新将新的地址广播出去*/
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("\nNew Connect Master!!!\n\n");
            ble_app_disconnect();
            tuya_set_adv_enable();
            void master_send_triple_info_to_slave();
            master_send_triple_info_to_slave();
        } else {
            printf("\nFirst Connect Slave!!!\n\n");
            /*从机ble关掉*/
            ble_app_disconnect();
            tuya_set_adv_disable();
        }
    }

    return 0;
}

int tuya_earphone_state_enter_soft_poweroff()
{
    extern void bt_ble_exit(void);
    bt_ble_exit();
    return 0;
}

int tuya_adv_bt_status_event_handler(struct bt_event *bt)
{
    return 0;
}


int tuya_adv_hci_event_handler(struct bt_event *bt)
{

    return 0;
}
enum {
    TRIPLE_NULL = 0,
    TRIPLE_FLASH = 1,
    TRIPLE_VM = TUYA_TRIPLE_LENGTH,
};
extern u16 get_triple_data(u8 *data);
extern int get_triple_info_result();
void bt_tws_tuya_triple_sync(u8 *data, u16 len, bool rx)
{
    //先是主机调用sibling 从机调用该函数 通过判断返回的三元组情况，是否给主机sibling
    u8 info_type = 3;
    u8 change_data[62];
    printf("slave rx: %d\n", rx);
    if (rx) {
        u8 slave_data[60];
        u16 slave_data_len;
        slave_data_len = get_triple_data(slave_data);
        info_type = data[0];

        printf("slave recv master triple data :[%d],info_type :[%d]\n", len, info_type);
        put_buf(data, len);
        int ret = get_triple_info_result();//获取从机的三元组情况
        put_buf(slave_data, slave_data_len);
        printf("slave self triple data:[%d],info_type:%d\n", slave_data_len, ret);
        switch (info_type) {
        case TRIPLE_NULL:
            if (ret == TRIPLE_FLASH) {
                change_data[0] = 1;
                memcpy(&change_data[1], slave_data, slave_data_len);
                tws_api_send_data_to_sibling(change_data, slave_data_len, TWS_FUNC_ID_TUYA_TRIPLE);
            }
            printf("slave triple data 0");
            break;
        case TRIPLE_FLASH:
            if (ret != TRIPLE_FLASH) {
                printf("slave write triple data 1");
                set_triple_info(&data[1]);

            }
            break;
        case TRIPLE_VM:
            printf("slave triple data 62");
            break;

        }
    }
}

void master_send_triple_info_to_slave()
{
    u8 data[62]  = {0};
    u16 data_len;
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        int ret = get_triple_info_result();
        printf("master read from [%d]\n", ret);
        data[0] = ret;
        data_len = get_triple_data(&data[1]);
        printf("master send triple data");
        put_buf(data, data_len);
        tws_api_send_data_to_sibling(data, data_len, TWS_FUNC_ID_TUYA_TRIPLE);
    }


}

void tuya_bt_tws_event_handler(struct bt_event *bt)
{
    int role = bt->args[0];
    int phone_link_connection = bt->args[1];
    int reason = bt->args[2];

    switch (bt->event) {
    case TWS_EVENT_CONNECTED:
        //bt_ble_adv_enable(1);
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            //master enable
            printf("\nTws Connect Slave!!!\n\n");
            /*从机ble关掉*/
            ble_app_disconnect();
            tuya_set_adv_disable();
        } else {
            printf("master send\n");
            master_send_triple_info_to_slave();
        }
        tuya_tws_bind_info_sync();

        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        /*
         * 跟手机的链路LMP层已完全断开, 只有tws在连接状态才会收到此事件
         */
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        /*
         * TWS连接断开
         */
        if (app_var.goto_poweroff_flag) {
            break;
        }
        if (get_app_connect_type() == 0) {
            printf("\ntws detach to open ble~~~\n\n");

            tuya_set_adv_enable();
        }
        set_ble_connect_type(TYPE_NULL);

        break;
    case TWS_EVENT_SYNC_FUN_CMD:
        break;
    case TWS_EVENT_ROLE_SWITCH:
        printf("tuya_role_switch");
        tuya_ble_adv_change();
        break;
    }

#if OTA_TWS_SAME_TIME_ENABLE
    tws_ota_app_event_deal(bt->event);
#endif
}

int tuya_sys_event_handler_specific(struct sys_event *event)
{
    switch (event->type) {
    case SYS_BT_EVENT:
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {

        } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {

        }
#if TCFG_USER_TWS_ENABLE
        else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
            tuya_bt_tws_event_handler(&event->u.bt);
        }
#endif
#if OTA_TWS_SAME_TIME_ENABLE
        else if (((u32)event->arg == SYS_BT_OTA_EVENT_TYPE_STATUS)) {
            bt_ota_event_handler(&event->u.bt);
        }
#endif
        break;
    case SYS_DEVICE_EVENT:
        break;
    }

    return 0;
}

int tuya_earphone_state_init()
{
    /* transport_spp_init(); */
    /* spp_data_deal_handle_register(user_spp_data_handler); */

    return 0;
}

_WEAK_ void tuya_state_deal(void *_data, u16 len)
{

}

static void find_device()
{
    tone_play_index(IDEX_TONE_NORMAL, 1);
}
void _tuya_tone_deal(u8 index)
{
    tone_play_index(index, 1);
}
void custom_earphone_key_init()
{
    u8 key_value_record[6][6] = {0};
    u8 value = syscfg_read(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
    eq_mode_set(EQ_MODE_NORMAL);
    if (value == sizeof(key_value_record)) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                if (((key_value_record[i][j] > KEY_MUSIC_PP - 1) && (key_value_record[i][j] < KEY_OPEN_SIRI + 1)) || \
                    ((key_value_record[i + 3][j] > KEY_MUSIC_PP - 1) && (key_value_record[i + 3][j] < KEY_OPEN_SIRI + 1))) {
                    key_table_l[i][j] = key_value_record[i][j];
                    key_table_r[i][j] = key_value_record[i + 3][j];
                }
            }
        }
    }
    printf("%d %d %d %d %d %d\n", key_table_l[0][0], key_table_l[0][1], key_table_l[0][4], key_table_r[0][0], key_table_r[0][1], key_table_r[0][4]);
}
void custom_earphone_key_remap(struct sys_event *event, struct key_event *key, u8 *key_event)
{
    extern void set_key_event_by_tuya_info(struct sys_event * event, struct key_event * key, u8 * key_event);
    set_key_event_by_tuya_info(event, key, key_event);
    if (key->type == TUYA_MUSIC_PP) {
        *key_event = KEY_MUSIC_PP;
    } else if (key->type == TUYA_MUSIC_NEXT) {
        *key_event = KEY_MUSIC_NEXT;
    } else if (key->type == TUYA_MUSIC_PREV) {
        *key_event = KEY_MUSIC_PREV;
    }
}
void battery_indicate(u8 cur_battery_level)
{
#if 0
    extern void tuya_battery_indicate(u8 left, u8 right, u8 chargebox);
    u8 chargestore = chargestore_get_power_level();
    u8 data = 2;
    if (get_bt_tws_connect_status()) {
        /* chargestore_get_tws_remote_info(&data); */
        if (tws_api_get_local_channel() == 'L') {
            tuya_battery_indicate(cur_battery_level * 10, data * 10, chargestore * 10);
        } else {
            tuya_battery_indicate(data * 10, cur_battery_level * 10, chargestore * 10);
        }
    } else {
        tuya_battery_indicate(cur_battery_level * 10, 50, chargestore * 10);
    }
#endif
}
void volume_indicate(s8 volume)
{
    u8 max_vol = app_audio_get_max_volume();
    printf("cur_vol is:%d, max:%d\n", volume, app_audio_get_max_volume());
    extern void tuya_valume_indicate(u8 valume);
    u8 tuya_sync_valume = (int)(volume * 100 / max_vol);
    tuya_valume_indicate(tuya_sync_valume);
}
static void tuya_tone_deal(u8 index)
{
    int argv[3];
    argv[0] = (int)_tuya_tone_deal;
    argv[1] = 1;
    argv[2] = index;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
}

static void _tuya_vm_info_modify(u8 mode)
{
    u8 value = 0;
    printf("coming in app_core sync tuya_info! mode:%d\n", mode);
    switch (mode) {
    case TUYA_KEY_SYNC_VM:
        printf("write key_info to vm!\n");
        put_buf(vm_info_modify.key_recored, 36);
        value = syscfg_write(TUYA_SYNC_KEY_INFO, vm_info_modify.key_recored, sizeof(vm_info_modify.key_recored));
        printf("value:%d\n", value);
        break;
    case TUYA_BT_NAME_SYNC_VM:
        printf("write bt_name to vm!:%s\n", vm_info_modify.bt_name);
        value = syscfg_write(CFG_BT_NAME, vm_info_modify.bt_name, LOCAL_NAME_LEN);
        printf("value:%d\n", value);
        break;
    case TUYA_EQ_INFO_SYNC_VM:
        printf("write eq_info to vm!\n");
        put_buf(vm_info_modify.eq_info, 11);
        value = syscfg_write(CFG_RCSP_ADV_EQ_DATA_SETTING, vm_info_modify.eq_info, 11);
        printf("value:%d\n", value);
        break;
    default:
        printf("key bt_name modify mode err!\n");
        break;
    }
}
static void tuya_vm_info_modify(char *info, u8 mode, u8 len)
{
    int argv[3];
    switch (mode) {
    case TUYA_KEY_SYNC_VM:
        memcpy(vm_info_modify.key_recored, info, len);
        break;
    case TUYA_BT_NAME_SYNC_VM:
        memcpy(vm_info_modify.bt_name, info, len);
        break;
    case TUYA_EQ_INFO_SYNC_VM:
        memcpy(vm_info_modify.eq_info, info, len);
        break;
    default:
        printf("key bt_name modify mode err!\n");
        break;
    }
    argv[0] = (int)_tuya_vm_info_modify;
    argv[1] = 1;
    argv[2] = mode;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
}
static void bt_tws_tuya_sync_info_received(void *_data, u16 len, bool rx)
{
    if (len < sizeof(struct TUYA_SYNC_INFO)) {
        printf("tws receive len error!");
        return;
    }
    if (rx) {
        struct TUYA_SYNC_INFO *tuya_sync_info = (struct TUYA_SYNC_INFO *)_data;
        put_buf(tuya_sync_info, sizeof(tuya_sync_info));
        printf("this is tuya received!\n");
        /* for (int i = 0; i < 10; i++) { */
        /*     printf("%d ", tuya_sync_info->eq_info[i]); */
        /* } */
        if (tuya_sync_info->tuya_eq_flag == 1) {
            printf("set remote eq_info!\n");
            char temp_eq_buf[10] = {0};
            if (!memcmp(temp_eq_buf, tuya_sync_info->eq_info, 10)) {
                eq_mode_set(EQ_MODE_NORMAL);
            } else {
                tuya_eq_data_deal(tuya_sync_info->eq_info);
            }
            tuya_vm_info_modify(tuya_sync_info->eq_info, TUYA_EQ_INFO_SYNC_VM, 11);
        }
        if ((tuya_sync_info->volume < 100 && tuya_sync_info->volume > 0) || tuya_sync_info->volume_flag == 1) {
            printf("set remote valume!\n");
            tuya_set_music_volume(tuya_sync_info->volume);
        }
        if (tuya_sync_info->key_change_flag == 1 || tuya_sync_info->key_reset == 1) {
            if (tuya_sync_info->key_change_flag == 1) {
                if (((tuya_sync_info->key_l1 > 8) && (tuya_sync_info->key_l1 < 21)) || \
                    ((tuya_sync_info->key_l2 > 8) && (tuya_sync_info->key_l2 < 21)) || \
                    ((tuya_sync_info->key_l3 > 8) && (tuya_sync_info->key_l3 < 21)) || \
                    ((tuya_sync_info->key_r1 > 8) && (tuya_sync_info->key_r1 < 21)) || \
                    ((tuya_sync_info->key_r2 > 8) && (tuya_sync_info->key_r2 < 21)) || \
                    ((tuya_sync_info->key_r3 > 8) && (tuya_sync_info->key_r3 < 21))) {
                    key_table_l[0][0] = tuya_sync_info->key_l1;
                    key_table_l[0][1] = tuya_sync_info->key_l2;
                    key_table_l[0][4] = tuya_sync_info->key_l3;
                    key_table_r[0][0] = tuya_sync_info->key_r1;
                    key_table_r[0][1] = tuya_sync_info->key_r2;
                    key_table_r[0][4] = tuya_sync_info->key_r3;
                }
            }
            if (tuya_sync_info->key_reset == 1) {
                key_table_l[0][0] = tuya_key_event_swith(0);
                key_table_l[0][1] = tuya_key_event_swith(1);
                key_table_l[0][4] = tuya_key_event_swith(2);
                key_table_r[0][0] = tuya_key_event_swith(0);
                key_table_r[0][1] = tuya_key_event_swith(1);
                key_table_r[0][4] = tuya_key_event_swith(2);
            }
            printf("%d %d %d %d %d %d\n", key_table_l[0][0], key_table_l[0][1], key_table_l[0][4], key_table_r[0][0], key_table_r[0][1], key_table_r[0][4]);
            u8 key_value_record[6][6] = {0};
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 6; j++) {
                    key_value_record[i][j] = key_table_l[i][j];
                    key_value_record[i + 3][j] = key_table_r[i][j];
                }
            }
            put_buf(key_value_record, 36);
            tuya_vm_info_modify(key_value_record, TUYA_KEY_SYNC_VM, sizeof(key_value_record));
        }
        if (tuya_sync_info->find_device == 1) {
            find_device_timer = sys_timer_add(NULL, find_device, 1000);
        } else if (tuya_sync_info->find_device == 0) {
            sys_timer_del(find_device_timer);
        }
        if (tuya_sync_info->device_conn_flag == 1) {
            tuya_tone_deal(IDEX_TONE_BT_CONN);
        }
        if (tuya_sync_info->phone_conn_flag == 1) {
            tuya_tone_deal(IDEX_TONE_BT_CONN);
        }
        if (tuya_sync_info->device_disconn_flag == 1) {
            tuya_tone_deal(IDEX_TONE_BT_DISCONN);
        }
        if (tuya_sync_info->phone_disconn_flag == 1) {
            tuya_tone_deal(IDEX_TONE_BT_DISCONN);
        }
        if (tuya_sync_info->tuya_bt_name_flag == 1) {
            printf("sync bt_name:%s\n", tuya_sync_info->bt_name);
            tuya_change_bt_name(tuya_sync_info->bt_name, LOCAL_NAME_LEN);
            tuya_vm_info_modify(tuya_sync_info->bt_name, TUYA_BT_NAME_SYNC_VM, LOCAL_NAME_LEN);
        }
    }
}


static void bt_tws_tuya_auth_info_received(void *data, u16 len, bool rx)
{
    log_info("bt_tws_tuya_auth_info_received, rx:%d", rx);
    if (rx) {
        tuya_tws_sync_info_t *recv_info = data;
        memcpy(tuya_ble_current_para.sys_settings.login_key, recv_info->login_key, LOGIN_KEY_LEN);
        memcpy(tuya_ble_current_para.sys_settings.device_virtual_id, recv_info->device_virtual_id, DEVICE_VIRTUAL_ID_LEN);
        tuya_ble_current_para.sys_settings.bound_flag = recv_info->bound_flag;
        os_taskq_post_msg(ble_task.ble_task_name, 1, BLE_TASK_PAIR_INFO_SYNC_SYS);
    }
}


REGISTER_TWS_FUNC_STUB(app_tuya_state_stub) = {
    .func_id = TWS_FUNC_ID_TUYA_STATE,
    .func    = bt_tws_tuya_sync_info_received,
};

REGISTER_TWS_FUNC_STUB(app_tuya_auth_stub) = {
    .func_id = TWS_FUNC_ID_TUYA_AUTH_SYNC,
    .func    = bt_tws_tuya_auth_info_received,
};
REGISTER_TWS_FUNC_STUB(app_tuya_triple_state) = {
    .func_id = TWS_FUNC_ID_TUYA_TRIPLE,
    .func    = bt_tws_tuya_triple_sync,
};

_WEAK_ u16 tuya_get_core_data(u8 *data)
{
    return 0;
}

extern u16 tuya_get_core_data(u8 *data);
void bt_tws_sync_tuya_state()
{
    u8 data[20];
    u16 data_len;

    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        data_len = tuya_get_core_data(data);

        //r_printf("master ll sync data");
        put_buf(&data, data_len);


        tws_api_send_data_to_sibling(data, data_len, TWS_FUNC_ID_TUYA_STATE);
    }
}

#endif
