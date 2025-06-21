#include "app_config.h"

#ifdef CONFIG_BOARD_AC700N_SD_PC_DEMO

#include "system/includes.h"
#include "media/includes.h"
#include "asm/sdmmc.h"
#include "asm/chargestore.h"
#include "asm/charge.h"
#include "asm/pwm_led.h"
#include "tone_player.h"
#include "audio_config.h"
#include "gSensor/gSensor_manage.h"
#include "key_event_deal.h"
#include "asm/lp_touch_key_api.h"
#include "user_cfg.h"
#include "asm/power/p11.h"
#include "asm/power/p33.h"
#include "dev_manager.h"
#include "usb/otg.h"
#include "audio_link.h"
#include "norflash.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif/*TCFG_AUDIO_ANC_ENABLE*/

#define LOG_TAG_CONST       BOARD
#define LOG_TAG             "[BOARD]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

void board_power_init(void);

/*各个状态下默认的闪灯方式和提示音设置，如果USER_CFG中设置了USE_CONFIG_STATUS_SETTING为1，则会从配置文件读取对应的配置来填充改结构体*/
STATUS_CONFIG status_config = {
    //灯状态设置
    .led = {
        .charge_start  = PWM_LED1_ON,
        .charge_full   = PWM_LED0_ON,
        .power_on      = PWM_LED0_ON,
        .power_off     = PWM_LED1_FLASH_THREE,
        .lowpower      = PWM_LED1_SLOW_FLASH,
        .max_vol       = PWM_LED_NULL,
        .phone_in      = PWM_LED_NULL,
        .phone_out     = PWM_LED_NULL,
        .phone_activ   = PWM_LED_NULL,
        .bt_init_ok    = PWM_LED0_LED1_SLOW_FLASH,
        .bt_connect_ok = PWM_LED0_ONE_FLASH_5S,
        .bt_disconnect = PWM_LED0_LED1_FAST_FLASH,
        .tws_connect_ok = PWM_LED0_LED1_FAST_FLASH,
        .tws_disconnect = PWM_LED0_LED1_SLOW_FLASH,
    },
    //提示音设置
    .tone = {
        .charge_start  = IDEX_TONE_NONE,
        .charge_full   = IDEX_TONE_NONE,
        .power_on      = IDEX_TONE_POWER_ON,
        .power_off     = IDEX_TONE_POWER_OFF,
        .lowpower      = IDEX_TONE_LOW_POWER,
        .max_vol       = IDEX_TONE_MAX_VOL,
        .phone_in      = IDEX_TONE_NONE,
        .phone_out     = IDEX_TONE_NONE,
        .phone_activ   = IDEX_TONE_NONE,
        .bt_init_ok    = IDEX_TONE_BT_MODE,
        .bt_connect_ok = IDEX_TONE_BT_CONN,
        .bt_disconnect = IDEX_TONE_BT_DISCONN,
        .tws_connect_ok   = IDEX_TONE_TWS_CONN,
        .tws_disconnect   = IDEX_TONE_TWS_DISCONN,
    }
};

#define __this (&status_config)

/************************** KEY MSG****************************/
/*各个按键的消息设置，如果USER_CFG中设置了USE_CONFIG_KEY_SETTING为1，则会从配置文件读取对应的配置来填充改结构体*/
u8 key_table[KEY_NUM_MAX][KEY_EVENT_MAX] = {
    // SHORT           LONG              HOLD              UP              DOUBLE           TRIPLE
    {KEY_MUSIC_PP,   KEY_POWEROFF,  KEY_POWEROFF_HOLD,  KEY_NULL,     KEY_MODE_SWITCH,     KEY_NULL},   //KEY_0
    {KEY_MUSIC_NEXT, KEY_VOL_UP,    KEY_VOL_UP,         KEY_NULL,     KEY_OPEN_SIRI,        KEY_NULL},   //KEY_1
    {KEY_MUSIC_PREV, KEY_VOL_DOWN,  KEY_VOL_DOWN,       KEY_NULL,     KEY_HID_CONTROL,      KEY_NULL},   //KEY_2
};

// *INDENT-OFF*
/************************** UART config****************************/
#if TCFG_UART0_ENABLE
UART0_PLATFORM_DATA_BEGIN(uart0_data)
    .tx_pin = TCFG_UART0_TX_PORT,                             //串口打印TX引脚选择
    .rx_pin = TCFG_UART0_RX_PORT,                             //串口打印RX引脚选择
    .baudrate = TCFG_UART0_BAUDRATE,                          //串口波特率

    .flags = UART_DEBUG,                                      //串口用来打印需要把改参数设置为UART_DEBUG
UART0_PLATFORM_DATA_END()
#endif //TCFG_UART0_ENABLE


/************************** CHARGE config****************************/
#if TCFG_CHARGE_ENABLE
CHARGE_PLATFORM_DATA_BEGIN(charge_data)
    .charge_en              = TCFG_CHARGE_ENABLE,              //内置充电使能
    .charge_poweron_en      = TCFG_CHARGE_POWERON_ENABLE,      //是否支持充电开机
    .charge_full_V          = TCFG_CHARGE_FULL_V,              //充电截止电压
    .charge_full_mA			= TCFG_CHARGE_FULL_MA,             //充电截止电流
    .charge_mA				= TCFG_CHARGE_MA,                  //充电电流
