#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
 *  板级配置选择
 */

#define CONFIG_BOARD_AC700N_DEMO
// #define CONFIG_BOARD_AC7006F_EARPHONE
// #define CONFIG_BOARD_AC700N_SD_PC_DEMO
//#define CONFIG_BOARD_AC7003F_DEMO
// #define CONFIG_BOARD_AC700N_ANC
//#define CONFIG_BOARD_AC700N_DMS
// #define CONFIG_BOARD_AC700N_TEST//ANC+ENC双mic+DNS+AEC+BLE+TWS+叠加提示音播放
// #define CONFIG_BOARD_AC700N_HEARING_AID  //辅听模式板级
//#define CONFIG_BOARD_AC700N_IIS_LINEIN  //linein anc板级

#include "media/audio_def.h"
#include "board_ac700n_demo_cfg.h"
#include "board_ac7006f_earphone_cfg.h"
#include "board_ac700n_sd_pc_demo_cfg.h"
#include "board_ac7003f_demo_cfg.h"
#include "board_ac700n_anc_cfg.h"
#include "board_ac700n_dms_cfg.h"
#include "board_ac700n_test_cfg.h"
#include "board_ac700n_hearing_aid_cfg.h"
#include "board_ac700n_iis_linein_cfg.h"

#define  DUT_AUDIO_DAC_LDO_VOLT                 DACVDD_LDO_1_35V

#ifdef CONFIG_NEW_CFG_TOOL_ENABLE
#define CONFIG_ENTRY_ADDRESS                    0x1e00100
#endif

#endif
