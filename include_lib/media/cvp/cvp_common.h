#ifndef _CVP_COMMON_H_
#define _CVP_COMMON_H_
#include "DualMicSystem_flexible.h"
#include "DualMicSystem.h"
#include "jlsp_ns.h"

/*DMS版本定义*/
#define DMS_GLOBAL_V100    0xB1
#define DMS_GLOBAL_V200    0xB2

/*
 * 新算法回声消除nlp模式
 * JLSP_NLP_MODE1: 模式1为单独的NLP回声抑制，回声压制会偏过，该模式下NLP模块可以单独开启
 * JLSP_NLP_MODE2: 模式2下回声信号会先经过AEC线性压制，然后进行NLP非线性压制
 *                 此模式NLP不能单独打开需要同时打开AEC,使用AEC模块压制不够时，建议开启该模式
 */
#define JLSP_NLP_MODE1 0x01   //模式一（默认）
#define JLSP_NLP_MODE2 0x02   //模式二

/*
 * 新算法风噪降噪模式
 * JLSP_WD_MODE1: 模式1为常规的降风噪模式，风噪残余会偏大些
 * JLSP_WD_MODE2: 模式2为神经网络降风噪，风噪抑制会比较干净，但是会需要多消耗31kb的flash
 */
#define JLSP_WD_MODE1 0x01   //常规降噪
#define JLSP_WD_MODE2 0x02   //nn降风噪，目前该算法启用会多31kflash

#endif
