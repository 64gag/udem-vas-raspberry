/*
	(Ab)using preprocessor conditionals wherever it is possible,
	produces faster code with less branches. Recompilation is the trade-off.
 */

#ifndef BUILDFLAGS_H_
#define BUILDFLAGS_H_

 #define IMG_W 320
 #define IMG_H 240

 #define DEBUG		0
 #define PRINT_TIME	1
 #define TRIG_LOOKUP	1

 /* These are constants and should probably be defined as such instead */
 #define LINES_FACTOR	3
 #define LINES_INITIAL	20

 #define DRAW_INPUT     0
 #define DRAW_ACCUM	0
 #define DRAW_OUTPUT	0
 #define SDL_USED	DRAW_INPUT | DRAW_ACCUM | DRAW_OUTPUT

 #define OV7670		0
 #define USBSTREAM	1
 #define FAKESTREAM	0
 #define JPEGALLOC	131072
#endif