/*ldo5v拔出过滤值，过滤时间 = (filter*2 + 20)ms,ldoin<0.6V且时间大于过滤时间才认为拔出
 对于充满直接从5V掉到0V的充电仓，该值必须设置成0，对于充满由5V先掉到0V之后再升压到xV的
 充电仓，需要根据实际情况设置该值大小*/
	.ldo5v_off_filter		= 100,
    .ldo5v_on_filter        = 50,
    .ldo5v_keep_filter      = 220,
    .ldo5v_pulldown_lvl     = CHARGE_PULLDOWN_200K,            //下拉电阻档位选择
    .ldo5v_pulldown_keep    = 1,                               //在仓软关机是否要保持下拉(ldo5v_pulldown_en使能时有效)
//1、对于自动升压充电舱,若充电舱需要更大的负载才能检测到插入时，请将该变量置1,并且根据需求配置下拉电阻档位
//2、对于按键升压,并且是通过上拉电阻去提供维持电压的舱,请将该变量设置1,并且根据舱的上拉配置下拉需要的电阻挡位
//3、对于常5V的舱,可将改变量设为0,省功耗
//4、为LDOIN防止被误触发唤醒,设置为200k下拉
	.ldo5v_pulldown_en		= 1,
CHARGE_PLATFORM_DATA_END()
#endif//TCFG_CHARGE_ENABLE

/************************** IIS ****************************/
#if TCFG_AUDIO_INPUT_IIS || TCFG_AUDIO_OUTPUT_IIS
ALINK_PARM	alink_param = {
    .mclk_io = IO_PORTA_03,
    .sclk_io = IO_PORTA_02,
    .lrclk_io = IO_PORTA_00,
    .ch_cfg[0].data_io = IO_PORTA_04,
    .ch_cfg[1].data_io = IO_PORTB_09,
    /* .ch_cfg[2].data_io = IO_PORTB_09, */
    /* .ch_cfg[3].data_io = IO_PORTB_09, */
    .mode = ALINK_MD_IIS,                           /*iis标准模式*/
    /* .mode = ALINK_MD_IIS_LALIGN, */
#if TCFG_IIS_MODE
    .role = ALINK_ROLE_SLAVE,
#else
    .role = ALINK_ROLE_MASTER,
#endif /*TCFG_IIS_MODE*/
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    /* .bitwide = ALINK_LEN_24BIT, */
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = ALNK_BUF_POINTS_NUM * 2 * 2,    /*循环buf点数*4，乒乓buf点数*8*/
    .sample_rate = TCFG_IIS_SAMPLE_RATE,            /*iis采样率*/
    .buf_mode = ALINK_BUF_CIRCLE,                   /*循环buf*/
    /* .buf_mode = ALINK_BUF_DUAL, */
};
#endif /*TCFG_AUDIO_INPUT_IIS || TCFG_AUDIO_OUTPUT_IIS*/

/************************** DAC ****************************/
#if TCFG_AUDIO_DAC_ENABLE
struct dac_platform_data dac_data = {
    .ldo_volt       = TCFG_AUDIO_DAC_LDO_VOLT,                   //DACVDD等级
    .ldo_volt_high  = TCFG_AUDIO_DAC_LDO_VOLT_HIGH,              //音量增强模式 DACVDD等级
    .vcmo_en        = 0,                                         //是否打开VCOMO
    .output         = TCFG_AUDIO_DAC_CONNECT_MODE,               //DAC输出配置，和具体硬件连接有关，需根据硬件来设置
    .ldo_isel       = 0,                                         //DACLDO 电流档位设置 0:1.25u 1:3.73u 2:6.25u 3:8.75u
    .ldo_fb_isel    = 0,
    .lpf_isel       = 0x2,
#if TCFG_LOWEST_POWER_EN
    .pa_isel        = 0x2,
#else
    .pa_isel        = 0x4,
#endif
    .dsm_clk        = DAC_DSM_6MHz,
    .analog_gain    = MAX_ANA_VOL,
    .light_close    = 0,
    .power_on_mode  = 0,
    .vcm_cap_en     = TCFG_AUDIO_DAC_VCM_CAP_EN,                 // VCM引脚是否有电容  0:没有  1:有
};
#endif

