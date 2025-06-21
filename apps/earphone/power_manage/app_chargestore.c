#include "init.h"
#include "app_config.h"
#include "system/includes.h"
#include "asm/charge.h"
#include "asm/chargestore.h"
#include "user_cfg.h"
#include "app_chargestore.h"
#include "device/vm.h"
#include "btstack/avctp_user.h"
#include "app_power_manage.h"
#include "app_action.h"
#include "app_main.h"
#include "app_charge.h"
#include "classic/tws_api.h"
#include "update.h"
#include "bt_ble.h"
#include "bt_tws.h"
#include "app_action.h"
#include "bt_common.h"
#include "le_rcsp_adv_module.h"

#if TCFG_ANC_BOX_ENABLE
#include "app_ancbox.h"
#endif

#if TCFG_CHARGE_CALIBRATION_ENABLE
#include "app_charge_calibration.h"
#endif

#define LOG_TAG_CONST       APP_CHARGESTORE
#define LOG_TAG             "[APP_CHARGESTORE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define CMD_TWS_CHANNEL_SET         0x01
#define CMD_TWS_REMOTE_ADDR         0x02
#define CMD_TWS_ADDR_DELETE         0x03
#define CMD_BOX_TWS_CHANNEL_SEL     0x04//测试盒获取地址
#define CMD_BOX_TWS_REMOTE_ADDR     0x05//测试盒交换地址
#define CMD_POWER_LEVEL_OPEN        0x06//开盖充电舱报告/获取电量
#define CMD_POWER_LEVEL_CLOSE       0x07//合盖充电舱报告/获取电量
#define CMD_RESTORE_SYS             0x08//恢复出厂设置
#define CMD_ENTER_DUT               0x09//进入测试模式
#define CMD_EX_FIRST_READ_INFO      0x0A//F95读取数据首包信息
#define CMD_EX_CONTINUE_READ_INFO   0x0B//F95读取数据后续包信息
#define CMD_EX_FIRST_WRITE_INFO     0x0C//F95写入数据首包信息
#define CMD_EX_CONTINUE_WRITE_INFO  0x0D//F95写入数据后续包信息
#define CMD_EX_INFO_COMPLETE        0x0E//F95完成信息交换
#define CMD_TWS_SET_CHANNEL         0x0F//F95设置左右声道信息
#define CMD_BOX_UPDATE              0x20//测试盒升级
#define CMD_BOX_MODULE              0x21//测试盒一级命令


#define CMD_SHUT_DOWN               0x80//充电舱关机,充满电关机,或者是低电关机
#define CMD_CLOSE_CID               0x81//充电舱盒盖
#define CMD_ANC_MODULE              0x90//ANC一级命令
#define CMD_CALIBRATION_MODULE      0x91//充电电流校准设备一级命令
#define CMD_FAIL                    0xfe//失败
#define CMD_UNDEFINE                0xff//未知命令回复

//testbox sub cmd
#define CMD_BOX_BT_NAME_INFO		0x01 //获取蓝牙名
#define CMD_BOX_SDK_VERSION			0x02 //获取sdk版本信息
#define CMD_BOX_BATTERY_VOL			0x03 //获取电量
#define CMD_BOX_ENTER_DUT			0x04 //进入dut模式
#define CMD_BOX_FAST_CONN			0x05 //进入快速链接
#define CMD_BOX_VERIFY_CODE			0x06 //获取校验码
#define CMD_BOX_ENTER_STORAGE_MODE  0x0a //进入仓储模式
#define CMD_BOX_GLOBLE_CFG			0x0b //测试盒配置命令(测试盒收到CMD_BOX_TWS_CHANNEL_SEL命令后发送,需使能测试盒某些配置)
#define CMD_BOX_GET_TWS_PAIR_INFO	0x0c //测试盒获取配对信息
#define CMD_BOX_CUSTOM_CODE			0xf0 //客户自定义命令处理

struct chargestore_info {
    int timer;
    int shutdown_timer;
    u8 version;
    u8 chip_type;
    u8 max_packet_size;//充电舱端一包的最大值
    u8 bt_init_ok;//蓝牙协议栈初始化成功
    u8 power_level;//本机记录的充电舱电量百分比
    u8 pre_power_lvl;
    u8 sibling_chg_lev;//对耳同步的充电舱电量
    u8 power_status;//充电舱供电状态 0:断电 5V不在线 1:升压 5V在线
    u8 cover_status;//充电舱盖子状态 0:合盖 1:开盖
    u8 testbox_status;//测试盒在线状态
    u8 connect_status;//通信成功
    u8 ear_number;//盒盖时耳机在线数
    u8 channel;//左右
    u8 tws_power;//对耳的电量
    u8 power_sync;//第一次获取到充电舱电量时,同步到对耳
    u8 pair_flag;//配对标记
    u8 close_ing;//等待合窗超时
    u8 active_disconnect;//主动断开连接
    u8 event_hdl_flag;
    u8 switch2bt;
    volatile u8 keep_tws_conn_flag; //保持tws连接标志
    volatile u32 global_cfg;       //全局配置信息,控制耳机在/离仓行为
    u8 tws_paired_flag;
};

enum {
    UPDATE_MASK_FLAG_INQUIRY_VBAT_LEVEL = 0,
    UPDATE_MASK_FLAG_INQUIRY_VERIFY_CODE,
    UPDATE_MASK_FLAG_INQUIRY_SDK_VERSION,
};

static const u32 support_update_mask = BIT(UPDATE_MASK_FLAG_INQUIRY_VBAT_LEVEL) | \
                                       BIT(UPDATE_MASK_FLAG_INQUIRY_SDK_VERSION) | \
                                       BIT(UPDATE_MASK_FLAG_INQUIRY_VERIFY_CODE);

enum {
    BOX_CFG_SOFTPWROFF_AFTER_PAIRED_BIT = 0,		//测试盒配对完耳机关机
    BOX_CFG_TOUCH_TRIM_OUT_OF_BOX_BIT = 1,		//离仓执行trim操作
    BOX_CFG_TRIM_START_TIME_BIT = 2,	//离仓多久trim,unit:2s (n+1)*2,即(2~8s)

};

