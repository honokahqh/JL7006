//#include "system/app_core.h"
#include "system/includes.h"
#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "btstack/le/le_common_define.h"
#include "dma_le_port.h"
#include "btstack/third_party/common/ble_user.h"
#include "btstack/third_party/common/spp_user.h"
#include "btstack/third_party/app_protocol_event.h"
#include "dma_setting.h"
#include "dma_wrapper.h"
#include "btstack/avctp_user.h"

#ifdef DMA_LIB_CODE_SIZE_CHECK
#pragma bss_seg(	".bt_dma_port_bss")
#pragma data_seg(	".bt_dma_port_data")
#pragma const_seg(	".bt_dma_port_const")
#pragma code_seg(	".bt_dma_port_code")
#endif

#define USE_OWN_ADV_DATA            0    /*百度提供的库会提供广播包数据，不需要自己生成*/
#define PRINT_DMA_DATA_EN           0
//------
#define ATT_LOCAL_PAYLOAD_SIZE    (216)                   //note: need >= 20
#define ATT_SEND_CBUF_SIZE        (512)                   //note: need >= 20,缓存大小，可修改
#define ATT_RAM_BUFSIZE           (ATT_CTRL_BLOCK_SIZE + ATT_LOCAL_PAYLOAD_SIZE + ATT_SEND_CBUF_SIZE)                   //note:
/* static u8 att_ram_buffer[ATT_RAM_BUFSIZE] __attribute__((aligned(4))); */
static u8 *att_ram_buffer = NULL;
//---------------

#define ADV_INTERVAL_MIN          (160)
#define ADV_INTERVAL_MIN_IBEACON          (32)
static hci_con_handle_t con_handle;
//加密设置
static const uint8_t sm_min_key_size = 7;
//连接参数设置
static const uint8_t connection_update_enable = 1; ///0--disable, 1--enable
static uint8_t connection_update_cnt = 0; //
static const struct conn_update_param_t connection_param_table[] = {
    {8,   8, 0, 600},
    {16, 24, 0, 600},//11
    {12, 28, 0, 600},//3.7
    {8,  20, 0,  600},
    /* {12, 28, 4, 600},//3.7 */
    /* {12, 24, 30, 600},//3.05 */
};
#define CONN_PARAM_TABLE_CNT      (sizeof(connection_param_table)/sizeof(struct conn_update_param_t))

#if (ATT_RAM_BUFSIZE < 64)
#error "adv_data & rsp_data buffer error!!!!!!!!!!!!"
#endif

// 广播包内容
u8 adv_data[ADV_RSP_PACKET_MAX];//max is 31
u8 adv_data_length = 0;
// scan_rsp 内容
u8 scan_rsp_data[ADV_RSP_PACKET_MAX];//max is 31
u8 scan_rsp_data_len = 0;
// ibeacon 内容
u8 dma_ibeacon_data[ADV_RSP_PACKET_MAX];//max is 31
u8 dma_ibeacon_data_len = 0;

static char *gap_device_name = "ble";
static u8 gap_device_name_len = 0;
static u8 ble_work_state = 0;
void dma_set_module(bool state);
static u8 adv_ctrl_en;
static bool dma_module_en = 0;

//------------------------------------------------------

static int ota_send_user_data_do(void *priv, u8 *data, u16 len);
static int app_send_user_data_check(u16 len);
static int app_send_user_data_do(void *priv, u8 *data, u16 len);
static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type);
//------------------------------------------------------
//广播参数设置
static void advertisements_setup_init();
extern const char *bt_get_local_name();
static int set_adv_enable(void *priv, u32 en);
static int get_buffer_vaild_len(void *priv);
static void bt_ble_adv_enable(u8 enable);
//------------------------------------------------------
static void send_request_connect_parameter(u8 table_index)
{
    struct conn_update_param_t *param = (void *)&connection_param_table[table_index];//static ram

    log_info("update_request:-%d-%d-%d-%d-\n", param->interval_min, param->interval_max, param->latency, param->timeout);
    if (con_handle) {
        ble_user_cmd_prepare(BLE_CMD_REQ_CONN_PARAM_UPDATE, 2, con_handle, param);
    }
}

static void check_connetion_updata_deal(void)
{
    if (connection_update_enable) {
        if (connection_update_cnt < CONN_PARAM_TABLE_CNT) {
            send_request_connect_parameter(connection_update_cnt);
        }
    }
}