/************************** ADC ****************************/
#if TCFG_AUDIO_ADC_ENABLE
#ifndef TCFG_AUDIO_MIC0_BIAS_EN
#define TCFG_AUDIO_MIC0_BIAS_EN				0
#endif
#ifndef TCFG_AUDIO_MIC1_BIAS_EN
#define TCFG_AUDIO_MIC1_BIAS_EN				0
#endif
#ifndef TCFG_AUDIO_MIC_LDO_EN
#define TCFG_AUDIO_MIC_LDO_EN				0
#endif
struct adc_platform_data adc_data = {
/*MIC工作模式配置*/
	.mic_mode    = TCFG_AUDIO_MIC_MODE,
/*MIC LDO电流档位设置：
    0:5ua    1:3.3ua    2:2.5ua    3:2ua*/
	.mic_ldo_isel   = 1,
/*MIC LDO电压档位设置,也会影响MIC的偏置电压:
    0:1.3v  1:1.4v  2:1.5v  3:2.0v 4:2.2v 5:2.4v 6:2.6v 7:2.8v */
	.mic_ldo_vsel  = 5,
/*MIC上拉电阻配置，影响MIC的偏置电压
    19:1.2K 	18:1.4K 	17:1.8K 	16:2.2K 	15:2.7K		14:3.2K
	13:3.6K 	12:3.9K 	11:4.3K 	10:4.8K  	9:5.3K 	 	8:6.0K
	7:6.6K		6:7.2K		5:7.7K		4:8.2K		3:8.9K		2:9.4K		1:10K	*/
    .mic_bias_res   = 17,
/*MIC电容隔直模式使用内部mic偏置(PA2)*/
	.mic_bias_inside = TCFG_AUDIO_MIC0_BIAS_EN,
/*保持内部mic偏置(PA2)输出*/
	.mic_bias_keep = 0,

/*MIC1工作模式配置*/
	.mic1_mode    = TCFG_AUDIO_MIC1_MODE,
/*MIC1 LDO电流档位设置：
    0:5ua    1:3.3ua    2:2.5ua    3:2ua*/
	.mic1_ldo_isel   = 3,
/*MIC1 LDO电压档位设置,也会影响MIC的偏置电压:
    0:1.3v  1:1.4v  2:1.5v  3:2.0v 4:2.2v 5:2.4v 6:2.6v 7:2.8v */
	.mic1_ldo_vsel  = 5,
/*MIC1上拉电阻配置，影响MIC的偏置电压
    19:1.2K 	18:1.4K 	17:1.8K 	16:2.2K 	15:2.7K		14:3.2K
	13:3.6K 	12:3.9K 	11:4.3K 	10:4.8K  	9:5.3K 	 	8:6.0K
	7:6.6K		6:7.2K		5:7.7K		4:8.2K		3:8.9K		2:9.4K		1:10K	*/
    .mic1_bias_res   = 17,
/*MIC1电容隔直模式使用内部mic偏置(PB7)*/
	.mic1_bias_inside = TCFG_AUDIO_MIC1_BIAS_EN,
/*保持内部mic偏置(PB7)输出*/
	.mic1_bias_keep = 0,
/*MICLDO供电输出到PAD(PA0)控制使能*/
	.mic_ldo_pwr = TCFG_AUDIO_MIC_LDO_EN,
/*adc输入源选择*/
	.adc0_sel = AUDIO_ADC_SEL_AMIC0,
	.adc1_sel = AUDIO_ADC_SEL_AMIC1,
	.adc2_sel = AUDIO_ADC_SEL_DMIC0,
	.adc3_sel = AUDIO_ADC_SEL_DMIC1,
/* adc_reserved0是否使能 */
    .adca_reserved0 = 0,
	.adcb_reserved0 = 0,
};
#endif/*TCFG_AUDIO_ADC_ENABLE*/

/************************** IO KEY ****************************/
#if TCFG_IOKEY_ENABLE
const struct iokey_port iokey_list[] = {
    {
        .connect_way = TCFG_IOKEY_POWER_CONNECT_WAY,          //IO按键的连接方式
        .key_type.one_io.port = TCFG_IOKEY_POWER_ONE_PORT,    //IO按键对应的引脚
        .key_value = 0,                                       //按键值
    },
	{
		.connect_way = TCFG_IOKEY_PREV_CONNECT_WAY,
		.key_type.one_io.port = TCFG_IOKEY_PREV_ONE_PORT,
		.key_value = 1,
	},
    {
        .connect_way = TCFG_IOKEY_NEXT_CONNECT_WAY,
        .key_type.one_io.port = TCFG_IOKEY_NEXT_ONE_PORT,
        .key_value = 2,
    },

};

const struct iokey_platform_data iokey_data = {
    .enable = TCFG_IOKEY_ENABLE,                              //是否使能IO按键
    .num = ARRAY_SIZE(iokey_list),                            //IO按键的个数
    .port = iokey_list,                                       //IO按键参数表
};

#if MULT_KEY_ENABLE
//组合按键消息映射表
//配置注意事项:单个按键按键值需要按照顺序编号,如power:0, prev:1, next:2
//bit_value = BIT(0) | BIT(1) 指按键值为0和按键值为1的两个按键被同时按下,
//remap_value = 3指当这两个按键被同时按下后重新映射的按键值;
const struct key_remap iokey_remap_table[] = {
	{.bit_value = BIT(0) | BIT(1), .remap_value = 3},
	{.bit_value = BIT(0) | BIT(2), .remap_value = 4},
	{.bit_value = BIT(1) | BIT(2), .remap_value = 5},
};

const struct key_remap_data iokey_remap_data = {
	.remap_num = ARRAY_SIZE(iokey_remap_table),
	.table = iokey_remap_table,
};
#endif

#endif

/************************** AD KEY ****************************/
#if TCFG_ADKEY_ENABLE
const struct adkey_platform_data adkey_data = {
    .enable = TCFG_ADKEY_ENABLE,                              //AD按键使能
    .adkey_pin = TCFG_ADKEY_PORT,                             //AD按键对应引脚
    .ad_channel = TCFG_ADKEY_AD_CHANNEL,                      //AD通道值
    .extern_up_en = TCFG_ADKEY_EXTERN_UP_ENABLE,              //是否使用外接上拉电阻
    .ad_value = {                                             //根据电阻算出来的电压值
        TCFG_ADKEY_VOLTAGE0,
        TCFG_ADKEY_VOLTAGE1,
        TCFG_ADKEY_VOLTAGE2,
        TCFG_ADKEY_VOLTAGE3,
        TCFG_ADKEY_VOLTAGE4,
        TCFG_ADKEY_VOLTAGE5,
        TCFG_ADKEY_VOLTAGE6,
        TCFG_ADKEY_VOLTAGE7,
        TCFG_ADKEY_VOLTAGE8,
        TCFG_ADKEY_VOLTAGE9,
    },
    .key_value = {                                             //AD按键各个按键的键值
        TCFG_ADKEY_VALUE0,
        TCFG_ADKEY_VALUE1,
        TCFG_ADKEY_VALUE2,
        TCFG_ADKEY_VALUE3,
        TCFG_ADKEY_VALUE4,
        TCFG_ADKEY_VALUE5,
        TCFG_ADKEY_VALUE6,
        TCFG_ADKEY_VALUE7,
        TCFG_ADKEY_VALUE8,
        TCFG_ADKEY_VALUE9,
    },
};
#endif

