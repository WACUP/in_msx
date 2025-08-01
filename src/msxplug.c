/* .KSS .MGS .BGM .MPK input plug-in for Winamp ver 2.xx Copyright (c) 2001, Mitsutaka Okazaki */
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <commctrl.h>
#include <strsafe.h>
#if defined(_DEBUG)
#include <crtdbg.h>
#define _CRTDBG_MAP_ALLOC 
#endif
#include <assert.h>
#include <malloc.h>
#include "in_msx.h"
#include "msxplug.h"

#define WA_UTILS_SIMPLE
#include <../loader/loader/paths.h>
#include <../loader/loader/utils.h>

/* Message Box */
#define MBOX_ERR(M,T) MessageBox(NULL, TEXT(M), TEXT(T), MB_ICONERROR);
#define MBOX_WARN(M,T) MessageBox(NULL, TEXT(M), TEXT(T), MB_ICONWARNING);

extern BOOL force_mono(void);

#ifdef DEBUG
/* Logfile */
static wchar_t mod_path[MAX_PATH]; /* where in_msx.dll wakes up. */
static FILE *logfp;
#define LOGWRITE(S,V) (logfp?fprintf(logfp,S,V),fflush(logfp):0)
#else
#define LOGWRITE(S,V)
#endif

/* User Interfaces */
#include "config/config.h"
#include "edit2413/edit2413.h"
#include "plsdlg/plsdlg.h"
#include "kssdlg/kssdlg.h"
#include "optdlg/optdlg.h"
#include "maskdlg/maskdlg.h"

#include <winamp/wa_cup.h>
#define THE_INPUT_PLAYBACK_GUID
#include <winamp/in2.h>

extern In_Module plugin ; 

/* the buffer size, sample rate, number of channel, BitsPerSample */
static int BUFSIZE;// , RATE, NCH, BPS;
int RATE = 0, NCH = 0, BPS = 0 ;

/* current song info */
static char current_file[_MAX_PATH] ;
static KSS *current_kss ;
static int current_pos ; /* playlist position */

static int pls_refresh_flag ; /* if this flag is true, play thread is killed immediately. */

static int playlist_mode ;
static int play_time ;
static int song ;
static int detect_mode ;
static int detect_flag ; /* TURE while detecting. */
static int play_time_unknown ;
static int play_arg ; 

static int flush_flag ; /* for bug(?) in Direct Sound Plugin */

static int loop1_pos, loop2_pos ;

/* the configuration data + window */
static CONFIG *cfg ;

/* the 2413 voice editor */
static EDIT2413 *edit2413 ;

/* INFO WINDOW */
static PLSDLG *plsdlg ;
static KSSDLG *kssdlg ;
static OPTDLG *optdlg ;
static MASKDLG *maskdlg;

/* the kss player */
static KSSPLAY *kssplay ;

/* the buffer for synthsize */
static void *sample_buf ; 

/* fade time */
static int fade_time ;

/* loop */
static int loop_num ;

/* delay time before stop playing */
static unsigned int stop_delay ;

/* current play position */
static unsigned int decode_pos;
/* seek position */
static unsigned int seek_pos ;

/* for pause */
static int pause_flag = 0 ;

/* position to time and time to position */
#define POS2MS(x) ((int)((double)(x)/RATE*1000))
#define MS2POS(x) ((unsigned int)((double)(x)/1000*RATE))

//#define FADE_BIT 8
//#define FADE_BASE_BIT 23

/* the decode thread procedure */
static DWORD WINAPI __stdcall PlayThread(void *b);
static void play_start() ;
static void play_stop() ;

/* Grab Winamp Control */
static HWND hMainWindow ;
static HHOOK hHookKeyboard ;
static HHOOK hHookCallWndProc ;

/* Critical Section */
static CRITICAL_SECTION cso ;

/*static void IMSG(char *title, int i)
{
  char buf[256] ;

  I2AStr(i,buf,ARRAYSIZE(buf)) ;
  MessageBox(NULL,buf,title,MB_OK) ;
}*/

static LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  CWPSTRUCT *pmsg = (CWPSTRUCT *)lParam ;

  if(nCode == HC_ACTION){

    int num=1, min=0;

    if(current_kss) {
      if(!playlist_mode&&current_kss->info_num>0) {
        num = current_kss->info_num;
        min = 0;
      } else {
        num = 1 + (current_kss->trk_max-current_kss->trk_min);
        min = current_kss->trk_min;
        if(num<=0) num = 1;
      }
    }

    switch (pmsg->message)
  	{
    case WM_COMMAND:
    case WM_SYSCOMMAND:
      switch(pmsg->wParam)
      {
      case WINAMP_BUTTON5_CTRL:
        MSXPLUG_set_song((MSXPLUG_get_song()+10)%num+min) ;
        SendMessage(hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
        break ;

      case WINAMP_BUTTON5:
        MSXPLUG_set_song((MSXPLUG_get_song()+1)%num+min) ;
        SendMessage(hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
        break ;

      case WINAMP_BUTTON1_CTRL:
        MSXPLUG_set_song((MSXPLUG_get_song()+num-10)%num+min) ;
        SendMessage(hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
        break ;

      case WINAMP_BUTTON1:
        MSXPLUG_set_song((MSXPLUG_get_song()+num-1)%num+min) ;
        SendMessage(hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
        break ;
      }
      break ;
    }
  }
    // there's no need to use the return hook proc as per MSDN
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  if((nCode == HC_ACTION)&&(lParam&0xc0000000)){
    switch (wParam)
  	{
    case VK_F12:
      MSXPLUG_config(hMainWindow) ;
      break ;
    case VK_F11:
      MSXPLUG_edit2413(hMainWindow) ;
      break ;
    case VK_F10:
      MSXPLUG_plsdlg(hMainWindow) ;
      break ;
    }
  }

	// there's no need to use the return hook proc as per MSDN
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

/* Grab Winamp Main Window */
void GrabWinamp(HWND hWinamp)
{
  DWORD dwThreadId;
  hMainWindow = hWinamp ;
  if(!hHookKeyboard)
  {
		dwThreadId = GetWindowThreadProcessId(hMainWindow, NULL);
		hHookKeyboard = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, dwThreadId);
    hHookCallWndProc = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, NULL, dwThreadId) ;
    LOGWRITE("Winamp's window is grabbed.\n",0) ;
  }
}

void UngrabWinamp(void)
{
  if(hHookKeyboard) UnhookWindowsHookEx(hHookKeyboard) ;
  if(hHookCallWndProc) UnhookWindowsHookEx(hHookCallWndProc) ;
  LOGWRITE("Winamp's window is released.\n",0) ;
}

static void load_drivers()
{
  LOGWRITE(CONFIG_get_str(cfg,"MBMDRV"),0);
  if(!cfg->mbmdrv_loaded&&CONFIG_get_int(cfg,"ENABLE_MBM")&&KSS_load_mbmdrv(CONFIG_get_str(cfg,"MBMDRV")))
  {
    LOGWRITE("File not found : %s\n", CONFIG_get_str(cfg,"MBMDRV"));
  }
  cfg->mbmdrv_loaded = 1;

  LOGWRITE(CONFIG_get_str(cfg,"MGSDRV"),0);
  if(!cfg->mgsdrv_loaded&&CONFIG_get_int(cfg,"ENABLE_MGS")&&KSS_load_mgsdrv(CONFIG_get_str(cfg,"MGSDRV")))
  {
    LOGWRITE("File not found : %s\n", CONFIG_get_str(cfg,"MGSDRV"));
  }
  cfg->mgsdrv_loaded = 1 ;

  LOGWRITE(CONFIG_get_str(cfg,"KINROU"),0);
  if(!cfg->kinrou_loaded&&CONFIG_get_int(cfg,"ENABLE_BGM")&&KSS_load_kinrou(CONFIG_get_str(cfg,"KINROU")))
  {
    LOGWRITE("File not found : %s\n", CONFIG_get_str(cfg,"KINROU"));
  }
  cfg->kinrou_loaded = 1 ;

  LOGWRITE(CONFIG_get_str(cfg,"OPXDRV"),0);
  if(!cfg->opxdrv_loaded&&CONFIG_get_int(cfg,"ENABLE_OPX")&&KSS_load_opxdrv(CONFIG_get_str(cfg,"OPXDRV")))
  {
    LOGWRITE("File not found : %s\n", CONFIG_get_str(cfg,"OPXDRV"));
  }
  cfg->opxdrv_loaded = 1 ;

  LOGWRITE(CONFIG_get_str(cfg,"FMBIOS"),0);
  if(!cfg->fmbios_loaded&&CONFIG_get_int(cfg,"ENABLE_OPX")&&KSS_load_fmbios(CONFIG_get_str(cfg,"FMBIOS")))
  {
    LOGWRITE("File not found : %s\n", CONFIG_get_str(cfg,"FMBIOS"));
  }
  cfg->fmbios_loaded = 1 ;

  LOGWRITE(CONFIG_get_str(cfg,"MPK106"),0);
  if(!cfg->mpk106_loaded&&CONFIG_get_int(cfg,"ENABLE_MPK")&&KSS_load_mpk106(CONFIG_get_str(cfg,"MPK106")))
  {
    LOGWRITE("File not found : %s\n", CONFIG_get_str(cfg,"MPK106")) ;
  }
  cfg->mpk106_loaded = 1 ;

  LOGWRITE(CONFIG_get_str(cfg,"MPK103"),0);
  if(!cfg->mpk103_loaded&&CONFIG_get_int(cfg,"ENABLE_MPK")&&KSS_load_mpk103(CONFIG_get_str(cfg,"MPK103")))
  {
    LOGWRITE("File not found : %s\n", CONFIG_get_str(cfg,"MPK103"));
  }
  cfg->mpk103_loaded = 1 ;

}

void MSXPLUG_set_song(int i){  song = i ; }
int MSXPLUG_get_song(void){ return song ; }


/*========================================================================

                            Buffered Information

========================================================================*/
const char *MSXPLUG_getMGStext()
{
  static char ret[128];

  if(!kssplay)
  {
    ret[0] = '\0';
  }
  else
    KSSPLAY_get_MGStext(kssplay, ret, 128);

  return ret;
}

int MSXPLUG_getDecodeTime()
{
  return RATE?POS2MS(decode_pos):0;
}

int MSXPLUG_getOutputTime()
{
  return plugin.GetOutputTime();
}

/*=========================================================================

                            External Dialogs

==========================================================================*/
void MSXPLUG_optdlg(HWND hwndParent)
{
  if(hwndParent==NULL) hwndParent = plugin.hMainWindow;
  OPTDLG_open(optdlg, hwndParent, plugin.hDllInstance);
}

void MSXPLUG_edit2413(HWND hwndParent)
{
  if(hwndParent==NULL) hwndParent = plugin.hMainWindow;
  EDIT2413_open(edit2413, hwndParent, plugin.hDllInstance) ;
}

void MSXPLUG_maskdlg(HWND hwndParent)
{
  if(hwndParent==NULL) hwndParent = plugin.hMainWindow;
  MASKDLG_open(maskdlg, hwndParent, plugin.hDllInstance);
}

/*=========================================================================

                            Module Interfaces

==========================================================================*/
void MSXPLUG_config(HWND hwndParent)
{
  MSXPLUG_init();
  if(hwndParent==NULL) hwndParent = plugin.hMainWindow;
  CONFIG_dialog_show(cfg, hwndParent, plugin.hDllInstance, 0) ;
}

#ifndef WACUP_BUILD
void MSXPLUG_about(HWND hwndParent)
{
  /* CONFIG_dialog_show(cfg, hwndParent, plugin.hDllInstance, 5) ; */
  //MessageBox(hwndParent, TEXT(IN_MSX_ABOUT), TEXT("About MSXplug"), MB_OK);
}

static HANDLE hDbgfile ;

int MSXPLUG_init()
#else
void MSXPLUG_init()
#endif
{
#ifdef _DEBUG
  wchar_t *p, logfile[MAX_PATH+16] = {0};
#endif

#if defined(_MSC_VER)
#ifdef _DEBUG
  hDbgfile = CreateFile("D:\\in_msx.log", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL) ;
  _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF ) ;
  _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE ) ;
  _CrtSetReportFile( _CRT_WARN, (HANDLE)hDbgfile ) ;
#endif
#endif

#ifdef WACUP_BUILD
  if (cfg)
  {
      return;
  }
#endif

  InitializeCriticalSection(&cso) ;

#ifdef _DEBUG
  /* Get plug-in directory */
  if(!GetModuleFileName(plugin.hDllInstance, mod_path, MAX_PATH))
  {
    MBOX_ERR("Fatal Error.", PLUGIN_NAME);
    return ;
  }
  p = mod_path + wcslen(mod_path) ;
  while((mod_path<=p)&&(*p!=L'\\')) p-- ;
  *(p+1) = L'\0' ;  

  /* Open logfile */
  wcscpy(logfile, mod_path);
  wcscat(logfile, PLUGIN_NAME ".log");
  logfp = fopen(logfile, "w");
  /* if(logfp==NULL)
    MBOX_WARN("Can't create logfile.", logfile); */
#endif
  LOGWRITE("I'm " PLUGIN_NAME " at %s\n", mod_path);
  /* Load Configuration data */
  cfg = CONFIG_new((char*)GetPaths()->settings_dir_83/*mod_path*/, PLUGIN_NAME) ;
#ifndef WACUP_BUILD
  cfg->iswinamp = 1;
#endif

  CONFIG_load(cfg) ;
  cfg->hYM2413 = (HICON)LoadImage(plugin.hDllInstance,MAKEINTRESOURCE(IDB_YM2413),IMAGE_BITMAP,57,23,LR_DEFAULTCOLOR);

  LOGWRITE("Configuration finished.\n", 0) ;

  edit2413 = EDIT2413_new() ;
  LOGWRITE("Voice Editor dialog is initialized.\n",0);
  kssdlg = KSSDLG_new() ;
  LOGWRITE("KSS Information dialog is initialized.\n",0);
  plsdlg = PLSDLG_new() ;
  LOGWRITE("PLS Editor dialog is initialized.\n",0);
  optdlg = OPTDLG_new() ;
  LOGWRITE("OPT dialog is initialized.\n",0);
  maskdlg = MASKDLG_new(cfg);
  LOGWRITE("MASK dialog is initialized.\n",0);
  
  load_drivers() ;
  LOGWRITE("Some drivers have been loaded.",0);

#ifndef WACUP_BUILD
  // under wacup we'll use the newer callback
  // instead of pre-generating this ansi copy
  plugin.FileExtensions = cfg->extensions ;
#endif
  LOGWRITE("Finished init().\n", 0);

#ifndef WACUP_BUILD
  if(CONFIG_get_int(cfg,"OPENSETUP"))
  {
    LOGWRITE("This is the first time to use " PLUGIN_NAME ".\n", 0);
    MSXPLUG_config(NULL);
    CONFIG_set_int(cfg,"OPENSETUP",0) ;
    LOGWRITE("Config dialog is opened.\n", 0);
  }
  return IN_INIT_SUCCESS;
#endif
}

