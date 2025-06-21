
	. = ALIGN(4);
	audio_sync_bss_begin = .;
        *(.audio_sync_bss)
	audio_sync_bss_end = .;
	. = ALIGN(4);
	*(.jlsp_bss)
	*(.mp3_bss)
	*(.mp3_dec_bss)
	*(.mp3tsy_dec_bss)
	*(.mp3tsy_bss)
	*(.flac_bss)
	*(.flac_dec_bss)

	*(.wma_bss)
	*(.wma_dec_bss)
    *(.wav_bss)
	*(.wav_dec_bss)
    
    *(.cvsd_bss)
    *(.aac_bss)
	*(.sbc_eng_bss)
	*(.bt_audioplc_bss)
    *(.lc3_decoder_bss)
#if (TCFG_ENC_LC3_ENABLE || TCFG_DEC_LC3_ENABLE)
	*(.lc3_codec_ari_c_bss)
	*(.lc3_codec_c_bss)
#endif
    *(.wtgv2_bss)
    *(.wtgv2dec_bss)

	*(.anc_bss)
	*(.anc_core_bss)
	*(.anc_user_bss)
	*(.anc_box_bss)
	*(.anc_btspp_bss)
	*(.anc_debug_bss)

	*(.ape_dec_bss)
	*(.ape_bss)

	*(.alac_bss)
   
	*(.amr_dec_bss)

	*(.m4a_dec_bss)
	*(.m4a_bss)
	*(.audio_echo_reverb_bss)
	*(.echo_cal_bss)
	*(.platereverb_bss)
	*(.audio_howling_bss)
	*(.howlings_phf_bss)
	*(.notchhowling_bss)
	*(.audio_track_bss)
    *(.audio_cfifo_bss)

