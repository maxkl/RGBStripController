
#pragma once

/*

none
00000000

on/off
00000010
00000011

brightness de-/increment
00000100
00000101

speed de-/increment
00000110
00000111

animation
0001xxxx

color
001xxxxx

brightness
10xxxxxx

speed
11xxxxxx

unused
00000001
00001xxx
01xxxxxx

*/

// TODO: broadcast state changes from master?

// TODO: inter-slave communication, e.g. enable visualizer from remote

#define SLAVE_COMMAND_NONE 0x00

#define SLAVE_COMMAND_OFF 0x02
#define SLAVE_COMMAND_ON 0x03

#define SLAVE_COMMAND_BRIGHTNESS_DECREMENT 0x04
#define SLAVE_COMMAND_BRIGHTNESS_INCREMENT 0x05
#define SLAVE_COMMAND_SPEED_DECREMENT 0x06
#define SLAVE_COMMAND_SPEED_INCREMENT 0x07

#define SLAVE_COMMAND_ANIMATION(animation_id) (0x10 | ((animation_id) & 0x0f))

#define SLAVE_COMMAND_COLOR(color_id) (0x20 | ((color_id) & 0x1f))

#define SLAVE_COMMAND_BRIGHTNESS_SET(brightness) (0x80 | ((brightness) & 0x3f))
#define SLAVE_COMMAND_SPEED_SET(speed) (0xc0 | ((speed) & 0x3f))

#define MASTER_COMMAND_ANIMATION_OFF 0x01
#define MASTER_COMMAND_ANIMATION_ON 0x02
