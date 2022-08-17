#include <windows.h>
#include <commctrl.h>
#include <malloc.h>
#include <crtdbg.h>
#include "control.h"

static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  int pos ;

  HWND hSlider = GetDlgItem(hDlg,IDC_SLIDER) ;

  switch(uMsg)
  {
  case WM_COMMAND:
    
    if((HIWORD(wParam)==BN_CLICKED)&&LOWORD(wParam)==IDC_TITLE)
    {
      pos = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0) ;
      SendMessage(GetParent(hDlg), WM_HSCROLL, pos, (LPARAM)hDlg) ; 
      return TRUE;
    }
    return FALSE;

  case WM_SETTEXT:
    return SendDlgItemMessage(hDlg, IDC_TITLE, uMsg, wParam, lParam) ;

  case WM_VSCROLL:
  case WM_HSCROLL:
    if((HWND)lParam == hSlider)
    {
      pos = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0) ;
      SendMessage(GetParent(hDlg), WM_HSCROLL, pos, (LPARAM)hDlg) ; 
      return TRUE ;
    }
    break ;

  case WM_ENABLE:
    EnableWindow(hSlider, !!wParam) ;
    EnableControl(hDlg, IDC_TITLE, !!wParam) ;
    return TRUE ;

  case WM_USER:
    return SendMessage(hSlider, TBM_GETPOS, 0, 0);

  case WM_USER+1:
    return SendMessage(hSlider, TBM_SETPOS, TRUE, wParam) ;
    
  case WM_DESTROY:
    break ;

  default:
    break ;
  }

  return FALSE ;
}

HWND CreatePanControl(HINSTANCE hInst, HWND hWndParent, char *title, int min, int max)
{
  HWND hwnd ;
  hwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_PANPOT), hWndParent, dlgProc) ;

  SetWindowTextA(hwnd, title) ;
  SendDlgItemMessage(hwnd, IDC_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(min,max)) ;
  SendDlgItemMessage(hwnd, IDC_SLIDER, TBM_SETPOS, TRUE, 0) ;
  if((max-min)>15)
    SendDlgItemMessage(hwnd, IDC_SLIDER, TBM_SETTICFREQ, (max-min)/16+1,0) ;

  return hwnd ; 
}

