// *INDENT-OFF*
#include "app_config.h"

/* =================  BR34 SDK memory ========================
 _______________ ___ 0x2C000(176K)
|   isr base    |
|_______________|___ _IRQ_MEM_ADDR(size = 0x100)
|rom export ram |
|_______________|
|    update     |
|_______________|___ RAM_LIMIT_H
|     HEAP      |
|_______________|___ data_code_pc_limit_H
| audio overlay |
|_______________|
|   data_code   |
|_______________|___ data_code_pc_limit_L
|     bss       |
|_______________|
|     data      |
|_______________|
|   irq_stack   |
|_______________|
|   boot info   |
|_______________|
|     TLB       |
|_______________|0 RAM_LIMIT_L

 =========================================================== */

#include "maskrom_stubs.ld"

EXTERN(
_start
#include "sdk_used_list.c"
);

//============ About RAM ================

UPDATA_SIZE     = 0x80;
UPDATA_BEG      = _MASK_MEM_BEGIN - UPDATA_SIZE;
UPDATA_BREDR_BASE_BEG = 0x2c000;

RAM_LIMIT_L     = 0;
RAM_LIMIT_H     = UPDATA_BEG;
PHY_RAM_SIZE    = RAM_LIMIT_H - RAM_LIMIT_L;

//from mask export
ISR_BASE       = _IRQ_MEM_ADDR;
ROM_RAM_SIZE   = _MASK_MEM_SIZE;
ROM_RAM_BEG    = _MASK_MEM_BEGIN;

RAM0_BEG 		= RAM_LIMIT_L;
RAM0_END 		= RAM_LIMIT_H;
RAM0_SIZE 		= RAM0_END - RAM0_BEG;

//============ About EQ coeff RAM ================
//internal EQ priv ram
EQ_PRIV_COEFF_BASE  = 0x02C600;//0x02E6A0;
EQ_PRIV_SECTION_NUM = 10;
EQ_PRIV_COEFF_END   = EQ_PRIV_COEFF_BASE + 4 * EQ_PRIV_SECTION_NUM * (5+3)*2;

//=============== About BT RAM ===================
//CONFIG_BT_RX_BUFF_SIZE = (1024 * 18);


//============ About CODE ================
#ifdef CONFIG_NEW_CFG_TOOL_ENABLE
CODE_BEG        = 0x1E00100;
#else
CODE_BEG        = 0x1E00120;
#endif

MEMORY
{
	code0(rx)       : ORIGIN = CODE_BEG,        LENGTH = CONFIG_FLASH_SIZE
	ram0(rwx)       : ORIGIN = RAM0_BEG,        LENGTH = RAM0_SIZE
}


ENTRY(_start)