static void connection_update_complete_success(u8 *packet)
{
    int con_handle, conn_interval, conn_latency, conn_timeout;

    con_handle = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
    conn_latency = hci_subevent_le_connection_update_complete_get_conn_latency(packet);
    conn_timeout = hci_subevent_le_connection_update_complete_get_supervision_timeout(packet);

    log_info("conn_interval = %d\n", conn_interval);
    log_info("conn_latency = %d\n", conn_latency);
    log_info("conn_timeout = %d\n", conn_timeout);
}


static void set_ble_work_state(ble_state_e state)
{
    if (state != ble_work_state) {
        //log_info("ble_work_st:%x->%x\n", ble_work_state, state);
        ble_work_state = state;
    }
}

static ble_state_e get_ble_work_state(void)
{
    return ble_work_state;
}

static void cbk_sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    sm_just_event_t *event = (void *)packet;
    u32 tmp32;
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            log_info("Just Works Confirmed.\n");
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            log_info_hexdump(packet, size);
            memcpy(&tmp32, event->data, 4);
            log_info("Passkey display: %06u.\n", tmp32);
            break;
        }
        break;
    }
}

/*
 * @section Packet Handler
 *
 * @text The packet handler is used to:
 *        - stop the counter after a disconnect
 *        - send a notification when the requested ATT_EVENT_CAN_SEND_NOW is received
 */

/* LISTING_START(packetHandler): Packet Handler */
u16 global_local_ble_mtu = 0;
bool dma_get_ble_update_ready_jump_flag();
static void cbk_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    int mtu;
    u32 tmp;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {

        /* case DAEMON_EVENT_HCI_PACKET_SENT: */
        /* break; */
        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            log_info("ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE\n");
        case ATT_EVENT_CAN_SEND_NOW:
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {

                ASSERT(att_ram_buffer == NULL, "att_pool is err\n");
                att_ram_buffer = malloc(ATT_RAM_BUFSIZE);
                ASSERT(att_ram_buffer, "att_pool is not ok\n");

                dma_message(APP_PROTOCOL_CONNECTED_BLE, NULL, 0);
                con_handle = little_endian_read_16(packet, 4);
                log_info("HCI_SUBEVENT_LE_CONNECTION_COMPLETE: %0x\n", con_handle);
                connection_update_complete_success(packet + 8);
                ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, att_ram_buffer, ATT_RAM_BUFSIZE, ATT_LOCAL_PAYLOAD_SIZE);
                set_ble_work_state(BLE_ST_CONNECT);
                dma_set_module(1);
            }
            break;

            case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                connection_update_complete_success(packet);
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            log_info("HCI_EVENT_DISCONNECTION_COMPLETE: %0x\n", packet[5]);

            if (con_handle == 0) {
                printf("some error disconnect,ignore\n");
                break;
            }
            con_handle = 0;
            ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, 0, 0, 0);
            set_ble_work_state(BLE_ST_DISCONN);
            if (!dma_get_ble_update_ready_jump_flag()) {
                if (dma_check_tws_is_master()) {
                    bt_ble_adv_enable(1);
                }
            }
            connection_update_cnt = 0;

            server_register_dueros_ota_send_callbak(NULL);
            server_register_dueros_send_callbak(NULL);
            dma_message(APP_PROTOCOL_DISCONNECT, NULL, 0);
            free(att_ram_buffer);
            att_ram_buffer = NULL;
            dma_set_module(0);
            break;

        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            log_info("ATT MTU = %u\n", mtu);
            global_local_ble_mtu = mtu;
            ble_user_cmd_prepare(BLE_CMD_ATT_MTU_SIZE, 1, mtu);
            break;

        case HCI_EVENT_VENDOR_REMOTE_TEST:
            log_info("--- HCI_EVENT_VENDOR_REMOTE_TEST\n");
            break;

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            tmp = little_endian_read_16(packet, 4);
            log_info("-update_rsp: %02x\n", tmp);
            if (tmp) {
                connection_update_cnt++;
                log_info("remoter reject!!!\n");
                check_connetion_updata_deal();
            } else {
                connection_update_cnt = CONN_PARAM_TABLE_CNT;
            }
            break;

        case HCI_EVENT_ENCRYPTION_CHANGE:
            log_info("HCI_EVENT_ENCRYPTION_CHANGE= %d\n", packet[2]);
            break;
        }
        break;
    }
}