void MSXPLUG_quit() 
{ 
  if(edit2413)
  {
    EDIT2413_save(edit2413,cfg->ill_path) ;
    EDIT2413_delete(edit2413) ;
    LOGWRITE("EDIT2413DLG was deleted.\n",0);
  }

  PLSDLG_delete(plsdlg) ;
  LOGWRITE("PLSDLG was deleted.\n",0);

  KSSDLG_delete(kssdlg) ;
  LOGWRITE("KSSDLG was deleted.\n",0);

  OPTDLG_delete(optdlg) ;  
  LOGWRITE("OPT dialog was deleted.\n",0);

  MASKDLG_delete(maskdlg);
  LOGWRITE("OPT dialog was deleted.\n",0);

  if(current_kss)
  {
    EnterCriticalSection(&cso) ;
    KSS_delete(current_kss) ;
    current_kss = NULL;
    LeaveCriticalSection(&cso) ;
    LOGWRITE("Current KSS data was deleted.\n",0);
  }

  DeleteCriticalSection(&cso) ;

  CONFIG_delete(cfg) ;
  LOGWRITE("Configuration Deleted.\n",0);

  UngrabWinamp() ;
  LOGWRITE("Ungrab Winamp.\n",0);

#ifdef _DEBUG
  if(logfp) fclose(logfp);
#endif

#ifdef _MSC_VER
#ifdef _DEBUG
  if(_CrtDumpMemoryLeaks()) MessageBox(NULL, "Memory is leaked.", "MEMORY LEAK", MB_OK) ;
  CloseHandle(hDbgfile) ;
#endif
#endif

}

