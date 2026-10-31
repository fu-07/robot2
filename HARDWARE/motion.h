#ifndef __MOTION_H
#define __MOTION_H

#include "main.h"

/* 运动状态 */
typedef enum {
    MOTION_STOP = 0,      // 停止
    MOTION_FORWARD,       // 前进
    MOTION_TURN_LEFT,     // 左转
    MOTION_TURN_RIGHT     // 右转
} MotionState;

#endif
