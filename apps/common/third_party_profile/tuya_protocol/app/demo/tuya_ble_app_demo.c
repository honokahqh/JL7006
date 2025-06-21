#include "tuya_ble_app_demo.h"
#include "tuya_ota.h"
#include "system/generic/printf.h"

#include "log.h"
#include "system/includes.h"
#include "btstack/avctp_user.h"
#include "tone_player.h"

#include "application/audio_eq.h"
#include "bt_tws.h"
#include "user_cfg.h"
#include "key_event_deal.h"
//#include "ota.h"

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_TUYA)

static OS_MUTEX	tuya_mutex;

extern tuya_ble_parameters_settings_t tuya_ble_current_para;
extern u8 key_table_l[KEY_NUM_MAX][KEY_EVENT_MAX];
extern u8 key_table_r[KEY_NUM_MAX][KEY_EVENT_MAX];
extern struct audio_dac_hdl dac_hdl;
static struct TUYA_SYNC_INFO tuya_sync_info;

static uint32_t sn = 0;
static uint32_t time_stamp = 1587795793;
static u16 find_device_timer = 0;

tuya_ble_device_param_t device_param = {0};

#define TUYA_TRIPLE_LENGTH      0x3E

#define TUYA_INFO_TEST 1

#if TUYA_INFO_TEST
static const char device_id_test[DEVICE_ID_LEN] = "tuya52c534229871";
static const char auth_key_test[AUTH_KEY_LEN] = "gqGPQQl4n540dc6sVPoGoh4fO7a8DzED";
static const uint8_t mac_test[6] = {0xDC, 0x23, 0x4E, 0x3E, 0xBD, 0x3D}; //The actual MAC address is : 66:55:44:33:22:11
#endif /* TUYA_INFO_TEST */

#define APP_CUSTOM_EVENT_1  1
#define APP_CUSTOM_EVENT_2  2
#define APP_CUSTOM_EVENT_3  3
#define APP_CUSTOM_EVENT_4  4
#define APP_CUSTOM_EVENT_5  5

#define LIC_PAGE_OFFSET     80

static uint8_t dp_data_array[255 + 3];
static uint16_t dp_data_len = 0;

typedef struct {
    uint8_t data[50];
} custom_data_type_t;

static __tuya_info tuya_info;
custom_data_type_t custom_data;
tuya_tws_sync_info_t tuya_tws_sync_info;

void user_avrcp_status_callback(u8 *addr, u8 flag)
{
    printf("a2dp_state_change_handler, status:%d", flag);
    uint8_t play_indicate_state = 0;
    if (flag == 1) {
        play_indicate_state = 1;
    } else {
        play_indicate_state = 0;
    }
    tuya_play_status_indicate(play_indicate_state);
}

static u16 tuya_get_one_info(const u8 *in, u8 *out)
{
    int read_len = 0;
    const u8 *p = in;

    while (TUYA_LEGAL_CHAR(*p) && *p != ',') { //read product_uuid
        *out++ = *p++;
        read_len++;
    }
    return read_len;
}

static void tuya_sn_increase(void)
{
    sn += 1;
}

static void tuya_bt_name_indicate()
{
    __tuya_bt_name_data p_dp_data;

    uint8_t name_len = strlen(bt_get_local_name());

    p_dp_data.id = 43;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_RAW;
    p_dp_data.len = U16_TO_LITTLEENDIAN(name_len);
    memcpy(p_dp_data.data, bt_get_local_name(), name_len);

    printf("tuya_bt_name_indicate state:%s", p_dp_data.data);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 4 + name_len);
    tuya_sn_increase();
#endif
}

static void tuya_conn_state_indicate()
{
    __tuya_conn_state_data p_dp_data;

    p_dp_data.id = 33;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_ENUM;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.tuya_conn_state;

    printf("tuya_conn_state_indicate state:%d", tuya_info.tuya_conn_state);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
    tuya_sn_increase();
#endif
}

// 经典蓝牙连接传1,断开传0
void tuya_conn_state_set_and_indicate(uint8_t state)
{
    if (state == 1) {
        tuya_info.tuya_conn_state = TUYA_CONN_STATE_CONNECTED;
    } else {
        tuya_info.tuya_conn_state = TUYA_CONN_STATE_DISCONNECT;
    }
    tuya_conn_state_indicate();
    tuya_one_click_connect_state_set(state);
}

static bool license_para_head_check(u8 *para)
{
    _flash_of_lic_para_head *head;

    //fill head
    head = (_flash_of_lic_para_head *)para;

    ///crc check
    u8 *crc_data = (u8 *)(para + sizeof(((_flash_of_lic_para_head *)0)->crc));
    u32 crc_len = sizeof(_flash_of_lic_para_head) - sizeof(((_flash_of_lic_para_head *)0)->crc)/*head crc*/ + (head->string_len)/*content crc,include end character '\0'*/;
    s16 crc_sum = 0;

    crc_sum = CRC16(crc_data, crc_len);

    if (crc_sum != head->crc) {
        printf("license crc error !!! %x %x \n", (u32)crc_sum, (u32)head->crc);
        return false;
    }

    return true;
}

