#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_protocol_api.h"
#include "system/includes.h"
#include "vm.h"
#include "audio_config.h"
#include "app_power_manage.h"
#include "user_cfg.h"
#include "app_main.h"
#include "bt_tws.h"
#include "btstack/avctp_user.h"
#if APP_PROTOCOL_DMA_CODE

#if 1
#define APP_DMA_LOG       printf
#define APP_DMA_DUMP      put_buf
#else
#define APP_DMA_LOG(...)
#define APP_DMA_DUMP(...)
#endif

//=================================================================================//
//                             用户自定义配置项[1 ~ 49]                            //
//=================================================================================//
//#define 	CFG_USER_DEFINE_BEGIN		1
#define VM_DMA_NAME_EXT                  3
#define VM_DMA_REMOTE_LIC                4
//#define 	CFG_USER_DEFINE_END			49


#define APP_DMA_VERSION             0x00020004 //app十进制显示 xx.xx.xx.xx

#define BT_NAME_EXT_EN          0//使用sn码作为蓝牙后缀名，用于区分耳机和给app回连
//*********************************************************************************//
//                                 DMA认证信息                                     //
//*********************************************************************************//
#define DMA_PRODUCT_INFO_EN         1 //三元组使能
#define DMA_PRODUCT_INFO_TEST       1
#define DMA_PRODUCT_READ_FROM_FILE  0

#if DMA_PRODUCT_INFO_EN
u8 use_triad_info = 1;
#else
u8 use_triad_info = 0;
#endif // DMA_PRODUCT_INFO_EN
/* 流控功能使能 */
#define ATT_DATA_RECIEVT_FLOW           0

#if DMA_PRODUCT_INFO_TEST
static const char *dma_product_id  = "ojCAw7een53mBQ0abM2tSCxV8YqDbLR8";
static const char *dma_triad_id    = "0024eakS0000000700000005";
static const char *dma_secret      = "1854032dc9f7f755";
#endif
static const char *dma_product_key = "jfyWVswabEAdq1hGoyZxjfztAxU9DSz3";

#define DMA_PRODUCT_ID_LEN      65
#define DMA_PRODUCT_KEY_LEN     65
#define DMA_TRIAD_ID_LEN        32
#define DMA_SECRET_LEN          32

#define DMA_LEGAL_CHAR(c)       ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))

static u16 dma_get_one_info(const u8 *in, u8 *out)
{
    int read_len = 0;
    const u8 *p = in;

    while (DMA_LEGAL_CHAR(*p) && *p != ',') { //read Product ID
        *out++ = *p++;
        read_len++;
    }
    return read_len;
}

u8 read_dma_product_info_from_flash(u8 *read_buf, u16 buflen)
{
    u8 *rp = read_buf;
    const u8 *dma_ptr = (u8 *)app_protocal_get_license_ptr();

    printf("%s", __func__);
    if (dma_ptr == NULL) {
        return FALSE;
    }

    if (dma_get_one_info(dma_ptr, rp) != 32) {
        return FALSE;
    }
    dma_ptr += 33;

    rp = read_buf + DMA_PRODUCT_ID_LEN;
    memcpy(rp, dma_product_key, strlen(dma_product_key));

    rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN;
    if (dma_get_one_info(dma_ptr, rp) != 24) {
        return FALSE;
    }
    dma_ptr += 25;

    rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN;
    if (dma_get_one_info(dma_ptr, rp) != 16) {
        return FALSE;
    }

    return TRUE;
}
__attribute__((weak))
void bt_update_testbox_addr(u8 *addr)
{

}

bool dueros_dma_get_manufacturer_info(u8 *read_buf, u16 len)
{
    bool ret = FALSE;

    APP_DMA_LOG("%s\n", __func__);

#if DMA_PRODUCT_INFO_TEST
    memcpy(read_buf, dma_product_id, strlen(dma_product_id));
    memcpy(read_buf + DMA_PRODUCT_ID_LEN, dma_product_key, strlen(dma_product_key));
    memcpy(read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN, dma_triad_id, strlen(dma_triad_id));
    memcpy(read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN, dma_secret, strlen(dma_secret));
    ret = TRUE;
#else
    ret = read_dma_product_info_from_flash(read_buf, sizeof(read_buf));
#endif
    return ret;
}

