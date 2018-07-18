#ifndef BUTTON_CONFIG_H
#define BUTTON_CONFIG_H

#include "input_event_manager.h"
extern const InputEventConfig_t InputEventConfig;
extern const InputActionMessage_t InputEventActions[24];

#define SW3                  (1UL <<  5)
#define SW2                  (1UL <<  4)
#define SW5                  (1UL <<  3)
#define SW4                  (1UL <<  1)
#define SW7                  (1UL <<  0)
#define SW6                  (1UL <<  2)

#define APP_MFB_BUTTON_6_SECOND                  1003
#define APP_BUTTON_FACTORY_RESET                 1022
#define APP_MFB_BUTTON_1_SECOND                  1001
#define APP_BUTTON_VOLUME_UP_DOWN                1007
#define APP_BUTTON_BACKWARD_HELD_RELEASE         1014
#define APP_MFB_BUTTON_HELD_3                    1006
#define APP_MFB_BUTTON_HELD_2                    1004
#define APP_BUTTON_VOLUME_UP_RELEASE             1011
#define APP_MFB_BUTTON_HELD_1                    1002
#define APP_BUTTON_HELD_1                        1019
#define APP_BUTTON_HELD_FACTORY_RESET            1023
#define APP_BUTTON_VOLUME_UP                     1009
#define APP_BUTTON_BACKWARD_HELD                 1013
#define APP_MFB_BUTTON_8_SECOND                  1005
#define APP_BUTTON_FORWARD_HELD_RELEASE          1017
#define APP_BUTTON_BACKWARD                      1012
#define APP_BUTTON_VOLUME_DOWN                   1008
#define APP_BUTTON_FORWARD_HELD                  1016
#define APP_BUTTON_DFU                           1020
#define APP_BUTTON_FORWARD                       1015
#define APP_BUTTON_VOLUME_DOWN_RELEASE           1010
#define APP_MFB_BUTTON_PRESS                     1000
#define APP_BUTTON_POWER_OFF                     1018
#define APP_BUTTON_HELD_DFU                      1021

#endif

