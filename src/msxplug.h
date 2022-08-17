#ifndef _MSXPLUG_H_
#define _MSXPLUG_H_
#include "kssplay.h"
#define THE_INPUT_PLAYBACK_GUID
#include <winamp/in2.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef WACUP_BUILD
int MSXPLUG_init() ;
#else
void MSXPLUG_init() ;
#endif
void MSXPLUG_quit(void) ;
void MSXPLUG_config(HWND hParent) ;
#ifndef WACUP_BUILD
void MSXPLUG_about(HWND hParent) ;
#endif
void MSXPLUG_getfileinfo(const in_char *file, in_char *title, int *length_in_ms) ;
int MSXPLUG_infoDlg(const in_char *file, HWND hParent) ;
//int MSXPLUG_isourfile(const in_char *fn) ;
	
int MSXPLUG_play(const in_char *fn) ;
void MSXPLUG_pause() ;
void MSXPLUG_unpause() ;
int MSXPLUG_ispaused() ;
void MSXPLUG_stop() ;
const char *MSXPLUG_getMGStext() ;
int MSXPLUG_getDecodeTime();
int MSXPLUG_getOutputTime();
	
int MSXPLUG_getlength() ;
int MSXPLUG_getoutputtime() ;
void MSXPLUG_setoutputtime(int time_in_ms) ;

void MSXPLUG_setvolume(int volume) ;
void MSXPLUG_setpan(int pan) ;

#ifndef WACUP_BUILD
void MSXPLUG_eq_set(int on, char data[10], int preamp) ;
#endif

void MSXPLUG_optdlg(HWND hwndParent) ;
void MSXPLUG_edit2413(HWND hwndParent) ;
int MSXPLUG_plsdlg(HWND hwndParent) ;
void MSXPLUG_maskdlg(HWND hwndParent);

void MSXPLUG_set_song(int pos) ;
int MSXPLUG_get_song(void) ;

void MSXPLUG_play_song(int pos);

void MSXPLUG_play2(int pos, int arg) ;

#if defined(__cplusplus)
}
#endif

#endif