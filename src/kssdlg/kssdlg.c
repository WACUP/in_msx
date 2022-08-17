#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <commctrl.h>
#include <assert.h>
#include "msxplug.h"
#include "kssplay.h"
#include "kssdlg.h"
#include "rc/resource.h"

#ifdef WACUP_BUILD
#define WA_UTILS_SIMPLE
#include <../../loader/loader/utils.h>
#endif

static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  KSSDLG *kssdlg = (KSSDLG *)GetProp(hDlg, TEXT("INFO")) ;

  switch (uMsg)
	{
  case WM_INITDIALOG:
    SendDlgItemMessage(hDlg, IDC_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0,255)) ;
    SendDlgItemMessage(hDlg, IDC_SLIDER, TBM_SETPOS, TRUE, 0) ;
    SetWindowText(GetDlgItem(hDlg,IDC_SONG), TEXT("0"));
		return TRUE;

  case WM_HSCROLL:
    if((HWND)(lParam) == GetDlgItem(hDlg,IDC_SLIDER))
    {
      char buf[16];
      int pos = (int)SendMessage((HWND)(lParam), TBM_GETPOS, 0, 0);
      SetDlgItemTextA(hDlg,IDC_SONG,I2AStr(pos,buf,ARRAYSIZE(buf)));
      if(wParam == SB_ENDSCROLL) MSXPLUG_play_song(pos);
    }
    break;

  case WM_COMMAND:
    
    if(LOWORD(wParam) == IDM_OPTDLG)
    {
      MSXPLUG_optdlg(NULL);
      break;
    }
    else if(LOWORD(wParam) == IDM_CONFIG)
    {
      MSXPLUG_config(NULL);
      break;
    }
    else if(LOWORD(wParam) == IDM_VEDIT)
    {
      MSXPLUG_edit2413(NULL);
      break;
    }
    else if(LOWORD(wParam) == IDM_MASK)
    {
      MSXPLUG_maskdlg(NULL);
      break;
    }

    if(HIWORD(wParam) == BN_CLICKED)
    {
      if(LOWORD(wParam) == IDC_PREV)
      {
        SendDlgItemMessage(hDlg,IDC_SLIDER,WM_KEYDOWN,VK_LEFT,1);
        SendDlgItemMessage(hDlg,IDC_SLIDER,WM_KEYUP,VK_LEFT,0);
        return TRUE;
      }
      if(LOWORD(wParam) == IDC_NEXT)
      {
        SendDlgItemMessage(hDlg,IDC_SLIDER,WM_KEYDOWN,VK_RIGHT,1);
        SendDlgItemMessage(hDlg,IDC_SLIDER,WM_KEYUP,VK_RIGHT,0);
        return TRUE;
      }
      if(LOWORD(wParam) == IDOK)
      {
        KSSDLG_close(kssdlg) ;
        return TRUE;
      }
      else if(LOWORD(wParam) == IDCANCEL)
      {
        KSSDLG_close(kssdlg) ;
        return TRUE ;
      }
      else if(LOWORD(wParam) == IDC_SYNC)
      {
        if(IsDlgButtonChecked(hDlg, IDC_SYNC) == BST_CHECKED)
          kssdlg->sync = 1 ;
        else
          kssdlg->sync = 0 ;
        return TRUE ;
      }
    }
    break ;

  default:
    break; 
	}

	return FALSE;
}

static const char *type2str(int type)
{
  switch(type)
  {
  case KSSDATA:
    return "KSS" ;
  case MGSDATA:
    return "MGSDRV" ;
  case MPK106DATA:
  case MPK103DATA:
    return "MPK" ;
  case OPXDATA:
    return "OPLLDriver" ;
  case BGMDATA:
    return "MuSICA" ;
  case MBMDATA:
    return "MoonBlaster" ;
  default:
    return "UNKNOWN" ;
  }
}