static bool dma_tws_get_manufacturer_info(u8 *read_buf, u16 len)
{
    u16 i = 0;
    bool ret = FALSE;

    if (tws_api_get_local_channel() == 'L') {
        return ret;
    }
    if (syscfg_read(VM_DMA_REMOTE_LIC, read_buf, len) == len) {
        for (i = 0; i < len; i++) {
            if (read_buf[i] != 0xff) {
                ret = TRUE;
                break;
            }
        }
    }
    return ret;
}

u8 dma_get_hex(u8 cha)
{
    u8 num = 0;
    if (cha >= '0' && cha <= '9') {
        num = cha - '0';
    } else if (cha >= 'a' && cha <= 'f') {
        num = cha - 'a' + 10;
    } else if (cha >= 'A' && cha <= 'F') {
        num = cha - 'A' + 10;
    }
    return num;
}

u8 dueros_dma_read_from_file(u8 *mac, u8 *read_buf, u16 len)
{
    u8 i = 0;
    u8 mac_tmp[6];
    u8 rbuf[256];
    u16 offset = 0;
    u8 *rp = read_buf;
    u8 *dma_ptr;
    u8 product_key[32];
    APP_DMA_LOG("read license from file\n");
    if (read_cfg_file(rbuf, sizeof(rbuf), SDFILE_RES_ROOT_PATH"dma.txt") == TRUE) {
        printf("%s", rbuf);
        for (i = 0; i < sizeof(mac_tmp); i++) {
            mac_tmp[i] = (dma_get_hex(rbuf[offset]) << 4) | dma_get_hex(rbuf[offset + 1]);
            offset += 3;
        }
        put_buf(mac_tmp, sizeof(mac_tmp));
        dma_ptr = rbuf + offset;

        if (dma_get_one_info(dma_ptr, product_key) != 32) {
            return FALSE;
        }
        dma_ptr += 33;

        if (dma_get_one_info(dma_ptr, rp) != 32) {
            return FALSE;
        }
        dma_ptr += 33;

        rp = read_buf + DMA_PRODUCT_ID_LEN;
        memcpy(rp, product_key, sizeof(product_key));

        rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN;
        if (dma_get_one_info(dma_ptr, rp) != 24) {
            return FALSE;
        }
        dma_ptr += 25;

        rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN;
        if (dma_get_one_info(dma_ptr, rp) != 16) {
            return FALSE;
        }
        memcpy(mac, mac_tmp, 6);
        return TRUE;
    }
    return FALSE;
}

