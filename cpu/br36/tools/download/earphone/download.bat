@echo off

cd %~dp0

copy ..\..\script.ver .
copy ..\..\tone.cfg .
copy ..\..\p11_code.bin .
copy ..\..\anc_coeff.bin .
copy ..\..\anc_gains.bin .
copy ..\..\br36loader.bin .
copy ..\..\br36loader.uart .
copy ..\..\ota.bin .

..\..\isd_download.exe ..\..\isd_config.ini -tonorflash -dev br36 -boot 0x20000 -div8 -wait 300 -uboot ..\..\uboot.boot -app ..\..\app.bin -res ..\..\cfg_tool.bin tone.cfg p11_code.bin ..\..\eq_cfg_hw.bin -uboot_compress 
:: -format all
::-reboot 2500

@rem ɾ����ʱ�ļ�-format all
if exist *.mp3 del *.mp3 
if exist *.PIX del *.PIX
if exist *.TAB del *.TAB
if exist *.res del *.res
if exist *.sty del *.sty

..\..\ufw_maker.exe -fw_to_ufw jl_isd.fw
copy jl_isd.ufw update.ufw
del jl_isd.ufw

@REM ���������ļ������ļ�
::ufw_maker.exe -chip AC800X %ADD_KEY% -output config.ufw -res bt_cfg.cfg

::IF EXIST jl_696x.bin del jl_696x.bin 

@rem ��������˵��
@rem -format vm        //����VM ����
@rem -format cfg       //����BT CFG ����
@rem -format 0x3f0-2   //��ʾ�ӵ� 0x3f0 �� sector ��ʼ�������� 2 �� sector(��һ������Ϊ16���ƻ�10���ƶ��ɣ��ڶ�������������10����)

ping /n 2 127.1>null
IF EXIST null del null
