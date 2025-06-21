#ifndef _AUD_ONLINE_DEBUG_
#define _AUD_ONLINE_DEBUG_

#include "generic/typedef.h"
#include "online_db_deal.h"

enum {
    AUD_RECORD_COUNT = 0x200,
    AUD_RECORD_START,
    AUD_RECORD_STOP,
    ONLINE_OP_QUERY_RECORD_PACKAGE_LENGTH,

    MIC_DUT_INFO_QUERY = 0x300,
    MIC_DUT_GAIN_SET,
    MIC_DUT_SAMPLE_RATE_SET,
    MIC_DUT_START,
    MIC_DUT_STOP,
    MIC_DUT_AMIC_SEL,
    MIC_DUT_DMIC_SEL,
    MIC_DUT_DAC_GAIN_SET,
    MIC_DUT_SCAN_START,
    MIC_DUT_SCAN_STOP,
};

#define RECORD_CH0_LENGTH		644
#define RECORD_CH1_LENGTH		644
#define RECORD_CH2_LENGTH		644

typedef struct {
    int cmd;
    int data;
} online_cmd_t;

int audio_online_debug_open(void);


#endif/*_AUD_ONLINE_DEBUG_*/