/*int MSXPLUG_isourfile(const in_char *fn) 
{
#if 0
  PLSITEM *item ;

  char filepath[_MAX_PATH]/* = { 0 }*//*;
  ConvertUnicodeFn(filepath, ARRAYSIZE(filepath), fn, CP_ACP);

  MSXPLUG_init();

  item = PLSITEM_new(filepath) ;

  if(item->type)
  {
    if(item->type==1&&!CONFIG_get_int(cfg,"ENABLE_KSS"))
    {
      PLSITEM_delete(item) ;
      return 0 ;
    }
    PLSITEM_delete(item) ;
    return 1 ;
  }
  else
  {
    PLSITEM_delete(item) ;
    return 0;
  }
#else
  return 0;
#endif
}*/

static void detect_time_setting(void)
{
  loop1_pos = loop2_pos = 0 ;

  if(pls_refresh_flag)
  {
    detect_mode = 0 ;
  }
  else if(play_arg)
  {
    if(current_kss->loop_detectable||current_kss->stop_detectable) detect_mode = play_arg ;
  }
  else if(play_time_unknown)
  {
    detect_mode = CONFIG_get_int(cfg,"TIME_DETECT_MODE") ;
  }
  else
  {
    detect_mode = 0 ;
  }

  if((current_kss->stop_detectable||current_kss->loop_detectable)&&detect_mode)
  {
    if(detect_mode>=2)
    {
      loop_num = CONFIG_get_int(cfg,"TIME_DETECT_LEVEL");
      play_time = CONFIG_get_int(cfg,"PLAYTIME");
      fade_time = CONFIG_get_int(cfg,"FADETIME");
    }
  }
  else detect_mode = 0 ;
}

