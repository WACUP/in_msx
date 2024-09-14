#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "config.h"

#include <shlobj.h>

/*int CALLBACK SHBrowseProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if( uMsg == BFFM_INITIALIZED && lpData )
    {
        SendMessage( hWnd, BFFM_SETSELECTION, TRUE, lpData);
    }
    return 0;
}*/

UINT GetOpenFolderName(HWND hWnd, LPCSTR lpszDefaultFolder, char* buf, int buflen)
/*{
  LPITEMIDLIST  pIDL;
  BROWSEINFOA  bi;
  char  szSelectedFolder[MAX_PATH];

  ZeroMemory( &bi, sizeof( BROWSEINFOA ));
  bi.hwndOwner = hWnd;
  bi.lpfn   = SHBrowseProc;
  bi.ulFlags = BIF_RETURNONLYFSDIRS;
  bi.lParam = (LPARAM)lpszDefaultFolder;
  bi.lpszTitle = "Select a folder";

  pIDL = SHBrowseForFolderA( &bi );
  if( pIDL )
  {
    SHGetPathFromIDListA( pIDL, szSelectedFolder );
    strncpy(buf, szSelectedFolder, buflen);
    buf[buflen-1] = '\0';
    CoTaskMemFree( pIDL );
    return IDOK;
  }
  return IDCANCEL;
}/*/;/**/

static void update_page(HWND hDlg, CONFIG *config)
{
  CONFIG_wr_radiobtn(config, hDlg, "MBM_MODE", IDC_MBM_MODE0, IDC_MBM_MODE2,0);
  CONFIG_wr_checkbtn(config, hDlg, "MBM_ENABLE_STEREO", IDC_ENABLE_STEREO);
  CONFIG_wr_radiobtn(config, hDlg, "MBM_SYNC", IDC_SYNC0, IDC_SYNC1,0);
  CONFIG_wr_textbox(config, hDlg, "MBK_PATH", IDC_MBKPATH);
  CONFIG_wr_textbox(config, hDlg, "MBK_DUMMY", IDC_MBKDUMMY);
  CONFIG_wr_checkbtn(config, hDlg, "EXPAND_INFO", IDC_EXPAND_INFO);

}


static void update_config(HWND hDlg, CONFIG *config)
{
  CONFIG_rd_radiobtn(config, hDlg, "MBM_MODE", IDC_MBM_MODE0, IDC_MBM_MODE2,0);
  CONFIG_rd_checkbtn(config, hDlg, "MBM_ENABLE_STEREO", IDC_ENABLE_STEREO);
  CONFIG_rd_radiobtn(config, hDlg, "MBM_SYNC", IDC_SYNC0, IDC_SYNC1,0);
  CONFIG_rd_textbox(config, hDlg, "MBK_PATH", IDC_MBKPATH);
  CONFIG_rd_textbox(config, hDlg, "MBK_DUMMY", IDC_MBKDUMMY);
  CONFIG_rd_checkbtn(config, hDlg, "EXPAND_INFO", IDC_EXPAND_INFO);
}

static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CONFIG *config ;

  if(uMsg == WM_INITDIALOG)
  {
    DarkModeSetup(hDlg);
    config = ((CONFIG *)((LPPROPSHEETPAGE)lParam)->lParam) ;
    SetProp(hDlg, TEXT("CONFIG"),config) ;
    update_page(hDlg, config) ;
    return TRUE ;
  }
  else config = (CONFIG *)GetProp(hDlg, TEXT("CONFIG")) ;

  switch(uMsg)
  {
  case WM_COMMAND:
    if(HIWORD(wParam)==BN_CLICKED&&LOWORD(wParam)==IDC_MBKBTN)
    {
      char szFolder[MAX_PATH];
      GetWindowTextA(GetDlgItem(hDlg,IDC_MBKPATH),szFolder,MAX_PATH);
      if(GetOpenFolderName(hDlg, szFolder, szFolder, MAX_PATH)==IDOK)
        SetDlgItemTextA(hDlg,IDC_MBKPATH,szFolder);
      PropSheet_Changed(config->dialog, hDlg);
      return TRUE;      
    }
    else if((HWND)lParam==GetDlgItem(hDlg,IDC_SYNC0)||
       (HWND)lParam==GetDlgItem(hDlg,IDC_SYNC1)||
       (HWND)lParam==GetDlgItem(hDlg,IDC_MBM_MODE0)||
       (HWND)lParam==GetDlgItem(hDlg,IDC_MBM_MODE1)||
       (HWND)lParam==GetDlgItem(hDlg,IDC_MBM_MODE2)||
       (HWND)lParam==GetDlgItem(hDlg,IDC_ENABLE_STEREO)||
       (HWND)lParam==GetDlgItem(hDlg,IDC_MBKPATH))
    {
      PropSheet_Changed(config->dialog, hDlg);
      return TRUE;
    }
    return FALSE ;

  case WM_NOTIFY:
    switch(((LPNMHDR)lParam)->code)
    {
    case PSN_APPLY:
      update_config(hDlg,config) ;
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

HPROPSHEETPAGE CreateConfigPage8(HINSTANCE hInst, CONFIG *config)
{
  PROPSHEETPAGE psp;

  psp.dwSize = sizeof(PROPSHEETPAGE);
  psp.dwFlags = PSP_DEFAULT ;
  psp.hInstance = hInst;
  psp.pszTemplate = MAKEINTRESOURCE(IDD_CFG_PAGE8);
  psp.pszIcon = NULL;
  psp.pfnDlgProc = dlgProc;
  psp.pszTitle = NULL;
  psp.lParam = (LPARAM)config ;
  
  return CreatePropSheetPage(&psp) ;
}