/* LISTING_END */

/*
 * @section ATT Read
 *
 * @text The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the registered
 * att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first called
 * with buffer == NULL, to request the total value length. Then it will be called again requesting a chunk of the value.
 * See Listing attRead.
 */


static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{

    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;

    /* printf("READ Callback, handle %04x, offset %u, buffer size %u\n", handle, offset, buffer_size); */

    log_info("read_callback, handle= 0x%04x,buffer= %08x\n", handle, (u32)buffer);

    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        att_value_len = strlen(gap_device_name);
        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }

        if (buffer) {
            memcpy(buffer, &gap_device_name[offset], buffer_size);
            att_value_len = buffer_size;
            log_info("\n------read gap_name: %s \n", gap_device_name);
        }
        break;
    case ATT_CHARACTERISTIC_0000FF00_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
        log_info("=====notify ota en ====\n");
    /*case ATT_CHARACTERISTIC_c2e758b9_0e78_41e0_b0cb_98a593193fc5_01_CLIENT_CONFIGURATION_HANDLE:*/
    case ATT_CHARACTERISTIC_b84ac9c6_29c5_46d4_bba1_9d534784330f_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_2a03_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_2a05_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_2a01_02_CLIENT_CONFIGURATION_HANDLE:
        if (buffer) {
            buffer[0] = att_get_ccc_config(handle);
            buffer[1] = 0;
        }
        att_value_len = 2;
        break;

    default:
        break;
    }

    log_info("att_value_len= %d\n", att_value_len);
    return att_value_len;

}


/* LISTING_END */
/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored and used
 * in the heartbeat handler to decide if a new value should be sent. See Listing attWrite.
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;

    u16 handle = att_handle;

    /* log_info("write_callback, handle= 0x%04x\n", handle); */

    switch (handle) {
    case ATT_CHARACTERISTIC_0000FF00_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
        log_info("----ota write----\n");
        set_ble_work_state(BLE_ST_NOTIFY_IDICATE);
        log_info_hexdump(buffer, buffer_size);
        check_connetion_updata_deal();
        log_info("------ota write ccc:%04x, %02x\n", handle, buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        server_register_dueros_ota_send_callbak(ota_send_user_data_do);
        break;

    case ATT_CHARACTERISTIC_b84ac9c6_29c5_46d4_bba1_9d534784330f_01_CLIENT_CONFIGURATION_HANDLE:

        set_ble_work_state(BLE_ST_NOTIFY_IDICATE);
        check_connetion_updata_deal();
        log_info("------write ccc:%04x, %02x\n", handle, buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        server_register_dueros_send_callbak(app_send_user_data_do);
        //dueros_send_ver();
        break;
    case ATT_CHARACTERISTIC_0000FF00_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE:
#if PRINT_DMA_DATA_EN
        log_info("-ota dma_rx(%d):", buffer_size);
        if (buffer_size < 100) {
            log_info_hexdump(buffer, buffer_size);
        }
#endif
        dma_ota_recieve_data(buffer, buffer_size);
        break;
    case ATT_CHARACTERISTIC_c2e758b9_0e78_41e0_b0cb_98a593193fc5_01_VALUE_HANDLE:
#if PRINT_DMA_DATA_EN
        log_info("-dma_rx(%d):", buffer_size);
        if (buffer_size < 100) {
            log_info_hexdump(buffer, buffer_size);
        }
#endif

        dma_recieve_data(buffer, buffer_size);
        break;

    /* case ATT_CHARACTERISTIC_c2e758b9_0e78_41e0_b0cb_98a593193fc5_01_CLIENT_CONFIGURATION_HANDLE:
         att_set_ccc_config(handle, buffer[0]);
         break;
    */
    default:
        break;
    }

    return 0;
}




extern int dma_update_tws_state_to_lib(int state);
static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type)
{
    u32 ret = APP_BLE_NO_ERROR;
    if (!con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    ret = ble_user_cmd_prepare(BLE_CMD_ATT_SEND_DATA, 4, handle, data, len, handle_type);
    if (ret == BLE_BUFFER_FULL) {
        ret = APP_BLE_BUFF_FULL;
    } else {
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_SEND_PREPARE_DONE);
    }
    if (ret) {
        log_info("app_send_fail:%d !!!!!!\n", ret);
    }
    return ret;
}