const u8 *tuya_get_license_ptr(void)
{
    u32 flash_capacity = sdfile_get_disk_capacity();
    u32 flash_addr = flash_capacity - 256 + LIC_PAGE_OFFSET;
    u8 *lic_ptr = NULL;
    _flash_of_lic_para_head *head;

    printf("flash capacity:%x \n", flash_capacity);
    lic_ptr = (u8 *)sdfile_flash_addr2cpu_addr(flash_addr);

    //head length check
    head = (_flash_of_lic_para_head *)lic_ptr;
    if (head->string_len >= 0xff) {
        printf("license length error !!! \n");
        return NULL;
    }

    ////crc check
    if (license_para_head_check(lic_ptr) == (false)) {
        printf("license head check fail\n");
        return NULL;
    }

    //put_buf(lic_ptr, 128);

    lic_ptr += sizeof(_flash_of_lic_para_head);
    return lic_ptr;
}

static uint8_t read_tuya_product_info_from_flash(uint8_t *read_buf, u16 buflen)
{
    uint8_t *rp = read_buf;
    const uint8_t *tuya_ptr = (uint8_t *)tuya_get_license_ptr();
    //printf("tuya_ptr:");
    //put_buf(tuya_ptr, 69);

    if (tuya_ptr == NULL) {
        return FALSE;
    }
    int data_len = 0;
    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 16) {
        printf("read uuid err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    tuya_ptr += 17;

    rp = read_buf + 16;

    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 32) {
        printf("read key err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    tuya_ptr += 33;

    rp = read_buf + 16 + 32;
    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 12) {
        printf("read mac err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    return TRUE;
}

static u8 ascii_to_hex(u8 in)
{
    if (in >= '0' && in <= '9') {
        return in - '0';
    } else if (in >= 'a' && in <= 'f') {
        return in - 'a' + 0x0a;
    } else if (in >= 'A' && in <= 'F') {
        return in - 'A' + 0x0a;
    } else {
        printf("tuya ascii to hex error, data:0x%x", in);
        return 0;
    }
}

static void parse_mac_data(u8 *in, u8 *out)
{
    for (int i = 0; i < 6; i++) {
        out[i] = (ascii_to_hex(in[2 * i]) << 4) + ascii_to_hex(in[2 * i + 1]);
    }
}


void custom_data_process(int32_t evt_id, void *data)
{
    custom_data_type_t *event_1_data;
    printf("custom event id = %d", evt_id);
    switch (evt_id) {
    case APP_CUSTOM_EVENT_1:
        event_1_data = (custom_data_type_t *)data;
        break;
    case APP_CUSTOM_EVENT_2:
        break;
    case APP_CUSTOM_EVENT_3:
        break;
    case APP_CUSTOM_EVENT_4:
        break;
    case APP_CUSTOM_EVENT_5:
        break;
    default:
        break;

    }
}


void custom_evt_1_send_test(uint8_t data)
{
    tuya_ble_custom_evt_t event;

    for (uint8_t i = 0; i < 50; i++) {
        custom_data.data[i] = data;
    }
    event.evt_id = APP_CUSTOM_EVENT_1;
    event.custom_event_handler = (void *)custom_data_process;
    event.data = &custom_data;
    tuya_ble_custom_event_send(event);
}

void tuya_battry_indicate_case()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 2;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.case_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_battry_indicate_left()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 3;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.left_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_battry_indicate_right()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 4;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.right_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_key_info_indicate()
{
    __key_indicate_data p_dp_data;

    uint8_t key_buf[30];
    uint8_t key_func;
    for (int key_idx = 0; key_idx < 6; key_idx++) {
        p_dp_data.id = key_idx + 19;
        p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_ENUM;
        p_dp_data.len = U16_TO_LITTLEENDIAN(1);
        switch (key_idx) {
        case 0:
            key_func = tuya_info.key_info.left1;
            break;
        case 1:
            key_func = tuya_info.key_info.right1;
            break;
        case 2:
            key_func = tuya_info.key_info.left2;
            break;
        case 3:
            key_func = tuya_info.key_info.right2;
            break;
        case 4:
            key_func = tuya_info.key_info.left3;
            break;
        case 5:
            key_func = tuya_info.key_info.right3;
            break;
        }
        p_dp_data.data = key_func;

        memcpy(&key_buf[5 * key_idx], &p_dp_data, 5);
    }
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, key_buf, 30);
    tuya_sn_increase();
#endif
}

void tuya_battery_indicate(u8 left, u8 right, u8 chargebox)
{
    //tuya_led_state_indicate();
    tuya_info.battery_info.left_battery = left;
    tuya_info.battery_info.right_battery = right;
    tuya_info.battery_info.case_battery = chargebox;
    tuya_battry_indicate_right();
    tuya_battry_indicate_left();
    tuya_battry_indicate_case();
}

void tuya_key_reset()
{
    tuya_info.key_info.right1 = 0;
    tuya_info.key_info.right2 = 1;
    tuya_info.key_info.right3 = 2;
    tuya_info.key_info.left1 = 0;
    tuya_info.key_info.left2 = 1;
    tuya_info.key_info.left3 = 2;
    tuya_key_info_indicate();
}

/* 设备音量数据上报 */
void tuya_valume_indicate(u8 valume)
{
    printf("tuya_valume_indicate, sn:%d", sn);
    __valume_indicate_data p_dp_data;

    p_dp_data.id = 5;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = valume;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
    tuya_sn_increase();
#endif
}
/* 音乐播放状态上报 */
void tuya_play_status_indicate(u8 status)
{
    printf("tuya_play_status_indicate:%d, sn:%d", status, sn);
    __play_status_indicate_data p_dp_data;

    p_dp_data.id = 7;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_BOOL;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = status;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_eq_onoff_indicate(uint8_t status)
{
    printf("tuya_eq_onoff_indicate:%d,sn:%d", status, sn);
    __eq_onoff_indicate_data p_dp_data;

    p_dp_data.id = 44;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_BOOL;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = status;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_eq_info_indicate(char *eq_setting)
{
    __eq_indicate_data p_dp_data;
    if (tuya_info.eq_info.eq_onoff == 0) {
        tuya_info.eq_info.eq_onoff = 1;
        tuya_eq_onoff_indicate(tuya_info.eq_info.eq_onoff);
    }
    p_dp_data.id = 18;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_RAW;
    p_dp_data.len = U16_TO_LITTLEENDIAN(13);
    p_dp_data.version = 0x00;
    p_dp_data.eq_num = 0x0a;
    p_dp_data.eq_mode = eq_setting[10];
    for (int i = 0; i < 10; i++) {
        p_dp_data.eq_data[i] = eq_setting[i] + 0xC0;
    }

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 17);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 17);
    tuya_sn_increase();
#endif
}

static earphone_to_tuya_keymapping(u8 event)
{
    u8 ret = 0;
    switch (event) {
    case KEY_VOL_DOWN:
        ret = 0;
        break;
    case KEY_VOL_UP:
        ret = 1;
        break;
    case KEY_MUSIC_NEXT:
        ret = 2;
        break;
    case KEY_MUSIC_PREV:
        ret = 3;
        break;
    case KEY_MUSIC_PP:
        ret = 4;
        break;
    case KEY_OPEN_SIRI:
        ret = 6;
        break;
    }
    return ret;
}
void tuya_key_info_to_app()
{
    tuya_info.key_info.left1  = earphone_to_tuya_keymapping(key_table_l[0][0]);
    tuya_info.key_info.left2  = earphone_to_tuya_keymapping(key_table_l[0][1]);
    tuya_info.key_info.left3  = earphone_to_tuya_keymapping(key_table_l[0][4]);
    tuya_info.key_info.right1 = earphone_to_tuya_keymapping(key_table_r[0][0]);
    tuya_info.key_info.right2 = earphone_to_tuya_keymapping(key_table_r[0][1]);
    tuya_info.key_info.right3 = earphone_to_tuya_keymapping(key_table_r[0][4]);
    tuya_key_info_indicate();
}
void set_key_event_by_tuya_info(struct sys_event *event, struct key_event *key, u8 *key_event)
{
    u8 channel = 'U';
    if (get_bt_tws_connect_status()) {
        channel = tws_api_get_local_channel();
        if ('L' == channel) {
            if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
                *key_event = key_table_r[key->value][key->event];
            } else {
                *key_event = key_table_l[key->value][key->event];
            }
        } else {
            if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
                *key_event = key_table_l[key->value][key->event];
            } else {
                *key_event = key_table_r[key->value][key->event];
            }
        }
    } else {
        *key_event = key_table_l[key->value][key->event];
    }
}
/*********************************************************/
/* 涂鸦功能同步到对耳 */
/* tuya_info:对应type的同步数据 */
/* 不同type的info都放到tuya_sync_info进行统一同步 */
/* data_type:对应不同功能 */
/* 索引号对应APP_TWS_TUYA_SYNC_XXX (tuya_ble_app_demo.h) */
/*********************************************************/
static void tuya_sync_info_send(u8 *tuya_info, u8 data_type)
{
#if TCFG_USER_TWS_ENABLE
    tuya_sync_info.key_reset = 0;
    tuya_sync_info.tuya_eq_flag = 0;
    tuya_sync_info.volume_flag = 0;
    tuya_sync_info.tuya_bt_name_flag = 0;
    tuya_sync_info.key_change_flag = 0;
    if (get_bt_tws_connect_status()) {
        printf("data_type:%d\n", data_type);
        switch (data_type) {
        case APP_TWS_TUYA_SYNC_EQ:
            printf("sync eq info!\n");
            memcpy(tuya_sync_info.eq_info, tuya_info, EQ_SECTION_MAX + 1);
            put_buf(tuya_sync_info.eq_info, EQ_SECTION_MAX + 1);
            tuya_sync_info.tuya_eq_flag = 1;
            break;
        case APP_TWS_TUYA_SYNC_ANC:
            printf("sync anc info!\n");
            break;
        case APP_TWS_TUYA_SYNC_VOLUME:
            tuya_sync_info.volume = *tuya_info;
            tuya_sync_info.volume_flag = 1;
            printf("sync volume info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R1:
            tuya_sync_info.key_r1 = *tuya_info;
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_r1 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R2:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = *tuya_info;
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_r2 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R3:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = *tuya_info;
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_r3 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L1:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = *tuya_info;
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_l1 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L2:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = *tuya_info;
            tuya_sync_info.key_l3 = key_table_l[0][4];
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_l2 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L3:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = *tuya_info;
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_l3 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_FIND_DEVICE:
            tuya_sync_info.find_device = *tuya_info;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync find_device info!\n");
            break;
        case APP_TWS_TUYA_SYNC_DEVICE_CONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = *tuya_info;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync device_conn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_DEVICE_DISCONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = *tuya_info;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync device_disconn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_PHONE_CONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = *tuya_info;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync phone_conn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_PHONE_DISCONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = *tuya_info;
            printf("sync phone_disconn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_BT_NAME:
            printf("sync bt name!\n");
            memcpy(tuya_sync_info.bt_name, tuya_info, LOCAL_NAME_LEN);
            tuya_sync_info.tuya_bt_name_flag = 1;
            printf("tuya bt name:%s\n", tuya_sync_info.bt_name);
            break;
        case APP_TWS_TUYA_SYNC_KEY_RESET:
            printf("sync key_reset!\n");
            tuya_sync_info.key_reset = 1;
            break;
        default:
            break;
        }
        /* if (tws_api_get_role() == TWS_ROLE_MASTER) { */
        printf("this is tuya master!\n");
        u8 status = tws_api_send_data_to_sibling(&tuya_sync_info, sizeof(tuya_sync_info), TWS_FUNC_ID_TUYA_STATE);
        printf("status:%d\n", status);
        /* } */
    }
#endif
}
void tuya_eq_data_deal(char *eq_info_data)
{
    char data;
    u8 mode = EQ_MODE_CUSTOM;
    // 自定义修改EQ参数
    for (u8 i = 0; i < EQ_SECTION_MAX; i++) {
        data = eq_info_data[i];
        eq_mode_set_custom_param(i, (s8)data);
    }
    eq_mode_set(mode);
}

static void tuya_eq_data_setting(char *eq_setting, char eq_mode)
{
    char eq_info[11] = {0};
    memcpy(eq_info, eq_setting, 10);
    eq_info[10] = eq_mode;
    if (!eq_setting) {
        ASSERT(0, "without eq_data!");
    } else {
        if (get_bt_tws_connect_status()) {
            printf("start eq sync!");
            tuya_sync_info_send(eq_info, 0);
        }
        printf("set eq_info:");
        put_buf(eq_info, 11);
        syscfg_write(CFG_RCSP_ADV_EQ_DATA_SETTING, eq_info, 11);
        tuya_eq_data_deal(eq_setting);
    }
}
void tuya_set_music_volume(int volume)
{
    s16 music_volume;
    u8 flag = 1;
    music_volume = ((volume + 1) * 16) / 100;
    music_volume--;
    if (music_volume < 0) {
        music_volume = 0;
        flag = 0;
    }
    printf("phone_vol:%d,dac_vol:%d", volume, music_volume);
    opid_play_vol_sync_fun(&music_volume, flag);
    user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_SEND_VOL, 0, NULL);
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, music_volume, 1);
}
static void find_device()
{
    tone_play_index(IDEX_TONE_NORMAL, 1);
}
/* 涂鸦app对应功能索引映射到sdk按键枚举 */
u8 tuya_key_event_swith(u8 event)
{
    u8 ret = 0;
    switch (event) {
    case 0:
        ret = KEY_VOL_DOWN;
        break;
    case 1:
        ret = KEY_VOL_UP;
        break;
    case 2:
        ret = KEY_MUSIC_NEXT;
        break;
    case 3:
        ret = KEY_MUSIC_PREV;
        break;
    case 4:
        ret = KEY_MUSIC_PP;
        break;
    case 5:
    case 6:
        ret = KEY_OPEN_SIRI;
        break;
    }
    return ret;
}
void tuya_change_bt_name(char *name, u8 name_len)
{
    extern BT_CONFIG bt_cfg;
    extern const char *bt_get_local_name();
    extern void lmp_hci_write_local_name(const char *name);
    memset(bt_cfg.edr_name, 0, name_len);
    memcpy(bt_cfg.edr_name, name, name_len);
    /* extern void bt_wait_phone_connect_control(u8 enable); */
    /* bt_wait_phone_connect_control(0); */
    lmp_hci_write_local_name(bt_get_local_name());
    /* bt_wait_phone_connect_control(1); */
}