void dma_set_local_version(u32 version);
void dueros_dma_manufacturer_info_init()
{
    dma_set_local_version(APP_DMA_VERSION);
#if DMA_PRODUCT_INFO_EN
    u8 mac[] = {0xF4, 0x43, 0x8D, 0x29, 0x17, 0x04};

    u8 read_buf[DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN + DMA_SECRET_LEN + 1] = {0};
    bool ret = FALSE;

    APP_DMA_LOG("dueros_dma_manufacturer_info_init\n");

    ret = dma_tws_get_manufacturer_info(read_buf, sizeof(read_buf));
    if (ret == FALSE) {
        ret = dueros_dma_get_manufacturer_info(read_buf, sizeof(read_buf));
    }
#if DMA_PRODUCT_READ_FROM_FILE
    if (ret == FALSE) {
        ret = dueros_dma_read_from_file(mac, read_buf, sizeof(read_buf));
    }
#endif

    if (ret == TRUE) {
        APP_DMA_LOG("read license success\n");
        APP_DMA_LOG("product id: %s\n", read_buf);
        APP_DMA_LOG("product key: %s\n", read_buf + DMA_PRODUCT_ID_LEN);
        APP_DMA_LOG("triad id: %s\n", read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN);
        APP_DMA_LOG("secret: %s\n", read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN);
        app_protocol_set_info_group(DMA_HANDLER_ID, read_buf);
        /* } else { */
        /*     app_protocol_set_info_group(DMA_HANDLER_ID, NULL); */
    }

#if 1
    u8 ble_mac[6];
    void bt_update_mac_addr(u8 * addr);
    void lmp_hci_write_local_address(const u8 * addr);
    void bt_update_testbox_addr(u8 * addr);
    extern int le_controller_set_mac(void *addr);
    extern void lib_make_ble_address(u8 * ble_address, u8 * edr_address);
    bt_update_mac_addr(mac);
    lmp_hci_write_local_address(mac);
    bt_update_testbox_addr(mac);
    /* lib_make_ble_address(ble_mac, mac); */
    memcpy(ble_mac, mac, 6);
    le_controller_set_mac(ble_mac); //修改BLE地址
    APP_DMA_DUMP(mac, 6);
    APP_DMA_DUMP(ble_mac, 6);
#endif
#else
    u8 read_buf[DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN + DMA_SECRET_LEN + 1] = {0};
    APP_DMA_LOG("dueros_dma_manufacturer_info_init\n");

    dueros_dma_get_manufacturer_info(read_buf, sizeof(read_buf));
    APP_DMA_LOG("read license success\n");
    APP_DMA_LOG("product id: %s\n", read_buf);
    APP_DMA_LOG("product key: %s\n", read_buf + DMA_PRODUCT_ID_LEN);
    app_protocol_set_info_group(DMA_HANDLER_ID, read_buf);
#endif

#if ATT_DATA_RECIEVT_FLOW
    APP_DMA_LOG("att_server_flow_enable\n");
    att_server_flow_enable(1);
#endif
}

//*********************************************************************************//
//                                 DMA提示音                                       //
//*********************************************************************************//
/**
* @brief 小度自定义提示音ID
*/
typedef enum {
    DMA_TTS_WAITING_DMA_CONNECTING_0_TIMES_NEVER_CONNECTED = 0,         /**< 耳机首次与手机配对 */
    DMA_TTS_WAITING_DMA_CONNECTING_6_TIMES_NEVER_CONNECTED,             /**< 始终未连接APP，第6次配对时 */
    DMA_TTS_WAITING_DMA_CONNECTING_12_TIMES_NEVER_CONNECTED,            /**< 始终未连接APP，第12次配对时 */
    DMA_TTS_WAITING_DMA_CONNECTING_18or24or50n_TIMES_NEVER_CONNECTED,   /**< 始终未连接APP，第18、24、50次配对时*/
    DMA_TTS_WAITING_DMA_CONNECTING_MORETHAN_50_TIMES_AFTER_LASTCONNECT, /**< 始终未连接APP，第50次配对后，每50次配对时*/
    DMA_TTS_USE_XIAODU_APP,                   /**< 请连接小度APP使用语音功能*/
    DMA_TTS_KEYWORD_DETECTED,                 /**< 在呢*/
    DMA_TTS_WAITING_FOR_DMA_CONNECT,          /**< 正在连接小度APP */
    DMA_TTS_WAITING_FOR_DMA_CONNECT_IGNORE,   /**< 请稍候 */
    DMA_TTS_OPEN_APP_FAIL,                    /**< 启动失败，请手动打开小度APP */

    DMA_TTS_NUM,
} DMA_TTS_ID;

