#ifndef __KUGOU_PACKET_H__
#define __KUGOU_PACKET_H__

struct kugou_info {
    u8 pubkey_x[24];
    u8 pubkey_y[24];
    u32 pid;
    u32 bid;
    u8 version[2];
    u8 mac[6];
};
int kg_info_register(struct kugou_info *kugou_info);

#endif

