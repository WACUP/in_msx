#include <windows.h>
#include "config.h"

static BOOL CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CONFIG *config ;

  if(uMsg == WM_INITDIALOG)
  {
    config = ((CONFIG *)((LPPROPSHEETPAGE)lParam)->lParam) ;
    SetProp(hDlg, TEXT("CONFIG"),config) ;
    SetDlgItemText(hDlg,IDC_COPYRIGHT,TEXT(IN_MSX_ABOUT));
    return TRUE ;
  }
  else config = (CONFIG *)GetProp(hDlg, TEXT("CONFIG")) ;

  switch(uMsg)
  {
  case WM_INITDIALOG:
    return TRUE ;

  case WM_DESTROY:
    RemoveProp(hDlg, TEXT("CONFIG"));
    return TRUE ;

  default:
    break;
  }
  return FALSE ;
}
HPROPSHEETPAGE CreateConfigPageA(HINSTANCE hInst, CONFIG *config)
{
  PROPSHEETPAGE psp;

  psp.dwSize = sizeof(PROPSHEETPAGE);
  psp.dwFlags = PSP_DEFAULT ;
  psp.hInstance = hInst;
  psp.pszTemplate = MAKEINTRESOURCE(IDD_ABOUT);
  psp.pszIcon = NULL;
  psp.pfnDlgProc = dlgProc;
  psp.pszTitle = NULL;
  psp.lParam = (long)config ;
  
  return CreatePropertySheetPage(&psp) ;
}
