
	. = ALIGN(4);
	audio_sync_data_begin = .;
        *(.audio_sync_data)
	audio_sync_data_end = .;
	. = ALIGN(4);
#if AUDIO_EFFECTS_GAIN_AT_RAM
		*(.audio_gain_code)
		*(.audio_gain_const)
#endif/*AUDIO_EFFECTS_GAIN_AT_RAM*/

	*(.mp3_data)
	*(.mp3_dec_data)
	*(.wma_data)
	*(.wma_dec_data)
    *(.wav_dec_data)
	*(.wav_data)
	*(.flac_data)
	*(.flac_dec_data)

    *(.mp3tsy_dec_data)
    *(.mp3tsy_data)
    *(.sbc_data)
    *(.msbc_data)
    *(.cvsd_data)
    *(.aac_data)
	*(.sbc_eng_data)
	*(.bt_audioplc_data)
    *(.lc3_decoder_data)
#if (TCFG_ENC_LC3_ENABLE || TCFG_DEC_LC3_ENABLE)
		*(.lc3_codec_ari_c_const)
		*(.lc3_codec_ari_c_data)
		*(.lc3_codec_c_data)
#endif
    *(.wtgv2_data)
    *(.wtgv2dec_data)

	*(.m4a_dec_data)
	*(.m4a_data)

	*(.amr_dec_data)

    *(.alac_data)

	*(.anc_data)
	*(.anc_core_data)
	*(.anc_user_data)
	*(.anc_box_data)
	*(.anc_btspp_data)
	*(.anc_debug_data)

	*(.ape_dec_data)
	*(.ape_data)
	*(.audio_track_data)
	*(.audio_cfifo_data)
	*(.audio_echo_reverb_data)
	*(.platereverb_data)
	*(.echo_cal_data)
	*(.audio_noisegate_data)
	*(.audio_howling_data)
	*(.howlings_phf_data)
	*(.notchhowling_data)
		
	audio_cvp_data_begin = .;
    *(.jlsp_aec_data)
    *(.jlsp_nlp_data)
    *(.jlsp_dns_data)
    *(.jlsp_enc_data)
    *(.jlsp_prep_data)
    *(.jlsp_wn_data)
	*(.sms_data)
	*(.dms_data)
	*(.data._*)
	*(.aec_data)
	*(.res_data)
	*(.ns_data)		
	*(.jlsp_data)
	*(.nlp_data)
	*(.der_data)
	*(.qmf_data)
	*(.fft_data)
	*(.aec_mux)
	audio_cvp_data_end = .;
	audio_cvp_data_size = audio_cvp_data_end - audio_cvp_data_begin ;