static const char *dma_tts_tab[DMA_TTS_NUM] = {
    [DMA_TTS_WAITING_DMA_CONNECTING_0_TIMES_NEVER_CONNECTED]               = SDFILE_APP_ROOT_PATH"TON/tone/app_wait0.*",
    [DMA_TTS_WAITING_DMA_CONNECTING_6_TIMES_NEVER_CONNECTED]               = SDFILE_APP_ROOT_PATH"TON/tone/app_wait6.*",
    [DMA_TTS_WAITING_DMA_CONNECTING_12_TIMES_NEVER_CONNECTED]              = SDFILE_APP_ROOT_PATH"TON/tone/app_wait12.*",
    [DMA_TTS_WAITING_DMA_CONNECTING_18or24or50n_TIMES_NEVER_CONNECTED]     = SDFILE_APP_ROOT_PATH"TON/tone/app_wait6.*",
    [DMA_TTS_WAITING_DMA_CONNECTING_MORETHAN_50_TIMES_AFTER_LASTCONNECT]   = SDFILE_APP_ROOT_PATH"TON/tone/app_wait100.*",
    [DMA_TTS_USE_XIAODU_APP]                                               = SDFILE_APP_ROOT_PATH"TON/tone/xd_key_fail.*",
    [DMA_TTS_KEYWORD_DETECTED]                                             = SDFILE_APP_ROOT_PATH"TON/tone/xd_key_succ.*",
    [DMA_TTS_WAITING_FOR_DMA_CONNECT]                                      = SDFILE_APP_ROOT_PATH"TON/tone/xd_conning.*",
    [DMA_TTS_WAITING_FOR_DMA_CONNECT_IGNORE]                               = SDFILE_APP_ROOT_PATH"TON/tone/xd_waiting.*",
    [DMA_TTS_OPEN_APP_FAIL]                                                = SDFILE_APP_ROOT_PATH"TON/tone/xd_openfail.*",
};

extern u8 app_protocol_speech_wait;
__attribute__((weak))
void audio_decoder_fmt_lock_en(u8 en)
{

}
static void dma_tone_play_end_callback(void *priv, int flag)
{
    u32 index = (u32)priv;
    audio_decoder_fmt_lock_en(1);
    switch (index) {
#if (BT_MIC_EN)
#if APP_PROTOCOL_SPEECH_EN
    case DMA_TTS_KEYWORD_DETECTED:
        APP_DMA_LOG("DMA_TTS_KEYWORD_DETECTED\n");
        if (app_protocol_speech_wait) {
            app_protocol_speech_wait = 0;
            if (BT_STATUS_TAKEING_PHONE == get_bt_connect_status() || ai_mic_is_busy() || app_var.siri_stu) {
                APP_DMA_LOG("cant start speech:%d, %d, %d\n", get_bt_connect_status(), ai_mic_is_busy(), app_var.siri_stu);
                break;
            }
            if (is_tws_master_role() && app_protocol_check_connect_success()) {
                APP_DMA_LOG("app_protocol_tone play end\n");
                __app_protocol_speech_start();
            }
        }
        break;
#endif
#endif
    default:
        break;
    }
}

void bt_drop_a2dp_frame_start(void);
static void dma_tone_play_in_app_core(int index)
{
    //bt_drop_a2dp_frame_start();
    if (dma_tts_tab[index]) {
        audio_decoder_fmt_lock_en(0);
        tone_play_with_callback(dma_tts_tab[index], 1, dma_tone_play_end_callback, (void *)index);
    } else {
        dma_tone_play_end_callback((void *)index, 0);
    }
}

static int dma_tone_play_index = -1;

void app_protocol_dma_tone_play(int index, int tws_sync);

void tone_connect_play_callback(void *priv) //连接成功提示音播放回调函数
{
    printf("tone_connect_play_callback:\n");
    if (dma_tone_play_index != -1) {
        if (is_tws_master_role()) {
            app_protocol_dma_tone_play(dma_tone_play_index, 1);
        } else {
            dma_tone_play_index = -1;
        }
    }
}

#if TCFG_USER_TWS_ENABLE
#define TWS_DMA_TTS_TONE_SYNC_ID        0xFFFF321C

static void tws_dma_tone_sync_handler(int tone_index, int err)
{
    printf("%s, index:%d, err:%d", __func__, tone_index, err);
    app_protocol_dma_tone_play(tone_index, 0);
}