void tuya_post_key_event(u8 type)
{
    struct sys_event e;
    /* user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL); */
    e.type = SYS_KEY_EVENT;
    e.u.key.type = type;
    e.u.key.event = 0;
    e.u.key.value = 0;
    e.u.key.init = 1;
    e.arg  = (void *)DEVICE_EVENT_FROM_KEY;
    sys_event_notify(&e);
}
void tuya_data_parse(tuya_ble_cb_evt_param_t *event)
{
    uint32_t sn = event->dp_received_data.sn;
    printf("tuya_data_parse, p_data:0x%x, len:%d", event->dp_received_data.p_data, event->dp_received_data.data_len);
    put_buf(event->dp_received_data.p_data, event->dp_received_data.data_len);
    uint16_t buf_len = event->dp_received_data.data_len;

    uint8_t dp_id = event->dp_received_data.p_data[0];
    uint8_t type = event->dp_received_data.p_data[1];
    uint16_t data_len = TWO_BYTE_TO_DATA((&event->dp_received_data.p_data[2]));
    uint8_t *data = &event->dp_received_data.p_data[4];
    printf("\n\n<--------------  tuya_data_parse  -------------->");
    printf("sn = %d, id = %d, type = %d, data_len = %d, data:", sn, dp_id, type, data_len);
    u8 key_value_record[6][6] = {0};
    put_buf(data, data_len);
    u8 value = 0;
    switch (dp_id) {
    case 1:
        //iot播报模式
        printf("tuya iot broadcast set to: %d\n", data[0]);
        break;
    case 5:
        //音量设置  yes
        printf("tuya voice set to :%d\n", data[3]);
        tuya_set_music_volume(data[3]);
        tuya_sync_info_send(&data[3], 2);
        break;
    case 6:
        //切换控制  yes
        printf("tuya change_control: %d\n", data[0]);
        if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
            tuya_post_key_event(TUYA_MUSIC_PP);
        } else {
            if (data[0]) {
                tuya_post_key_event(TUYA_MUSIC_NEXT);
            } else {
                tuya_post_key_event(TUYA_MUSIC_PREV);
            }
        }
        break;
    case 7:
        //播放/暂停  yes
        tuya_post_key_event(TUYA_MUSIC_PP);
        printf("tuya play:%d\n", data[0]);
        break;
    case 8:
        //设置降噪模式
#if TCFG_AUDIO_ANC_ENABLE
        anc_mode_switch(data[0], 1);
#endif
        tuya_info.noise_info.noise_mode = data[0];
        printf("tuya noise_mode: %d\n", tuya_info.noise_info.noise_mode);
        break;
    case 9:
        //设置降噪场景
        tuya_info.noise_info.noise_scenes = data[0];
        printf("tuya noise_scenes:%d\n", tuya_info.noise_info.noise_scenes);
        break;
    case 10:
        //设置通透模式
        tuya_info.noise_info.transparency_scenes = data[0];
        printf("tuya transparency_scenes: %d\n", tuya_info.noise_info.transparency_scenes);
        break;
    case 11:
        //设置降噪强度
        tuya_info.noise_info.noise_set = data[3];
        printf("tuya noise_set: %d\n", tuya_info.noise_info.noise_set);
        break;
    case 12:
        //寻找设备
        tuya_sync_info_send(&data[0], 9);
        if (data[0]) {
            find_device_timer = sys_timer_add(NULL, find_device, 1000);
        } else {
            sys_timer_del(find_device_timer);
        }
        printf("tuya find device set:%d\n", data[0]);
        break;
    case 13:
        //设备断连提醒
        tuya_sync_info_send(&data[0], 11);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_DISCONN, 1);
        }
        printf("tuya device disconnect notify set:%d", data[0]);
        break;
    case 14:
        //设备重连提醒
        tuya_sync_info_send(&data[0], 10);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_CONN, 1);
        }
        printf("tuya device reconnect notify set:%d", data[0]);
        break;
    case 15:
        //手机断连提醒
        tuya_sync_info_send(&data[0], 13);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_DISCONN, 1);
        }
        printf("tuya phone disconnect notify set:%d", data[0]);
        break;
    case 16:
        //手机重连提醒
        tuya_sync_info_send(&data[0], 12);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_CONN, 1);
        }
        printf("tuya phone reconnect notify set:%d", data[0]);
        break;
    case 17:
        //设置eq模式
        printf("tuya eq_mode set:0x%x", data[0]);
        break;
    case 18:
        // 设置eq参数.此处也会设置eq模式  yes
        if (tuya_info.eq_info.eq_onoff == 0) {
            printf("tuya eq_data set fail, eq_onoff is 0!");
            return;
        }
        tuya_info.eq_info.eq_mode = data[2];
        printf("tuya eq_mode:0x%x\n", tuya_info.eq_info.eq_mode);
        memcpy(tuya_info.eq_info.eq_data, &data[3], EQ_CNT);
        for (int i = 0; i < 10; i++) {
            tuya_info.eq_info.eq_data[i] -= 0xc0;
            printf("tuya eq_data[%d]:%d\n", i, tuya_info.eq_info.eq_data[i]);
        }
        tuya_eq_data_setting(tuya_info.eq_info.eq_data, tuya_info.eq_info.eq_mode);
        break;
    case 19:
        //左按键1
        key_table_l[0][0] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_l[0][0], 6);
        break;
    case 20:
        //右按键1
        key_table_r[0][0] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_r[0][0], 3);
        break;
    case 21:
        //左按键2
        key_table_l[0][1] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_l[0][1], 7);
        break;
    case 22:
        //右按键2
        key_table_r[0][1] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_r[0][1], 4);
        break;
    case 23:
        //左按键3
        key_table_l[0][4] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_l[0][4], 8);
        break;
    case 24:
        //右按键3
        key_table_r[0][4] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_r[0][4], 5);
        break;
    case 25:
        //按键重置
        printf("tuya key reset, sn:%d", sn);
        key_table_l[0][0] = tuya_key_event_swith(0);
        key_table_l[0][1] = tuya_key_event_swith(1);
        key_table_l[0][4] = tuya_key_event_swith(2);
        key_table_r[0][0] = tuya_key_event_swith(0);
        key_table_r[0][1] = tuya_key_event_swith(1);
        key_table_r[0][4] = tuya_key_event_swith(2);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(NULL, 15);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
        tuya_ble_dp_data_report(data, data_len); //1
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
        tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, data, data_len);
