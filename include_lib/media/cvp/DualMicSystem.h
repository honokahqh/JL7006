#ifndef DualMicSystem_H
#define DualMicSystem_H
#define DMS_AEC_EN 1
#define DMS_NLP_EN 2
#define DMS_ANS_EN 4
#define DMS_ENC_EN 8
#define DMS_WN_EN 32
#define DMS_MFDT_EN 64 // mic malfunction detect
#define DUALMICSYSTEM_CONFIG_SETENABLEBIT 0
#define DUALMICSYSTEM_CONFIG_FREEZENOISEESTIMATION 1
#define DUALMICSYSTEM_CONFIG_GETWINDDETECTSTATE 2
#define DUALMICSYSTEM_CONFIG_GETMALFUNCTIONSTATE 3
#define DUALMICSYSTEM_CONFIG_GETMALFUNCTION_MAINMIC_ENG 4
#define DUALMICSYSTEM_CONFIG_GETMALFUNCTION_AUXMIC_ENG 5
#define DUALMICSYSTEM_CONFIG_SETMALFUNCTIONSTATE 6
#define DUALMICSYSTEM_CONFIG_SET_MAIN_GAIN 7
#define DUALMICSYSTEM_CONFIG_SET_AUXILIARY_GAIN 8

typedef struct {
    int AEC_Process_MaxFrequency;
    int AEC_Process_MinFrequency;
    int AF_Length;
} DualMicSystem_AECConfig;

typedef struct {
    int NLP_Process_MaxFrequency;
    int NLP_Process_MinFrequency;
    float OverDrive;
} DualMicSystem_NLPConfig;

typedef struct {
    int UseExWeight;
    int *ENC_WeightA;
    int *ENC_WeightB;
    int ENC_Weight_Q;
    int ENC_Process_MaxFrequency;
    int ENC_Process_MinFrequency;
    int SIR_MaxFrequency;
    float Mic_Distance;
    float Target_Signal_Degradation;
    float AggressFactor;
    float minSuppress;
} DualMicSystem_ENCConfig;

typedef struct {
    float AggressFactor;
    float minSuppress;
    float init_noise_lvl;
    float high_gain; // for dns range: [1,3]
    float rb_rate;   // for dns range: [0,1]
} DualMicSystem_ANSConfig;

typedef struct {
    float detect_time;           //in second
    float detect_time_ratio_thr; //0~1
    float detect_thr;            //0~1
    float minSuppress;           //0~1
} DualMicSystem_WNConfig;

typedef struct {
    float detect_time;           // in second
    float detect_eng_diff_thr;   // 0~-90 dB
    float detect_eng_lowerbound; // 0~-90 dB start detect when mic energy lower than this
    int MalfuncDet_MaxFrequency;
    int MalfuncDet_MinFrequency;
    int OnlyDetect; // 1 for only detect，do not switch to single mode
} DualMicSystem_MalfuncDetConfig;

#ifdef __cplusplus
extern "C"
{
#endif
int DualMicSystem_GetProcessDelay(int is_wideband);
int DualMicSystem_GetMiniFrameSize(int is_wideband);
int DualMicSystem_QueryBufSize(DualMicSystem_AECConfig *aec_cfg,
                               DualMicSystem_NLPConfig *nlp_cfg,
                               DualMicSystem_ENCConfig *enc_cfg,
                               DualMicSystem_ANSConfig *ans_cfg,
                               DualMicSystem_WNConfig *wn_cfg,
                               DualMicSystem_MalfuncDetConfig *malfunc_cfg,
                               int is_wideband,
                               int EnableBit);
int DualMicSystem_QueryTempBufSize(DualMicSystem_AECConfig *aec_cfg,
                                   DualMicSystem_NLPConfig *nlp_cfg,
                                   DualMicSystem_ENCConfig *enc_cfg,
                                   DualMicSystem_ANSConfig *ans_cfg,
                                   DualMicSystem_WNConfig *wn_cfg,
                                   DualMicSystem_MalfuncDetConfig *malfunc_cfg,
                                   int is_wideband,
                                   int EnableBit);
void DualMicSystem_Init(void *runbuf,
                        void *tempbuf,
                        DualMicSystem_AECConfig *aec_cfg,
                        DualMicSystem_NLPConfig *nlp_cfg,
                        DualMicSystem_ENCConfig *enc_cfg,
                        DualMicSystem_ANSConfig *ans_cfg,
                        DualMicSystem_WNConfig *wn_cfg,
                        DualMicSystem_MalfuncDetConfig *malfunc_cfg,
                        float GloabalMinSuppress,
                        int is_wideband,
                        int EnableBit);
void DualMicSystem_Process(void *runbuf,
                           void *tempbuf,
                           short *reference,
                           short *main_mic,
                           short *auxiliary_mic,
                           short *output,
                           int nMiniFrame);
void DualMicSystem_Config(void *runbuf, int ConfigType, int *ConfigPara);
#ifdef __cplusplus
}
#endif
#endif