#define BOX_CFG_BITS_GET_FROM_MASK(mask,index,len)  (((mask & BIT(index))>>index) & (0xffffffff>>(32-len)))
extern void sys_enter_soft_poweroff(void *priv);
//extern void bt_tws_cancle_wait_pair();
extern int tws_api_cancle_wait_pair_internal();
extern int tws_cancle_create_connection_internal();
extern void tws_api_set_connect_aa_allways(u32 connect_aa);
extern u32 tws_le_get_pair_aa(void);
extern u32 tws_le_get_connect_aa(void);
extern u32 tws_le_get_search_aa(void);
extern void bt_page_scan_for_test(u8 inquiry_en);
extern u8 get_jl_chip_id(void);
extern u8 get_jl_chip_id2(void);
extern void bt_get_vm_mac_addr(u8 *addr);
extern bool get_tws_sibling_connect_state(void);
extern bool get_tws_phone_connect_state(void);
extern void local_irq_disable();
extern void cpu_reset();
extern u32 dual_bank_update_exist_flag_get(void);
extern u32 classic_update_task_exist_flag_get(void);
extern void set_temp_link_key(u8 *linkkey);
extern char tws_api_get_local_channel();
extern u16 bt_get_tws_device_indicate(u8 *tws_device_indicate);
extern void bt_tws_remove_pairs();
extern void bt_bredr_enter_dut_mode(u8 mode, u8 inquiry_scan_en);

#if TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE || TCFG_ANC_BOX_ENABLE || TCFG_CHARGE_CALIBRATION_ENABLE

#define WRITE_LIT_U16(a,src)   {*((u8*)(a)+1) = (u8)(src>>8); *((u8*)(a)+0) = (u8)(src&0xff); }
#define WRITE_LIT_U32(a,src)   {*((u8*)(a)+3) = (u8)((src)>>24);  *((u8*)(a)+2) = (u8)(((src)>>16)&0xff);*((u8*)(a)+1) = (u8)(((src)>>8)&0xff);*((u8*)(a)+0) = (u8)((src)&0xff);}
#define READ_LIT_U32(a)   		(*((u8*)(a))  + (*((u8*)(a)+1)<<8) + (*((u8*)(a)+2)<<16) + (*((u8*)(a)+3)<<24))
static struct chargestore_info info;
#define __this  (&info)
static u8 send_buf[36];
static u8 local_packet[36];
static CHARGE_STORE_INFO read_info, write_info;
static u8 read_index, write_index;


u8 testbox_get_touch_trim_en(u8 *sec)
{
    if (sec) {
        u8 sec_bits = BOX_CFG_BITS_GET_FROM_MASK(__this->global_cfg, BOX_CFG_TRIM_START_TIME_BIT, 2);
        *sec = (sec_bits + 1) * 2;
    }

    return BOX_CFG_BITS_GET_FROM_MASK(__this->global_cfg, BOX_CFG_TOUCH_TRIM_OUT_OF_BOX_BIT, 1);
}

u8 testbox_get_softpwroff_after_paired(void)
{
    return BOX_CFG_BITS_GET_FROM_MASK(__this->global_cfg, BOX_CFG_SOFTPWROFF_AFTER_PAIRED_BIT, 1);
}
#if TCFG_CHARGESTORE_ENABLE
void chargestore_clean_tws_conn_info(u8 type)
{
    CHARGE_STORE_INFO charge_store_info;
    log_info("chargestore_clean_tws_conn_info=%d\n", type);
    if (type == TWS_DEL_TWS_ADDR) {
        log_info("TWS_DEL_TWS_ADDR\n");
    } else if (type == TWS_DEL_PHONE_ADDR) {
        log_info("TWS_DEL_PHONE_ADDR\n");
    } else if (type == TWS_DEL_ALL_ADDR) {
        log_info("TWS_DEL_ALL_ADDR\n");
    }
    memset(&charge_store_info, 0xff, sizeof(CHARGE_STORE_INFO));
    syscfg_write(CFG_TWS_REMOTE_ADDR, charge_store_info.tws_remote_addr, sizeof(charge_store_info.tws_remote_addr));
}
#endif