#endif
        tuya_key_reset();
        return;
    case 26:
        //入耳检测
        printf("tuya inear_detection set:%d\n", data[0]);
        break;
    case 27:
        //贴合度检测
        printf("tuya fit_detection set:%d\n", data[0]);
        break;
    case 31:
        //倒计时
        u32 count_time = FOUR_BYTE_TO_DATA(data);
        printf("tuya count down:%d\n", count_time);
        break;
    case 43:
        //蓝牙名字
        char *name = malloc(data_len + 1);
        memcpy(name, &data[0], data_len);
        name[data_len] = '\0';
        tuya_change_bt_name(name, LOCAL_NAME_LEN);
        syscfg_write(CFG_BT_NAME, name, LOCAL_NAME_LEN);
        tuya_sync_info_send(name, 14);
        printf("tuya bluetooth name: %s, len:%d\n", name, data_len);
        free(name);
        break;
    case 44:
        // 设置eq开关
        tuya_info.eq_info.eq_onoff = data[0];
        for (int i = 0; i < 10; i++) {
            tuya_info.eq_info.eq_data[i] = 0;
        }
        char eq_info[11] = {0};
        memcpy(eq_info, tuya_info.eq_info.eq_data, 10);
        eq_info[10] = 0;
        tuya_sync_info_send(eq_info, 0);
        eq_mode_set(EQ_MODE_NORMAL);
        syscfg_write(CFG_RCSP_ADV_EQ_DATA_SETTING, eq_info, 11);
        printf("tuya eq_onoff set:%d\n", tuya_info.eq_info.eq_onoff);
        break;
    case 45:
        //设置通透强度
        tuya_info.noise_info.trn_set = data[3];
        printf("tuya trn_set:%d\n", tuya_info.noise_info.trn_set);
        break;
    default:
        printf("unknow control msg len = %d\n, data:", data_len);
        break;
    }
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(event->dp_received_data.p_data, data_len + 4); //1
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, event->dp_received_data.p_data, data_len + 4);
#endif
}