SECTIONS
{

    . = ORIGIN(ram0);
	//TLB 起始需要16K 对齐；
    .mmu_tlb ALIGN(0x4000):
    {
        *(.mmu_tlb_segment);
    } > ram0

	.boot_info ALIGN(32):
	{
		*(.boot_info)
        . = ALIGN(32);
	} > ram0

	.irq_stack ALIGN(32):
    {
		*(.stack_magic)
        _cpu0_sstack_begin = .;
        PROVIDE(cpu0_sstack_begin = .);
        *(.stack)
        _cpu0_sstack_end = .;
        PROVIDE(cpu0_sstack_end = .);
    	_stack_end = . ;
		*(.stack_magic0)
		. = ALIGN(4);

    } > ram0

	.data ALIGN(32):
	{
		//cpu start
        . = ALIGN(4);
        *(.data_magic)

        *(.data*)
        *(*.data)

		. = ALIGN(4);
        __a2dp_movable_slot_start = .;
        *(.movable.slot.1);
        __a2dp_movable_slot_end = .;
		. = ALIGN(4);
		#include "media/cpu/br36/audio_lib_data.ld"

		. = ALIGN(4);
		#include "system/system_lib_data.ld"

		. = ALIGN(4);

	} > ram0

	.bss ALIGN(32):
    {
        . = ALIGN(4);
		#include "system/system_lib_bss.ld"

        . = ALIGN(4);
        *(.bss)
        *(*.data.bss)

        . = ALIGN(4);
		#include "media/cpu/br36/audio_lib_bss.ld"

        . = ALIGN(4);

        *(.os_bss)
        *(.volatile_ram)
		*(.btstack_pool)

        . = ALIGN(8);
        *(.usb_ep0)
        *(.usb_msd_dma)
        *(.usb_config_var)
        *(.cdc_var)

		*(.audio_buf)
        *(.src_filt)
        *(.src_dma)
        *(.mem_heap)
		*(.memp_memory_x)

        . = ALIGN(4);
		*(.non_volatile_ram)


        . = ALIGN(32);

    } > ram0

	.data_code ALIGN(32):
	{
        data_code_pc_limit_begin = .;
		*(.bank_critical_code)

        . = ALIGN(4);
    } > ram0

    .common ALIGN(32):
    {
        //common bank code addr
        common_code_run_addr = .;
        *(.common*)

        *(.math_fast_funtion_code)
		*(.flushinv_icache)
        *(.cache)
        *(.volatile_ram_code)

        *(.os_critical_code)
		*(.os_code)
		*(.os_str)
        *(.movable.stub.1)
        *(*.text.cache.L1)
        *(*.text.const.cache.L2)

#if TCFG_USER_TWS_ENABLE
		. = ALIGN(4);
        tws_sync_channel_begin = .;
        *(.tws.text.cache.L1.sync_channel0)
        *(.tws.text.cache.L1.sync_channel1)
        *(.tws.text.cache.L1.sync_channel2)
        *(.tws.text.cache.L1.sync_channel3)
        *(.tws.text.cache.L1.sync_channel4)
        *(.tws.text.cache.L1.sync_channel5)
        *(.tws.text.cache.L1.sync_channel6)
        *(.tws.text.cache.L1.sync_channel7)
        *(.tws.text.cache.L1.sync_channel8)
        *(.tws.text.cache.L1.sync_channel9)
        *(.tws.text.cache.L1.sync_channel10)
        *(.tws.text.cache.L1.sync_channel11)
        *(.tws.text.cache.L1.sync_channel12)
        *(.tws.text.cache.L1.sync_channel13)
        tws_sync_channel_end = .;
#endif

		. = ALIGN(4);
        _SPI_CODE_START = . ;
        *(.spi_code)
		. = ALIGN(4);
        _SPI_CODE_END = . ;

        *(.fat_data_code)

        /* *(.rodata*) */

       . = ALIGN(4);
    } > ram0


	__report_overlay_begin = .;
	overlay_code_begin = .;
	OVERLAY : AT(0x200000) SUBALIGN(4)
    {
		.overlay_aec
		{
            . = ALIGN(4);
			aec_code_begin  = . ;
#if AUDIO_CVP_CODE_TEXT_AT_RAM
			*(.text._*)
#endif/*AUDIO_CVP_CODE_TEXT_AT_RAM*/
#if AUDIO_CVP_CODE_NLP_AT_RAM
			*(.nlp_code)
			*(.nlp_const)
            *(.jlsp_nlp_code)
            *(.jlsp_nlp_const)
#endif/*AUDIO_CVP_CODE_NLP_AT_RAM*/
#if AUDIO_CVP_CODE_AEC_AT_RAM
			*(.aec_code)
			*(.aec_const)
            *(.jlsp_aec_code)
            *(.jlsp_aec_const)
#endif/*AUDIO_CVP_CODE_AEC_AT_RAM*/
#if AUDIO_CVP_CODE_COMMON_AT_RAM
			*(.res_code)
			*(.res_const)
#endif/*AUDIO_CVP_CODE_COMMON_AT_RAM*/
            . = ALIGN(4);
#if AUDIO_CVP_CODE_DNS_AT_RAM
            *(.dns_common_data)
            *(.dns_param_data_single)
            *(.dns_param_data_dual)
#endif/*AUDIO_CVP_CODE_DNS_AT_RAM*/
#if AUDIO_CVP_CODE_NS_AT_RAM
			*(.ns_code)
			*(.ns_const)
#endif/*AUDIO_CVP_CODE_NS_AT_RAM*/
#if AUDIO_CVP_CODE_COMMON_AT_RAM
			*(.fft_code)
			*(.fft_const)
			*(.der_code)
			*(.der_const)
			*(.qmf_code)
			*(.qmf_const)
            *(.jlsp_prep_code)
            *(.jlsp_prep_const)
#endif/*AUDIO_CVP_CODE_COMMON_AT_RAM*/
#if AUDIO_CVP_CODE_DMS_AT_RAM
			*(.dms_code)
			*(.dms_const)
            *(.jlsp_enc_code)
            *(.jlsp_enc_const)
#endif/*AUDIO_CVP_CODE_DMS_AT_RAM*/
#if AUDIO_CVP_CODE_WIND_AT_RAM
            *(.jlsp_wn_code)
            *(.jlsp_wn_const)
#endif/*AUDIO_CVP_CODE_WIND_AT_RAM*/
#if AUDIO_CVP_CODE_AGC_AT_RAM
			*(.agc_code)
#endif/*AUDIO_CVP_CODE_AGC_AT_RAM*/
#if AUDIO_CVP_CODE_COMMON_AT_RAM
            *(.opcore_maskrom)
			*(.cvp_dev_overlay_mem)
#endif/*AUDIO_CVP_CODE_COMMON_AT_RAM*/
			aec_code_end = . ;
			aec_code_size = aec_code_end - aec_code_begin ;
		}

		.overlay_aac
		{
#if TCFG_AUDIO_AAC_CODE_AT_RAM
			. = ALIGN(4);
			aac_dec_code_begin = .;
			*(.bt_aac_dec_code)
            *(.bt_aac_dec_sparse_code)
			aac_dec_code_end = .;
			aac_dec_code_size  = aac_dec_code_end - aac_dec_code_begin ;

			. = ALIGN(4);
			bt_aac_dec_const_begin = .;
			*(.bt_aac_dec_const)
            *(.bt_aac_dec_sparse_const)
			. = ALIGN(4);
			bt_aac_dec_const_end = .;
			bt_aac_dec_const_size = bt_aac_dec_const_end -  bt_aac_dec_const_begin ;
			*(.bt_aac_dec_data)
			LONG(0xFFFFFFFF);
			. = ALIGN(4);
			*(.bt_aac_dec_bss)
			. = ALIGN(4);
			*(.aac_mem)
			*(.aac_ctrl_mem)
#endif/*TCFG_AUDIO_AAC_CODE_AT_RAM*/
		}
	    } > ram0

   //加个空段防止下面的OVERLAY地址不对
    .ram0_empty0 ALIGN(4) :
	{
        . = . + 4;
    } > ram0

	//__report_overlay_begin = .;
	OVERLAY : AT(0x210000) SUBALIGN(4)
    {
		.overlay_aec_ram
		{
            . = ALIGN(4);
			*(.msbc_enc)
			*(.cvsd_codec)
			*(.aec_bss)
			*(.res_bss)
			*(.ns_bss)
			*(.nlp_bss)
        	*(.der_bss)
        	*(.qmf_bss)
        	*(.fft_bss)
			*(.aec_mem)
			*(.dms_bss)
            *(.jlsp_aec_bss)
            *(.jlsp_nlp_bss)
            *(.jlsp_dns_bss)
            *(.jlsp_enc_bss)
            *(.jlsp_prep_bss)
            *(.jlsp_wn_bss)
		}

		.overlay_mp3
		{
			*(.mp3_mem)
			*(.mp3_ctrl_mem)
			*(.mp3pick_mem)
			*(.mp3pick_ctrl_mem)
			*(.dec2tws_mem)
		}

    	.overlay_wav
		{
			/* *(.wav_bss_id) */
			*(.wav_mem)
			*(.wav_ctrl_mem)
		}


		.overlay_wma
		{
			*(.wma_mem)
			*(.wma_ctrl_mem)
			*(.wmapick_mem)
			*(.wmapick_ctrl_mem)
		}
		.overlay_ape
        {
            *(.ape_mem)
            *(.ape_ctrl_mem)
		}
		.overlay_flac
        {
            *(.flac_mem)
            *(.flac_ctrl_mem)
		}
		.overlay_m4a
        {
            *(.m4a_mem)
            *(.m4a_ctrl_mem)
	        *(.m4apick_mem)
			*(.m4apick_ctrl_mem)

		}
		.overlay_amr
        {
            *(.amr_mem)
            *(.amr_ctrl_mem)
		}
		.overlay_alac
		{
			*(.alac_ctrl_mem)
		}
		.overlay_dts
        {
            *(.dts_mem)
            *(.dts_ctrl_mem)
		}
		.overlay_fm
		{
			*(.fm_mem)
		}
        .overlay_pc
		{
		}


		/* .overlay_moveable */
		/* { */
			/* . += 0xa000 ; */
		/* } */
    } > ram0

    //bank code addr
    bank_code_run_addr = .;

    OVERLAY : AT(0x300000) SUBALIGN(4)
    {
        .overlay_bank0
        {
            *(.bank.code.0*)
            *(.bank.const.0*)
            . = ALIGN(4);
        }
        .overlay_bank1
        {
            *(.bank.code.1*)
            *(.bank.const.1*)
            . = ALIGN(4);
        }
        .overlay_bank2
        {
            *(.bank.code.2*)
            *(.bank.const.2*)
            *(.bank.ecdh.*)
            . = ALIGN(4);
        }
        .overlay_bank3
        {
            *(.bank.code.3*)
            *(.bank.const.3*)
            *(.bank.enc.*)
            . = ALIGN(4);
        }
        .overlay_bank4
        {
            *(.bank.code.4*)
            *(.bank.const.4*)
            . = ALIGN(4);
        }
        .overlay_bank5
        {
            *(.bank.code.5*)
            *(.bank.const.5*)
            . = ALIGN(4);
        }
        .overlay_bank6
        {
            *(.bank.code.6*)
            *(.bank.const.6*)
            . = ALIGN(4);
        }
        .overlay_bank7
        {
            *(.bank.code.7*)
            *(.bank.const.7*)
            . = ALIGN(4);
        }
        .overlay_bank8
        {
            *(.bank.code.8*)
            *(.bank.const.8*)
            . = ALIGN(4);
        }
        .overlay_bank9
        {
            *(.bank.code.9*)
            *(.bank.const.9*)
            . = ALIGN(4);
        }
    } > ram0

	data_code_pc_limit_end = .;
	__report_overlay_end = .;

	_HEAP_BEGIN = . ;
	_HEAP_END = RAM0_END;

    . = ORIGIN(code0);
    .text ALIGN(4):
    {
        PROVIDE(text_rodata_begin = .);

        *(.entry_text)
        *(.startup.text)


		*(.text)
		*(*.text)
        *(*.text.cache.L2)
        *(*.text.cache.L2.*)

        . = ALIGN(4);
		*(*.text.const)

		. = ALIGN(4);
        tws_sync_call_begin = .;
        KEEP(*(.tws.text.sync_call))
        tws_sync_call_end = .;

		. = ALIGN(4);
        tws_func_stub_begin = .;
        KEEP(*(.tws.text.func_stub))
        tws_func_stub_end = .;

		*(.LOG_TAG_CONST*)
        *(.rodata*)
        *(.fat_data_code_ex)

		. = ALIGN(4); // must at tail, make rom_code size align 4
        PROVIDE(text_rodata_end = .);

        clock_critical_handler_begin = .;
        KEEP(*(.clock_critical_txt))
        clock_critical_handler_end = .;

        gsensor_dev_begin = .;
        KEEP(*(.gsensor_dev))
        gsensor_dev_end = .;

        fm_dev_begin = .;
        KEEP(*(.fm_dev))
        fm_dev_end = .;

        fm_emitter_dev_begin = .;
        KEEP(*(.fm_emitter_dev))
        fm_emitter_dev_end = .;

        storage_device_begin = .;
        KEEP(*(.storage_device))
        storage_device_end = .;

		. = ALIGN(4);
		tool_interface_begin = .;
		KEEP(*(.tool_interface))
		tool_interface_end = .;

        . = ALIGN(4);
        __VERSION_BEGIN = .;
        KEEP(*(.sys.version))
        __VERSION_END = .;

        *(.noop_version)

		. = ALIGN(4);


        MEDIA_CODE_BEGIN = .;
		#include "media/cpu/br36/audio_lib_text.ld"

    	audio_sync_code_begin = .;
        *(.audio_sync_code)
    	audio_sync_code_end = .;

		/********maskrom arithmetic ****/
        *(.opcore_table_maskrom)
        *(.bfilt_table_maskroom)
        *(.bfilt_code)
        *(.bfilt_const)
		/********maskrom arithmetic end****/
		. = ALIGN(4);
            . = ALIGN(4);
			aec_code_begin  = . ;
#if !AUDIO_CVP_CODE_TEXT_AT_RAM
			*(.text._*)
#endif/*AUDIO_CVP_CODE_TEXT_AT_RAM*/
#if !AUDIO_CVP_CODE_AEC_AT_RAM
			*(.aec_code)
			*(.aec_const)
            *(.jlsp_aec_code)
            *(.jlsp_aec_const)
#endif/*AUDIO_CVP_CODE_AEC_AT_RAM*/
#if !AUDIO_CVP_CODE_NLP_AT_RAM
			*(.nlp_code)
			*(.nlp_const)
            *(.jlsp_nlp_code)
            *(.jlsp_nlp_const)
#endif/*AUDIO_CVP_CODE_NLP_AT_RAM*/
#if !AUDIO_CVP_CODE_COMMON_AT_RAM
			*(.res_code)
			*(.res_const)
#endif/*AUDIO_CVP_CODE_COMMON_AT_RAM*/
            . = ALIGN(4);
#if !AUDIO_CVP_CODE_DNS_AT_RAM
            *(.dns_common_data)
            *(.dns_param_data_single)
            *(.dns_param_data_dual)
#endif/*AUDIO_CVP_CODE_DNS_AT_RAM*/
#if !AUDIO_CVP_CODE_NS_AT_RAM
			*(.ns_code)
			*(.ns_const)
#endif/*AUDIO_CVP_CODE_NS_AT_RAM*/
#if !AUDIO_CVP_CODE_COMMON_AT_RAM
			*(.fft_code)
			*(.fft_const)
			*(.der_code)
			*(.der_const)
			*(.qmf_code)
			*(.qmf_const)
            *(.jlsp_prep_code)
            *(.jlsp_prep_const)
#endif/*AUDIO_CVP_CODE_COMMON_AT_RAM*/
#if !AUDIO_CVP_CODE_DMS_AT_RAM
			*(.dms_code)
			*(.dms_const)
            *(.jlsp_enc_code)
            *(.jlsp_enc_const)
#endif/*AUDIO_CVP_CODE_DMS_AT_RAM*/
#if !AUDIO_CVP_CODE_WIND_AT_RAM
            *(.jlsp_wn_code)
            *(.jlsp_wn_const)
#endif/*AUDIO_CVP_CODE_WIND_AT_RAM*/
#if !AUDIO_CVP_CODE_AGC_AT_RAM
			*(.agc_code)
#endif/*AUDIO_CVP_CODE_AGC_AT_RAM*/
#if !AUDIO_CVP_CODE_COMMON_AT_RAM
            *(.opcore_maskrom)
#endif/*AUDIO_CVP_CODE_COMMON_AT_RAM*/
			aec_code_end = . ;
			aec_code_size = aec_code_end - aec_code_begin ;

#if (TCFG_AUDIO_AAC_CODE_AT_RAM == 0)
			. = ALIGN(4);
			aac_dec_code_begin = .;
			*(.bt_aac_dec_code)
            *(.bt_aac_dec_sparse_code)
			aac_dec_code_end = .;
			aac_dec_code_size  = aac_dec_code_end - aac_dec_code_begin ;

			. = ALIGN(4);
			bt_aac_dec_const_begin = .;
			*(.bt_aac_dec_const)
            *(.bt_aac_dec_sparse_const)
			. = ALIGN(4);
			bt_aac_dec_const_end = .;
			bt_aac_dec_const_size = bt_aac_dec_const_end -  bt_aac_dec_const_begin ;
			*(.bt_aac_dec_data)
			. = ALIGN(4);
#endif/*TCFG_AUDIO_AAC_CODE_AT_RAM*/
        MEDIA_CODE_SIZE = . - MEDIA_CODE_BEGIN;

		/* #include "btstack/btstack_lib_text.ld" */

        SYSTEM_CODE_BEGIN = .;
		#include "system/system_lib_text.ld"
		. = ALIGN(4);
        SYSTEM_CODE_SIZE = . - SYSTEM_CODE_BEGIN;

		. = ALIGN(4);
	    update_target_begin = .;
	    PROVIDE(update_target_begin = .);
	    KEEP(*(.update_target))
	    update_target_end = .;
	    PROVIDE(update_target_end = .);

		. = ALIGN(4);
        *(.kws_common_data_multi_57k)
        *(.kws_common_data_multi_26k)
        *(.kws_common_data_multi)
        *(.kws_param_data_multi)
        *(.kws_param_data_multi_ram26)
        *(.fst_topo_param_multi)
        *(.kws_fst_data_multi)
		. = ALIGN(4);
        *(.kws_param_data_phone_call)
        *(.kws_common_data_phone_call)
        *(.fst_topo_param_phone_call)
        *(.kws_fst_data_phone_call)

		. = ALIGN(4);
        __a2dp_text_cache_L2_start = .;
        *(.movable.region.1);
		. = ALIGN(4);
        __a2dp_text_cache_L2_end = .;

		. = ALIGN(32);
	  } > code0
}