static u8 chargestore_crc8(u8 *ptr, u8 len)
{
    u8 i, crc;
    crc = 0;
    while (len--) {
        crc ^= *ptr++;
        for (i = 0; i < 8; i++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0x8c;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

#if TCFG_USER_TWS_ENABLE


bool chargestore_set_tws_remote_info(u8 *data, u8 len)
{
    u8 i;
    bool ret = true;
    int w_ret = 0;
    u8 remote_addr[6];
    u8 common_addr[6];
    u8 local_addr[6];

    CHARGE_STORE_INFO *charge_store_info = (CHARGE_STORE_INFO *)data;
    if (len > sizeof(CHARGE_STORE_INFO)) {
        log_error("len err\n");
        return false;
    }
    //set remote addr
    syscfg_read(CFG_TWS_REMOTE_ADDR, remote_addr, sizeof(remote_addr));


    if (memcmp(remote_addr, charge_store_info->tws_local_addr, sizeof(remote_addr))) {
        ret = false;
    }


    if (sizeof(remote_addr) != syscfg_write(CFG_TWS_REMOTE_ADDR, charge_store_info->tws_local_addr, sizeof(remote_addr))) {
        w_ret = -1;
    }

    bt_get_tws_local_addr(local_addr);


#if (CONFIG_TWS_COMMON_ADDR_SELECT != CONFIG_TWS_COMMON_ADDR_USED_LEFT)
    //set common addr
    syscfg_read(CFG_TWS_COMMON_ADDR, common_addr, sizeof(common_addr));

    for (i = 0; i < sizeof(common_addr); i++) {
        if (common_addr[i] != (u8)(local_addr[i] + charge_store_info->tws_local_addr[i])) {
            ret = false;
        }
        common_addr[i] = local_addr[i] + charge_store_info->tws_local_addr[i];
    }

    if (sizeof(common_addr) != syscfg_write(CFG_TWS_COMMON_ADDR, common_addr, sizeof(common_addr))) {
        w_ret = -3;
    }
#endif

#ifndef CONFIG_NEW_BREDR_ENABLE
    if (__this->channel == 'L') {
        if (tws_le_get_connect_aa() != tws_le_get_pair_aa()) {
            ret = false;
        }
        tws_api_set_connect_aa_allways(tws_le_get_pair_aa());
    } else {
        if (tws_le_get_connect_aa() != charge_store_info->pair_aa) {
            ret = false;
        }
        tws_api_set_connect_aa_allways(charge_store_info->pair_aa);
    }
#endif
    if (__this->testbox_status && (0 == w_ret)) {
        u8 cmd = CMD_BOX_TWS_REMOTE_ADDR;
        chargestore_api_write(&cmd, 1);
        __this->tws_paired_flag = 1;
        /* r_printf("tws_paired_flag=%d\n",__this->tws_paired_flag ); */
    }
    return ret;
}

bool chargestore_check_data_succ(u8 *data, u8 len)
{
    u16 crc;
    u16 device_ind;
    CHARGE_STORE_INFO *charge_store_info = (CHARGE_STORE_INFO *)data;
    if (len > sizeof(CHARGE_STORE_INFO)) {
        log_error("len err\n");
        return false;
    }
    crc = chargestore_crc8(data, len - 2);
    if (crc != charge_store_info->reserved_data) {
        log_error("crc err\n");
        return false;
    }
    device_ind = bt_get_tws_device_indicate(NULL);
    if (device_ind != charge_store_info->device_ind) {
        log_error("device_ind err\n");
        return false;
    }
    return true;
}

#endif

extern const u8 *bt_get_mac_addr();
u16 chargestore_get_tws_remote_info(u8 *data)
{
    u16 ret_len = 0;
    u16 device_ind;
    CHARGE_STORE_INFO *charge_store_info = (CHARGE_STORE_INFO *)data;
#if TCFG_USER_TWS_ENABLE
    bt_get_tws_local_addr(charge_store_info->tws_local_addr);
    if (__this->keep_tws_conn_flag) {
        memcpy(charge_store_info->tws_mac_addr, bt_get_mac_addr(), 6);
    } else {
        bt_get_vm_mac_addr(charge_store_info->tws_mac_addr);
    }
#ifndef CONFIG_NEW_BREDR_ENABLE
    charge_store_info->search_aa = tws_le_get_search_aa();
    charge_store_info->pair_aa = tws_le_get_pair_aa();
#endif

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_LEFT_CHANNEL) \
    ||(CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_RIGHT_CHANNEL) \
    ||(CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_DOWN_AS_LEFT) \
    ||(CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_UP_AS_LEFT)
    charge_store_info->local_channel = tws_api_get_local_channel();
#else
    charge_store_info->local_channel = 'U';
#endif
    charge_store_info->device_ind = bt_get_tws_device_indicate(NULL);
    if (__this->testbox_status) {
        charge_store_info->reserved_data = 0;
    } else {
        charge_store_info->reserved_data = chargestore_crc8(data, sizeof(CHARGE_STORE_INFO) - 2);
    }
#else
    bt_get_vm_mac_addr(charge_store_info->tws_mac_addr);
#endif
    ret_len = sizeof(CHARGE_STORE_INFO);

    return ret_len;
}

#if TCFG_CHARGESTORE_ENABLE
u16 chargestore_f95_read_tws_remote_info(u8 *data, u8 flag)
{
    u8 read_len;
    u8 *pbuf = (u8 *)&read_info;
    if (flag) {//first packet
        read_index = 0;
        chargestore_get_tws_remote_info((u8 *)&read_info);
    }
    read_len = sizeof(read_info) - read_index;
    read_len = (read_len > (__this->max_packet_size - 1)) ? (__this->max_packet_size - 1) : read_len;
    memcpy(data, pbuf + read_index, read_len);
    read_index += read_len;
    return read_len;
}

u16 chargestore_f95_write_tws_remote_info(u8 *data, u8 len, u8 flag)
{
    u8 write_len;
    u8 *pbuf = (u8 *)&write_info;
    if (flag) {
        write_index = 0;
        memset(&write_info, 0, sizeof(write_info));
    }
    write_len = sizeof(write_info) - write_index;
    write_len = (write_len >= len) ? len : write_len;
    memcpy(pbuf + write_index, data, write_len);
    write_index += write_len;
    return write_len;
}
#endif

void chargestore_set_tws_channel_info(void)
{
    if ((__this->channel == 'L') || (__this->channel == 'R')) {
        syscfg_write(CFG_CHARGESTORE_TWS_CHANNEL, &__this->channel, 1);
    }
}

#if TCFG_CHARGESTORE_ENABLE
void chargestore_timeout_deal(void *priv)
{
    __this->timer = 0;
    __this->close_ing = 0;
    if ((!__this->cover_status) || __this->active_disconnect) {//当前为合盖或者主动断开连接
        struct application *app = get_current_app();
        if (app && strcmp(app->name, "idle")) {
            sys_enter_soft_poweroff((void *)1);
        }
    } else {

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)

        /* if ((!get_total_connect_dev()) && (tws_api_get_role() == TWS_ROLE_MASTER) && (get_bt_tws_connect_status())) { */
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("charge_icon_ctl...\n");
            bt_ble_icon_reset();
#if CONFIG_NO_DISPLAY_BUTTON_ICON
            if (get_total_connect_dev()) {
                //蓝牙未真正断开;重新广播
                bt_ble_icon_open(ICON_TYPE_RECONNECT);
            } else {
                //蓝牙未连接，重新开可见性
                bt_ble_icon_open(ICON_TYPE_INQUIRY);
            }
#else
            //不管蓝牙是否连接，默认打开
            bt_ble_icon_open(ICON_TYPE_RECONNECT);
#endif

        }
#elif (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV_RCSP)
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("charge_icon_ctl...\n");
#if CONFIG_NO_DISPLAY_BUTTON_ICON
            if (get_total_connect_dev()) {
                //蓝牙未真正断开;重新广播
                bt_ble_adv_ioctl(BT_ADV_SET_EDR_CON_FLAG, SECNE_CONNECTED, 1);
            } else {
                //蓝牙未连接，重新开可见性
                bt_ble_adv_ioctl(BT_ADV_SET_EDR_CON_FLAG, SECNE_UNCONNECTED, 1);
            }
#else
            //不管蓝牙是否连接，默认打开
            bt_ble_adv_ioctl(BT_ADV_SET_EDR_CON_FLAG, SECNE_CONNECTED, 1);
#endif

        }
#endif

    }
    __this->ear_number = 1;
}

void chargestore_set_phone_disconnect(void)
{
#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (!CONFIG_NO_DISPLAY_BUTTON_ICON)
    if (__this->pair_flag && get_tws_sibling_connect_state()) {
        //check ble inquiry
        //printf("get box log_key...con_dev=%d\n",get_tws_phone_connect_state());
        if ((bt_ble_icon_get_adv_state() == ADV_ST_RECONN)
            || (bt_ble_icon_get_adv_state() == ADV_ST_DISMISS)
            || (bt_ble_icon_get_adv_state() == ADV_ST_END)) {
            bt_ble_icon_reset();
            bt_ble_icon_open(ICON_TYPE_INQUIRY);
        }
    }
#endif
    __this->pair_flag = 0;
#elif (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV_RCSP)
#if (!CONFIG_NO_DISPLAY_BUTTON_ICON)
    if (__this->pair_flag && get_tws_sibling_connect_state()) {
        //check ble inquiry
        bt_ble_adv_ioctl(BT_ADV_SET_EDR_CON_FLAG, SECNE_UNCONNECTED, 1);
    }
#endif
    __this->pair_flag = 0;
#endif
}