//------------------------------------------------------
extern const u8 *bt_get_mac_addr();
//------------------------------------------------------
static int make_set_adv_data(void)
{
    u8 *buf = adv_data;
    u8 offset = adv_data_length;

#if USE_OWN_ADV_DATA
    static const u8 dueros_dma_uuid_16bit[] = {0xC2, 0xFD};
    u8 dma_manufacturer_data[10] = {0xD6, 0x05, 0xFD, 0xC2};
    if (adv_data_length == 0) {
        swapX(bt_get_mac_addr(), &dma_manufacturer_data[4], 6);
        /* offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, 0x1a, 1); */
        offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, 0x06, 1);
        offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MORE_16BIT_SERVICE_UUIDS, (void *)dueros_dma_uuid_16bit, 2);
        offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA, (void *)dma_manufacturer_data, sizeof(dma_manufacturer_data));

    }
#endif
    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***adv_data overflow!!!!!!\n");
        return -1;
    }
    //r_printf("adv_data(%d):", offset);
    //log_info_hexdump(buf, offset);
    ble_user_cmd_prepare(BLE_CMD_ADV_DATA, 2, offset, buf);
    return 0;
}

static int make_set_rsp_data(void)
{
    u8 offset = scan_rsp_data_len;
    u8 *buf = scan_rsp_data;
    u8 name_len = gap_device_name_len;
    u8 vaild_len = ADV_RSP_PACKET_MAX - (offset + 2);
    if (name_len > vaild_len) {
        name_len = vaild_len;
    }
#if USE_OWN_ADV_DATA
    if (adv_data_length == 0) {
        offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)gap_device_name, name_len);
    }
#endif
    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return -1;
    }

    //log_info("rsp_data(%d):", offset);
    //log_info_hexdump(buf, offset);
    ble_user_cmd_prepare(BLE_CMD_RSP_DATA, 2, offset, buf);
    return 0;
}

#define BLE_ADV_TYPE_NORMAL		     0
#define BLE_ADV_TYPE_IBEACON	     1
static u8 ble_adv_type = BLE_ADV_TYPE_NORMAL;
static u16 ble_ibeacon_adv_timer_hdl = 0;
static u16 ble_ibeacon_adv_timerout_hdl = 0;
static void ble_ibeacon_timer_close(void);

static int make_set_adv_data_ibeacon(void)
{
    u8 offset = dma_ibeacon_data_len;
    u8 *buf = dma_ibeacon_data;
    if (offset == 0) {
        return -1;
    }
    if (offset > ADV_RSP_PACKET_MAX) {
        printf("***adv_data overflow!!!!!! %d\n", offset);
        return -1;
    }
    ble_user_cmd_prepare(BLE_CMD_ADV_DATA, 2, offset, buf);
    return 0;
}

static void ble_ibeacon_adv_timeout(void *priv)
{
    ble_ibeacon_timer_close();

    printf(">>>>>>>ble_ibeacon_adv_timeout \n");
    ///close ble adv
    bt_ble_adv_enable(0);
    ///switch adv data packet
    ble_adv_type = BLE_ADV_TYPE_NORMAL;
    ///open ble adv
    bt_ble_adv_enable(1);
}
static void ble_ibeacon_adv_timer(void *priv)
{
    static int adv_type = 0;
    if ((check_can_send_data_or_not()) || (BT_CALL_HANGUP != get_call_status())) {
        ///ble connect or adv close
        printf(">>>>>>>>>close ibeacon timer \n");
        ble_ibeacon_timer_close();
        ble_adv_type = BLE_ADV_TYPE_NORMAL;
        bt_ble_adv_enable(0);
        return;
    }

    adv_type ++;
    ///close ble adv
    //50ms后关，下50ms开A，再下50ms来关，再下50ms来开B，如此循环
    if ((adv_type == 1) || (adv_type == 3)
        || (ble_work_state == BLE_ST_CONNECT)) {
        //有时候设置界面连上了之后，没有连baidu的UUID，也要先临时关闭广播
        putchar('$');
        bt_ble_adv_enable(0);
        return ;
    } else if (adv_type == 2) {
        putchar('B');
        ble_adv_type = BLE_ADV_TYPE_IBEACON;
    } else if (adv_type >= 4) {
        putchar('A');
        adv_type = 0;
        ble_adv_type = BLE_ADV_TYPE_NORMAL;
    }

    ///open ble adv
    bt_ble_adv_enable(1);

    ///reset status
    ble_adv_type = BLE_ADV_TYPE_NORMAL;
}