#include "btstack/btstack_lib.ld"
#include "update/update.ld"
#include "driver/cpu/br36/driver_lib.ld"
//================== Section Info Export ====================//
text_begin  = ADDR(.text);
text_size   = SIZEOF(.text);
text_end    = text_begin + text_size;

bss_begin = ADDR(.bss);
bss_size  = SIZEOF(.bss);
bss_end    = bss_begin + bss_size;

data_addr = ADDR(.data);
data_begin = text_begin + text_size;
data_size =  SIZEOF(.data);

/* moveable_slot_addr = ADDR(.moveable_slot); */
/* moveable_slot_begin = data_begin + data_size; */
/* moveable_slot_size =  SIZEOF(.moveable_slot); */

data_code_addr = ADDR(.data_code);
data_code_begin = data_begin + data_size;
data_code_size =  SIZEOF(.data_code);

//================ OVERLAY Code Info Export ==================//
aec_addr = ADDR(.overlay_aec);
aec_begin = data_code_begin + data_code_size;
aec_size =  SIZEOF(.overlay_aec);

aac_addr = ADDR(.overlay_aac);
aac_begin = aec_begin + aec_size;
aac_size =  SIZEOF(.overlay_aac);

wav_addr = ADDR(.overlay_wav);
wav_begin = aec_begin + aec_size;
wav_size =  SIZEOF(.overlay_wav);