void chargestore_set_phone_connect(void)
{
    __this->active_disconnect = 0;
}

#endif

extern void bt_fast_test_api(void);
void app_chargestore_testbox_sub_event_handle(u8 *data, u16 size)
{
    u8 mac = 0;
    switch (data[0]) {
    case CMD_BOX_FAST_CONN:
    case CMD_BOX_ENTER_DUT:
        __this->event_hdl_flag = 0;
        struct application *app = get_current_app();
        if (app) {
            if (strcmp(app->name, APP_NAME_BT)) {
                if (!app_var.goto_poweroff_flag) {
                    app_var.play_poweron_tone = 0;
                    task_switch_to_bt();
                }
            } else {
                if ((!__this->connect_status) && __this->bt_init_ok) {
                    log_info("\n\nbt_page_inquiry_scan_for_test\n\n");
                    __this->connect_status = 1;
                    log_info("bredr_dut_enbale\n");
                    /* extern void bt_bredr_enter_dut_mode(u8 mode); */
                    bt_bredr_enter_dut_mode(1, 1);
                }
            }
        }
        break;
    case CMD_BOX_CUSTOM_CODE:
        __this->event_hdl_flag = 0;
        if (data[1] == 0x00) {
            bt_fast_test_api();
        }
        break;
    }
}

void log_outpout_paired_addr_info(void)
{
    u8 l_addr[6 * 3] = {0};
    log_info("L R C ADDR:");
    bt_get_tws_local_addr_from_vm(l_addr);
    //log_info_hexdump(l_addr,6);
    syscfg_read(CFG_TWS_REMOTE_ADDR, l_addr + 6, sizeof(l_addr));
    //log_info_hexdump(l_addr,6);
    syscfg_read(CFG_TWS_COMMON_ADDR, l_addr + 12, sizeof(l_addr));
    log_info_hexdump(l_addr, sizeof(l_addr));
}

u8 user_box_tws_remote_addr_flag = 0;
int app_chargestore_event_handler(struct chargestore_event *chargestore_dev)
{
    int ret = false;
    struct application *app = get_current_app();

#if defined(TCFG_CHARGE_KEEP_UPDATA) && TCFG_CHARGE_KEEP_UPDATA
    //在升级过程中,不响应智能充电舱app层消息
    if (dual_bank_update_exist_flag_get() || classic_update_task_exist_flag_get()) {
        return ret;
    }
#endif

    switch (chargestore_dev->event) {
    case CMD_RESTORE_SYS:
#if TCFG_AUDIO_ANC_ENABLE

#if TCFG_USER_TWS_ENABLE
        bt_tws_remove_pairs();
#endif
        user_send_cmd_prepare(USER_CTRL_DEL_ALL_REMOTE_INFO, 0, NULL);
        cpu_reset();
#endif
        break;
#if TCFG_TEST_BOX_ENABLE
    case CMD_BOX_TWS_CHANNEL_SEL:
        __this->event_hdl_flag = 0;
        chargestore_set_tws_channel_info();
        if (get_vbat_need_shutdown() == TRUE) {
            //电压过低,不进行测试
            break;
        }
        if (app) {
            if (strcmp(app->name, APP_NAME_BT)) {
                if (!app_var.goto_poweroff_flag) {
                    app_var.play_poweron_tone = 0;
                    task_switch_to_bt();
                }
            } else {
                if ((!__this->connect_status) && __this->bt_init_ok) {
                    __this->connect_status = 1;
#if	TCFG_USER_TWS_ENABLE
                    if (0 == __this->keep_tws_conn_flag) {
                        log_info("\n\nbt_page_scan_for_test\n\n");
                        bt_bredr_enter_dut_mode(1, 0);
                    }
#endif
                }
            }
        }
        break;
#if TCFG_USER_TWS_ENABLE
    case CMD_BOX_TWS_REMOTE_ADDR:
        log_info("event_CMD_BOX_TWS_REMOTE_ADDR \n");
        user_box_tws_remote_addr_flag = 1;
        chargestore_set_tws_remote_info(chargestore_dev->packet, chargestore_dev->size);
        break;
#endif
#endif
#if TCFG_CHARGESTORE_ENABLE
    case CMD_TWS_CHANNEL_SET:
        chargestore_set_tws_channel_info();
        break;
#if TCFG_USER_TWS_ENABLE
    case CMD_TWS_REMOTE_ADDR:
        log_info("event_CMD_TWS_REMOTE_ADDR\n");
        if (chargestore_set_tws_remote_info(chargestore_dev->packet, chargestore_dev->size) == false) {
            //交换地址后,断开与手机连接,并删除所有连过的手机地址
            user_send_cmd_prepare(USER_CTRL_DEL_ALL_REMOTE_INFO, 0, NULL);
            __this->ear_number = 2;
            sys_enter_soft_poweroff((void *)1);
        } else {
            __this->pair_flag = 1;
            if (get_tws_phone_connect_state() == TRUE) {
                __this->active_disconnect = 1;
                user_send_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            } else {
                chargestore_set_phone_disconnect();
            }
        }
        break;
    case CMD_TWS_ADDR_DELETE:
        log_info("event_CMD_TWS_ADDR_DELETE\n");
        chargestore_clean_tws_conn_info(chargestore_dev->packet[0]);
        break;
#endif
    case CMD_POWER_LEVEL_OPEN:
        log_info("event_CMD_POWER_LEVEL_OPEN\n");

        //电压过低,不进响应开盖命令
#if TCFG_SYS_LVD_EN
        if (get_vbat_need_shutdown() == TRUE) {
            log_info(" lowpower deal!\n");
            break;
        }
#endif

        if (__this->cover_status) {//当前为开盖
            if (__this->power_sync) {
                if (chargestore_sync_chg_level() == 0) {
                    __this->power_sync = 0;
                }
            }
            if (app && strcmp(app->name, APP_NAME_BT) && (app_var.goto_poweroff_flag == 0)) {
                /* app_var.play_poweron_tone = 1; */
                app_var.play_poweron_tone = 0;
                power_set_mode(TCFG_LOWPOWER_POWER_SEL);
                __this->switch2bt = 1;
                task_switch_to_bt();
                __this->switch2bt = 0;
            }
        }
        break;
    case CMD_POWER_LEVEL_CLOSE:
        log_info("event_CMD_POWER_LEVEL_CLOSE\n");
        if (!__this->cover_status) {//当前为合盖
            if (app && strcmp(app->name, "idle")) {
                sys_enter_soft_poweroff((void *)1);
            }
        }
        break;
    case CMD_CLOSE_CID:
        log_info("event_CMD_CLOSE_CID\n");
        if (!__this->cover_status) {//当前为合盖
#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
            if ((__this->bt_init_ok) && (tws_api_get_role() == TWS_ROLE_MASTER))  {
                bt_ble_icon_close(1);
            }
#elif (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV_RCSP)
            if ((__this->bt_init_ok) && (tws_api_get_role() == TWS_ROLE_MASTER))  {
                bt_ble_adv_ioctl(BT_ADV_SET_EDR_CON_FLAG, SECNE_DISMISS, 1);
            }
#endif
            if (!__this->timer) {
                __this->timer = sys_timeout_add(NULL, chargestore_timeout_deal, 2000);
                if (!__this->timer) {
                    log_error("timer alloc err!\n");
                } else {
                    __this->close_ing = 1;
                }
            } else {
                sys_timer_modify(__this->timer, 2000);
                __this->close_ing = 1;
            }
        } else {
            __this->ear_number = 1;
            __this->close_ing = 0;
        }
        break;
#endif
    case CMD_BOX_MODULE:
        app_chargestore_testbox_sub_event_handle(chargestore_dev->packet, chargestore_dev->size);
        break;

    default:
        break;
    }

    return ret;
}

