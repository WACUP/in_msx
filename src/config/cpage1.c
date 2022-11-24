#include <windows.h>
#include "config.h"

static void update_page(HWND hDlg, CONFIG *config)
{
  CONFIG_wr_checkbtn(config, hDlg, "ENABLE_KSS", IDC_ENABLE_KSS);
  CONFIG_wr_checkbtn(config, hDlg, "ENABLE_MGS", IDC_ENABLE_MGS);
  CONFIG_wr_checkbtn(config, hDlg, "ENABLE_BGM", IDC_ENABLE_BGM);
  CONFIG_wr_checkbtn(config, hDlg, "ENABLE_OPX", IDC_ENABLE_OPX);
  CONFIG_wr_checkbtn(config, hDlg, "ENABLE_MPK", IDC_ENABLE_MPK);
  CONFIG_wr_checkbtn(config, hDlg, "ENABLE_MBM", IDC_ENABLE_MBM);
}

static void update_config(HWND hDlg, CONFIG *config)
{
  CONFIG_rd_checkbtn(config, hDlg, "ENABLE_KSS", IDC_ENABLE_KSS);
  CONFIG_rd_checkbtn(config, hDlg, "ENABLE_MGS", IDC_ENABLE_MGS);
  CONFIG_rd_checkbtn(config, hDlg, "ENABLE_BGM", IDC_ENABLE_BGM);
  CONFIG_rd_checkbtn(config, hDlg, "ENABLE_OPX", IDC_ENABLE_OPX);
  CONFIG_rd_checkbtn(config, hDlg, "ENABLE_MPK", IDC_ENABLE_MPK);
  CONFIG_rd_checkbtn(config, hDlg, "ENABLE_MBM", IDC_ENABLE_MBM);
}

static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CONFIG *config ;

  if(uMsg == WM_INITDIALOG)
  {
    DarkModeSetup(hDlg);
    config = ((CONFIG *)((LPPROPSHEETPAGE)lParam)->lParam) ;
    SetProp(hDlg,TEXT("CONFIG"),config) ;
    update_page(hDlg, config) ;
    return TRUE ;
  }
  else config = (CONFIG *)GetProp(hDlg, TEXT("CONFIG")) ;

  switch(uMsg)
  {
  case WM_COMMAND:
    PropSheet_Changed(config->dialog,hDlg) ;
#ifndef WACUP_BUILD
    config->need_to_restart = 1 ;
#endif
    return TRUE;
  case WM_NOTIFY:
    switch(((LPNMHDR)lParam)->code)
    {
    case PSN_APPLY:
      update_config(hDlg,config) ;
#ifndef WACUP_BUILD
      if(config->need_to_restart)
      {
        if (config->iswinamp)
          MessageBox(hDlg, TEXT("New settings will be enabled after restart Winamp."),
			  TEXT("Notice"),MB_ICONEXCLAMATION) ;
        else
          MessageBox(hDlg, TEXT("New settings will be enabled after reload plugin."),
			  TEXT("Notice"),MB_ICONEXCLAMATION) ;
        config->need_to_restart=0;
      }
#endif
      return TRUE ;
    case PSN_RESET:
      return TRUE ;
    }
    break ;
  case WM_DESTROY:
    RemoveProp(hDlg, TEXT("CONFIG")) ;
    return TRUE ;

  default:
    break;
  }
  return FALSE ;
}

HPROPSHEETPAGE CreateConfigPage1(HINSTANCE hInst, CONFIG *config)
{
  PROPSHEETPAGE psp;

  psp.dwSize = sizeof(PROPSHEETPAGE);
  psp.dwFlags = PSP_DEFAULT ;
  psp.hInstance = hInst;
  psp.pszTemplate = MAKEINTRESOURCE(IDD_CFG_PAGE1);
  psp.pszIcon = NULL;
  psp.pfnDlgProc = dlgProc;
  psp.pszTitle = NULL;
  psp.lParam = (LPARAM)config ;
  
  return CreatePropSheetPage(&psp) ;
}
