#ifndef __KUGOU_PROTOCOL_H__
#define __KUGOU_PROTOCOL_H__

u32 kugou_scsi_cmd(const struct usb_device_t *usb_device, struct usb_scsi_cbw *cbw);

#endif