ape_addr = ADDR(.overlay_ape);
ape_begin = wav_begin + wav_size;
ape_size =  SIZEOF(.overlay_ape);

flac_addr = ADDR(.overlay_flac);
flac_begin = ape_begin + ape_size;
flac_size =  SIZEOF(.overlay_flac);

m4a_addr = ADDR(.overlay_m4a);
m4a_begin = flac_begin + flac_size;
m4a_size =  SIZEOF(.overlay_m4a);

amr_addr = ADDR(.overlay_amr);
amr_begin = m4a_begin + m4a_size;
amr_size =  SIZEOF(.overlay_amr);

dts_addr = ADDR(.overlay_dts);
dts_begin = amr_begin + amr_size;
dts_size =  SIZEOF(.overlay_dts);


//================ BANK ==================//
bank_code_load_addr = data_code_begin + data_code_size;
/* bank_code_load_addr = aac_begin + aac_size; */

//===================== HEAP Info Export =====================//
PROVIDE(HEAP_BEGIN = _HEAP_BEGIN);
PROVIDE(HEAP_END = _HEAP_END);
_MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN;
PROVIDE(MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN);

/* ASSERT(MALLOC_SIZE  >= 0x8000, "heap space too small !") */

//============================================================//
//=== report section info begin:
//============================================================//
report_text_beign = ADDR(.text);
report_text_size = SIZEOF(.text);
report_text_end = report_text_beign + report_text_size;