void MSXPLUG_play2(int pos, int arg)
{
  play_arg = arg ;
  SendMessage(plugin.hMainWindow,WM_WA_IPC,pos,IPC_SETPLAYLISTPOS) ;
  PostMessage(plugin.hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
}

void MSXPLUG_play_song(int pos)
{
  MSXPLUG_set_song(pos);
  PostMessage(plugin.hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
}

static void kss2pls(KSS *kss, char *base, int i, char *buf, int buflen, int playtime, int fadetime) {
  
  int j=0; char *p;

  if(kss->info_num<=i)
    _snprintf(buf,buflen,"Error!");
  else {
    playtime = kss->info[i].time_in_ms>=0?kss->info[i].time_in_ms:playtime;
    fadetime = kss->info[i].fade_in_ms>=0?kss->info[i].fade_in_ms:fadetime;
    j+=_snprintf(buf+j,buflen-j,"%s::KSS,%d,",base,kss->info[i].song);
    p = kss->info[i].title;
    while( j<buflen-2 && *p!='\0' ) {
      if(*p==',')
        buf[j++]='\\';
      else if((0x80<=*p&&*p<=0x9F)||(0xE0<=*p)) 
        buf[j++]=*(p++);
      buf[j++]=*(p++);
    }
    if(j<buflen)
      _snprintf(buf+j,buflen-j,",%d,,%d,1\n",playtime/1000,fadetime/1000);
    buf[buflen-1]='\0';
  }

}

void MSXPLUG_get_channel_output_config(void)
{
  RATE = CONFIG_get_int(cfg,"RATE") ;

  if (!force_mono())
  {
    switch(CONFIG_get_int(cfg,"STEREO"))
    {
    case 0: /* AUTO */ 
      NCH = current_kss->stereo||(CONFIG_get_int(cfg,"OPLL_STEREO")&&current_kss->fmpac)?2:1;
      break;

    case 1: /* msx */
      NCH = 1;
      break;

    case 2: /* STEREO */
    default:
      NCH = 2;
      break;
    }
  }
  else
  {
      // if the force mono playback mode is enabled then
      // this will show that state via the config dialog
      // but will prevent this plug-in's mode being used
      NCH = 1;
  }
}

static int play_setup(const char *fn)
{
  PLSITEM *item ;
  int i;

  load_drivers() ;
  KSS_set_mbmparam(CONFIG_get_int(cfg,"MBM_MODE"), CONFIG_get_int(cfg,"MBM_ENABLE_STEREO"), CONFIG_get_int(cfg, "MBM_SYNC"));
  KSS_autoload_mbk(fn, CONFIG_get_str(cfg, "MBK_PATH"), CONFIG_get_str(cfg,"MBK_DUMMY"));

  item = PLSITEM_new(fn) ;
  if(!item->filename) goto error_exit;

  if(strcmp(item->filename,current_file)!=0) song = 0;

  if(current_kss)
  {
    EnterCriticalSection(&cso) ;
    KSS_delete(current_kss) ;
    current_kss = NULL ;
    LeaveCriticalSection(&cso) ;
  }
  if((current_kss = KSS_load_file(item->filename))==NULL)
  {
    goto error_exit;
  }
  CopyCchStrA(current_file,_MAX_PATH,item->filename) ;
  current_file[_MAX_PATH-1] = '\0' ;
  if(current_kss->title[0]=='\0')
  {
    CopyCchStrA((char *)current_kss->title, KSS_TITLE_MAX, item->filename) ;
    current_kss->title[KSS_TITLE_MAX-1] = '\0' ;
  }

  MSXPLUG_get_channel_output_config();

  BUFSIZE = CONFIG_get_int(cfg,"BUFSIZE") ;
  BPS = CONFIG_get_int(cfg,"BPS") ;

  assert(kssplay == NULL);
  if((kssplay = KSSPLAY_new(RATE,NCH,BPS))==NULL) goto error_exit;
  assert(sample_buf == NULL);
  if((sample_buf = malloc(BUFSIZE*NCH*BPS/8))==NULL) goto error_exit;
  memset(sample_buf,0,BUFSIZE*NCH*BPS/8) ;

  playlist_mode = 0 ;
  play_time_unknown = 1 ;
  play_time = CONFIG_get_int(cfg,"PLAYTIME");
  fade_time = CONFIG_get_int(cfg,"FADETIME");
  loop_num = CONFIG_get_int(cfg,"LOOP");

  KSSPLAY_set_master_volume(kssplay, CONFIG_get_int(cfg,"MASTER_VOL"));

  for(i=0;i<EDSC_MAX;i++)
  {
    cfg->plsvol[i] = cfg->vol[i];
    KSSPLAY_set_device_volume(kssplay, i, cfg->vol[i]);
  }

  if(CONFIG_get_int(cfg,"OPLL_STEREO")) kssplay->opll_stereo = 1; else kssplay->opll_stereo = 0;

  if(item->type)
  {
    playlist_mode = 1 ;
    if(item->time_in_ms>=0) play_time_unknown = 0 ;
    if(item->title)
    {
      CopyCchStrA((char *)current_kss->title, KSS_TITLE_MAX, item->title) ;
      current_kss->title[KSS_TITLE_MAX-1] = '\0' ;
    }
    song = item->song ;
    PLSITEM_adjust(item, 
      CONFIG_get_int(cfg,"PLAYTIME"),
      CONFIG_get_int(cfg,"FADETIME"),
      CONFIG_get_int(cfg,"LOOP"),
      cfg->vol);
    play_time = item->time_in_ms ;
    fade_time = item->fade_in_ms ;
    loop_num = item->loop_num ;
    if(loop_num) play_time += item->loop_in_ms * (loop_num - 1) ;

    for(i=0;i<EDSC_MAX;i++)
    {
      cfg->plsvol[i] = item->vol[i] ;
      if(!CONFIG_get_int(cfg,"OVERWRITE_VOLUME"))
        KSSPLAY_set_device_volume(kssplay,i,cfg->plsvol[i]);
      else 
        KSSPLAY_set_device_volume(kssplay,i,cfg->vol[i]) ;
    }

  }
  else if(current_kss->type==KSSDATA)
  {
    static char tmp[KSS_TITLE_MAX]; /* important! */
    if(current_kss->title[0])
    {
      memcpy(tmp,current_kss->title,KSS_TITLE_MAX);
      _snprintf((char *)current_kss->title,KSS_TITLE_MAX-1,"[KSS:$%02x]%s",song,tmp) ;
    }
    else
      _snprintf((char *)current_kss->title,KSS_TITLE_MAX-1,"[KSS:$%02x]%s",song,item->filename) ;
  }  


  if(CONFIG_get_int(cfg,"EXPAND_INFO")>0&&!playlist_mode&&current_kss->info_num>0) 
  {
    static char plsfile[MAX_PATH+16];
    static char buf[MAX_PATH];

    strcpy(plsfile, GetPaths()->settings_dir_83/*mod_path*/);
    strcat(plsfile,"in_msx.pls");
    kss2pls(current_kss, (char *)fn, 0, buf, MAX_PATH, play_time, fade_time);
    SendMessage(hMainWindow,WM_WA_IPC,(WPARAM)buf,IPC_CHANGECURRENTFILE);
    FILE *fp = fopen(plsfile,"w");
    if(fp) {
      fprintf(fp,"[playlist]\n");
      for(i=1;i<current_kss->info_num;i++) {
        kss2pls(current_kss, (char *)fn, i, buf, MAX_PATH, play_time, fade_time);
        fprintf(fp,"File%d=%s\n",i,buf);
      }
      fprintf(fp,"NumberofEntries=%d\n",current_kss->info_num-1);
      fputs("Version=2",fp);
      fclose(fp);
      const COPYDATASTRUCT cds = { IPC_ENQUEUEFILE, (int)strlen(plsfile) + 1, (void*)plsfile };
      SendMessage(hMainWindow,WM_COPYDATA,(WPARAM)NULL,(LPARAM)&cds);
    }
    song=0;
    PostMessage(hMainWindow,WM_COMMAND,(WPARAM)WINAMP_BUTTON2,(LPARAM)0);
    return 1;
  }

  for(i=0;i<EDSC_MAX;i++)
    cfg->curvol[i] = KSSPLAY_get_device_volume(kssplay, i) ;
  cfg->refresh_mixer = 1 ;

  EDIT2413_set_target(edit2413, kssplay->vm->opll) ;
  EDIT2413_apply(edit2413) ;

  if(KSSPLAY_set_data(kssplay, current_kss)) goto error_exit;

  cfg->mode_update = 1;
  CONFIG_apply_update(cfg,kssplay);
  if(!playlist_mode&&kssplay->kss->info_num>0) {
    memcpy(current_kss->title,kssplay->kss->info[song%kssplay->kss->info_num].title,KSS_TITLE_MAX);
    if(play_time>0) play_time = current_kss->info[song%current_kss->info_num].time_in_ms;
    if(fade_time>0) fade_time = current_kss->info[song%current_kss->info_num].fade_in_ms;
    if(fade_time==-1) fade_time = CONFIG_get_int(cfg,"FADETIME");
    play_time_unknown = FALSE;
    KSSPLAY_reset(kssplay,kssplay->kss->info[song%kssplay->kss->info_num].song, CONFIG_get_int(cfg,"CPUSPEED"));
  } else 
    KSSPLAY_reset(kssplay,song, CONFIG_get_int(cfg,"CPUSPEED"));
  CONFIG_force_update(cfg,kssplay) ;

  decode_pos = 0 ;
  seek_pos = 0 ;
  stop_delay = 0 ;
  pause_flag = 0 ;
  
  detect_time_setting() ;

  if(CONFIG_get_int(cfg,"SILENT_DETECT"))
    KSSPLAY_set_silent_limit(kssplay, CONFIG_get_int(cfg,"SILENT_LIMIT")*1000);
  else
    KSSPLAY_set_silent_limit(kssplay, 0);

  if(kssdlg&&kssdlg->sync) KSSDLG_update(kssdlg,item->filename,playlist_mode) ;

  PLSITEM_delete(item);

  return 0 ;

error_exit:
  PLSITEM_delete(item);
  item = NULL;
  KSSPLAY_delete(kssplay);
  kssplay = NULL;
  free(sample_buf);
  sample_buf = NULL;

  return 1 ;
}

static void setup_window(void)
{
  static int flag = 1 ;
  if(flag)
  {
    MSXPLUG_init() ;
    GrabWinamp(plugin.hMainWindow) ;
    cfg->hWinamp = plugin.hMainWindow ;
    EDIT2413_load(edit2413,cfg->ill_path) ;
    flag = 0 ;
  }
}

static int refresh_current_item(void) ;
int MSXPLUG_play(const in_char *fn) {
  
  int maxlatency ;

  setup_window() ;  

  current_pos = (int)SendMessage(plugin.hMainWindow,WM_WA_IPC,0,IPC_GETLISTPOS) ;

  if(plsdlg&&plsdlg->sync)
    PLSDLG_set_item(plsdlg,current_pos) ;

  char filepath[_MAX_PATH]/* = { 0 }*/;
  if(play_setup(ConvertUnicodeFn(filepath, ARRAYSIZE(filepath), fn, CP_ACP)))
  {
    play_arg = 0 ;
    return 1 ;
  }

  maxlatency = (plugin.outMod && plugin.outMod->Open && RATE && NCH ?
                plugin.outMod->Open(RATE, NCH, BPS, -1, -1) : -1);
	if(maxlatency < 0)
  {
    play_arg = 0 ;
    return 1 ;
  }
	
  plugin.SetInfo((RATE*BPS*NCH)/1000,RATE/1000,NCH,1);
  plugin.SAVSAInit(maxlatency,RATE);
  plugin.VSASetInfo(RATE,NCH);
  plugin.outMod->SetVolume(-666);
  plugin.outMod->Flush(0) ;
  flush_flag = 1 ;

  if(pls_refresh_flag)
  {
    if(refresh_current_item())
    {
      MessageBox(plugin.hMainWindow, TEXT("MSXplug internal Error"), TEXT("Internal Error"), MB_OK) ;
    }
    else return 0 ;
  }
  else
  {
    play_start() ;
    if( detect_mode >= 2 ) MSXPLUG_setoutputtime(MS2POS(play_time)) ; /* Auto seek start */
  }

  play_arg = 0 ;
  return 0 ; 
} 

void MSXPLUG_pause() { pause_flag = 1 ; if (plugin.outMod) plugin.outMod->Pause(1) ; }
void MSXPLUG_unpause() { pause_flag = 0 ; if (plugin.outMod) plugin.outMod->Pause(0) ; }
int MSXPLUG_ispaused() { return pause_flag ; } 

static int isPlaying(void) ;
static void update_pls(void)
{
  int pos ;
  char buf[PLSITEM_PRINT_SIZE] ;
  PLSITEM *item ;

  item = PLSITEM_new((char *)SendMessage(plugin.hMainWindow,WM_WA_IPC,current_pos,IPC_GETPLAYLISTFILE)) ;
  if(loop1_pos==0)
  {
    if((detect_mode==1)||(detect_mode==4))
    {
      PLSITEM_delete(item) ;
      return ;
    }
    else
    {
      item->time_in_ms = CONFIG_get_int(cfg,"PLAYTIME") ;
    }
  }
  else item->time_in_ms = POS2MS(loop1_pos) ;
  
  if(loop2_pos) item->loop_in_ms = POS2MS(loop2_pos - loop1_pos) ;
  
  PLSITEM_set_title(item, (char *)current_kss->title) ;

  if(!isPlaying())
  {
    item->time_in_ms += 1000 ;
    item->loop_num = 1 ;
    item->fade_in_ms = 0 ;
    item->loop_in_ms = 0 ;
  }

  pos = (int)SendMessage(plugin.hMainWindow,WM_WA_IPC,0,IPC_GETLISTPOS) ;
  SendMessage(plugin.hMainWindow,WM_WA_IPC,current_pos,IPC_SETPLAYLISTPOS) ;
  SendMessage(plugin.hMainWindow,WM_WA_IPC,(WPARAM)PLSITEM_print(item,buf,NULL),IPC_CHANGECURRENTFILE) ;
  SendMessage(plugin.hMainWindow,WM_WA_IPC,pos,IPC_SETPLAYLISTPOS) ;
  PLSDLG_set_item(plsdlg,-1) ;
  PLSITEM_delete(item) ;
}

void MSXPLUG_stop()
{ 
  play_stop() ;
  EDIT2413_unset_target(edit2413) ;
  KSSPLAY_delete(kssplay) ;
  kssplay = NULL ;
  free(sample_buf) ;
  sample_buf = NULL;
  if (plugin.outMod && plugin.outMod->Close)
  {
      plugin.outMod->Close();
  }
  if (plugin.outMod)
  {
      plugin.SAVSADeInit();
  }
}

int MSXPLUG_getlength()
{
  return play_time + fade_time ;
}

int MSXPLUG_getoutputtime()
{
  if(seek_pos) 
	return POS2MS(decode_pos);
  if(plugin.outMod) 
	return plugin.outMod->GetOutputTime();
  
  return 0;
}
void MSXPLUG_setoutputtime(int time_in_ms) 
{
  play_stop() ;

  if(POS2MS(decode_pos)<time_in_ms) 
  {
    seek_pos = MS2POS(time_in_ms) ;
  }
  else
  {
    cfg->mode_update=1;
    CONFIG_apply_update(cfg,kssplay);
    KSSPLAY_reset(kssplay, song, CONFIG_get_int(cfg,"CPUSPEED"));
    CONFIG_force_update(cfg,kssplay);
    seek_pos = MS2POS(time_in_ms) ;
    decode_pos = 0 ;
  }

  if (plugin.outMod)
  {
      plugin.outMod->Flush(POS2MS(decode_pos));
  }
  flush_flag = 1 ;

  play_start() ;
} 

void MSXPLUG_setvolume(int volume)
{
  if (plugin.outMod)
  {
      plugin.outMod->SetVolume(volume);
  }
}

void MSXPLUG_setpan(int pan)
{
  if (plugin.outMod)
  {
    plugin.outMod->SetPan(pan) ;
  }
}

int MSXPLUG_plsdlg(HWND hwnd)
{
  PLSDLG_open(plsdlg, plugin.hDllInstance, plugin.hMainWindow) ;
  PLSDLG_set_item(plsdlg,-1) ;
  return 0 ;
}

int MSXPLUG_infoDlg(const in_char *fn, HWND hwnd)
{
  PLSITEM *item ;
  char filepath[_MAX_PATH]/* = { 0 }*/;
  item = PLSITEM_new(ConvertUnicodeFn(filepath, ARRAYSIZE(filepath), fn, CP_ACP)) ;
  KSSDLG_open(kssdlg, plugin.hDllInstance, plugin.hMainWindow) ;

  if(strcmp(filepath,current_file)==0)
    KSSDLG_update(kssdlg, item->filename,item->type);
  else
    KSSDLG_update(kssdlg, item->filename,1);

  PLSITEM_delete(item) ;

  return 0 ;
}

void MSXPLUG_getfileinfo(const in_char *filename, in_char *title, int *length_in_ms)
{
  const int MAXLEN = _MAX_PATH ;
  PLSITEM *item ;

  MSXPLUG_init();

  if((!filename)||(!filename[0]))
  {
#ifndef WACUP_BUILD
    strncpy(title, (char*)current_kss->title, MAXLEN);
    title[MAXLEN] = '\0';
#else
    ConvertANSI((char*)current_kss->title, -1, CP_ACP,
                title, GETFILEINFO_TITLE_LENGTH, NULL);
#endif

    if(play_time_unknown) *length_in_ms = 0 ;
    else if(loop_num) *length_in_ms = play_time + fade_time ;
    else *length_in_ms = 0 ;
    return ;
  }
    
  if(filename)
  {
    char filepath[_MAX_PATH]/* = { 0 }*/;
    item = PLSITEM_new(ConvertUnicodeFn(filepath, ARRAYSIZE(filepath), filename, CP_ACP)) ;

    if(item->time_in_ms<=0) *length_in_ms = 0 ;
    else
    {
      PLSITEM_adjust(item, 
        CONFIG_get_int(cfg,"PLAYTIME"), 
        CONFIG_get_int(cfg,"FADETIME"), 
        CONFIG_get_int(cfg,"LOOP"),
        cfg->vol) ;
      *length_in_ms = item->time_in_ms + item->fade_in_ms ;
      if(item->loop_num) *length_in_ms += item->loop_in_ms * (item->loop_num - 1) ;
      else *length_in_ms = 0 ;
    }

    if(item->title)
    {
#ifndef WACUP_BUILD
      strncpy(title, item->title, MAXLEN);
      title[MAXLEN] = '\0';
#else
      ConvertANSI(item->title, -1, CP_ACP, title,
                  GETFILEINFO_TITLE_LENGTH, NULL);
#endif
    }
    else
    {
      KSS *kss ;

      if((kss = KSS_load_file(item->filename))==NULL)
      {
#ifndef WACUP_BUILD
        strncpy(title, item->filename, MAXLEN);
        title[MAXLEN] = '\0';
#else
        ConvertANSI(item->filename, -1, CP_ACP, title,
                      GETFILEINFO_TITLE_LENGTH, NULL);
#endif
      }
      else
      {
        if(kss->title[0]=='\0')
        {
          CopyCchStrA((char *)kss->title, KSS_TITLE_MAX, item->filename) ;
          kss->title[KSS_TITLE_MAX-1] = '\0' ;
        }

#ifndef WACUP_BUILD
        strncpy(title, (char*)kss->title, MAXLEN);
        title[MAXLEN] = '\0';
#else
        ConvertANSI((char*)kss->title, -1, CP_ACP, title,
                         GETFILEINFO_TITLE_LENGTH, NULL);
#endif
        KSS_delete(kss) ;
      }
    }

    PLSITEM_delete(item) ;
  }
  
}

#ifndef WACUP_BUILD
void MSXPLUG_eq_set(int on, char data[10], int preamp){} 
#endif

/*=========================================================================

                               Play Threads

==========================================================================*/
static int killPlayThread=0 ;					
static HANDLE thread_handle=INVALID_HANDLE_VALUE ;

static DWORD WINAPI __stdcall NoPlayThread(void *b)
{
  EnterCriticalSection(&cso) ;

  Sleep(100) ;
  if(pls_refresh_flag==1)
  {      
    PostMessage(plugin.hMainWindow, WM_WA_MPEG_EOF, 0, 0) ; // Stop and Next
  }
  else
  {
    PostMessage(plugin.hMainWindow, WM_COMMAND, WINAMP_BUTTON4, 0) ; // Stop Only
  }
  pls_refresh_flag = 0 ;

  LeaveCriticalSection(&cso) ;
  return 0 ;
}

static int refresh_current_item(void)
{
  if(thread_handle!=INVALID_HANDLE_VALUE) return 1 ;
  killPlayThread = 0 ;
  thread_handle = StartThread(NoPlayThread, &killPlayThread,
                          THREAD_PRIORITY_HIGHEST, 0, NULL);
  return 0 ;
}

static void play_start()
{
  if(thread_handle!=INVALID_HANDLE_VALUE) return ;
  killPlayThread = 0 ;
  thread_handle = StartThread(PlayThread, &killPlayThread, /*plugin.config->
							  GetInt(playbackConfigGroupGUID, L"priority",
								   */THREAD_PRIORITY_HIGHEST/*)*/, 0, NULL);
}

static void play_stop()
{
  if(thread_handle==INVALID_HANDLE_VALUE) return ;

  killPlayThread = 1 ;
  if (WaitForSingleObject(thread_handle,3000) == WAIT_TIMEOUT)
  {
    MessageBox(plugin.hMainWindow, TEXT("Error asking thread to die!\n"), TEXT("error killing decode thread"), 0);
	TerminateThread(thread_handle,0);
  }
  CloseHandle(thread_handle);
  thread_handle = INVALID_HANDLE_VALUE;
}

static int isStopped(void)
{
  return KSSPLAY_get_stop_flag(kssplay);
}

/* check loop */
static int isLooped(void)
{
  int ret = KSSPLAY_get_loop_count(kssplay) ;

  if( ret==1&&loop1_pos==0 ) loop1_pos = decode_pos ;
  else if( ret==2&&loop2_pos==0 ) loop2_pos = decode_pos ;
  
  if(loop_num&&ret>=loop_num) return 1 ;

  return 0 ;
}

/* check playing */
static int isPlaying(void)
{
  if(stop_delay)
  {
    if(stop_delay==1)
    {
      loop1_pos = decode_pos ;
      return 0 ;
    }
    stop_delay-- ;
    return 1 ;
  }
  else if(KSSPLAY_get_stop_flag(kssplay))
  {
    stop_delay = RATE/BUFSIZE ;
    return 1 ;
  }
  else
  {
    return 1 ;
  }
}

static int isBeforeFade(void)
{
  return KSSPLAY_get_fade_flag(kssplay)==KSSPLAY_FADE_NONE;
}

static int isFadeEnd(void)
{
  return KSSPLAY_get_fade_flag(kssplay)==KSSPLAY_FADE_END;
}

/* This function is too dirty to read.... It will be rewritten by using state machine */
static DWORD WINAPI __stdcall PlayThread(void *b)
{ 

  int decode_pos_ms = 0 ;
  int length = BUFSIZE * NCH * (BPS/8) ; /* buffer length (byte) */

  while (!(*(int *)b)) 
  {
    if(!isPlaying()||isFadeEnd()) /* Stop : Wait until finish playing */
    {  
      if(plugin.outMod->IsPlaying()&&!flush_flag)
      {
        Sleep(10) ;
        continue ;
      }

      if(detect_mode)
      {
        update_pls() ;
        switch(detect_mode)
        {
        case 1: /* Refresh playlist and play next. */
        case 3:
          pls_refresh_flag = 1 ;
          PostMessage(plugin.hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
          break ;
        case 2: /* Replay current song. */
          pls_refresh_flag = 0 ;
          PostMessage(plugin.hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
          break;
        case 4: /* Refresh playlist and stop. */
          pls_refresh_flag = 2 ;
          PostMessage(plugin.hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
          break ;
        default:
          break ;
        }        
      }
      else if(kssplay->kss->type!=KSSDATA||playlist_mode)
      {
        PostMessage(plugin.hMainWindow, WM_WA_MPEG_EOF, 0, 0) ;
      }
      else
      {
        song=(song+1)%0xff ;
        PostMessage(plugin.hMainWindow, WM_COMMAND, WINAMP_BUTTON2, 0) ;
      }

      *(int*)b = 1 ;
    }
    else if(seek_pos) /* Seeking */
    {

      if(decode_pos < seek_pos)
      {
        KSSPLAY_calc_silent(kssplay, BUFSIZE) ;
		decode_pos += BUFSIZE ;
        decode_pos_ms = POS2MS(decode_pos) ;
        if(isBeforeFade()&&(isLooped()||play_time<decode_pos_ms))
          KSSPLAY_fade_start(kssplay,fade_time) ;
      }
      else
      {
        seek_pos = 0 ;
        plugin.outMod->Flush(decode_pos_ms) ;
        flush_flag = 1 ;
      }

    }
    else if(plugin.outMod->CanWrite() >= (length<<(plugin.dsp_isactive()?1:0))) /* Create wave buffer */
	{

#define PRE_BLANK 5
      if(decode_pos < PRE_BLANK) /* A measure for DirectSound Plug-in */
      {
        decode_pos++ ;
        plugin.outMod->Write((char *)sample_buf, length) ;
        continue ;
      }

      CONFIG_apply_update(cfg,kssplay);
	  KSSPLAY_calc(kssplay, (short *)sample_buf, BUFSIZE) ;

	  decode_pos += BUFSIZE  ;
      decode_pos_ms = POS2MS(decode_pos) ;

      if(isBeforeFade()&&(isLooped()||play_time<decode_pos_ms-PRE_BLANK))
        KSSPLAY_fade_start(kssplay,fade_time) ;

      if (plugin.dsp_isactive())
        length=plugin.dsp_dosamples((short *)sample_buf,BUFSIZE,BPS,NCH,RATE) * (NCH*(BPS/8)) ;

      plugin.outMod->Write((char *)sample_buf,length);
      flush_flag = 0 ;
      plugin.SAAddPCMData(sample_buf,NCH,BPS,decode_pos_ms) ;
      /*plugin.VSAAddPCMData(sample_buf,NCH,BPS,decode_pos_ms) ;*/

    }
    else Sleep(10);
  }

  return 0;

}