TWS_SYNC_CALL_REGISTER(tws_dma_tone_sync) = {
    .uuid = TWS_DMA_TTS_TONE_SYNC_ID,
    .task_name = "app_core",
    .func = tws_dma_tone_sync_handler,
};
#endif

u8 dma_tone_status_update(int index, int tws_sync)
{
    if (index >= DMA_TTS_NUM) {
        return 1;
    }
    if (index <= DMA_TTS_WAITING_DMA_CONNECTING_MORETHAN_50_TIMES_AFTER_LASTCONNECT) {
        if (app_protocol_check_connect_success()) { //经典蓝牙未连接，或者连接上APP
            printf("don't play tone:%d, app:%d", index, app_protocol_check_connect_success());
            dma_tone_play_index = -1;
            return 1; //不播放
        }
        dma_tone_play_index = -1;
    }
    if (index == DMA_TTS_KEYWORD_DETECTED) {
        APP_DMA_LOG("app_protocol_speech_wait:1");
        app_protocol_speech_wait = 1; //等待提示音播完再开启mic
    }
    return 0;
}

void app_protocol_dma_tone_play(int index, int tws_sync)
{
#if (BT_MIC_EN)
    if (app_var.goto_poweroff_flag || ai_mic_is_busy() || get_call_status() != BT_CALL_HANGUP) {
        APP_DMA_LOG("dma tone %d ignore, poweroff:%d, mic_busy:%d, calling:%d\n", index, app_var.goto_poweroff_flag, ai_mic_is_busy(), get_call_status());
        return;
    }
#endif
    if (tws_sync && app_protocol_speech_wait) {
        APP_DMA_LOG("DMA_TTS_KEYWORD_DETECTED playing, don't play");
        return;
    }
    if (tws_sync && get_bt_tws_connect_status() && is_tws_master_role()) { //让对耳同步一下提示音状态
        app_protocol_tws_send_to_sibling(DMA_TWS_TONE_INFO_SYNC, (u8 *)&index, sizeof(index));
    }
    if (dma_tone_status_update(index, tws_sync)) {
        return;
    }
#if TCFG_USER_TWS_ENABLE
    if (tws_sync && get_bt_tws_connect_status()) {
        if (is_tws_master_role()) {
            APP_DMA_LOG("dma tws_sync play index:%d\n", index);
            /* app_protocol_tws_sync_send(APP_PROTOCOL_SYNC_DMA_TONE, index); */
            tws_api_sync_call_by_uuid(TWS_DMA_TTS_TONE_SYNC_ID, index, 300);
        }
        return;
    }
#endif
    if (strcmp(os_current_task(), "app_core")) {
        APP_DMA_LOG("dma tone play in app core index:%d\n", index);
        app_protocol_post_app_core_callback((int)dma_tone_play_in_app_core, (void *)index); //提示音放到app_core播放
    } else {
        APP_DMA_LOG("dma tone play index:%d\n", index);
        dma_tone_play_in_app_core(index);
    }
}

int app_protocol_special_tone_play(int index, int tws_sync)
{
#if 0
    switch (index) {
    case APP_RROTOCOL_TONE_OPEN_APP:
        app_protocol_dma_tone_play(DMA_TTS_USE_XIAODU_APP, tws_sync);
        break;
    case APP_RROTOCOL_TONE_SPEECH_APP_START:
        app_protocol_post_bt_event(AI_EVENT_SPEECH_START, NULL);
        break;
    case APP_RROTOCOL_TONE_SPEECH_KEY_START:
        app_protocol_dma_tone_play(DMA_TTS_KEYWORD_DETECTED, tws_sync);
        break;
    }
#endif
    return 1;
}