#if TCFG_CHARGESTORE_ENABLE

u8 chargestore_get_vbat_percent(void)
{
    u8 power;
#if CONFIG_DISPLAY_DETAIL_BAT
    power = get_vbat_percent();//显示个位数的电量
#else
    power = get_self_battery_level() * 10 + 10; //显示10%~100%
#endif

#if TCFG_CHARGE_ENABLE
    if (get_charge_full_flag()) {
        power = 100;
    } else if (power == 100) {
        power = 99;
    }
    if (get_charge_online_flag()) {
        power |= BIT(7);
    }
#endif
    return power;
}

void chargestore_set_power_status(u8 *buf, u8 len)
{
    __this->version = buf[0] & 0x0f;
    __this->chip_type = (buf[0] >> 4) & 0x0f;
    //f95可能传一个大于100的电量
    if ((buf[1] & 0x7f) > 100) {
        __this->power_level = (buf[1] & 0x80) | 100;
    } else {
        __this->power_level = buf[1];
    }
    if (len > 2) {
        __this->max_packet_size = buf[2];
        if (len > 3) {
            __this->tws_power = buf[3];
        }
    }
}

void chargestore_shutdown_do(void *priv)
{
    log_info("chargestore shutdown!\n");
    power_set_soft_poweroff();
}

#endif


void chargestore_event_to_user(u8 *packet, u32 type, u8 event, u8 size)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    if (packet != NULL) {
        if (size > sizeof(local_packet)) {
            return;
        }
        e.u.chargestore.packet = local_packet;
        memcpy(e.u.chargestore.packet, packet, size);
    }
    e.arg  = (void *)type;
    e.u.chargestore.event = event;
    e.u.chargestore.size = size;
    sys_event_notify(&e);
}

extern const char *bt_get_local_name();
extern u16 get_vbat_value(void);
extern const int config_btctler_eir_version_info_len;
extern const char *sdk_version_info_get(void);
extern u8 *sdfile_get_burn_code(u8 *len);
static u8  ex_enter_dut_flag = 0;
static u8 ex_enter_storage_mode_flag = 0;//1 仓储模式, 0 普通模式

u8 testbox_get_ex_enter_storage_mode_flag(void)
{
    return ex_enter_storage_mode_flag;
}

u8 chargestore_get_ex_enter_dut_flag(void)
{
    return ex_enter_dut_flag;
}

static const u8 own_private_linkkey[16] = {
    0x06, 0x77, 0x5f, 0x87, 0x91, 0x8d, 0xd4, 0x23,
    0x00, 0x5d, 0xf1, 0xd8, 0xcf, 0x0c, 0x14, 0x2b
};

typedef struct _tws_paired_info_t {
    u8 version;		//
    union {
        struct {
            u8 common_addr[6];
            u8 remote_addr[6];
            u8 local_addr[6];
        };

        struct {
            u8 search_aa[4];
            u8 pair_aa[4];
        };
    };

} tws_paired_info_t;

enum {
    PAIR_INFO_VERSION_V1 = 0,
    PAIR_INFO_VERSION_V2,
};

static int chargestore_get_tws_paired_info(u8 *buf, u8 *len)
{
    tws_paired_info_t info;
    u8 data_len = 0;

#ifdef CONFIG_NEW_BREDR_ENABLE
    memset((u8 *)&info, 0x00, sizeof(tws_paired_info_t));
    info.version = PAIR_INFO_VERSION_V2;
    bt_get_tws_local_addr(info.local_addr);
    syscfg_read(CFG_TWS_REMOTE_ADDR, info.remote_addr, 6);
    syscfg_read(CFG_TWS_COMMON_ADDR, info.common_addr, 6);
    data_len = 6 + 6 + 6 + 1;
#else
    info.version = PAIR_INFO_VERSION_V1;
#error "Not implentment!\n";
#endif

    if (len) {
        *len = data_len;
    }

    if (buf) {
        memcpy(buf, (u8 *)&info, data_len);
    }

    //log_info("paired info len:%x\n", data_len);
    //put_buf(buf, data_len);

    return 0;
}