/************************** LP TOUCH KEY ****************************/
#if TCFG_LP_TOUCH_KEY_ENABLE
const struct lp_touch_key_platform_data lp_touch_key_config = {
	/*触摸按键*/
	.ch[0].enable = 1,
	.ch[0].port = IO_PORTB_01,
	.ch[0].sensitivity = TCFG_LP_TOUCH_KEY_SENSITIVITY,
	.ch[0].key_value = 0,

#if TCFG_LP_EARTCH_KEY_ENABLE
	/*入耳检测1*/
	.ch[1].enable = 1,
	.ch[1].port = IO_PORTB_02,
	.ch[1].sensitivity = TCFG_EARIN_TOUCH_KEY_SENSITIVITY,
	.ch[1].key_value = 3, //非0xFF, 使用标准入耳检测流程; 0xFF, 使用用户自定义入耳检测流程
#endif /* #if TCFG_LP_EARTCH_KEY_ENABLE */

#if 0
	/*入耳检测温度补偿*/
	.ch[2].enable = 1,
	.ch[2].port = IO_PORTB_02,
	.ch[2].sensitivity = TCFG_EARIN_TOUCH_KEY_SENSITIVITY,
	.ch[2].key_value = 0,

	/*入耳检测2*/
	.ch[3].enable = 1,
	.ch[3].port = IO_PORTB_04,
	.ch[3].sensitivity = TCFG_EARIN_TOUCH_KEY_SENSITIVITY,
	.ch[3].key_value = 3, //非0xFF, 使用标准入耳检测流程; 0xFF, 使用用户自定义入耳检测流程

	/*入耳检测温度补偿2*/
	.ch[4].enable = 1,
	.ch[4].port = IO_PORTB_05,
	.ch[4].sensitivity = TCFG_EARIN_TOUCH_KEY_SENSITIVITY,
	.ch[4].key_value = 0,
#endif
};
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */



/************************** PWM_LED ****************************/
#if TCFG_PWMLED_ENABLE
LED_PLATFORM_DATA_BEGIN(pwm_led_data)
	.io_mode = TCFG_PWMLED_IOMODE,              //推灯模式设置:支持单个IO推两个灯和两个IO推两个灯
	.io_cfg.one_io.pin = TCFG_PWMLED_PIN,       //单个IO推两个灯的IO口配置
LED_PLATFORM_DATA_END()
#endif

#if 0
const struct soft_iic_config soft_iic_cfg[] = {
    //iic0 data
    {
        .scl = TCFG_SW_I2C0_CLK_PORT,                   //IIC CLK脚
        .sda = TCFG_SW_I2C0_DAT_PORT,                   //IIC DAT脚
        .delay = TCFG_SW_I2C0_DELAY_CNT,                //软件IIC延时参数，影响通讯时钟频率
        .io_pu = 1,                                     //是否打开上拉电阻，如果外部电路没有焊接上拉电阻需要置1
    },
#if 0
    //iic1 data
    {
        .scl = IO_PORTA_05,
        .sda = IO_PORTA_06,
        .delay = 50,
        .io_pu = 1,
    },
#endif
};


const struct hw_iic_config hw_iic_cfg[] = {
    //iic0 data
    {
        /*硬件IIC端口下选择
 			    SCL         SDA
		  	{IO_PORT_DP,  IO_PORT_DM},    //group a
        	{IO_PORTC_04, IO_PORTC_05},  //group b
        	{IO_PORTC_02, IO_PORTC_03},  //group c
        	{IO_PORTA_05, IO_PORTA_06},  //group d
         */
        .port = TCFG_HW_I2C0_PORTS,
        .baudrate = TCFG_HW_I2C0_CLK,      //IIC通讯波特率
        .hdrive = 0,                       //是否打开IO口强驱
        .io_filter = 1,                    //是否打开滤波器（去纹波）
        .io_pu = 1,                        //是否打开上拉电阻，如果外部电路没有焊接上拉电阻需要置1
    },
};
#endif

#if TCFG_SD0_ENABLE

SD0_PLATFORM_DATA_BEGIN(sd0_data)
	.port = {
        TCFG_SD0_PORT_CMD,
        TCFG_SD0_PORT_CLK,
        TCFG_SD0_PORT_DA0,
        TCFG_SD0_PORT_DA1,
        TCFG_SD0_PORT_DA2,
        TCFG_SD0_PORT_DA3,
    },
	.data_width             = TCFG_SD0_DAT_MODE,
	.speed                  = TCFG_SD0_CLK,
	.detect_mode            = TCFG_SD0_DET_MODE,
	.priority				= 3,

#if (TCFG_SD0_DET_MODE == SD_IO_DECT)
    .detect_io              = TCFG_SD0_DET_IO,
    .detect_io_level        = TCFG_SD0_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_io_detect,
    .power                  = sd_set_power,
    /* .power                  = NULL, */
#elif (TCFG_SD0_DET_MODE == SD_CLK_DECT)
    .detect_io_level        = TCFG_SD0_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_clk_detect,
    .power                  = sd_set_power,
    /* .power                  = NULL, */
#else
    .detect_func            = sdmmc_cmd_detect,
    .power                  = sd_set_power,
#endif

SD0_PLATFORM_DATA_END()
#endif /* #if TCFG_SD0_ENABLE */