#if 0
const char *dma_notice_tab[APP_RROTOCOL_TONE_MAX] = {
    [APP_PROTOCOL_TONE_CONNECTED_ALL_FINISH]		= TONE_RES_ROOT_PATH"tone/xd_ok.*",//所有连接完成【已连接，你可以按AI键来和我进行对话】
    [APP_PROTOCOL_TONE_PROTOCOL_CONNECTED]		= TONE_RES_ROOT_PATH"tone/xd_con.*",//小度APP已连接，经典蓝牙未连接【请在手机上完成蓝牙配对】
    [APP_PROTOCOL_TONE_CONNECTED_NEED_OPEN_APP]	= TONE_RES_ROOT_PATH"tone/xd_btcon.*",//经典蓝牙已连接，小度app未连接【已配对，请打开小度app进行连接】
    [APP_PROTOCOL_TONE_DISCONNECTED]				= TONE_RES_ROOT_PATH"tone/xd_dis.*",//经典蓝牙已断开【蓝牙已断开，请在手机上完成蓝牙配对】
    [APP_PROTOCOL_TONE_DISCONNECTED_ALL]			= TONE_RES_ROOT_PATH"tone/xd_alldis.*",//经典蓝牙和小度都断开了【蓝牙未连接，请用手机蓝牙和我连接吧】
    [APP_RROTOCOL_TONE_SPEECH_APP_START]	    	= TONE_NORMAL,
    [APP_RROTOCOL_TONE_SPEECH_KEY_START]	    	= TONE_NORMAL,
};
#endif


extern int multi_spp_send_data(u8 local_cid, u8 rfcomm_cid,  u8 *buf, u16 len);
#if (defined TCFG_USER_RSSI_TEST_EN && TCFG_USER_RSSI_TEST_EN)
extern s8 get_rssi_api(s8 *phone_rssi, s8 *tws_rssi);
void T18U_spp_send_rssi(u8 spp_id, s8 slave_rssi)
{
    s8 send_buf[8];
    send_buf[0] = 0xfe;
    send_buf[1] = 0xdc;
    send_buf[2] = 0x03;
    send_buf[3] = 0x02;
    if (tws_api_get_local_channel() == 'L') {
        get_rssi_api(&send_buf[4], &send_buf[6]);
        send_buf[5] = slave_rssi;
    } else {
        get_rssi_api(&send_buf[5], &send_buf[6]);
        send_buf[4] = slave_rssi;
    }
    send_buf[7] = 0xba;
    multi_spp_send_data(spp_id, 0, send_buf, sizeof(send_buf));
    printf("get rssi, l:%d, r:%d, tws:%d", send_buf[4], send_buf[5], send_buf[6]);
}
#endif

//*********************************************************************************//
//                                 DMA私有消息处理                                 //
//*********************************************************************************//
#if TCFG_USER_TWS_ENABLE
//固定使用左耳的三元组

#if 0
#ifdef CONFIG_NEW_BREDR_ENABLE
void tws_host_get_common_addr(u8 *remote_mac_addr, u8 *common_addr, char channel)
#else
void tws_host_get_common_addr(u8 *local_addr, u8 *remote_addr, u8 *common_addr, char channel)
#endif
{
    if (channel == 'L') {
        memcpy(common_addr, bt_get_mac_addr(), 6);
    } else {
        memcpy(common_addr, remote_mac_addr, 6);
    }
}
#endif

static void tws_conn_sync_lic()
{
    u8 read_buf[DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN + DMA_SECRET_LEN + 1] = {0};
    bool ret = FALSE;
    if (tws_api_get_local_channel() == 'L') {
        ret = dueros_dma_get_manufacturer_info(read_buf, sizeof(read_buf));
    }
    if (ret) {
        APP_DMA_LOG("dueros_dma start sync manufacturer info\n");
        app_protocol_tws_send_to_sibling(DMA_TWS_CMD_SYNC_LIC, read_buf, sizeof(read_buf));
    }
}

static void dma_tws_rx_license_deal(u8 *license, u16 len)
{
    syscfg_write(VM_DMA_REMOTE_LIC, license, len);
    if (is_tws_master_role()) {
        app_protocol_set_info_group(DMA_HANDLER_ID, license);
        app_protocol_disconnect(NULL);
        app_protocol_ble_adv_switch(0);
        if (0 == get_esco_coder_busy_flag()) {
            app_protocol_ble_adv_switch(1);
        }
    }
}

