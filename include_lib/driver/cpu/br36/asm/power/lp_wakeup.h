/**@file  	    power_reset.h
* @brief    	低功耗唤醒接口
* @date     	2021-8-27
* @version  	V1.0
 */
#ifndef __LP_WAKEUP_H__
#define __LP_WAKEUP_H__

#define RISING_EDGE         0
#define FALLING_EDGE        1
#define BOTH_EDGE           2

//P3_PORT_FLT
enum {
    PORT_FLT_NULL = 0,
    PORT_FLT_256us,
    PORT_FLT_512us,
    PORT_FLT_1ms,
    PORT_FLT_2ms,
    PORT_FLT_4ms,
    PORT_FLT_8ms,
    PORT_FLT_16ms,
};


#define MAX_WAKEUP_PORT     12
#define MAX_WAKEUP_ANA_PORT 3

struct port_wakeup {
    u8 pullup_down_enable;
    u8 edge;
    u8 both_edge;
    u8 filter;
    u8 iomap;
};

struct wakeup_param {
    const struct port_wakeup *port[MAX_WAKEUP_PORT];
    const struct port_wakeup *aport[MAX_WAKEUP_ANA_PORT];
};

void power_wakeup_index_enable(u8 index);

void power_wakeup_index_disable(u8 index);

void power_wakeup_disable_with_port(u8 port);

void power_wakeup_enable_with_port(u8 port);

void power_wakeup_set_edge(u8 port_io, u8 edge);

void power_awakeup_index_enable(u8 index);

void power_awakeup_index_disable(u8 index);

void power_awakeup_disable_with_port(u8 port);

void power_awakeup_enable_with_port(u8 port);

void power_wakeup_init(const struct wakeup_param *param);

void port_edge_wkup_set_callback(void (*wakeup_callback)(u8 index, u8 gpio));

void aport_edge_wkup_set_callback(void (*wakeup_callback)(u8 index, u8 gpio, u8 edge));

void alm_wkup_set_callback(void (*wakeup_callback)(u8 index));

void power_wakeup_init_test();

u8 get_wakeup_source(void);

u8 is_ldo5v_wakeup(void);

u8 is_alarm_wakeup(void);

void p33_soft_reset(void);

#endif