report_mmu_tlb_begin = ADDR(.mmu_tlb);
report_mmu_tlb_size = SIZEOF(.mmu_tlb);
report_mmu_tlb_end = report_mmu_tlb_begin + report_mmu_tlb_size;

report_boot_info_begin = ADDR(.boot_info);
report_boot_info_size = SIZEOF(.boot_info);
report_boot_info_end = report_boot_info_begin + report_boot_info_size;

report_irq_stack_begin = ADDR(.irq_stack);
report_irq_stack_size = SIZEOF(.irq_stack);
report_irq_stack_end = report_irq_stack_begin + report_irq_stack_size;

report_data_begin = ADDR(.data);
report_data_size = SIZEOF(.data);
report_data_end = report_data_begin + report_data_size;

report_bss_begin = ADDR(.bss);
report_bss_size = SIZEOF(.bss);
report_bss_end = report_bss_begin + report_bss_size;

report_data_code_begin = ADDR(.data_code);
report_data_code_size = SIZEOF(.data_code);
report_data_code_end = report_data_code_begin + report_data_code_size;

report_overlay_begin = __report_overlay_begin;
report_overlay_size = __report_overlay_end - __report_overlay_begin;
report_overlay_end = __report_overlay_end;

report_heap_beign = _HEAP_BEGIN;
report_heap_size = _HEAP_END - _HEAP_BEGIN;
report_heap_end = _HEAP_END;

BR34_PHY_RAM_SIZE = PHY_RAM_SIZE;
BR34_SDK_RAM_SIZE = report_mmu_tlb_size + \
					report_boot_info_size + \
					report_irq_stack_size + \
					report_data_size + \
					report_bss_size + \
					report_overlay_size + \
					report_data_code_size + \
					report_heap_size;
//============================================================//
//=== report section info end
//============================================================//