/************************** spi ****************************/
#if TCFG_HW_SPI1_ENABLE
    const struct spi_platform_data spi1_p_data = {
        .port = {
            TCFG_HW_SPI1_PORT_CLK,
            TCFG_HW_SPI1_PORT_DO,
            TCFG_HW_SPI1_PORT_DI,
        },
        .mode = TCFG_HW_SPI1_MODE,
        .clk  = TCFG_HW_SPI1_BAUD,
        .role = TCFG_HW_SPI1_ROLE,
    };
#endif

#if TCFG_HW_SPI2_ENABLE
const struct spi_platform_data spi2_p_data = {
    .port = {
        TCFG_HW_SPI2_PORT_CLK,
        TCFG_HW_SPI2_PORT_DO,
        TCFG_HW_SPI2_PORT_DI,
    },
    .mode = TCFG_HW_SPI2_MODE,
    .clk  = TCFG_HW_SPI2_BAUD,
    .role = TCFG_HW_SPI2_ROLE,
};
#endif

/************************** norflash ****************************/
NORFLASH_DEV_PLATFORM_DATA_BEGIN(norflash_fat_dev_data)
    .spi_hw_num     = TCFG_FLASH_DEV_SPI_HW_NUM,
    .spi_cs_port    = TCFG_FLASH_DEV_SPI_CS_PORT,
    .spi_read_width = 1,
#if (TCFG_FLASH_DEV_SPI_HW_NUM == 1)
    .spi_pdata      = &spi1_p_data,
#elif (TCFG_FLASH_DEV_SPI_HW_NUM == 2)
    .spi_pdata      = &spi2_p_data,
#endif
    .start_addr     = 0,
    .size           = 0,//16*1024*1024,
NORFLASH_DEV_PLATFORM_DATA_END()

/************************** otg data****************************/
#if TCFG_OTG_MODE
struct otg_dev_data otg_data = {
    .usb_dev_en = TCFG_OTG_USB_DEV_EN,
	.slave_online_cnt = TCFG_OTG_SLAVE_ONLINE_CNT,
	.slave_offline_cnt = TCFG_OTG_SLAVE_OFFLINE_CNT,
	.host_online_cnt = TCFG_OTG_HOST_ONLINE_CNT,
	.host_offline_cnt = TCFG_OTG_HOST_OFFLINE_CNT,
	.detect_mode = TCFG_OTG_MODE,
	.detect_time_interval = TCFG_OTG_DET_INTERVAL,
};
#endif

REGISTER_DEVICES(device_table) = {
    /* { "audio", &audio_dev_ops, (void *) &audio_data }, */

#if TCFG_CHARGE_ENABLE
    { "charge", &charge_dev_ops, (void *)&charge_data },
#endif
#if TCFG_SD0_ENABLE
	{ "sd0", 	&sd_dev_ops, 	(void *) &sd0_data},
#endif
#if TCFG_OTG_MODE
    { "otg",     &usb_dev_ops, (void *) &otg_data},
#endif
#if TCFG_NORFLASH_DEV_ENABLE
#if TCFG_HW_SPI1_ENABLE
    { "norflash1", &norflash_dev_ops, (void *)&norflash_fat_dev_data },
#endif
/* #if TCFG_HW_SPI2_ENABLE */
/*     { "norflash2", &norflash_dev_ops, (void *)&spi2_p_data }, */
/* #endif */
#endif

};

/************************** LOW POWER config ****************************/
const struct low_power_param power_param = {
    .config         = TCFG_LOWPOWER_LOWPOWER_SEL,          //0：sniff时芯片不进入低功耗  1：sniff时芯片进入powerdown
    .btosc_hz       = TCFG_CLOCK_OSC_HZ,                   //外接晶振频率
    .delay_us       = TCFG_CLOCK_SYS_HZ / 1000000L,        //提供给低功耗模块的延时(不需要需修改)
    .btosc_disable  = TCFG_LOWPOWER_BTOSC_DISABLE,         //进入低功耗时BTOSC是否保持
    .vddiom_lev     = TCFG_LOWPOWER_VDDIOM_LEVEL,          //强VDDIO等级,可选：2.0V  2.2V  2.4V  2.6V  2.8V  3.0V  3.2V  3.6V
    .vddiow_lev     = TCFG_LOWPOWER_VDDIOW_LEVEL,          //弱VDDIO等级,可选：2.1V  2.4V  2.8V  3.2V
    .osc_type       = TCFG_LOWPOWER_OSC_TYPE,
	.lpctmu_en 		= TCFG_LP_TOUCH_KEY_ENABLE,
	.light_sleep_attribute = TCFG_LOWPOWER_LIGHT_SLEEP_ATTRIBUTE,
};



/************************** PWR config ****************************/
struct port_wakeup port0 = {
    .pullup_down_enable = ENABLE,                            //配置I/O 内部上下拉是否使能
    .edge               = FALLING_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿
    .both_edge          = 0,
    .filter             = PORT_FLT_8ms,
    .iomap              = IO_PORTB_01,                       //唤醒口选择
};

#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
struct port_wakeup port1 = {
    .pullup_down_enable = DISABLE,                            //配置I/O 内部上下拉是否使能
    .edge               = FALLING_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿
    .both_edge          = 1,
    .filter             = PORT_FLT_1ms,
    .iomap              = TCFG_CHARGESTORE_PORT,             //唤醒口选择
};
#endif

#if TCFG_CHARGE_ENABLE
struct port_wakeup charge_port = {
    .edge               = RISING_EDGE,                       //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .both_edge          = 0,
    .filter             = PORT_FLT_16ms,
    .iomap              = IO_CHGFL_DET,                      //唤醒口选择
};

struct port_wakeup vbat_port = {
    .edge               = BOTH_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .both_edge          = 1,
    .filter             = PORT_FLT_16ms,
    .iomap              = IO_VBTCH_DET,                      //唤醒口选择
};

