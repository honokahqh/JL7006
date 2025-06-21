/**@file  	    power_reset.h
* @brief    	复位、唤醒接口
* @date     	2021-8-27
* @version  	V1.0
 */

#ifndef __POWER_RESET_H__
#define __POWER_RESET_H__

/*复位原因*/
enum RST_REASON {
    /*主系统*/
    MSYS_P11_RST,
    MSYS_DVDD_POR_RST,
    MSYS_SOFT_RST,
    MSYS_P2M_RST,
    MSYS_POWER_RETURN,
    /*P11*/
    P11_PVDD_POR_RST,
    P11_IVS_RST,
    P11_P33_RST,
    P11_WDT_RST,
    P11_SOFT_RST,
    P11_MSYS_RST = 10,
    P11_POWER_RETURN,
    /*P33*/
    P33_VDDIO_POR_RST,
    P33_VDDIO_LVD_RST,
    P33_VCM_RST,
    P33_PPINR_RST,
    P33_P11_RST,
    P33_SOFT_RST,
    P33_PPINR1_RST,
    P33_POWER_RETURN,
    /*SUB*/
    P33_EXCEPTION_SOFT_RST = 20,
    P33_ASSERT_SOFT_RST,
    //P33 WAKEUP
    PWR_WK_REASON_PLUSE_CNT_OVERFLOW,
    PWR_WK_REASON_PORT_EDGE,
    PWR_WK_REASON_ANA_EDGE,
    PWR_WK_REASON_VDDIO_LVD,
    PWR_WK_REASON_EDGE_INDEX0,
    PWR_WK_REASON_EDGE_INDEX1,
    PWR_WK_REASON_EDGE_INDEX2,
    PWR_WK_REASON_EDGE_INDEX3,
    PWR_WK_REASON_EDGE_INDEX4 = 30,
    PWR_WK_REASON_EDGE_INDEX5,
    PWR_WK_REASON_EDGE_INDEX6,
    PWR_WK_REASON_EDGE_INDEX7,
    PWR_WK_REASON_EDGE_INDEX8,
    PWR_WK_REASON_EDGE_INDEX9,
    PWR_WK_REASON_EDGE_INDEX10,
    PWR_WK_REASON_EDGE_INDEX11,
};

void power_reset_close();

u8 power_reset_source_dump(void);

u8 p11_wakeup_query(void);

void p33_soft_reset(void);

void set_reset_source_value(enum RST_REASON index);

u32 get_reset_source_value(void);

u8 is_reset_source(enum RST_REASON index);

int cpu_reset_by_soft();

void latch_reset(void);
#endif
