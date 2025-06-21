#include "system/includes.h"
#include "app_config.h"
#include "app_task.h"
#include "app_music.h"
#include "app_action.h"
#include "earphone.h"
#include "bt_background.h"

u8 app_curr_task = 0;
u8 app_next_task = 0;
u8 app_prev_task = 0;

extern int pc_app_check(void);

const u8 app_task_list[5] = {
    APP_BT_TASK,
    APP_MUSIC_TASK,
    APP_PC_TASK,
    APP_HEARING_AID_TASK,
    APP_LINEIN_TASK,
};

const u8 app_task_action_tab[5] = {
    ACTION_EARPHONE_MAIN,
    ACTION_MUSIC_MAIN,
    ACTION_PC_MAIN,
    ACTION_HEARING_AID_MAIN,
    ACTION_LINEIN_MAIN,
};

const char *app_task_name_tab[5] = {
    APP_NAME_BT,
    APP_NAME_MUSIC,
    APP_NAME_PC,
    APP_NAME_HEARING_AID,
    APP_NAME_LINEIN,
};

//*----------------------------------------------------------------------------*/
/**@brief    模式进入检查
   @param    app_task:目标模式
   @return   TRUE可以进入， FALSE不可以进入
   @note	 例如一些需要设备在线的任务(music)，
   			 如果设备在线可以进入， 没有设备在线不进入可以在这里处理
*/
/*----------------------------------------------------------------------------*/
int app_task_switch_check(u8 app_task)
{
    int ret = false;
    printf("app_task %x\n", app_task);
    switch (app_task) {
    case APP_BT_TASK:
        ret = true;
        break;
#if TCFG_APP_LINEIN_EN
    case APP_LINEIN_TASK:
        ret = true;
        break;
#endif
    case APP_MUSIC_TASK:
#if TCFG_APP_MUSIC_EN
        ret = music_app_check();
#endif
        break;
    case APP_PC_TASK:
#if TCFG_PC_ENABLE
        ret = pc_app_check();
#endif
        break;
    case APP_HEARING_AID_TASK:
#if  TCFG_ENTER_HEARING_AID_MODE
        ret = true;
#endif
        break;
    default:
        ret = false;
        break;
    }
    return ret;
}

//*----------------------------------------------------------------------------*/
/**@brief    模式退出检查
   @param    curr_task:当前模式
   @return   TRUE可以退出， FALSE不可以退出
   @note
*/
/*----------------------------------------------------------------------------*/
static int app_task_switch_exit_check(u8 curr_task)
{
    int ret = false;
    switch (curr_task) {
    case APP_BT_TASK:
        ret = bt_app_exit_check();
        break;
    case APP_HEARING_AID_TASK:
#if  TCFG_ENTER_HEARING_AID_MODE
        ret = true;
#endif
        break;
    default:
        ret = TRUE;
        break;

    }
    return ret;
}

//*----------------------------------------------------------------------------*/
/**@brief    切换到指定模式
   @param    app_task:指定模式
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
int app_task_switch_to(u8 app_task, int priv)
{
    struct intent it;

    //相同模式不切
    if (app_curr_task == app_task) {
        return false;
    }

    //不在线不切
    if (!app_task_switch_check(app_task)) {
        return false;
    }

    //上一个模式不允许退出不切
    if (!app_task_switch_exit_check(app_curr_task)) {
        return false;
    }

    printf("cur --- %x \n", app_curr_task);
    printf("new +++ %x \n", app_task);

    app_prev_task = app_curr_task;
    app_curr_task = app_task;

#if CONFIG_BT_BACKGROUND_ENABLE
    //bt_in_background()为0时，切到蓝牙模式需要初始化(关机插pc切模式的情况)
    if ((app_task == APP_BT_TASK) && bt_in_background()) {
        if (priv == ACTION_A2DP_START) {
            bt_switch_to_foreground(ACTION_A2DP_START, 1);
        } else {
            bt_switch_to_foreground(ACTION_TONE_PLAY, 1);
        }
        return true; //切到蓝牙前台
    }
#endif

#if CONFIG_BT_BACKGROUND_ENABLE
    if (app_prev_task != APP_BT_TASK)
#endif
    {
        init_intent(&it);
        it.action = ACTION_BACK;
        start_app(&it);
    }
    init_intent(&it);
    it.name = app_task_name_tab[app_task];
    it.action = app_task_action_tab[app_task];
    start_app(&it);
    return true;
}

//*----------------------------------------------------------------------------*/
/**@brief    切换到下一个模式
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void app_task_switch_next(void)
{
    int i = 0;
    int cur_index = 0;

    for (cur_index = 0; cur_index < ARRAY_SIZE(app_task_list); cur_index++) {
        if (app_curr_task == app_task_list[cur_index]) {//遍历当前索引
            break;
        }
    }

    for (i = cur_index ;;) { //遍历一圈
        if (++i >= ARRAY_SIZE(app_task_list)) {
            i = 0;
        }
        if (i == cur_index) {
            return;
        }
        if (app_task_switch_to(app_task_list[i], NULL_VALUE)) {
            return;
        }
    }
}

//*----------------------------------------------------------------------------*/
/**@brief    切换到上一个模式
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void app_task_switch_prev(void)
{
    int i = 0;
    int cur_index = 0;

    for (cur_index = 0; cur_index < ARRAY_SIZE(app_task_list); cur_index++) {
        if (app_curr_task == app_task_list[cur_index]) {//遍历当前索引
            break;
        }
    }

    for (i = cur_index; ;) { //遍历一圈
        if (i-- == 0) {
            i = ARRAY_SIZE(app_task_list) - 1;
        }
        if (i == cur_index) {
            return;
        }
        if (app_task_switch_to(app_task_list[i], NULL_VALUE)) {
            return;
        }
    }
}

//*----------------------------------------------------------------------------*/
/**@brief    获取当前模式
   @param
   @return   当前模式id
   @note
*/
/*----------------------------------------------------------------------------*/
u8 app_get_curr_task(void)
{
    return app_curr_task;
}