struct port_wakeup ldoin_port = {
    .edge               = BOTH_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .both_edge          = 1,
    .filter             = PORT_FLT_16ms,
    .iomap              = IO_LDOIN_DET,                      //唤醒口选择
};
#endif

const struct wakeup_param wk_param = {
#if (!TCFG_LP_TOUCH_KEY_ENABLE)
	.port[1] = &port0,
#endif
#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
	.port[2] = &port1,
#endif

#if TCFG_CHARGE_ENABLE
    .aport[0] = &charge_port,
    .aport[1] = &vbat_port,
    .aport[2] = &ldoin_port,
#endif
};

void gSensor_wkupup_disable(void)
{
    log_info("gSensor wkup disable\n");
    power_wakeup_index_disable(1);
}

void gSensor_wkupup_enable(void)
{
    log_info("gSensor wkup enable\n");
    power_wakeup_index_enable(1);
}

void debug_uart_init(const struct uart_platform_data *data)
{
#if TCFG_UART0_ENABLE
    if (data) {
        uart_init(data);
    } else {
        uart_init(&uart0_data);
    }
#endif
}


STATUS *get_led_config(void)
{
    return &(__this->led);
}

STATUS *get_tone_config(void)
{
    return &(__this->tone);
}

u8 get_sys_default_vol(void)
{
    return 21;
}

u8 get_power_on_status(void)
{
#if TCFG_IOKEY_ENABLE
    struct iokey_port *power_io_list = NULL;
    power_io_list = iokey_data.port;

    if (iokey_data.enable) {
        if (gpio_read(power_io_list->key_type.one_io.port) == power_io_list->connect_way){
            return 1;
        }
    }
#endif

#if TCFG_ADKEY_ENABLE
    if (adkey_data.enable) {
    	return 1;
    }
#endif

#if TCFG_LP_TOUCH_KEY_ENABLE
	return lp_touch_key_power_on_status();
#endif

    return 0;
}

static void board_devices_init(void)
{
#if TCFG_PWMLED_ENABLE
    pwm_led_init(&pwm_led_data);
#endif

#if (TCFG_IOKEY_ENABLE || TCFG_ADKEY_ENABLE)
	key_driver_init();
#endif

#if TCFG_UART_KEY_ENABLE
	extern int uart_key_init(void);
	uart_key_init();
#endif /* #if TCFG_UART_KEY_ENABLE */

#if TCFG_LP_TOUCH_KEY_ENABLE
	lp_touch_key_init(&lp_touch_key_config);
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */

#if (!TCFG_CHARGE_ENABLE)
    CHARGE_EN(0);
    CHGGO_EN(0);
#endif
}

extern void sdx_dev_detect_modify(u32 modify_time);
void sniff_hook(u32 slot,u8 more_num,u8 first_num,int t_sniff)
{

	if(more_num){
		if(first_num){
		   /* printf("^%d",t_sniff ); */
	       sdx_dev_detect_modify(t_sniff-12);//498ms检测一次sd卡插入
		}
	}else{
		   /* printf("^%d",t_sniff ); */
	    sdx_dev_detect_modify(t_sniff-3);//498ms检测一次sd卡插入
	}
}

extern void cfg_file_parse(u8 idx);
void board_init()
{
    board_power_init();
    adc_vbg_init();
    adc_init();
#if TCFG_AUDIO_ANC_ENABLE
	anc_init();
#endif/*TCFG_AUDIO_ANC_ENABLE*/
    cfg_file_parse(0);


	/********************M\N\O\P版芯片电池电压低于vddio设置电压时采集异常，需要把关机电压设置高于vddio电压************/
#define IC_VER_M		0x6d0c
#define IC_VER_N		0x6d0d
#define IC_VER_O		0x6d0e
#define IC_VER_P		0x6d0f
#include "app_main.h"
	if (JL_SYSTEM->CHIP_ID == IC_VER_M ||\
			JL_SYSTEM->CHIP_ID == IC_VER_N ||\
			JL_SYSTEM->CHIP_ID == IC_VER_O ||\
			JL_SYSTEM->CHIP_ID == IC_VER_P) {
		if((TCFG_LOWPOWER_VDDIOM_LEVEL==VDDIOM_VOL_34V)&&app_var.poweroff_tone_v<=340){
			app_var.poweroff_tone_v=340;
            app_var.warning_tone_v = 350;
			/* puts("app_var.poweroff_tone_v=340\n"); */
		}else if(TCFG_LOWPOWER_VDDIOM_LEVEL==VDDIOM_VOL_32V&&app_var.poweroff_tone_v<=320){
			app_var.poweroff_tone_v=320;
            app_var.warning_tone_v = 330;
			/* puts("app_var.poweroff_tone_v=320\n"); */
		}
		printf("waring----------rset_poweroff_value=%d\n",app_var.poweroff_tone_v);
	}
	/*************************************************************************************************************/



#if TCFG_SD0_ENABLE
	dev_manager_init();
#else
	devices_init();
#endif //TCFG_SD0_ENABLE

	board_devices_init();

	if(get_charge_online_flag()){
        power_set_mode(PWR_LDO15);
    }else{
    	power_set_mode(TCFG_LOWPOWER_POWER_SEL);
    }

 #if TCFG_UART0_ENABLE
    if (uart0_data.rx_pin < IO_MAX_NUM) {
        gpio_set_die(uart0_data.rx_pin, 1);
    }
#endif

#ifdef AUDIO_PCM_DEBUG
	extern void uartSendInit();
	uartSendInit();
#endif/*AUDIO_PCM_DEBUG*/

}