static void ble_ibeacon_timer_init(void)
{
    ///active timer,250ms
    if (ble_ibeacon_adv_timer_hdl != 0) {
        return;
    }

    ble_ibeacon_adv_timer_hdl = sys_timer_add(NULL, ble_ibeacon_adv_timer, 250);
    ble_ibeacon_adv_timerout_hdl = sys_timeout_add(NULL, ble_ibeacon_adv_timeout, 150000);
}

static void ble_ibeacon_timer_close(void)
{
    if (ble_ibeacon_adv_timer_hdl) {
        sys_timer_del(ble_ibeacon_adv_timer_hdl);
        ble_ibeacon_adv_timer_hdl = 0;

        sys_timeout_del(ble_ibeacon_adv_timerout_hdl);
        ble_ibeacon_adv_timerout_hdl = 0;
    }
}

void dma_ble_ibeacon_adv(u8 sw)
{
    if (sw && dma_pair_state()) {
        printf("already connect app , return enable adv\n");
        return;
    }
    if (sw && !dma_check_tws_is_master()) {
        return;
    }
    if (sw) {
        if ((check_can_send_data_or_not()) || (BT_CALL_HANGUP != get_call_status())
            || (dma_ibeacon_data_len == 0)) {
            printf("can't open ibeacon...\n");
            return;
        }
        ble_ibeacon_timer_init();
    } else {
        ble_ibeacon_timer_close();
    }
}

_INLINE_
bool ble_ibeacon_is_active(void)
{
    return (ble_ibeacon_adv_timer_hdl != 0);
}

//广播参数设置
static void advertisements_setup_init()
{
    uint8_t adv_type = 0;
    uint8_t adv_channel = 7;
    int   ret = 0;

    ble_user_cmd_prepare(BLE_CMD_ADV_PARAM, 3, ADV_INTERVAL_MIN, adv_type, adv_channel);

    if (ble_adv_type == BLE_ADV_TYPE_NORMAL) {
        ret |= make_set_adv_data();
    } else {
        ret |= make_set_adv_data_ibeacon();
    }

    ret |= make_set_rsp_data();

    if (ret) {
        printf("advertisements_setup_init fail !!!!!!\n");
        return;
    }

}


static void dma_sm_setup_init(io_capability_t io_type, u8 auth_req, uint8_t min_key_size, u8 security_en)
{
    //setup SM: Display only
    sm_init();
    sm_set_io_capabilities(io_type);
    sm_set_authentication_requirements(auth_req);
    sm_set_encryption_key_size_range(min_key_size, 16);
    sm_set_request_security(security_en);
    sm_event_callback_set(&cbk_sm_packet_handler);
}


//配对加密使能
extern void le_device_db_init(void);
void dma_ble_profile_init(void)
{
    printf("ble profile init\n");
    con_handle = 0;
    le_device_db_init();
    dma_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_BONDING, 7, DMA_BLE_SECURITY_EN);
    /* setup ATT server */
    att_server_init(dma_profile_data, att_read_callback, att_write_callback);
    att_server_register_packet_handler(cbk_packet_handler);

    // register for HCI events
    hci_event_callback_set(&cbk_packet_handler);
    le_l2cap_register_packet_handler(&cbk_packet_handler);
}



