#include <windows.h>
#include <commctrl.h>
#include <malloc.h>
#include <crtdbg.h>
#include "control.h"
#define WA_UTILS_SIMPLE
#include <../../loader/loader/utils.h>

static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  int pos ;

  HWND hSpin = GetDlgItem(hDlg,IDC_MIXER_SPIN) ;
  HWND hSlider = GetDlgItem(hDlg,IDC_MIXER_SLIDER) ;

  switch(uMsg)
  {
  case WM_SETTEXT:
    return SendDlgItemMessage(hDlg, IDC_MIXER_TITLE, uMsg, wParam, lParam) ;

  case WM_VSCROLL:
  case WM_HSCROLL:
    if(HWNDIsCtrl(lParam, hDlg, IDC_MIXER_SLIDER))
    {
      pos = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0) ;
      SendMessage(hSpin, UDM_SETPOS, 0, MAKELONG((short)pos, 0)) ;
      SendMessage(GetParent(hDlg), WM_HSCROLL, pos, (LPARAM)hDlg) ; 
      return TRUE ;
    }
    else if(HWNDIsCtrl(lParam, hDlg, IDC_MIXER_SLIDER))
    {
      pos = (int)SendMessage(hSpin, UDM_GETPOS, 0, 0) ;
      SendMessage(hSlider, TBM_SETPOS, TRUE, pos) ;
      SendMessage(GetParent(hDlg), WM_HSCROLL, pos, (LPARAM)hDlg) ;
      return TRUE ;
    }
    break ;

  case WM_ENABLE:
    EnableWindow(hSpin, !!wParam) ;
    EnableWindow(hSlider, !!wParam) ;
    EnableControl(hDlg, IDC_MIXER_VALUE, !!wParam) ;
    EnableControl(hDlg, IDC_MIXER_TITLE, !!wParam) ;
    return TRUE ;

  case WM_USER:
    return SendMessage(hSpin, UDM_GETPOS, 0, 0) ;

  case WM_USER+1:
    SendMessage(hSpin, UDM_SETPOS, 0, MAKELONG((short)wParam,0)) ;
    SendMessage(hSlider, TBM_SETPOS, TRUE, wParam) ;
    return TRUE ;

  case WM_DESTROY:
    break ;

  default:
    break ;
  }

  return FALSE ;
}

HWND CreateMixerControl(HINSTANCE hInst, HWND hWndParent, char *title, int min, int max)
{
  HWND hwnd ;
  hwnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_MIXER), hWndParent, dlgProc, 0) ;

  SetWindowTextA(hwnd, title) ;
  SendDlgItemMessage(hwnd, IDC_MIXER_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(min,max)) ;
  SendDlgItemMessage(hwnd, IDC_MIXER_SLIDER, TBM_SETPOS, TRUE, max) ;
  SendDlgItemMessage(hwnd, IDC_MIXER_SPIN, UDM_SETRANGE, TRUE, MAKELONG(max,min)) ;
  if((max-min)>15)
    SendDlgItemMessage(hwnd, IDC_MIXER_SLIDER, TBM_SETTICFREQ, (max-min)/16+1,0) ;

  return hwnd ; 
}

