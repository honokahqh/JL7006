#ifndef _CPU_CLOCK_
#define _CPU_CLOCK_

#include "typedef.h"

#include "clock_hw.h"

///原生时钟源作系统时钟源
#define   SYS_CLOCK_INPUT_RC            0
#define   SYS_CLOCK_INPUT_BT_OSC        1          //BTOSC 双脚(12-26M)
#define   SYS_CLOCK_INPUT_RTOSCH        2
#define   SYS_CLOCK_INPUT_RTOSCL        3
#define   SYS_CLOCK_INPUT_PAT           4
///衍生时钟源作系统时钟源
#define         SYS_CLOCK_INPUT_PLL_BT_OSC  5
#define         SYS_CLOCK_INPUT_PLL_RTOSCH  6
#define         SYS_CLOCK_INPUT_PLL_PAT     7
#define         SYS_CLOCK_INPUT_PLL_RCL     8

typedef int SYS_CLOCK_INPUT;

typedef enum {
    SYS_ICLOCK_INPUT_BTOSC,          //BTOSC 双脚(12-26M)
    SYS_ICLOCK_INPUT_RTOSCH,
    SYS_ICLOCK_INPUT_RTOSCL,
    SYS_ICLOCK_INPUT_PAT,
} SYS_ICLOCK_INPUT;

typedef enum {
    ALINK_CLOCK_12M288K,  //160M div 13, 48k采样率类型
    ALINK_CLOCK_11M2896K, //92M div 17, 44.1k采样率类型
} ALINK_INPUT_CLK_TYPE;
/*
 * system enter critical and exit critical handle
 * */
struct clock_critical_handler {
    void (*enter)();
    void (*exit)();
};

#define CLOCK_CRITICAL_HANDLE_REG(name, enter, exit) \
	const struct clock_critical_handler clock_##name \
		 SEC_USED(.clock_critical_txt) = {enter, exit};

extern struct clock_critical_handler clock_critical_handler_begin[];
extern struct clock_critical_handler clock_critical_handler_end[];

#define list_for_each_loop_clock_critical(h) \
	for (h=clock_critical_handler_begin; h<clock_critical_handler_end; h++)


int clk_early_init(u8 sys_in, u32 input_freq, u32 out_freq);

int clk_get(const char *name);

int clk_set(const char *name, int clk);

int clk_set_sys_lock(int clk, int lock_en);

enum CLK_OUT_SOURCE {
    NONE = 0,
    SFC_CLK_OUT,
    HSB_CLK_OUT,
    LSB_CLK_OUT,
    STD_48M_CLK_OUT,
    STD_24M_CLK_OUT,
    RC16M_CLK_OUT,
    LRC_CLK_OUT,
    RTC_OSC_CLK_OUT,
    BTOSC_24M_CLK_OUT,
    BTOSC_48M_CLK_OUT,
    XOSC_FSCL_CLK_OUT,
    P33_RCLK_CLK_OUT,
    PLL_ALINK0_CLK_OUT,
    PLL_D4P5_CLK_OUT,
    PLL_75M_CLK_OUT,
};

void clk_out(u8 gpio, enum CLK_OUT_SOURCE clk);

void clock_dump(void);

#define MHz	(1000000L)
enum sys_clk {
    SYS_24M = 24 * MHz,
    SYS_32M = 32 * MHz,
    SYS_48M = 48 * MHz,
    SYS_60M = 60 * MHz,
    SYS_76M = 76800000,
    SYS_96M = 96 * MHz,
};

enum clk_mode {
    CLOCK_MODE_ADAPTIVE = 0,
    CLOCK_MODE_USR,
};

//clk : SYS_48M / SYS_24M
void sys_clk_set(enum sys_clk clk);

void clk_voltage_init(u8 mode, u8 sys_dvdd);


void clk_set_osc_cap(u8 sel_l, u8 sel_r);

u32 clk_get_osc_cap();

void audio_link_clock_sel(ALINK_INPUT_CLK_TYPE type);

void clock_set_pll_target_frequency(u32 freq);	//配置PLL_TARGET_FREQUENCY

u32 clock_get_pll_target_frequency();			//获取PLL_TARGET_FREQUENCY

/* ***************************************************************************/
/**
 * \Brief :         频率电压适配模式接口，支持动态配置频率电压为自适应或用户设置
 *
 * \Param :         mode    : CLOCK_MODE_ADAPTIVE 频率电压自适应使能 / CLOCK_MODE_USR 频率电压用户控制
 * \Param :         sys_dvdd: 用户设置值
 */
/* *****************************************************************************/
void clk_voltage_mode(u8 mode, u8 sys_dvdd);
/**
 * @brief clock_set_sfc_max_freq
 * 使用前需要保证所使用的flash支持4bit 100Mhz 模式
 *
 * @param dual_max_freq for cmd 3BH BBH
 * @param quad_max_freq for cmd 6BH EBH
 */
void clock_set_sfc_max_freq(u32 dual_max_freq, u32 quad_max_freq);

/**
 * @brief clock_set_lowest_voltage 设置dvdd工作的最低 工作电压
 *
 * @param dvdd_lev  mic 工作时候 建议 SYSVDD_VOL_SEL_105V，关闭的时候设置为 SYSVDD_VOL_SEL_084V
 */
void clock_set_lowest_voltage(u32 dvdd_lev);
#endif

