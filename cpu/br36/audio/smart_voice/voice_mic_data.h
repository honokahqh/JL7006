/*****************************************************************
>file name : voice_mic_data.h
>author : lichao
>create time : Mon 01 Nov 2021 05:08:11 PM CST
*****************************************************************/
#ifndef _VOICE_MIC_DATA_H_
#define _VOICE_MIC_DATA_H_

#include "asm/audio_adc.h"

#define VOICE_P11_MIC         0
#define VOICE_MCU_MIC         1
#define VOICE_DEFULT_MIC      VOICE_P11_MIC

void audio_vco_clock_trim(void);

int voice_mic_data_init(void);

void *voice_mic_data_open(u8 source, int buffer_size, int sample_rate);

void voice_mic_data_close(void *mic);

int voice_mic_data_read(void *mic, void *data, int len);

int voice_mic_data_buffered_samples(void *mic);

void voice_mic_data_clear(void *mic);

void voice_mic_data_dump(void *mic);

void *voice_mic_data_capture(int sample_rate, void *priv, void (*output)(void *priv, s16 *data, int len));

void voice_mic_data_stop_capture(void *mic);

void voice_mic_data_switch_source(void *mic, int buffer_size, int sample_rate, int source);

#endif