extern void dma_ota_peer_recieve_data(u8 *buf, u16 len);
extern void dma_peer_recieve_data(u8 *buf, u16 len);
static void dma_rx_tws_data_deal(u16 opcode, u8 *data, u16 len)
{
    switch (opcode) {
#if DMA_PRODUCT_INFO_EN
    case DMA_TWS_CMD_SYNC_LIC:
        APP_DMA_LOG(">>> DMA_TWS_CMD_SYNC_LIC \n");
        dma_tws_rx_license_deal(data, len);
        break;
#endif
    case DMA_TWS_LIB_INFO_SYNC:
        dma_peer_recieve_data(data, len);
        break;
    case DMA_TWS_TONE_INFO_SYNC:
        int index = -1;
        memcpy(&index, data, len);
        printf("slave sync tone index:%d", index);
        dma_tone_status_update(index, 1);
        break;
    case DMA_TWS_OTA_CMD_SYNC:
        dma_ota_peer_recieve_data(data, len);
        break;
    case DMA_TWS_OTA_INFO_SYNC:
        printf("APP_PROTOCOL_DMA_SAVE_OTA_INFO");
        put_buf(data, len);
        syscfg_write(VM_TME_AUTH_COOKIE, data, len);
        break;
#if (defined TCFG_USER_RSSI_TEST_EN && TCFG_USER_RSSI_TEST_EN)
    case DMA_TWS_SLAVE_SEND_RSSI:
        put_buf(data, len);
        T18U_spp_send_rssi(data[0], data[1]);
        break;
#endif
    }
}

static void dma_update_ble_addr()
{
    u8 comm_addr[6];

    printf("%s\n", __func__);

    tws_api_get_local_addr(comm_addr);
    le_controller_set_mac(comm_addr); //地址发生变化，更新地址
    app_protocol_ble_adv_switch(0);
    if (0 == get_esco_coder_busy_flag()) {
        //esco在用的时候开广播会影响质量
        app_protocol_ble_adv_switch(1);
    }
}

extern void dma_start_role_switch();
static void dma_bt_tws_event_handler(struct bt_event *bt)
{
    int role = bt->args[0];
    int phone_link_connection = bt->args[1];
    int reason = bt->args[2];

    switch (bt->event) {
    case TWS_EVENT_CONNECTED:
        app_protocal_update_tws_state_to_lib(USER_NOTIFY_STATE_TWS_CONNECT);
        dma_update_ble_addr();
#if DMA_PRODUCT_INFO_EN
        tws_conn_sync_lic();
#endif
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        app_protocal_update_tws_state_to_lib(USER_NOTIFY_STATE_BATTERY_LEVEL_UPDATE); //主动上报电量
        app_protocal_update_tws_state_to_lib(USER_NOTIFY_STATE_TWS_DISCONNECT);
        break;
    case TWS_EVENT_REMOVE_PAIRS:
        break;
    case TWS_EVENT_ROLE_SWITCH_START:
        dma_start_role_switch();
        break;
    }
}
static void app_protocol_dma_tws_sync_deal(int cmd, int value)
{
    switch (cmd) {
    case APP_PROTOCOL_SYNC_DMA_TONE:
        APP_DMA_LOG("APP_PROTOCOL_SYNC_DMA_TONE:%d\n", value);
        app_protocol_dma_tone_play(value, 0);
        break;
    }
}
#endif

void bt_tws_dma_ota_fail(void);
static int dma_bt_connction_status_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
        printf("phone disconned, cannel dma tone");
        dma_tone_play_index = -1;
        break;
    case BT_STATUS_PHONE_INCOME:
    case BT_STATUS_PHONE_OUT:
    case BT_STATUS_PHONE_ACTIVE:
    case BT_STATUS_VOICE_RECOGNITION:
        //bt_tws_dma_ota_fail();
        break;
    }
    return 0;
}

