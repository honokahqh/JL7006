/**@file  	    power_reset.h
* @brief    	电源、低功耗接口
* @date     	2021-8-27
* @version  	V1.0
 */

#ifndef __POWER_API__
#define __POWER_API__

#define NEW_BASEBAND_COMPENSATION       0

#define AT_VOLATILE_RAM             AT(.volatile_ram)
#define AT_VOLATILE_RAM_CODE        AT(.volatile_ram_code)

enum {
    OSC_TYPE_LRC = 0,
    OSC_TYPE_BT_OSC,
};

enum {
    PWR_NO_CHANGE = 0,
    PWR_LDO33,
    PWR_LDO15,
    PWR_DCDC15,
};

enum {
    VDDIOM_VOL_20V = 0,
    VDDIOM_VOL_22V,
    VDDIOM_VOL_24V,
    VDDIOM_VOL_26V,
    VDDIOM_VOL_28V,
    VDDIOM_VOL_30V, //default
    VDDIOM_VOL_32V,
    VDDIOM_VOL_34V,
};

enum {
    VDDIOW_VOL_20V = 0,
    VDDIOW_VOL_22V,
    VDDIOW_VOL_24V,
    VDDIOW_VOL_26V,
    VDDIOW_VOL_28V,
    VDDIOW_VOL_30V,
    VDDIOW_VOL_32V,
    VDDIOW_VOL_34V,
};

enum {
    DCDC_L_10uH = 0,
    DCDC_L_4p7uH,
};

struct low_power_param {
    u8 osc_type;
    u32 btosc_hz;
    u8  delay_us;
    u8  config;
    u8  btosc_disable;

    u8 vddiom_lev;
    u8 vddiow_lev;
    u8 pd_wdvdd_lev;
    u8 lpctmu_en;
    u8 vddio_keep;

    u32 osc_delay_us;

    u8 rtc_clk;
    u8 light_sleep_attribute;
    u8 dcdc_L;
};

struct soft_flag0_t {
    u8 wdt_dis: 1;
    u8 poweroff: 1;
    u8 is_port_b: 1;
    u8 lvd_en: 1;
    u8 wvio_fbres_en: 1;
    u8 fast_boot: 1;
    u8 res: 2;
};

struct soft_flag1_t {
    u8 usbdp: 2;
    u8 usbdm: 2;
    u8 uart_key_port: 1;
    u8 ldoin: 3;
};

struct soft_flag2_t {
    u8 pa7: 4;
    u8 pb4: 4;
};

struct soft_flag3_t {
    u8 pc3: 4;
    u8 pc5: 4;
};

struct boot_soft_flag_t {
    union {
        struct soft_flag0_t boot_ctrl;
        u8 value;
    } flag0;
    union {
        struct soft_flag1_t misc;
        u8 value;
    } flag1;
    union {
        struct soft_flag2_t pa7_pb4;
        u8 value;
    } flag2;
    union {
        struct soft_flag3_t pc3_pc5;
        u8 value;
    } flag3;
};

enum soft_flag_io_stage {
    SOFTFLAG_HIGH_RESISTANCE,
    SOFTFLAG_PU,
    SOFTFLAG_PD,

    SOFTFLAG_OUT0,
    SOFTFLAG_OUT0_HD0,
    SOFTFLAG_OUT0_HD,
    SOFTFLAG_OUT0_HD0_HD,

    SOFTFLAG_OUT1,
    SOFTFLAG_OUT1_HD0,
    SOFTFLAG_OUT1_HD,
    SOFTFLAG_OUT1_HD0_HD,
};

struct low_power_operation {

    const char *name;

    u32(*get_timeout)(void *priv);

    void (*suspend_probe)(void *priv);

    void (*suspend_post)(void *priv, u32 usec);

    void (*resume)(void *priv, u32 usec);

    void (*resume_post)(void *priv, u32 usec);
};

/*-----------------------------------------------------------*/
void p11_init(void);
void power_init(const struct low_power_param *param);
void power_set_mode(u8 mode);
void power_set_charge_mode(u8 mode);
void power_keep_dacvdd_en(u8 en);
void power_set_callback(u8 mode, void (*powerdown_enter)(u8 step), void (*powerdown_exit)(u32), void (*soft_poweroff_enter)(void));
void sdpg_config(int enable);

/*-----------------------------------------------------------*/
void mask_softflag_config(const struct boot_soft_flag_t *softflag);

void power_set_soft_poweroff();

/*-----------------------------------------------------------*/
void low_power_request(void);

void *low_power_get(void *priv, const struct low_power_operation *ops);

#define  power_is_poweroff_post()   0

void low_power_put(void *priv);

void low_power_sys_request(void *priv);

void *low_power_sys_get(void *priv, const struct low_power_operation *ops);

void low_power_sys_put(void *priv);

u8 low_power_sys_is_idle(void);

s32 low_power_trace_drift(u32 usec);

void low_power_reset_osc_type(u8 type);

u8 low_power_get_default_osc_type(void);

u8 low_power_get_osc_type(void);

u32 lower_power_bt_group_query();

u32 lower_power_sys_group_query();

u32 low_power_sys_not_idle_cnt(void);

/*-----------------------------------------------------------*/
u8 get_wvdd_trim_level();

u8 get_pvdd_trim_level();

void update_wvdd_pvdd_trim_level(u8 wvdd_level, u8 pvdd_level);

u8 check_wvdd_pvdd_trim(u8 tieup);

/*-----------------------------------------------------------*/
u8 power_set_vddio_keep(u8 en);

//配置Low power target 睡眠深度
// -- LOW_POWER_MODE_SLEEP : 系统掉电，RAM 进入省电模式，数字逻辑不掉电，模拟掉电
// -- LOW_POWER_MODE_LIGHT_SLEEP : 系统不掉电，BTOSC 保持，系统运行RC
// -- LOW_POWER_MODE_DEEP_SLEEP : 数字逻辑不掉电，模拟掉电
enum LOW_POWER_LEVEL {
    LOW_POWER_MODE_SLEEP = 0,
    LOW_POWER_MODE_LIGHT_SLEEP,
    LOW_POWER_MODE_DEEP_SLEEP,
};

#define LOWPOWER_LIGHT_SLEEP_ATTRIBUTE_KEEP_CLOCK 		BIT(0)

typedef u8(*idle_handler_t)(void);
typedef enum LOW_POWER_LEVEL(*level_handler_t)(void);

struct lp_target {
    char *name;
    level_handler_t level;
    idle_handler_t is_idle;
};

#define REGISTER_LP_TARGET(target) \
        const struct lp_target target sec(.lp_target)


extern const struct lp_target lp_target_begin[];
extern const struct lp_target lp_target_end[];

#define list_for_each_lp_target(p) \
    for (p = lp_target_begin; p < lp_target_end; p++)
/*-----------------------------------------------------------*/

struct deepsleep_target {
    char *name;
    u8(*enter)(void);
    u8(*exit)(void);
};

#define DEEPSLEEP_TARGET_REGISTER(target) \
        const struct deepsleep_target target sec(.deepsleep_target)


extern const struct deepsleep_target deepsleep_target_begin[];
extern const struct deepsleep_target deepsleep_target_end[];

#define list_for_each_deepsleep_target(p) \
    for (p = deepsleep_target_begin; p < deepsleep_target_end; p++)
/*-----------------------------------------------------------*/

#endif


