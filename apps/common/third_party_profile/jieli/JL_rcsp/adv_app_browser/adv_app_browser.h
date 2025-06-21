#ifndef __ADV_APP_BROWSER_H__
#define __ADV_APP_BROWSER_H__

#include "typedef.h"

///对应的设备id枚举
enum {
    ADV_APP_BS_UDISK = 0,
    ADV_APP_BS_SD0,
    ADV_APP_BS_SD1,
    ADV_APP_BS_FLASH,
    ADV_APP_BS_AUX,
    ADV_APP_BS_FLASH_2,
    ADV_APP_BS_DEV_MAX,
};
// 浏览设备映射
char *adv_app_browser_dev_remap(u32 index);
// 获取文件浏览后缀配置
char *adv_app_browser_file_ext(void);
// 获取文件浏览后缀配置数据长度
u16 adv_app_browser_file_ext_size(void);
// 限制文件名长度（包括文件夹,如：xxx~~~, 省略以"~~~"表示）
u16 adv_app_file_name_cut(u8 *name, u16 len, u16 len_limit);
// 文件浏览开始
void adv_app_browser_start(u8 *data, u16 len);
// 文件浏览停止
void adv_app_browser_stop(void);
// 文件浏览繁忙查询
bool adv_app_browser_busy(void);
// 在主线程做文件浏览停止回复
void adv_app_file_browser_stop(u8 reason);

#endif