enum {
    PORTA_GROUP = 0,
    PORTB_GROUP,
    PORTC_GROUP,
    PORTD_GROUP,
	PORTP_GROUP,
};

static void port_protect(u16 *port_group, u32 port_num)
{
    if (port_num == NO_CONFIG_PORT) {
        return;
    }
    port_group[port_num / IO_GROUP_NUM] &= ~BIT(port_num % IO_GROUP_NUM);
}

static void close_gpio(u8 is_softoff)
{

	u16 port_group[] = {
        [PORTA_GROUP] = 0x1ff,
        [PORTB_GROUP] = 0x3ff,//
        [PORTC_GROUP] = 0x7f,//
       	[PORTD_GROUP] = 0x7f,
		[PORTP_GROUP] = 0x1,//
    };

    extern u32 spi_get_port(void);
	if(spi_get_port() == 0){
		port_group[PORTD_GROUP] &= ~0x1f;
	}else if(spi_get_port() == 1){
		port_group[PORTD_GROUP] &= ~0x73;
	}

	if(get_sfc_bit_mode()==4){
		port_group[PORTC_GROUP] &= ~BIT(6);
		port_group[PORTA_GROUP] &= ~BIT(7);
	}

#if TCFG_IOKEY_ENABLE
    port_protect(port_group, TCFG_IOKEY_POWER_ONE_PORT);
#endif

#if (!TCFG_LP_TOUCH_KEY_ENABLE)
   	//默认唤醒io
	port_protect(port_group, IO_PORTB_01);
#endif

#if TCFG_PWMLED_ENABLE
	if(!is_softoff){
		port_protect(port_group,TCFG_PWMLED_PIN);
	}
#endif /*TCFG_PWMLED_ENABLE*/

#if TCFG_AUDIO_ANC_ENABLE
	if(anc_status_get() && (!is_softoff)){
		//ANC 低功耗状态下需要保持的IO口
		/* port_protect(port_group, TCFG_AUDIO_MIC_PWR_PORT); */
	}
#endif/*TCFG_AUDIO_ANC_ENABLE*/

	gpio_dir(GPIOA, 0, 9, port_group[PORTA_GROUP], GPIO_OR);
    gpio_set_pu(GPIOA, 0, 9, ~port_group[PORTA_GROUP], GPIO_AND);
    gpio_set_pd(GPIOA, 0, 9, ~port_group[PORTA_GROUP], GPIO_AND);
    gpio_die(GPIOA, 0, 9, ~port_group[PORTA_GROUP], GPIO_AND);
    gpio_dieh(GPIOA, 0, 9, ~port_group[PORTA_GROUP], GPIO_AND);

	gpio_dir(GPIOB, 0, 10, port_group[PORTB_GROUP], GPIO_OR);
    gpio_set_pu(GPIOB, 0, 10, ~port_group[PORTB_GROUP], GPIO_AND);
    gpio_set_pd(GPIOB, 0, 10, ~port_group[PORTB_GROUP], GPIO_AND);
    gpio_die(GPIOB, 0, 10, ~port_group[PORTB_GROUP], GPIO_AND);
    gpio_dieh(GPIOB, 0, 10, ~port_group[PORTB_GROUP], GPIO_AND);

	gpio_dir(GPIOC, 0, 7, port_group[PORTC_GROUP], GPIO_OR);
    gpio_set_pu(GPIOC, 0, 7, ~port_group[PORTC_GROUP], GPIO_AND);
    gpio_set_pd(GPIOC, 0, 7, ~port_group[PORTC_GROUP], GPIO_AND);
    gpio_die(GPIOC, 0, 7, ~port_group[PORTC_GROUP], GPIO_AND);
    gpio_dieh(GPIOC, 0, 7, ~port_group[PORTC_GROUP], GPIO_AND);

	gpio_dir(GPIOD, 0, 7, port_group[PORTD_GROUP], GPIO_OR);
    gpio_set_pu(GPIOD, 0, 7, ~port_group[PORTD_GROUP], GPIO_AND);
    gpio_set_pd(GPIOD, 0, 7, ~port_group[PORTD_GROUP], GPIO_AND);
    gpio_die(GPIOD, 0, 7, ~port_group[PORTD_GROUP], GPIO_AND);
    gpio_dieh(GPIOD, 0, 7, ~port_group[PORTD_GROUP], GPIO_AND);

	gpio_dir(GPIOP, 0, 1, port_group[PORTP_GROUP], GPIO_OR);
    gpio_set_pu(GPIOP, 0, 1, ~port_group[PORTP_GROUP], GPIO_AND);
    gpio_set_pd(GPIOP, 0, 1, ~port_group[PORTP_GROUP], GPIO_AND);
    gpio_die(GPIOP, 0, 1, ~port_group[PORTP_GROUP], GPIO_AND);
    gpio_dieh(GPIOP, 0, 1, ~port_group[PORTP_GROUP], GPIO_AND);

	gpio_set_pull_up(IO_PORT_DP, 0);
    gpio_set_pull_down(IO_PORT_DP, 0);
    gpio_set_die(IO_PORT_DP, 0);
    gpio_set_dieh(IO_PORT_DP, 0);
    gpio_set_direction(IO_PORT_DP, 1);

    gpio_set_pull_up(IO_PORT_DM, 0);
    gpio_set_pull_down(IO_PORT_DM, 0);
    gpio_set_die(IO_PORT_DM, 0);
    gpio_set_dieh(IO_PORT_DM, 0);
    gpio_set_direction(IO_PORT_DM, 1);

#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
    if (is_softoff) {
	    power_wakeup_index_disable(2);
        gpio_set_pull_up(TCFG_CHARGESTORE_PORT, 0);
        gpio_set_pull_down(TCFG_CHARGESTORE_PORT, 0);
        gpio_set_die(TCFG_CHARGESTORE_PORT, 0);
        gpio_set_dieh(TCFG_CHARGESTORE_PORT, 0);
        gpio_set_direction(TCFG_CHARGESTORE_PORT, 1);
    } else {
        gpio_set_pull_up(TCFG_CHARGESTORE_PORT, 0);
        gpio_set_pull_down(TCFG_CHARGESTORE_PORT, 0);
        gpio_set_die(TCFG_CHARGESTORE_PORT, 1);
        gpio_set_dieh(TCFG_CHARGESTORE_PORT, 1);
        gpio_set_direction(TCFG_CHARGESTORE_PORT, 1);
    }
#endif

	P11_PORT_CON0 = 0;
}

