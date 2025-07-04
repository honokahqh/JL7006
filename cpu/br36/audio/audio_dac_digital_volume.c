/*
 ******************************************************************
 * 			AUDIO DAC DIGITAL VOLUME MODULE ONCHIP
 * 芯片内部数字音量模块，默认淡入淡出
 * 范围：0~16384
 * 实现原理和audio_digital_vol类似，实现方式有硬件和软件的区别
 ******************************************************************
 */

#include "media/includes.h"
extern struct audio_dac_hdl dac_hdl;

//#define DAC_DIGITAL_VOL_LOG_ENABLE
#ifdef DAC_DIGITAL_VOL_LOG_ENABLE
#define DV_LOG	y_printf
#else
#define DV_LOG(...)
#endif/*DAC_DIGITAL_VOL_LOG_ENABLE*/

enum {
    DAC_DV_STA_CLOSE = 0,
    DAC_DV_STA_OPEN,
};

/*
 *dac数字音量级数 DIGITAL_VOL_MAX
 *数组长度 DIGITAL_VOL_MAX + 1
 */
#define DIGITAL_VOL_MAX		31
#define USER_AUDIO_GAIN (1)
static const u16 dac_dig_vol_tab[DIGITAL_VOL_MAX + 1] = {
    0	* USER_AUDIO_GAIN, //0
    93	* USER_AUDIO_GAIN, //1
    111	* USER_AUDIO_GAIN, //2
    132	* USER_AUDIO_GAIN, //3
    158	* USER_AUDIO_GAIN, //4
    189	* USER_AUDIO_GAIN, //5
    226	* USER_AUDIO_GAIN, //6
    270	* USER_AUDIO_GAIN, //7
    323	* USER_AUDIO_GAIN, //8
    386	* USER_AUDIO_GAIN, //9
    462	* USER_AUDIO_GAIN, //10
    552	* USER_AUDIO_GAIN, //11
    660	* USER_AUDIO_GAIN, //12
    789	* USER_AUDIO_GAIN, //13
    943	* USER_AUDIO_GAIN, //14
    1127* USER_AUDIO_GAIN, //15
    1347* USER_AUDIO_GAIN, //16
    1610* USER_AUDIO_GAIN, //17
    1925* USER_AUDIO_GAIN, //18
    2301* USER_AUDIO_GAIN, //19
    2751* USER_AUDIO_GAIN, //20
    3288* USER_AUDIO_GAIN, //21
    3930* USER_AUDIO_GAIN, //22
    4698* USER_AUDIO_GAIN, //23
    5616* USER_AUDIO_GAIN, //24
    6713* USER_AUDIO_GAIN, //25
    8025* USER_AUDIO_GAIN, //26
    9592* USER_AUDIO_GAIN, //27
    11466* USER_AUDIO_GAIN,//28
    15200* USER_AUDIO_GAIN,//29
    16000* USER_AUDIO_GAIN,//30
    16384* USER_AUDIO_GAIN //31
};


typedef struct {
    u8 max_level;
    u8 state;
    u8 fade;
    u16 *vol_tab;
} dac_digital_volume_t;
dac_digital_volume_t dac_dv;

void dac_digital_vol_open()
{
    dac_dv.vol_tab = dac_dig_vol_tab;
    dac_dv.max_level = ARRAY_SIZE(dac_dig_vol_tab) - 1;
    DV_LOG("dac_digital_vol_open,max_level:%d", dac_dv.max_level);
    dac_dv.state = DAC_DV_STA_OPEN;
}

void dac_digital_vol_tab_register(u16 *tab, u8 max_level)
{
    if (max_level <= 1) {
        DV_LOG("dac_digital_vol_tab_reg err,max_level:%d", max_level);
        return ;
    }
    dac_dv.vol_tab = tab;
    dac_dv.max_level = max_level - 1;
    DV_LOG("dac_digital_vol_tab_reg,max_level:%d", dac_dv.max_level);
}

void dac_digital_vol_set(u16 vol_l, u16 vol_r, u8 fade)
{
    if (dac_dv.state == DAC_DV_STA_OPEN) {
        DV_LOG("dac_digital_vol_set:%d,%d,fade:%d", vol_l, vol_r, fade);
        u16 dvol_l = (vol_l > dac_dv.max_level) ? dac_dv.max_level : vol_l;
        u16 dvol_r = (vol_r > dac_dv.max_level) ? dac_dv.max_level : vol_r;
        if (dac_dv.vol_tab) {
            dvol_l = dac_dv.vol_tab[dvol_l];
            dvol_r = dac_dv.vol_tab[dvol_r];
            DV_LOG("Dvol:%d,%d", JL_AUDIO->DAC_VL0 & 0xFFFF, (JL_AUDIO->DAC_VL0) >> 16 & 0xFFFF);
            DV_LOG("Avol:%d,%d", JL_ADDA->DAA_CON1 & 0xF, (JL_ADDA->DAA_CON1 >> 4) & 0xF);
            DV_LOG("Dvol_new:%d,%d", dvol_l, dvol_r);
            audio_dac_set_L_digital_vol(&dac_hdl, dvol_l);
            audio_dac_set_R_digital_vol(&dac_hdl, dvol_r);
        }
    }
}

void dac_digital_vol_close()
{
    DV_LOG("dac_digital_vol_close\n");
    dac_dv.state = DAC_DV_STA_CLOSE;
}

