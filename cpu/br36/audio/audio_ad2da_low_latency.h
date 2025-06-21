#ifndef _AD2DA_LOW_LATENCY_H_
#define _AD2DA_LOW_LATENCY_H_

#include "generic/typedef.h"
#include "board_config.h"

struct __ad2da_low_latency_param {
    u16 mic_ch_sel;
    u16 sample_rate;
    u16 adc_irq_points;
    u16 adc_buf_num;
    u16	dac_delay;
    u16 mic_gain;
};

extern int ad2da_low_latency_open(struct __ad2da_low_latency_param *param);
extern int ad2da_low_latency_close(void);

#endif/*_AD2DA_LOW_LATENCY_H_*/