void KSSDLG_update(KSSDLG *kssdlg, char *filename, int playlist_mode)
{
  KSS *kss ;
  char buf[16];

  assert(kssdlg) ;

  if(!kssdlg->dialog) return ;

  SetDlgItemTextA(kssdlg->dialog, IDC_FILENAME, filename ) ;
  kss = KSS_load_file(filename) ;
  if(kss)
  {
    SetDlgItemTextA(kssdlg->dialog, IDC_TYPE, type2str(kss->type)) ;
    SetDlgItemTextA(kssdlg->dialog, IDC_ID, (char *)kss->idstr) ;
    SetDlgItemTextA(kssdlg->dialog, IDC_TITLE, (char *)kss->title) ;
    SetDlgItemTextA(kssdlg->dialog, IDC_EXTRA, (char *)kss->extra) ;
    SendDlgItemMessage(kssdlg->dialog, IDC_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(kss->trk_min,kss->trk_max)) ;
    SendDlgItemMessage(kssdlg->dialog, IDC_SLIDER, TBM_SETPOS, TRUE, MSXPLUG_get_song()) ;
    if(kss->trk_max==kss->trk_min)
    {
      EnableControl(kssdlg->dialog,IDC_SLIDER,FALSE);
      EnableControl(kssdlg->dialog,IDC_PREV,FALSE);
      EnableControl(kssdlg->dialog,IDC_NEXT,FALSE);
      SendDlgItemMessage(kssdlg->dialog, IDC_SLIDER, TBM_SETTICFREQ, 1,0) ;
      SendDlgItemMessage(kssdlg->dialog, IDC_SLIDER, TBM_SETPAGESIZE, 0,1);
      SetWindowText(GetDlgItem(kssdlg->dialog,IDC_SONG), TEXT("N/A"));
    }
    else
    {
      EnableControl(kssdlg->dialog,IDC_SLIDER,playlist_mode?FALSE:TRUE);
      EnableControl(kssdlg->dialog,IDC_PREV,playlist_mode?FALSE:TRUE);
      EnableControl(kssdlg->dialog,IDC_NEXT,playlist_mode?FALSE:TRUE);
      SendDlgItemMessage(kssdlg->dialog, IDC_SLIDER, TBM_SETTICFREQ, (kss->trk_max-kss->trk_min+1)/16,0) ;
      SendDlgItemMessage(kssdlg->dialog, IDC_SLIDER, TBM_SETPAGESIZE, 0, (kss->trk_max-kss->trk_min+1)/16) ;
      SendDlgItemMessage(kssdlg->dialog, IDC_SLIDER, TBM_SETLINESIZE, 0, 1);
      SetDlgItemTextA(kssdlg->dialog,IDC_SONG,I2AStr(MSXPLUG_get_song(),buf,ARRAYSIZE(buf)));
    }
    KSS_delete(kss) ;
  }
  else
  {
    SetWindowText(GetDlgItem(kssdlg->dialog, IDC_TYPE), NULL) ;
    SetWindowText(GetDlgItem(kssdlg->dialog, IDC_ID), NULL) ;
    SetWindowText(GetDlgItem(kssdlg->dialog, IDC_TITLE), NULL) ;
    SetWindowText(GetDlgItem(kssdlg->dialog, IDC_EXTRA), NULL) ;
  }

  if(kssdlg->sync)
    CheckDlgButton(kssdlg->dialog, IDC_SYNC, BST_CHECKED) ;
  else
    CheckDlgButton(kssdlg->dialog, IDC_SYNC, BST_UNCHECKED) ;

}

void KSSDLG_close(KSSDLG *kssdlg)
{
  if(kssdlg->dialog)
  {
    RemoveProp(kssdlg->dialog, TEXT("INFO")) ;
    DestroyWindow(kssdlg->dialog) ;
    kssdlg->dialog = NULL ;
  }
}

static void setDefaultGuiFont(HWND hWnd)
{
  SendMessage(hWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));    
}

void KSSDLG_open(KSSDLG *kssdlg, HINSTANCE hInst, HWND hParent)
{
  if(!kssdlg->dialog)
  {
    kssdlg->dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_KSSDLG), hParent, dlgProc ) ;
    assert(kssdlg->dialog) ;
    SetProp(kssdlg->dialog, TEXT("INFO"), kssdlg) ;
    setDefaultGuiFont(GetDlgItem(kssdlg->dialog, IDC_TITLE));
    setDefaultGuiFont(GetDlgItem(kssdlg->dialog, IDC_FILENAME));
    setDefaultGuiFont(GetDlgItem(kssdlg->dialog, IDC_EXTRA));
  }
  ShowWindow(kssdlg->dialog, SW_SHOW) ;
}

KSSDLG *KSSDLG_new(void)
{
  KSSDLG *kssdlg ;

  if(!(kssdlg = (KSSDLG *)malloc(sizeof(KSSDLG)))) return NULL ;
  kssdlg->dialog = NULL ;
  kssdlg->sync = 1 ;

  return kssdlg ;
}

void KSSDLG_delete(KSSDLG *kssdlg)
{
  if (kssdlg)
  {
  KSSDLG_close(kssdlg) ;
  free(kssdlg) ;
  }
}