int dma_sys_event_deal(struct sys_event *event)
{
    switch (event->type) {
    case SYS_BT_EVENT:
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
            dma_bt_connction_status_event_handler(&event->u.bt);
        }
#if TCFG_USER_TWS_ENABLE
        else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
            dma_bt_tws_event_handler(&event->u.bt);
        }
#endif
        break;

    }

    return 0;

}

struct app_protocol_private_handle_t dma_private_handle = {
#if TCFG_USER_TWS_ENABLE
    .tws_rx_from_siblling = dma_rx_tws_data_deal,
#endif
    .tws_sync_func = app_protocol_dma_tws_sync_deal,
    .sys_event_handler = dma_sys_event_deal,
};


extern u32 lmp_private_get_tx_remain_buffer();
void platform_ota_send_custom_info_to_peer(uint16_t param_size, uint8_t *param_buf)
{
    u8 retry_cnt = 0;
    int error_code = 0;
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        putchar('r');
        return;
    }
__try_again:
    error_code = app_protocol_tws_send_to_sibling(DMA_TWS_OTA_CMD_SYNC, param_buf, param_size);
    printf("%s--%d , error %d\n", __func__, param_size, error_code);
    printf("OTA remain_buffer = 0x%x", lmp_private_get_tx_remain_buffer());
    if (error_code != 0) {
        os_time_dly(5);
        retry_cnt ++;

        if (retry_cnt > 35) {
            printf(">>>>>>>>>>tws send slave faile");
            //bt_tws_dma_ota_fail();
            void app_protocol_ota_update_fail(void);
            app_protocol_ota_update_fail();
            return;
        }

#if ATT_DATA_RECIEVT_FLOW
        /* flow control */
        extern void app_protocol_flow_control();
        app_protocol_flow_control();
#endif

        goto __try_again;
    }
}
void dma_start_role_switch()
{
    app_protocol_ble_adv_switch(0);
    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
        if (is_tws_master_role()) {
            printf("master dev will role switch\n");
            app_protocal_update_tws_state_to_lib(USER_NOTIFY_STATE_ROLE_SWITCH_START);
            /* os_time_dly(10); */
        }
    }
}

char *dma_get_local_name_ext(char *ext)
{
    const u8 *sn_addr = app_protocal_get_license_ptr();
    if (sn_addr != NULL) {
        if (ext) {
            memcpy(ext, sn_addr + 16, 4);
        }
        return sn_addr + 16;
    }
    return NULL;
}

int update_bt_name_ext(u8 *name_ext, int ext_len);
void dma_save_remote_name_ext(const u8 *name_ext, u8 len)
{
    APP_DMA_LOG("remote name ext:");
    APP_DMA_DUMP(name_ext, len);

    syscfg_write(VM_DMA_NAME_EXT, name_ext, len);
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_local_channel() == 'L') {
        update_bt_name_ext(name_ext, len);
    }
#endif
}

int dma_get_name_ext(char *ext)
{
    int ret = 0;
    char name_ext[4];
    char *p = NULL;

    if (ext == NULL) {
        return -1;
    }

#if TCFG_USER_TWS_ENABLE
    int result = 0;
    u8 addr[6];
    char channel = bt_tws_get_local_channel();
    extern u8 tws_get_sibling_addr(u8 * addr, int *result);
    ret = tws_get_sibling_addr(addr, &result);
    if (channel == 'L' && ret == 0) { //配对过，使用右耳的
        ret = syscfg_read(VM_DMA_NAME_EXT, name_ext, sizeof(name_ext));
        if (ret == sizeof(name_ext)) {
            APP_DMA_LOG("use right earphone's name");
            memcpy(ext, name_ext, sizeof(name_ext));
            return sizeof(name_ext);
        } else {
            APP_DMA_LOG("right earphone miss sn");
            return -1;
        }
    }
#endif

    p = dma_get_local_name_ext(ext);
    if (p) {
        APP_DMA_LOG("use local name ext");
        return sizeof(name_ext);
    }
    return -1;
}


#else
void dma_start_role_switch()
{
}
int dma_get_name_ext(char *ext)
{
    return -1;
}
#endif

