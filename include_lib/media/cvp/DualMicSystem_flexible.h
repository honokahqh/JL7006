#ifndef DualMicSystem_flexible_H
#define DualMicSystem_flexible_H
#define DMS_AEC_EN 1
#define DMS_NLP_EN 2
#define DMS_ANS_EN 4
#define DMS_ENC_EN 8
#define DMS_CNG_EN 128
#define DUALMICSYSTEM_CONFIG_SETENABLEBIT 0
#define DUALMICSYSTEM_CONFIG_FREEZENOISEESTIMATION 1
#define DUALMICSYSTEM_CONFIG_USECOHENXDMAPING 2
#define DUALMICSYSTEM_CONFIG_USESINGLEMIC 3
#define DUALMICSYSTEM_CONFIG_SETENCOUTLEV 4
#define DUALMICSYSTEM_CONFIG_SETGLOBALMINGAIN 5

typedef struct {
    int AEC_Process_MaxFrequency;
    int AEC_Process_MinFrequency;
    int AF_Length;
    float AdaptiveRate;
    float Disconverge_ERLE_Thr;
} DualMicSystem_flexible_AECConfig;

typedef struct {
    int NLP_Process_MaxFrequency;
    int NLP_Process_MinFrequency;
    float OverDrive;
} DualMicSystem_flexible_NLPConfig;

typedef struct {
    int AF_Length;
    int ENC_Process_MaxFrequency;
    int ENC_Process_MinFrequency;
    int SIR_MinFrequency;
    int SIR_MaxFrequency;
    int SIR_mean_MinFrequency;
    int SIR_mean_MaxFrequency;

    float ENC_CoheFlgMax_gamma;
    float ENC_MuMax_gamma;
    float ENC_SuppressMax_gamma;

    float coheXD_thr;
    float Engdiff_overdrive;

    float AdaptiveRate;
    float AggressFactor;
    float minSuppress;

    float Disconverge_ERLE_Thr;
} DualMicSystem_flexible_ENCConfig;

typedef struct {
    float AggressFactor;
    float minSuppress;
    float init_noise_lvl;
    float high_gain; // for dns range: [1,3]
    float rb_rate;   // for dns range: [0,1]
} DualMicSystem_flexible_ANSConfig;

typedef struct {
    float CNG_Power_Level;
    float CNG_OverDrive;
} DualMicSystem_flexible_CNGConfig;

#ifdef __cplusplus
extern "C"
{
#endif
int DualMicSystem_flexible_GetProcessDelay(int is_wideband);
int DualMicSystem_flexible_GetMiniFrameSize(int is_wideband);
int DualMicSystem_flexible_QueryBufSize(DualMicSystem_flexible_AECConfig *aec_cfg,
                                        DualMicSystem_flexible_NLPConfig *nlp_cfg,
                                        DualMicSystem_flexible_CNGConfig *cng_cfg,
                                        DualMicSystem_flexible_ENCConfig *enc_cfg,
                                        DualMicSystem_flexible_ANSConfig *ans_cfg,
                                        int is_wideband,
                                        int EnableBit);
int DualMicSystem_flexible_QueryTempBufSize(DualMicSystem_flexible_AECConfig *aec_cfg,
        DualMicSystem_flexible_NLPConfig *nlp_cfg,
        DualMicSystem_flexible_CNGConfig *cng_cfg,
        DualMicSystem_flexible_ENCConfig *enc_cfg,
        DualMicSystem_flexible_ANSConfig *ans_cfg,
        int is_wideband,
        int EnableBit);
void DualMicSystem_flexible_Init(void *runbuf,
                                 void *tempbuf,
                                 DualMicSystem_flexible_AECConfig *aec_cfg,
                                 DualMicSystem_flexible_NLPConfig *nlp_cfg,
                                 DualMicSystem_flexible_CNGConfig *cng_cfg,
                                 DualMicSystem_flexible_ENCConfig *enc_cfg,
                                 DualMicSystem_flexible_ANSConfig *ans_cfg,
                                 float GloabalMinSuppress,
                                 int is_wideband,
                                 int EnableBit);
void DualMicSystem_flexible_Process(void *runbuf,
                                    void *tempbuf,
                                    short *ref_mic,
                                    short *main_mic,
                                    short *auxiliary_mic,
                                    short *output,
                                    int nMiniFrame);
void DualMicSystem_flexible_Config(void *runbuf, void *tempbuf, int ConfigType, int *ConfigPara);
#ifdef __cplusplus
}
#endif
#endif
