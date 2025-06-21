// *INDENT-OFF*
#include "app_config.h"

#ifdef __SHELL__

##!/bin/sh
${OBJDUMP} -D -address-mask=0x1ffffff -print-dbg $1.elf > $1.lst
${OBJCOPY} -O binary -j .text $1.elf text.bin
${OBJCOPY} -O binary -j .data $1.elf data.bin

${OBJCOPY} -O binary -j .data_code $1.elf data_code.bin
${OBJCOPY} -O binary -j .overlay_aec $1.elf aec.bin
${OBJCOPY} -O binary -j .overlay_aac $1.elf aaco.bin
${OBJCOPY} -O binary -j .overlay_aptx $1.elf aptx.bin

${OBJCOPY} -O binary -j .common $1.elf common.bin

/opt/utils/remove_tailing_zeros -i aaco.bin -o aac.bin -mark ff

bank_files=
for i in $(seq 0 20)
do
    ${OBJCOPY} -O binary -j .overlay_bank$i $1.elf bank$i.bin
    if [ ! -s bank$i.bin ]
    then
        break
    fi
    bank_files=$bank_files"bank$i.bin 0x0 "
done
echo $bank_files
lz4_packet -dict text.bin -input common.bin 0 aec.bin 0 aac.bin 0 $bank_files -o bank.bin

${OBJDUMP} -section-headers -address-mask=0x1ffffff $1.elf
${OBJSIZEDUMP} -lite -skip-zero -enable-dbg-info $1.elf | sort -k 1 >  symbol_tbl.txt

cat text.bin data.bin data_code.bin bank.bin > app.bin

#if TCFG_DRC_ENABLE
#if defined(TCFG_AUDIO_HEARING_AID_ENABLE) && TCFG_AUDIO_HEARING_AID_ENABLE && defined(AUDIO_VBASS_CONFIG)&&AUDIO_VBASS_CONFIG
cp eq_cfg_hw_aid_vbass.bin  eq_cfg_hw.bin
#elif defined(TCFG_AUDIO_HEARING_AID_ENABLE) && TCFG_AUDIO_HEARING_AID_ENABLE
cp eq_cfg_hw_wdrc.bin  eq_cfg_hw.bin
#elif defined(AUDIO_VBASS_CONFIG)&&AUDIO_VBASS_CONFIG
cp eq_cfg_hw_drc_vbass.bin  eq_cfg_hw.bin
#else
cp eq_cfg_hw_full.bin  eq_cfg_hw.bin
#endif /*TCFG_AUDIO_HEARING_AID_ENABLE*/
#else
#if defined(AUDIO_VBASS_CONFIG)&&AUDIO_VBASS_CONFIG
cp eq_cfg_hw_vbass.bin  eq_cfg_hw.bin
#else
cp eq_cfg_hw_less.bin  eq_cfg_hw.bin
#endif
#endif



/opt/utils/strip-ini -i isd_config.ini -o isd_config.ini
files="app.bin ${CPU}loader.* uboot*  ota*.bin p11_code.bin isd_config.ini isd_download.exe fw_add.exe ufw_maker.exe"


host-client -project ${NICKNAME}$2 -f ${files} $1.elf

#else

@echo off
Setlocal enabledelayedexpansion
@echo ********************************************************************************
@echo           SDK BR36
@echo ********************************************************************************
@echo %date%


cd /d %~dp0

set OBJDUMP=C:\JL\pi32\bin\llvm-objdump.exe
set OBJCOPY=C:\JL\pi32\bin\llvm-objcopy.exe
set ELFFILE=sdk.elf
set bankfiles=
set LZ4_PACKET=.\lz4_packet.exe

REM %OBJDUMP% -D -address-mask=0x1ffffff -print-dbg $1.elf > $1.lst
%OBJCOPY% -O binary -j .text %ELFFILE% text.bin
%OBJCOPY% -O binary -j .data %ELFFILE% data.bin
%OBJCOPY% -O binary -j .data_code %ELFFILE% data_code.bin
%OBJCOPY% -O binary -j .overlay_aec %ELFFILE% aec.bin
%OBJCOPY% -O binary -j .overlay_aac %ELFFILE% aaco.bin
%OBJCOPY% -O binary -j .overlay_aptx %ELFFILE% aptx.bin

%OBJCOPY% -O binary -j .common %ELFFILE% common.bin

remove_tailing_zeros -i aaco.bin -o aac.bin -mark ff

for /L %%i in (0,1,20) do (
            %OBJCOPY% -O binary -j .overlay_bank%%i %ELFFILE% bank%%i.bin
                set bankfiles=!bankfiles! bank%%i.bin 0x0
        )


%LZ4_PACKET% -dict text.bin -input common.bin 0 aec.bin 0 aac.bin 0 !bankfiles! -o bank.bin

%OBJDUMP% -section-headers -address-mask=0x1ffffff %ELFFILE%
REM %OBJDUMP% -t %ELFFILE% >  symbol_tbl.txt

copy /b text.bin + data.bin + data_code.bin + bank.bin app.bin

del !bankfiles! common.bin text.bin data.bin bank.bin

#if TCFG_DRC_ENABLE

#if defined(TCFG_AUDIO_HEARING_AID_ENABLE) && TCFG_AUDIO_HEARING_AID_ENABLE && defined(AUDIO_VBASS_CONFIG)&&AUDIO_VBASS_CONFIG
copy eq_cfg_hw_aid_vbass.bin  eq_cfg_hw.bin
#elif defined(TCFG_AUDIO_HEARING_AID_ENABLE) && TCFG_AUDIO_HEARING_AID_ENABLE
copy eq_cfg_hw_wdrc.bin  eq_cfg_hw.bin
#elif defined(AUDIO_VBASS_CONFIG)&&AUDIO_VBASS_CONFIG
copy eq_cfg_hw_drc_vbass.bin  eq_cfg_hw.bin
#else
copy eq_cfg_hw_full.bin  eq_cfg_hw.bin
#endif /*TCFG_AUDIO_HEARING_AID_ENABLE*/

#else
#if defined(AUDIO_VBASS_CONFIG)&&AUDIO_VBASS_CONFIG
copy eq_cfg_hw_vbass.bin  eq_cfg_hw.bin
#else
copy eq_cfg_hw_less.bin  eq_cfg_hw.bin
#endif
#endif


#ifdef CONFIG_WATCH_CASE_ENABLE
call download/watch/download.bat
#elif defined(CONFIG_SOUNDBOX_CASE_ENABLE)
call download/soundbox/download.bat
#elif defined(CONFIG_EARPHONE_CASE_ENABLE)
#if (RCSP_ADV_EN == 0)
call download/earphone/download.bat
#else
call download/earphone/download_app_ota.bat
#endif
#elif defined(CONFIG_HID_CASE_ENABLE) ||defined(CONFIG_SPP_AND_LE_CASE_ENABLE)||defined(CONFIG_MESH_CASE_ENABLE)||defined(CONFIG_DONGLE_CASE_ENABLE)    //数传
call download/data_trans/download.bat
#else
//to do other case
#endif  //endif app_case

#endif
