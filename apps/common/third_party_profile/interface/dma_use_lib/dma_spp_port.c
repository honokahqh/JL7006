#include "btstack/avctp_user.h"
#include "system/includes.h"
#include "spp_user.h"
#include "string.h"
#include "dma_setting.h"
#include "btstack/third_party/common/spp_user.h"
#include "btstack/third_party/app_protocol_event.h"

#ifdef DMA_LIB_CODE_SIZE_CHECK
#pragma bss_seg(	".bt_dma_port_bss")
#pragma data_seg(	".bt_dma_port_data")
#pragma const_seg(	".bt_dma_port_const")
#pragma code_seg(	".bt_dma_port_code")
#endif
static struct spp_operation_t *spp_api = NULL;
static int dueros_spp_send_data(void *priv, u8 *data, u16 len)
{
    if (spp_api) {
        return spp_api->send_data(NULL, data, len);
    }
    return SPP_USER_ERR_SEND_FAIL;
}

static int dueros_spp_send_data_check(u16 len)
{
    if (spp_api) {
        if (spp_api->busy_state()) {
            return 0;
        }
    }
    return 1;
}

u8 dma_ota_connect_flag = 0;
extern void dma_set_module(bool state);
static void dueros_spp_state_cbk(u8 state)
{
    switch (state) {
    case SPP_USER_ST_CONNECT:
        dma_log("dma_spp conn\n");
        dma_message(APP_PROTOCOL_CONNECTED_SPP, NULL, 0);
        server_register_dueros_send_callbak(dueros_spp_send_data);
        //dueros_send_ver();
        dma_set_module(1);
        break;

    case SPP_USER_ST_DISCONN:
        //smart_dueros_dma_close();
        dma_log("dma spp disconnect \n");
        server_register_dueros_send_callbak(NULL);
        dma_message(APP_PROTOCOL_DISCONNECT, NULL, 0);
        dma_set_module(0);
        break;
    case SPP_USER_ST_CONNECT_OTA:
        dma_log("SPP_USER_ST_CONNECT_OTA\n");
        dma_ota_connect_flag = 1;
        break;
    case SPP_USER_ST_DISCONN_OTA:
        dma_log("SPP_USER_ST_DISCONN_OTA\n");
        dma_ota_connect_flag = 0;
        break;
    default:
        break;
    }

}

void dma_spp_disconnect(void)
{
    if (spp_api) {
        spp_api->disconnect(NULL);
    }
}

static void dueros_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    //putchar('R');
    //printf("spp_api_rx ~~~\n");
    //put_buf(buf, len);
    if (dma_ota_connect_flag) {
        dma_ota_recieve_data(buf, len);
    } else {
        dma_recieve_data(buf, len);
    }
}
void dma_spp_init(void)
{
    spp_get_operation_table(&spp_api);
    spp_api->regist_recieve_cbk(0, dueros_spp_recieve_cbk);
    spp_api->regist_state_cbk(0, dueros_spp_state_cbk);
    spp_api->regist_wakeup_send(NULL, dma_send_process_resume);
}

#if DMA_LIB_OTA_EN
extern int multi_spp_send_data(u8 local_cid, u8 rfcomm_cid,  u8 *buf, u16 len);
int spp_ota_send_data(void *priv, u8 *buf, u16 len)
{
    multi_spp_send_data(0x0A, 0, buf, len);
    return 0;
}

////**dma正常链路和OTA需要同时的情况*///
int mutil_handle_data_deal(u8 local_id, u8 packet_type, u16 channel, u8 *packet, u16 size)
{
    switch (packet_type) {
    case 1:
        //printf("mutil connect###########weak handler\n");
        server_register_dueros_ota_send_callbak(spp_ota_send_data);
        dma_set_module(1);
        break;
    case 2:
        //printf("nutil disconnect#########weak_handler\\n");
        server_register_dueros_ota_send_callbak(NULL);
        dma_set_module(0);
        break;
    case 7:
        dma_ota_recieve_data(packet, size);
        break;
    }
    return 0;
}
#endif