void tuya_tws_bind_info_sync()
{
    printf("tuya_tws_bind_info_sync");
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        memcpy(tuya_tws_sync_info.login_key, tuya_ble_current_para.sys_settings.login_key, LOGIN_KEY_LEN);
        memcpy(tuya_tws_sync_info.device_virtual_id, tuya_ble_current_para.sys_settings.device_virtual_id, DEVICE_VIRTUAL_ID_LEN);
        tuya_tws_sync_info.bound_flag = tuya_ble_current_para.sys_settings.bound_flag;

        int ret = tws_api_send_data_to_sibling(&tuya_tws_sync_info, sizeof(tuya_tws_sync_info), TWS_FUNC_ID_TUYA_AUTH_SYNC);
    } else {
        printf("slaver don't sync pair info");
    }
}

void tuya_info_indicate()
{
    tuya_info.eq_info.eq_onoff = 1;
    tuya_eq_onoff_indicate(1);
    tuya_key_info_to_app();
    u8 volume = app_audio_get_volume(1);
    u8 max_vol = app_audio_get_max_volume();
    u8 tuya_sync_volume = (int)(volume * 100 / max_vol);
    printf("volume:%d\n", tuya_sync_volume);
    tuya_valume_indicate(tuya_sync_volume);
    if (a2dp_get_status() == 1) {
        tuya_play_status_indicate(1);
    } else {
        tuya_play_status_indicate(0);
    }
    char eq_info[11] = {0};
    syscfg_read(CFG_RCSP_ADV_EQ_DATA_SETTING, eq_info, 11);
    printf("tuya_eq_info indicate\n");
    put_buf(eq_info, 11);
    tuya_eq_info_indicate(eq_info);
    tuya_conn_state_indicate();
    tuya_bt_name_indicate();
}