void app_chargestore_testbox_sub_cmd_handle(u8 *buf, u8 len)
{
    u8 temp_len;
    u8 send_len = 0;

    send_buf[0] = buf[0];
    send_buf[1] = buf[1];

    log_info("sub_cmd:%x\n", buf[1]);

    switch (buf[1]) {
    case CMD_BOX_BT_NAME_INFO:
        temp_len = strlen(bt_get_local_name());
        if (temp_len < (sizeof(send_buf) - 2)) {
            memcpy(&send_buf[2], bt_get_local_name(), temp_len);
            send_len = temp_len + 2;
            chargestore_api_write(send_buf, send_len);
        } else {
            log_error("bt name buf len err\n");
        }
        break;

    case CMD_BOX_BATTERY_VOL:
        send_buf[2] = get_vbat_value();
        send_buf[3] = get_vbat_value() >> 8;
        send_buf[4] = get_vbat_percent();
        send_len = sizeof(u16) + sizeof(u8) + 2; //vbat_value;u16,vabt_percent:u8,opcode:2 bytes
        log_info("bat_val:%d %d\n", get_vbat_value(), get_vbat_percent());
        chargestore_api_write(send_buf, send_len);
        break;

    case CMD_BOX_SDK_VERSION:
        if (config_btctler_eir_version_info_len) {
            temp_len = strlen(sdk_version_info_get());
            send_len = (temp_len > (sizeof(send_buf) - 2)) ? sizeof(send_buf) : temp_len + 2;
            log_info("version:%s ver_len:%x send_len:%x\n", sdk_version_info_get(), temp_len, send_len);
            memcpy(send_buf + 2, sdk_version_info_get(), temp_len);
            chargestore_api_write(send_buf, send_len);
        }
        break;

    case CMD_BOX_FAST_CONN:
        log_info("enter fast dut\n");
        set_temp_link_key((u8 *)own_private_linkkey);
        bt_get_vm_mac_addr(&send_buf[2]);
        if (0 == __this->event_hdl_flag) {
            chargestore_event_to_user(&buf[1], DEVICE_EVENT_CHARGE_STORE, buf[0], 1);
            __this->event_hdl_flag = 1;
        }
        if (__this->bt_init_ok) {
            ex_enter_dut_flag = 1;
            chargestore_api_write(send_buf, 8);
        }
        break;

    case CMD_BOX_ENTER_DUT:
        log_info("enter dut\n");
        //__this->testbox_status = 1;
        ex_enter_dut_flag = 1;

        if (0 == __this->event_hdl_flag) {
            chargestore_event_to_user(&buf[1], DEVICE_EVENT_CHARGE_STORE, buf[0], 1);
            __this->event_hdl_flag = 1;
        }
        if (__this->bt_init_ok) {
            chargestore_api_write(send_buf, 2);
        }
        break;

    case CMD_BOX_VERIFY_CODE:
        log_info("get_verify_code\n");
        u8 *p = sdfile_get_burn_code(&temp_len);
        send_len = (temp_len > (sizeof(send_buf) - 2)) ? sizeof(send_buf) : temp_len + 2;
        memcpy(send_buf + 2, p, temp_len);
        chargestore_api_write(send_buf, send_len);
        break;

    case CMD_BOX_CUSTOM_CODE:
        log_info("CMD_BOX_CUSTOM_CODE value=%x", buf[2]);
        send_len = 0x3;
        if (buf[2] == 0) {//测试盒自定义命令，样机进入快速测试模式
            if (0 == __this->event_hdl_flag) {
                __this->event_hdl_flag = 1;
                chargestore_event_to_user(&buf[1], DEVICE_EVENT_CHARGE_STORE, buf[0], 2);
            }
#if TCFG_LP_TOUCH_KEY_ENABLE
        } else if (buf[2] == 0x6a) {//测试盒自定义命令， 0x6a 用于触摸动态阈值算法的结果显示，请大家不要冲突
            send_buf[2] = 0x6a;
            send_buf[3] = 'c';
            send_buf[4] = 's';
            send_buf[5] = 'm';
            send_buf[6] = 'r';
            send_buf[7] = lp_touch_key_alog_range_display((u8 *)&send_buf[8]);
            send_len = 8 + send_buf[7];
            log_info("send_len = %d\n", send_len);
#endif
        }
        chargestore_api_write(send_buf, send_len);
        break;

    case CMD_BOX_ENTER_STORAGE_MODE:
        log_info("CMD_BOX_ENTER_STORAGE_MODE");
        ex_enter_storage_mode_flag = 1;
        chargestore_api_write(send_buf, 2);
        break;

    case CMD_BOX_GET_TWS_PAIR_INFO:
        log_info("CMD_BOX_GET_TWS_PAIR_INFO");
        chargestore_get_tws_paired_info(send_buf + 2, &send_len);
        chargestore_api_write(send_buf, send_len + 2);
        break;

    case CMD_BOX_GLOBLE_CFG:
        log_info("CMD_BOX_GLOBLE_CFG:%d %x %x", len, READ_LIT_U32(buf + 2), READ_LIT_U32(buf + 6));
        __this->global_cfg = READ_LIT_U32(buf + 2);
#if 0 //for test
        u8 sec;
        u32 trim_en = testbox_get_touch_trim_en(&sec);
        log_info("box_cfg:%x %x %x\n",
                 testbox_get_softpwroff_after_paired(),
                 trim_en, sec);
#endif

        chargestore_api_write(send_buf, 2);
        break;

    default:
        send_buf[0] = CMD_UNDEFINE;
        send_len = 1;
        chargestore_api_write(send_buf, send_len);
        break;
    }

    log_info_hexdump(send_buf, send_len);
}

