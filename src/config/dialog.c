#include <windows.h>
#include <assert.h>
#include <commctrl.h>
#include <string.h>
#include "config.h"
#include "pandlg/pandlg.h"

__declspec(dllimport) const INT_PTR CreatePropSheets(LPCPROPSHEETHEADERW lpcpsw);

static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  INT_PTR ret;
  CONFIG * config = (CONFIG *)GetProp(hDlg, TEXT("CONFIG")) ;
  switch(uMsg)
  {
  case WM_COMMAND:
    if((LOWORD(wParam)==IDOK)||(LOWORD(wParam)==IDCANCEL))
    {
      ret = config->dlgProc(hDlg, uMsg, wParam, lParam) ;
      CONFIG_dialog_end(config) ;
      return ret ;
    }
    break ;

  case WM_DESTROY:
	if (config->dialog)
	{
	  ret = config->dlgProc(hDlg, uMsg, wParam, lParam);
	  CONFIG_dialog_end(config);
	  return ret;
	}
	break;

  case WM_SYSCOMMAND:
    if(wParam==SC_CLOSE)
    {
      ret = config->dlgProc(hDlg, uMsg, wParam, lParam) ;
      CONFIG_dialog_end(config) ;
      return ret ;
    }
    break ;

  default:
    break;
  }

  return config->dlgProc(hDlg, uMsg, wParam, lParam) ;
}

int CALLBACK PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
    if (uMsg == PSCB_INITIALIZED)
    {
        DarkModeSetup(hwndDlg);
    }
    return 0;
}

void CONFIG_dialog_show(CONFIG *config, HWND hWnd, HINSTANCE hInst, int page)
{
  HPROPSHEETPAGE hpsp[9] = { 0 };
  PROPSHEETHEADER psh;

  if(!config->dialog)
  {
    hpsp[0] = CreateConfigPage5(hInst, config);
    hpsp[1] = CreateConfigPage7(hInst, config);
    hpsp[2] = CreateConfigPage1(hInst, config);
    hpsp[3] = CreateConfigPage2(hInst, config);
    hpsp[4] = CreateConfigPage3(hInst, config);
#ifndef WACUP_BUILD
    if(config->iswinamp)
    {
#endif
      hpsp[5] = CreateConfigPage4(hInst, config);
      hpsp[6] = CreateConfigPage6(hInst, config);
      hpsp[7] = CreateConfigPage8(hInst, config);
#ifndef WACUP_BUILD
      hpsp[8] = CreateConfigPageA(hInst, config);
    }
    else
    {
      hpsp[5] = CreateConfigPage8(hInst, config);
      hpsp[6] = CreateConfigPageA(hInst, config);
    }
#endif

    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_MODELESS | PSH_USECALLBACK;
    psh.hwndParent = hWnd;
    psh.hInstance = hInst;
    psh.pszIcon = NULL;
#ifndef WACUP_BUILD
    psh.pszCaption = config->iswinamp?(LPSTR)"MSXplug for Winamp"
                                     :(LPSTR)"MSXplug for KbMediaPlayer";
    psh.nPages = config->iswinamp?9:7;
#else
	psh.pszCaption = TEXT("MSXplug for WACUP");
	psh.nPages = 8;
#endif
    psh.phpage = hpsp;
    psh.pfnCallback = PropSheetProc;

    config->dialog = (HWND)CreatePropSheets(&psh) ;
    SetProp(config->dialog, TEXT("CONFIG"), config) ;
    config->dlgProc = (DLGPROC)SetWindowLongPtr(config->dialog,GWLP_WNDPROC,(LONG_PTR)dlgProc) ;
    if(!config->pandlg) config->pandlg = PANDLG_new(hInst, config);
  }
  PropSheet_SetCurSel(config->dialog,0,page) ;
  SetForegroundWindow(config->dialog) ;
}

void CONFIG_dialog_end(CONFIG *config)
{
  if(!config->dialog) return ;
  RemoveProp(config->dialog, TEXT("CONFIG")) ;
  SetWindowLongPtr(config->dialog,GWLP_WNDPROC,(LONG_PTR)config->dlgProc) ;
  DestroyWindow(config->dialog) ;
  config->dialog = NULL ;
  CONFIG_save(config) ;
}