void tuya_app_cb_handler(tuya_ble_cb_evt_param_t *event)
{
    printf("tuya_app_cb_handler, evt:0x%x, task:%s\n", event->evt, os_current_task());
    int16_t result = 0;
    switch (event->evt) {
    case TUYA_BLE_CB_EVT_CONNECTE_STATUS:
        printf("received tuya ble conncet status update event,current connect status = %d", event->connect_status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_RECEIVED:
        tuya_data_parse(event);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_REPORT_RESPONSE:
        printf("received dp data report response result code =%d", event->dp_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WTTH_TIME_REPORT_RESPONSE:
        printf("received dp data report response result code =%d", event->dp_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WITH_FLAG_REPORT_RESPONSE:
        printf("received dp data with flag report response sn = %d , flag = %d , result code =%d", event->dp_with_flag_response_data.sn, event->dp_with_flag_response_data.mode
               , event->dp_with_flag_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WITH_FLAG_AND_TIME_REPORT_RESPONSE:
        printf("received dp data with flag and time report response sn = %d , flag = %d , result code =%d", event->dp_with_flag_and_time_response_data.sn,
               event->dp_with_flag_and_time_response_data.mode, event->dp_with_flag_and_time_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_UNBOUND:
        printf("received unbound req");
        break;
    case TUYA_BLE_CB_EVT_ANOMALY_UNBOUND:
        printf("received anomaly unbound req");
        break;
    case TUYA_BLE_CB_EVT_DEVICE_RESET:
        printf("received device reset req");
        break;
    case TUYA_BLE_CB_EVT_DP_QUERY:
        printf("received TUYA_BLE_CB_EVT_DP_QUERY event");
        printf("dp_query len:", event->dp_query_data.data_len);
        put_buf(event->dp_query_data.p_data, event->dp_query_data.data_len);
        tuya_info_indicate();
        break;
    case TUYA_BLE_CB_EVT_OTA_DATA:
        tuya_ota_proc(event->ota_data.type, event->ota_data.p_data, event->ota_data.data_len);
        break;
    case TUYA_BLE_CB_EVT_NETWORK_INFO:
        printf("received net info : %s", event->network_data.p_data);
        tuya_ble_net_config_response(result);
        break;
    case TUYA_BLE_CB_EVT_WIFI_SSID:
        break;
    case TUYA_BLE_CB_EVT_TIME_STAMP:
        printf("received unix timestamp : %s ,time_zone : %d", event->timestamp_data.timestamp_string, event->timestamp_data.time_zone);
        break;
    case TUYA_BLE_CB_EVT_TIME_NORMAL:
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_SEND_RESPONSE:
        printf("TUYA_BLE_CB_EVT_DP_DATA_SEND_RESPONSE, sn:%d, type:0x%x, mode:0x%x, ack:0x%x, status:0x%x", event->dp_send_response_data.sn, event->dp_send_response_data.type, event->dp_send_response_data.mode, event->dp_send_response_data.ack, event->dp_send_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DATA_PASSTHROUGH:
        tuya_ble_data_passthrough(event->ble_passthrough_data.p_data, event->ble_passthrough_data.data_len);
        break;
    case TUYA_BLE_CB_EVT_UPDATE_LOGIN_KEY_VID:
        printf("TUYA_BLE_CB_EVT_UPDATE_LOGIN_KEY_VID");
        break;
    default:
        printf("tuya_app_cb_handler msg: unknown event type 0x%04x", event->evt);
        break;
    }
    tuya_ble_inter_event_response(event);
}

typedef struct {
    u8  flag;
    u8  data[0x50];
} tuya_triple_info;

tuya_triple_info triple_info;

typedef struct {
    u8  flag;
    u8  data[0x50];
} vm_tuya_triple_info;

vm_tuya_triple_info vm_triple_info;

enum {
    TRIPLE_NULL = 0,
    TRIPLE_FLASH,
    TRIPLE_VM,
};

int flash_ret = 0;
typedef struct {
    u8  data[TUYA_TRIPLE_LENGTH];
} vm_tuya_triple;

extern vm_tuya_triple vm_triple;
int vm_read_result;

static uint8_t app_tuya_get_vm_id(u32 addr)
{
    uint8_t index;
    switch (addr) {
    case TUYA_BLE_AUTH_FLASH_ADDR:
        index = CFG_USER_TUYA_INFO_AUTH;
        break;
    case TUYA_BLE_AUTH_FLASH_BACKUP_ADDR:
        index = CFG_USER_TUYA_INFO_AUTH_BK;
        break;
    case TUYA_BLE_SYS_FLASH_ADDR:
        index = CFG_USER_TUYA_INFO_SYS;
        break;
    case TUYA_BLE_SYS_FLASH_BACKUP_ADDR:
        index = CFG_USER_TUYA_INFO_SYS_BK;
        break;
    default:
        return TUYA_BLE_ERR_INVALID_ADDR;
    }
    return index;
}

void tuya_ble_app_init(void)
{
    device_param.device_id_len = 16;    //If use the license stored by the SDK,initialized to 0, Otherwise 16 or 20.
    uint8_t read_buf[16 + 32 + 6 + 1] = {0};

    int ret = 0;

    char eq_setting[11] = {0};
    syscfg_read(CFG_RCSP_ADV_EQ_DATA_SETTING, eq_setting, 11);
    printf(">>> tuya init eq_info:\n");
    put_buf(eq_setting, 11);
    char temp_eq_buf[10] = {0};
    if (!memcmp(temp_eq_buf, eq_setting, 10)) {
        eq_mode_set(EQ_MODE_NORMAL);
    } else {
        tuya_eq_data_deal(eq_setting);
    }

    device_param.use_ext_license_key = 1;
    if (device_param.device_id_len == 16) {
#if TUYA_INFO_TEST
        memcpy(device_param.auth_key, (void *)auth_key_test, AUTH_KEY_LEN);
        memcpy(device_param.device_id, (void *)device_id_test, DEVICE_ID_LEN);
        memcpy(device_param.mac_addr.addr, mac_test, 6);
#else
        flash_ret = read_tuya_product_info_from_flash(read_buf, sizeof(read_buf));
        if (flash_ret == TRUE) {
            uint8_t mac_data[6];
            memcpy(device_param.device_id, read_buf, 16);
            memcpy(device_param.auth_key, read_buf + 16, 32);
            parse_mac_data(read_buf + 16 + 32, mac_data);
            memcpy(device_param.mac_addr.addr, mac_data, 6);
            memcpy((u8 *)&triple_info.data, read_buf, TUYA_TRIPLE_LENGTH);
            triple_info.flag = 1;


        } else {
            triple_info.flag = 2;
            vm_read_result = syscfg_read(VM_TUYA_TRIPLE, (u8 *)&vm_triple, TUYA_TRIPLE_LENGTH);
            printf("vm result: %d\n", vm_read_result);
            printf("read vm data:");
            put_buf((u8 *)&vm_triple, 0x40);
            memcpy((u8 *)&triple_info.data, (u8 *)&vm_triple, TUYA_TRIPLE_LENGTH);
            if (vm_read_result == TUYA_TRIPLE_LENGTH) {
                uint8_t mac_data[6];
                memcpy(device_param.device_id, (u8 *)&vm_triple, 16);
                memcpy(device_param.auth_key, (u8 *)&vm_triple + 16, 32);
                parse_mac_data((u8 *)&vm_triple + 16 + 32, mac_data);
                memcpy(device_param.mac_addr.addr, mac_data, 6);
                printf("read after write vm data:");
                put_buf((u8 *)&vm_triple, 0x40);
            } else  {
                printf("tripe  null");
                put_buf((u8 *)&vm_triple_info, TUYA_TRIPLE_LENGTH);
                triple_info.flag = 0;
            }

        }
#endif /* TUYA_INFO_TEST */
        device_param.mac_addr.addr_type = TUYA_BLE_ADDRESS_TYPE_RANDOM;
    }
    printf("device_id:");
    put_buf(device_param.device_id, 16);
    printf("auth_key:");
    put_buf(device_param.auth_key, 32);
    printf("mac:");
    put_buf(device_param.mac_addr.addr, 6);

    device_param.p_type = TUYA_BLE_PRODUCT_ID_TYPE_PID;
    device_param.product_id_len = 8;
    memcpy(device_param.product_id, APP_PRODUCT_ID, 8);
    device_param.firmware_version = TY_APP_VER_NUM;
    device_param.hardware_version = TY_HARD_VER_NUM;
    tuya_get_vm_id_register(app_tuya_get_vm_id);
    tuya_one_click_connect_init();
    tuya_ble_sdk_init(&device_param);
    ret = tuya_ble_callback_queue_register(tuya_app_cb_handler);
    y_printf("tuya_ble_callback_queue_register,ret=%d\n", ret);

    //tuya_ota_init();

    //printf("demo project version : "TUYA_BLE_DEMO_VERSION_STR);
    printf("app version : "TY_APP_VER_STR);
}
#if TUYA_DEMO_EN

int get_triple_info_result()
{
    if (vm_read_result != TUYA_TRIPLE_LENGTH) {
        vm_read_result = 0;
    }

    return flash_ret | vm_read_result;
}


u16 get_triple_data(u8 *data)
{
    if (flash_ret) {
        memcpy(data, (u8 *)&triple_info.data, TUYA_TRIPLE_LENGTH - 2);

    } else if (vm_read_result == TUYA_TRIPLE_LENGTH) {
        memcpy(data, (u8 *)&vm_triple, TUYA_TRIPLE_LENGTH - 2);

    }
    return TUYA_TRIPLE_LENGTH ;
}
extern void tws_conn_send_event(int code, const char *format, ...);
u8 tuya_data[TUYA_TRIPLE_LENGTH] = {0};
void set_triple_info(u8 *data)
{
    printf("--------------------------------------------");
    printf("Tuya triple function is no available now!!!");
    printf("--------------------------------------------");
    memset((u8 *)&tuya_data, 0, 0x3e);
    memcpy((u8 *)&tuya_data, data, TUYA_TRIPLE_LENGTH - 2);
    //tws_conn_send_event(TWS_EVENT_CHANGE_TRIPLE, "111", 0, 1, 0);

}
#endif

#endif