void app_chargestore_data_deal(u8 *buf, u8 len)
{
    //u8 send_buf[30];
    /* log_info_hexdump(buf, len); */
#if TCFG_CHARGESTORE_ENABLE//有通信则关机定时器删掉
    chargestore_shutdown_reset();
#endif
    send_buf[0] = buf[0];
#ifdef CONFIG_CHARGESTORE_REMAP_ENABLE
    if (remap_app_chargestore_data_deal(buf, len)) {
        return;
    }
#endif
    switch (buf[0]) {
#if TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE
#if TCFG_TEST_BOX_ENABLE
    //case CMD_ENTER_DUT:
    //	__this->testbox_status = 1;
    //log_info("enter_dut\n");
    //break;

    case CMD_BOX_MODULE:
        app_chargestore_testbox_sub_cmd_handle(buf, len);
        break;

    case CMD_BOX_UPDATE:
        __this->testbox_status = 1;
        if (buf[13] == get_jl_chip_id() || buf[13] == get_jl_chip_id2()) {
            chargestore_set_update_ram();
#if CONFIG_UPDATE_JUMP_TO_MASK
            /* y_printf(">>>[test]:latch reset update\n"); */
            /* latch_reset(); */

            printf("\n >>>[test]:func = %s,line= %d\n", __FUNCTION__, __LINE__);
            /* clk_set("sys", 24 * 1000000L); */
            update_close_hw("null");
            hwi_all_close();
            ram_protect_close();
            /* clock_dump(); */
            extern void __BT_UPDATA_JUMP(void);
            printf(">>>[test]:jump to update!!!!!!!!\n");
            __BT_UPDATA_JUMP();
#else
            cpu_reset();
#endif
        } else if (buf[13] == 0xff) {
            send_buf[1] = 0xff;
            WRITE_LIT_U32(&send_buf[2], support_update_mask);
            chargestore_api_write(send_buf, 2 + sizeof(support_update_mask));
            log_info("rsp update_mask\n");
        } else {
            send_buf[1] = 0x01;//chip id err
            chargestore_api_write(send_buf, 2);
        }
        break;
    case CMD_BOX_TWS_CHANNEL_SEL:
        __this->testbox_status = 1;
        if (len == 3) {
            __this->keep_tws_conn_flag = buf[2];
            putchar('K');
        }  else {
            __this->keep_tws_conn_flag = 0;
        }
#endif
#if TCFG_CHARGESTORE_ENABLE
    case CMD_TWS_CHANNEL_SET:
#endif
        __this->channel = (buf[1] == TWS_CHANNEL_LEFT) ? 'L' : 'R';
        if (0 == __this->event_hdl_flag) {
            chargestore_event_to_user(NULL, DEVICE_EVENT_CHARGE_STORE, buf[0], 0);
            __this->event_hdl_flag = 1;
        }
        if (__this->bt_init_ok) {
            len = chargestore_get_tws_remote_info(&send_buf[1]);
            chargestore_api_write(send_buf, len + 1);
        } else {
            send_buf[0] = CMD_UNDEFINE;
            chargestore_api_write(send_buf, 1);
        }
        break;
#endif
#if TCFG_CHARGESTORE_ENABLE
    case CMD_TWS_SET_CHANNEL:
        __this->channel = (buf[1] == TWS_CHANNEL_LEFT) ? 'L' : 'R';
        log_info("f95 set channel = %c\n", __this->channel);
        chargestore_event_to_user(NULL, DEVICE_EVENT_CHARGE_STORE, CMD_TWS_CHANNEL_SET, 0);
        if (__this->bt_init_ok) {
            chargestore_api_write(send_buf, 1);
        } else {
            send_buf[0] = CMD_UNDEFINE;
            chargestore_api_write(send_buf, 1);
        }
        break;
#endif

#if TCFG_USER_TWS_ENABLE
#if TCFG_TEST_BOX_ENABLE
    case CMD_BOX_TWS_REMOTE_ADDR:
        __this->testbox_status = 1;
        __this->close_ing = 0;
        chargestore_event_to_user((u8 *)&buf[1], DEVICE_EVENT_CHARGE_STORE, buf[0], len - 1);
        chargestore_api_set_timeout(100);
        break;
#endif
#if TCFG_CHARGESTORE_ENABLE
    case CMD_TWS_REMOTE_ADDR:
        __this->close_ing = 0;
        if (chargestore_check_data_succ((u8 *)&buf[1], len - 1) == true) {
            chargestore_event_to_user((u8 *)&buf[1], DEVICE_EVENT_CHARGE_STORE, buf[0], len - 1);
        } else {
            send_buf[0] = CMD_FAIL;
        }
        chargestore_api_write(send_buf, 1);
        break;
    case CMD_EX_FIRST_READ_INFO:
        log_info("read first!\n");
        __this->close_ing = 0;
        len = chargestore_f95_read_tws_remote_info(&send_buf[1], 1);
        chargestore_api_write(send_buf, len + 1);
        break;
    case CMD_EX_CONTINUE_READ_INFO:
        log_info("read continue!\n");
        __this->close_ing = 0;
        len = chargestore_f95_read_tws_remote_info(&send_buf[1], 0);
        chargestore_api_write(send_buf, len + 1);
        break;
    case CMD_EX_FIRST_WRITE_INFO:
        log_info("write first!\n");
        __this->close_ing = 0;
        chargestore_f95_write_tws_remote_info(&buf[1], len - 1, 1);
        chargestore_api_write(send_buf, 1);
        break;
    case CMD_EX_CONTINUE_WRITE_INFO:
        log_info("write continue!\n");
        __this->close_ing = 0;
        chargestore_f95_write_tws_remote_info(&buf[1], len - 1, 0);
        chargestore_api_write(send_buf, 1);
        break;
    case CMD_EX_INFO_COMPLETE:
        log_info("ex complete!\n");
        if (chargestore_check_data_succ((u8 *)&write_info, sizeof(write_info)) == true) {
            chargestore_event_to_user((u8 *)&write_info, DEVICE_EVENT_CHARGE_STORE, CMD_TWS_REMOTE_ADDR, sizeof(write_info));
        } else {
            send_buf[0] = CMD_FAIL;
        }
        chargestore_api_write(send_buf, 1);
        break;
#endif
#endif

#if TCFG_CHARGESTORE_ENABLE
    case CMD_TWS_ADDR_DELETE:
        __this->close_ing = 0;
        chargestore_event_to_user(&buf[1], DEVICE_EVENT_CHARGE_STORE, CMD_TWS_ADDR_DELETE, len - 1);
        chargestore_api_write(send_buf, 1);
        break;
    case CMD_POWER_LEVEL_OPEN:
        __this->power_status = 1;
        __this->cover_status = 1;
        __this->close_ing = 0;
        if (__this->power_level == 0xff) {
            __this->power_sync = 1;
        }
        chargestore_set_power_status(&buf[1], len - 1);
        if (__this->power_level != __this->pre_power_lvl) {
            __this->power_sync = 1;
        }
        __this->pre_power_lvl = __this->power_level;
        send_buf[1] = chargestore_get_vbat_percent();
        send_buf[2] = chargestore_get_det_level(__this->chip_type);
        chargestore_api_write(send_buf, 3);
        //切模式过程中不发送消息,防止堆满消息
        if (__this->switch2bt == 0) {
            chargestore_event_to_user(NULL, DEVICE_EVENT_CHARGE_STORE, CMD_POWER_LEVEL_OPEN, 0);
        }
        break;
    case CMD_POWER_LEVEL_CLOSE:
        __this->power_status = 1;
        __this->cover_status = 0;
        __this->close_ing = 0;
        chargestore_set_power_status(&buf[1], len - 1);
        send_buf[1] = chargestore_get_vbat_percent();
        send_buf[2] = chargestore_get_det_level(__this->chip_type);
        chargestore_api_write(send_buf, 3);
        chargestore_event_to_user(NULL, DEVICE_EVENT_CHARGE_STORE, CMD_POWER_LEVEL_CLOSE, 0);
        break;
    case CMD_SHUT_DOWN:
        log_info("shut down\n");
        __this->power_status = 0;
        __this->cover_status = 0;
        __this->close_ing = 0;
        chargestore_api_write(send_buf, 1);
        __this->shutdown_timer = sys_hi_timer_add(NULL, chargestore_shutdown_do, 1000);
        break;
    case CMD_CLOSE_CID:
        log_info("close cid\n");
        __this->power_status = 1;
        __this->cover_status = 0;
        __this->ear_number = buf[1];
        chargestore_api_write(send_buf, 1);
        chargestore_event_to_user(NULL, DEVICE_EVENT_CHARGE_STORE, CMD_CLOSE_CID, 0);
        break;

    case CMD_RESTORE_SYS:
        r_printf("restore sys\n");
        __this->power_status = 1;
        __this->cover_status = 1;
        __this->close_ing = 0;
        chargestore_api_write(send_buf, 1);
        chargestore_event_to_user(NULL, DEVICE_EVENT_CHARGE_STORE, CMD_RESTORE_SYS, 0);
        break;
#endif

#if TCFG_ANC_BOX_ENABLE
    case CMD_ANC_MODULE:
        app_ancbox_module_deal(buf, len);
        break;
#endif
#if TCFG_CHARGE_CALIBRATION_ENABLE
    case CMD_CALIBRATION_MODULE:
        app_charge_calibration_data_handler(buf, len);
        break;
#endif
    default:
        send_buf[0] = CMD_UNDEFINE;
        chargestore_api_write(send_buf, 1);
        break;
    }
}

