#include "system/includes.h"
#include "kugou/kugou_info.h"
#include "usb/device/kugou/kugou_packet.h"

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[KG]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

static void kg_info_print(struct kugou_info kg_info);
static unsigned char char2hex(char x);
static void kg_char2byte_arry(const char *p, u8 *arry);
int kg_info_init()
{
    struct kugou_info kg_info;
    log_info("kg_info_init start\n");
    char pubkey_x[] = {KG_PubKeyA_x};
    char pubkey_y[] = {KG_PubKeyA_y};
    kg_char2byte_arry(pubkey_x, (u8 *)kg_info.pubkey_x);
    kg_char2byte_arry(pubkey_y, (u8 *)kg_info.pubkey_y);
    kg_info.pid = atoi(KG_PID);
    kg_info.bid = atoi(KG_BID);
    char kg_version[] = {KG_VERSION};
    kg_char2byte_arry(kg_version, (u8 *)kg_info.version);
    extern const u8 *bt_get_mac_addr();
    memcpy((u8 *)kg_info.mac, bt_get_mac_addr(), 6);

    /* kg_info_print(kg_info); //打印用户配置的信息  */
    kg_info_register(&kg_info);

    log_info("kg_info_init end\n");
}

u8 kg_get_battery_level()   //在此函数实现电量等级查询功能,返回1Byte数据.
{
    log_info("get_battery_level\n");
    return 0x42;
}

static void kg_info_print(struct kugou_info kg_info)
{
    printf("kg_info_init\n");
    printf("\n-------------kugou info pubkey_x---------------");
    put_buf(kg_info.pubkey_x, 24);
    printf("\n-------------kugou info pubkey_y---------------");
    put_buf(kg_info.pubkey_y, 24);
    printf("\n-------------kugou info pid---------------");
    printf("pid = %d\n", kg_info.pid);
    printf("\n-------------kugou info bid---------------");
    printf("bid = %d\n", kg_info.bid);
    printf("\n-------------kugou info version---------------");
    put_buf(kg_info.version, 2);
    printf("\n-------------kugou info mac---------------");
    put_buf(kg_info.mac, 6);
}

static void kg_char2byte_arry(const char *p, u8 *arry)
{
    int size;
    int i;
    size = strlen(p);
    if (size % 2) {
        log_info("sring format error!!!\n");
        return;
    }
    for (i = 0; i < size;) {
        *arry = char2hex(p[i]);
        if (*arry == 0xff) {
            i++;
            continue;
        }

        //*arry = temp;
        *arry <<= 4;
        *arry++ |= char2hex(p[i + 1]);
        i += 2;
    }
}

static unsigned char char2hex(char x)
{
    switch (x) {
    case '0' ... '9':
        return x - '0';
    case 'a' ... 'f':
        return x - 'a' + 10;
    case 'A' ... 'F':
        return x - 'A' + 10;
    default:
        return 255;
    }
}