//maskrom 使用到的io
static void mask_io_cfg()
{
    struct boot_soft_flag_t boot_soft_flag = {0};
    boot_soft_flag.flag0.boot_ctrl.wdt_dis = 0;
    boot_soft_flag.flag0.boot_ctrl.poweroff = 0;
    boot_soft_flag.flag0.boot_ctrl.is_port_b = JL_IOMAP->CON0 & BIT(16) ? 1 : 0;

    boot_soft_flag.flag1.misc.usbdm = SOFTFLAG_HIGH_RESISTANCE;
    boot_soft_flag.flag1.misc.usbdp = SOFTFLAG_HIGH_RESISTANCE;

    boot_soft_flag.flag1.misc.uart_key_port = 0;
    boot_soft_flag.flag1.misc.ldoin = SOFTFLAG_HIGH_RESISTANCE;

    boot_soft_flag.flag2.pa7_pb4.pa7 = SOFTFLAG_HIGH_RESISTANCE;
    boot_soft_flag.flag2.pa7_pb4.pb4 = SOFTFLAG_HIGH_RESISTANCE;

    boot_soft_flag.flag3.pc3_pc5.pc3 = SOFTFLAG_HIGH_RESISTANCE;
    boot_soft_flag.flag3.pc3_pc5.pc5 = SOFTFLAG_HIGH_RESISTANCE;
    mask_softflag_config(&boot_soft_flag);

}

/*进软关机之前默认将IO口都设置成高阻状态，需要保留原来状态的请修改该函数*/
extern void dac_power_off(void);
void board_set_soft_poweroff(void)
{
	mask_io_cfg();

	close_gpio(1);

    dac_power_off();
}

#define     APP_IO_DEBUG_0(i,x)       //{JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT &= ~BIT(x);}
#define     APP_IO_DEBUG_1(i,x)       //{JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT |= BIT(x);}

void sleep_exit_callback(u32 usec)
{
    putchar('>');
    APP_IO_DEBUG_1(A, 6);
}
void sleep_enter_callback(u8  step)
{
    /* 此函数禁止添加打印 */
    if (step == 1) {
        putchar('<');
        APP_IO_DEBUG_0(A, 6);
		/* dac_power_off(); */
    } else {
	    close_gpio(0);
	}
}

static void wl_audio_clk_on(void)
{
    JL_WL_AUD->CON0 = 1;
}

static void port_wakeup_callback(u8 index, u8 gpio)
{
	/* log_info("%s:%d,%d",__FUNCTION__,index,gpio); */

	switch (index) {
#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
		case 2:
			extern void chargestore_ldo5v_fall_deal(void);
			chargestore_ldo5v_fall_deal();
			break;
#endif
	}
}

static void aport_wakeup_callback(u8 index, u8 gpio, u8 edge)
{
#if TCFG_CHARGE_ENABLE
    switch (gpio) {
        case IO_CHGFL_DET://charge port
            charge_wakeup_isr();
            break;
        case IO_VBTCH_DET://vbat port
        case IO_LDOIN_DET://ldoin port
            ldoin_wakeup_isr();
            break;
    }
#endif
}

void board_power_init(void)
{
    //disable PA6 PC3 USB_IO by ic
    {
        gpio_set_die(IO_PORTA_05, 0);
        gpio_set_dieh(IO_PORTA_05, 0);

        gpio_set_die(IO_PORTA_06, 0);
        gpio_set_dieh(IO_PORTA_06, 0);

        gpio_set_die(IO_PORTC_03, 0);
        gpio_set_dieh(IO_PORTC_03, 0);

        gpio_set_die(IO_PORT_DP, 0);
        gpio_set_dieh(IO_PORT_DP, 0);

		/************mic**************/
		gpio_set_pull_up(IO_PORTC_04, 0);
		gpio_set_pull_down(IO_PORTC_04, 0);
		gpio_set_die(IO_PORTC_04, 0);
		gpio_set_dieh(IO_PORTC_04, 0);

    }
    log_info("Power init : %s", __FILE__);

    power_init(&power_param);

    power_set_callback(TCFG_LOWPOWER_LOWPOWER_SEL, sleep_enter_callback, sleep_exit_callback, board_set_soft_poweroff);

    wl_audio_clk_on();

    power_keep_dacvdd_en(0);

    power_wakeup_init(&wk_param);

    aport_edge_wkup_set_callback(aport_wakeup_callback);
    port_edge_wkup_set_callback(port_wakeup_callback);

#if (!TCFG_IOKEY_ENABLE && !TCFG_ADKEY_ENABLE)
    charge_check_and_set_pinr(1);
#endif
}
#endif /* #ifdef CONFIG_BOARD_AC700N_SD_PC_DEMO */