static int set_adv_enable(void *priv, u32 en)
{
    ble_state_e next_state, cur_state;

    if (!adv_ctrl_en) {
        return APP_BLE_OPERATION_ERROR;
    }

    if (con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    if (en) {
        next_state = BLE_ST_ADV;
    } else {
        next_state = BLE_ST_IDLE;
    }

    cur_state =  get_ble_work_state();
    switch (cur_state) {
    case BLE_ST_ADV:
    case BLE_ST_IDLE:
    case BLE_ST_INIT_OK:
    case BLE_ST_NULL:
    case BLE_ST_DISCONN:
        break;
    default:
        return APP_BLE_OPERATION_ERROR;
        break;
    }

    if (cur_state == next_state) {
        return APP_BLE_NO_ERROR;
    }
    //log_info("adv_en:%d\n", en);
    putchar('X' + en);
    set_ble_work_state(next_state);
    if (en) {
        advertisements_setup_init();
    }
    ble_user_cmd_prepare(BLE_CMD_ADV_ENABLE, 1, en);
    return APP_BLE_NO_ERROR;
}

static int ble_disconnect(void *priv)
{
    if (con_handle) {
        if (BLE_ST_SEND_DISCONN != get_ble_work_state()) {
            log_info(">>>ble send disconnect\n");
            set_ble_work_state(BLE_ST_SEND_DISCONN);
            ble_user_cmd_prepare(BLE_CMD_DISCONNECT, 1, con_handle);
        } else {
            log_info(">>>ble wait disconnect...\n");
        }
        return APP_BLE_NO_ERROR;
    } else {
        return APP_BLE_OPERATION_ERROR;
    }
}

static int get_buffer_vaild_len(void *priv)
{
    u32 vaild_len = 0;
    ble_user_cmd_prepare(BLE_CMD_ATT_VAILD_LEN, 1, &vaild_len);
    return vaild_len;
}

static int app_send_user_data_do(void *priv, u8 *data, u16 len)
{
#if PRINT_DMA_DATA_EN
    if (len < 128) {
        log_info("-dma_tx(%d):", len);
        log_info_hexdump(data, len);
    } else {
        log_info("-dma_tx(%d):", len);
        //putchar('L');
    }
#endif
    return app_send_user_data(ATT_CHARACTERISTIC_b84ac9c6_29c5_46d4_bba1_9d534784330f_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
}
static int ota_send_user_data_do(void *priv, u8 *data, u16 len)
{
#if PRINT_DMA_DATA_EN
    if (len < 128) {
        log_info("-ota dma_tx(%d):", len);
        log_info_hexdump(data, len);
    } else {
        log_info("-ota dma_tx(%d):", len);
        //putchar('L');
    }
#endif
    return app_send_user_data(ATT_CHARACTERISTIC_0000FF00_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
}

static int app_send_user_data_check(u16 len)
{
    u32 buf_space = get_buffer_vaild_len(0);
    if (len <= buf_space) {
        return 1;
    }
    putchar('@');
    return 0;
}

static void bt_ble_adv_enable(u8 enable)
{
    set_adv_enable(0, enable);
}

static void ble_module_enable(u8 en)
{
    log_info("mode_en:%d\n", en);
    if (en) {
        adv_ctrl_en = 1;
        bt_ble_adv_enable(1);
    } else {
        if (con_handle) {
            adv_ctrl_en = 0;
            ble_disconnect(NULL);
        } else {
            bt_ble_adv_enable(0);
            adv_ctrl_en = 0;
        }
    }
}

void dma_ble_adv_enable(u8 enable)
{
    if (enable && dma_pair_state()) {
        printf("already connect app , return enable adv\n");
        return;
    }
    if (dma_check_tws_is_master()) {
        set_adv_enable(0, enable);
        dma_ble_ibeacon_adv(enable);
    } else {
        set_adv_enable(0, 0);
        dma_ble_ibeacon_adv(0);
    }
}
void dma_ble_init(void)
{
    log_info("***** ble_init******\n");

    gap_device_name = bt_get_local_name();
    gap_device_name_len = strlen(gap_device_name);
    if (gap_device_name_len > BT_NAME_LEN_MAX) {
        gap_device_name_len = BT_NAME_LEN_MAX;
    }

    log_info("ble name(%d): %s \n", gap_device_name_len, gap_device_name);

    set_ble_work_state(BLE_ST_INIT_OK);
    ble_module_enable(1);
    bt_ble_adv_enable(0);
    ///reset ble mac address
    le_controller_set_mac(bt_get_mac_addr());
}

void dma_ble_exit(void)
{
    log_info("***** ble_exit******\n");
    ble_module_enable(0);
}

void dma_ble_disconnect(void)
{
    ble_disconnect(NULL);
}

void dma_set_module(bool state)
{
    dma_module_en = state;
}

bool get_dma_module(void)
{
    return dma_module_en;
}

bool check_dma_le_handle(void)
{
    if (con_handle) {
        return true;
    } else {
        return false;
    }
}
extern void dma_spp_disconnect(void);
int dma_disconnect(void *addr)
{
    dma_ble_disconnect();
    dma_spp_disconnect();
    return 0;
}



