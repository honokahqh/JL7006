#ifndef __ADC_API_H__
#define __ADC_API_H__


//AD channel define
#define AD_CH_PA3     (0x0)
#define AD_CH_PA5     (0x1)
#define AD_CH_PA6     (0x2)
#define AD_CH_PA8     (0x3)
#define AD_CH_PC4     (0x4)
#define AD_CH_PC5     (0x5)
#define AD_CH_PB1     (0x6)
#define AD_CH_PB2     (0x7)
#define AD_CH_PB4     (0x8)
#define AD_CH_PB6     (0x9)
#define AD_CH_DP      (0xa)
#define AD_CH_DM      (0xb)
#define AD_CH_LCTM    (0xc)
#define AD_CH_AUDIO   (0xd)
#define AD_CH_PMU     (0xe)
#define AD_CH_BT      (0xf)



#define ADC_PMU_CH_VBG        (0x0<<16)  //MVBG
#define ADC_PMU_CH_VSW        (0x1<<16)
#define ADC_PMU_CH_PROGI      (0x2<<16)
#define ADC_PMU_CH_PROGF      (0x3<<16)
#define ADC_PMU_CH_VTEMP      (0x4<<16)
#define ADC_PMU_CH_LDO5V      (0x5<<16)     //1/4 LDO5V
#define ADC_PMU_CH_VBAT       (0x6<<16)     //1/4vbat
/* #define ADC_PMU_CH_VBAT_2  (0x7<<16) */
#define ADC_PMU_CH_VB17       (0x8<<16)
#define ADC_PMU_CH_PVDD_PROB  (0x9<<16)
#define ADC_PMU_CH_VDC15      (0xa<<16)
#define ADC_PMU_CH_DVDD       (0xb<<16)
#define ADC_PMU_CH_RVDD       (0xc<<16)
#define ADC_PMU_CH_TWVDD      (0xd<<16)
#define ADC_PMU_CH_PVDD       (0xe<<16)
#define ADC_PMU_CH_EVDD       (0xf<<16)


#define AD_CH_LDOREF    AD_CH_PMU_VBG

#define AD_CH_PMU_VBG   (AD_CH_PMU | ADC_PMU_CH_VBG)
#define AD_CH_VDC15     (AD_CH_PMU | ADC_PMU_CH_VDC15)
#define AD_CH_SYSVDD    (AD_CH_PMU | ADC_PMU_CH_DVDD)
#define AD_CH_DTEMP     (AD_CH_PMU | ADC_PMU_CH_VTEMP)
#define AD_CH_VBAT      (AD_CH_PMU | ADC_PMU_CH_VBAT)
#define AD_CH_LDO5V     (AD_CH_PMU | ADC_PMU_CH_LDO5V)
#define AD_CH_WVDD      (AD_CH_PMU | ADC_PMU_CH_TWVDD)
#define AD_CH_PVDD      (AD_CH_PMU | ADC_PMU_CH_PVDD)
#define AD_CH_PROGF     (AD_CH_PMU | ADC_PMU_CH_PROGF)
#define AD_CH_PROGI     (AD_CH_PMU | ADC_PMU_CH_PROGI)

#define AD_CH_LDOREF    AD_CH_PMU_VBG

#define     ADC_MAX_CH  10


extern void adc_init();
extern void adc_reset(void);
extern void adc_vbg_init();
//p33 define

extern void adc_pmu_ch_select(u32 ch);
extern void adc_pmu_detect_en(u32 ch);
extern void adc_vdc13_save();
extern void adc_vdc13_restore();

//
u32 adc_get_value(u32 ch);

u32 adc_add_sample_ch(u32 ch);

u32 adc_remove_sample_ch(u32 ch);

u32 adc_get_voltage(u32 ch);
u32 adc_check_vbat_lowpower();
u32 adc_set_sample_freq(u32 ch, u32 ms);

u32 adc_sample(u32 ch);
#endif