#if TCFG_CHARGESTORE_ENABLE
u8 chargestore_get_power_level(void)
{
    if ((__this->power_level == 0xff) ||
        ((!get_charge_online_flag()) &&
         (__this->sibling_chg_lev != 0xff))) {
        return __this->sibling_chg_lev;
    }
    return __this->power_level;
}

u8 chargestore_get_power_status(void)
{
    return __this->power_status;
}

u8 chargestore_get_cover_status(void)
{
    return __this->cover_status;
}

u8 chargestore_get_earphone_online(void)
{
    return __this->ear_number;
}

void chargestore_set_earphone_online(u8 ear_number)
{
    __this->ear_number = ear_number;
}

void chargestore_set_pair_flag(u8 pair_flag)
{
    __this->pair_flag = pair_flag;
}

void chargestore_set_active_disconnect(u8 active_disconnect)
{
    __this->active_disconnect = active_disconnect;
}

u8 chargestore_get_earphone_pos(void)
{
    log_info("get_ear_channel = %c\n", __this->channel);
    return __this->channel;
}

u8 chargestore_get_sibling_power_level(void)
{
    return __this->tws_power;
}


#if TCFG_USER_TWS_ENABLE


static void set_tws_sibling_charge_level(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        chargestore_set_sibling_chg_lev(data[0]);
    }
}

REGISTER_TWS_FUNC_STUB(charge_level_stub) = {
    .func_id = TWS_FUNC_ID_CHARGE_SYNC,
    .func    = set_tws_sibling_charge_level,
};

#endif

int chargestore_sync_chg_level(void)
{
#if TCFG_USER_TWS_ENABLE
    int err = -1;
    struct application *app = get_current_app();
    if (app && (!strcmp(app->name, "earphone")) && (!app_var.goto_poweroff_flag)) {
        err = tws_api_send_data_to_sibling((u8 *)&__this->power_level, 1,
                                           TWS_FUNC_ID_CHARGE_SYNC);
    }
    return err;
#else
    return 0;
#endif
}

void chargestore_set_sibling_chg_lev(u8 chg_lev)
{
    __this->sibling_chg_lev = chg_lev;
}

void chargestore_set_power_level(u8 power)
{
    __this->power_level = power;
}

u8 chargestore_check_going_to_poweroff(void)
{
    return __this->close_ing;
}

void chargestore_shutdown_reset(void)
{
    if (__this->shutdown_timer) {
        sys_hi_timer_del(__this->shutdown_timer);
        __this->shutdown_timer = 0;
    }
}

#endif

void chargestore_set_bt_init_ok(u8 flag)
{
    __this->bt_init_ok = flag;
}

#if TCFG_TEST_BOX_ENABLE
u8 chargestore_get_connect_status(void)
{
    return __this->connect_status;
}

void chargestore_clear_connect_status(void)
{
    __this->connect_status = 0;
}
u8 chargestore_get_testbox_tws_paired(void)
{
    return __this->tws_paired_flag;
}

u8 chargestore_get_testbox_status(void)
{
    return __this->testbox_status;
}

void chargestore_clear_testbox_status(void)
{
    __this->testbox_status = 0;
    chargestore_clear_connect_status();
}

u8 chargestore_get_keep_tws_conn_flag(void)
{
    return __this->keep_tws_conn_flag;
}
#endif

CHARGESTORE_PLATFORM_DATA_BEGIN(chargestore_data)
.uart_irq                = TCFG_CHARGESTORE_UART_ID,
 .io_port                = TCFG_CHARGESTORE_PORT,
  CHARGESTORE_PLATFORM_DATA_END()

  __BANK_INIT
  int app_chargestore_init(void)
{
    __this->power_status = 1;
    __this->cover_status = 0;
    __this->bt_init_ok = 0;
    __this->testbox_status = 0;
    __this->connect_status = 0;
    __this->ear_number = 1;
    __this->tws_power = 0xff;
    __this->power_level = 0xff;
    __this->sibling_chg_lev = 0xff;
    __this->max_packet_size = 32;
    __this->channel = 'U';
    __this->close_ing = 0;
    __this->event_hdl_flag = 0;
    __this->switch2bt = 0;
    __this->tws_paired_flag = 0;
    __this->global_cfg = BIT(BOX_CFG_SOFTPWROFF_AFTER_PAIRED_BIT);
    chargestore_api_init(&chargestore_data);
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &__this->channel, 1);
    log_info("channel = %c\n", __this->channel);
    return 0;
}
__initcall(app_chargestore_init);

#else
u8 testbox_get_ex_enter_storage_mode_flag(void)
{
    return 0;
}

u8 chargestore_get_ex_enter_dut_flag(void)
{
    return 0;
}

#endif

