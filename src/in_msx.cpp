#include <windows.h>
#ifndef _SHLOBJ_H_
#pragma component(browser, off, references)
#include <shlobj.h>
#pragma component(browser, on, references)
#endif
#include <commdlg.h>
#include <commctrl.h>
#include <strsafe.h>
#include "in_msx.h"
#include "msxplug.h"
#include <winamp/wa_cup.h>
#include <winamp/in2.h>
#include "rc/resource.h"
#include "api.h"

#define WA_UTILS_SIMPLE
#include <../loader/loader/utils.h>
#include <../loader/loader/paths.h>

#include <../wacup_version.h>

// TODO add to lang.h
// {36AEAA76-A84C-4bda-87F2-DEB0EC91285D}
static const GUID InMSXLangGUID =
{ 0x36aeaa76, 0xa84c, 0x4bda, { 0x87, 0xf2, 0xde, 0xb0, 0xec, 0x91, 0x28, 0x5d } };

// wasabi based services for localisation support
SETUP_API_LNG_VARS;

int Init(void);
void About(HWND hwndParent);
void GetFileExtensions(void);

extern "C" In_Module plugin =
{
    IN_VER_WACUP, // module type
	"MSXplug "PLUGIN_VERSION, // description of module, with version string
	0,	/* hMainWindow (filled in by winamp after init() ) */
	0,  /* hDllInstance ( Also filled in by winamp) */
    NULL // FileExtensions handled by this DLL
	,
	1,	/* is_seekable (is this stream seekable?) */
	1, /* uses output (does this plug-in use the output plug-ins?) */
	MSXPLUG_config, /* configuration dialog */
    About/*MSXPLUG_about*/,  /* about dialog */
    Init/*MSXPLUG_init*/,   /* called at program init */
	MSXPLUG_quit,   /* called at program quit */
	MSXPLUG_getfileinfo, /* if file == NULL, current playing is used */
	MSXPLUG_infoDlg, 
    0/*MSXPLUG_isourfile*/, /* called before extension checks, to allow detection of mms://, etc */
	
  /* PLAYBACK STUFF */
  MSXPLUG_play, /* return zero on success, -1 on file-not-found, some other value on other (stopping winamp) error */
	MSXPLUG_pause, /* pause stream */
	MSXPLUG_unpause, /* unpause stream */
	MSXPLUG_ispaused, /* ispaused? return 1 if pause, 0 if not */
	MSXPLUG_stop, /* stop (unload) stream */
	
  /* TIME STUFF */
	MSXPLUG_getlength, /* get length in ms */
	MSXPLUG_getoutputtime, /* returns current output time in ms. (usually returns outMod->GetOutputTime()) */
	MSXPLUG_setoutputtime, /* seeks to point in stream (in ms). Usually you signal yoru thread to seek, which seeks and calls outMod->Flush()..)

  /* VOLUME STUFF */
	MSXPLUG_setvolume, /* from 0 to 255.. usually just call outMod->SetVolume */
	MSXPLUG_setpan, /* from -127 to 127.. usually just call outMod->SetPan */

  /* VIS STUFF */
	0,0,0,0,0,0,0,0,0,
    0, 0,	// dsp bits
    NULL,
    NULL,	// setinfo
    NULL,	// out_mod
    NULL,	// api_service
    // TODO finish this off so things can be enabled
    INPUT_HAS_READ_META | /*INPUT_HAS_WRITE_META |*/
    INPUT_USES_UNIFIED_ALT3 /*|
    INPUT_HAS_FORMAT_CONVERSION_UNICODE |
    INPUT_HAS_FORMAT_CONVERSION_SET_TIME_MODE*/,
    GetFileExtensions,	// loading optimisation
    IN_INIT_WACUP_END_STRUCT
};

extern "C" char* safe_strdup(char* str)
{
    return plugin.memmgr->sysDupStr(str);
}

extern "C" BOOL force_mono(void)
{
    // {B6CB4A7C-A8D0-4c55-8E60-9F7A7A23DA0F}
    const GUID playbackConfigGroupGUID =
    {
        0xb6cb4a7c, 0xa8d0, 0x4c55, { 0x8e, 0x60, 0x9f, 0x7a, 0x7a, 0x23, 0xda, 0xf }
    };
    return !!plugin.config->GetBool(playbackConfigGroupGUID, L"mono", false);
}

const struct
{
    const wchar_t* ext;
    const UINT id;
} extension_list[] =
{
    { L"KSS", IDS_KSS_FAMILY_STRING },
    { L"MGS", IDS_MGS_FAMILY_STRING },
    { L"BGM", IDS_BGM_FAMILY_STRING },
    { L"BGR", IDS_BGR_FAMILY_STRING },
    { L"MBM", IDS_MBM_FAMILY_STRING },
    { L"MPK", IDS_MPK_FAMILY_STRING },
    { L"OPX", IDS_OPX_FAMILY_STRING },
};

void GetFileExtensions(void)
{
	static BOOL loaded_extensions;
	if (!loaded_extensions)
	{
		static wchar_t fileExtensionsString[256] = { 0 },
					   *end = 0, *dest = fileExtensionsString;
		for (size_t i = 0; i < ARRAYSIZE(extension_list); i++)
		{
			StringCchCopyExW(dest, ARRAYSIZE(fileExtensionsString),
							 extension_list[i].ext, &end, 0, 0);
			dest = (end + 1);
			StringCchCopyExW(dest, ARRAYSIZE(fileExtensionsString),
							 WASABI_API_LNGSTRINGW(extension_list[i].id), &end, 0, 0);
			dest = (end + 1);
		}
		//MessageBox(0, fileExtensionsString, 0, 0);
		plugin.FileExtensions = (char*)fileExtensionsString;

		loaded_extensions = TRUE;
	}
}

int Init(void)
{
    WASABI_API_LNG = plugin.language;

    // need to have this initialised before we try to do anything with localisation features
    WASABI_API_START_LANG(plugin.hDllInstance, InMSXLangGUID);

	wchar_t pluginTitle[256] = { 0 };
	StringCchPrintfW(pluginTitle, ARRAYSIZE(pluginTitle),
					 WASABI_API_LNGSTRINGW(IDS_PLUGIN_NAME), PLUGIN_VERSION);
	plugin.description = (char *)plugin.memmgr->sysDupStr(pluginTitle);

    /*preferences = (prefsDlgRecW*)GlobalAlloc(GPTR, sizeof(prefsDlgRecW));
    if (preferences)
    {
        preferences->hInst = GetModuleHandleW(GetPaths()->wacup_core_dll);
        preferences->dlgID = IDD_TABBED_PREFS_DIALOG;
        preferences->name = WASABI_API_LNGSTRINGW_DUP(IDS_NSF);
        preferences->proc = ConfigDialogProc;
        preferences->where = 10;
        preferences->_id = 99;
        preferences->next = (_prefsDlgRec*)0xACE;
        AddPrefsPage((WPARAM)preferences, TRUE);
    }*/

    return IN_INIT_SUCCESS;
}

void About(HWND hwndParent)
{
	wchar_t message[1024] = { 0 };
	StringCchPrintfW(message, ARRAYSIZE(message), WASABI_API_LNGSTRINGW(IDS_ABOUT_TEXT),
				     plugin.description, L"Darren Owen aka DrO", L"2022-" WACUP_COPYRIGHT, __DATE__);
	AboutMessageBox(hwndParent, message, (LPCWSTR)WASABI_API_LNGSTRINGW(IDS_ABOUT_TITLE));
}

/* the Dll initializer */
//BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
//{
//	return TRUE;
//}

extern "C" __declspec( dllexport ) In_Module * winampGetInModule2()
{
	return &plugin;
}

extern "C" __declspec(dllexport) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param)
{
    // prompt to remove our settings with default as no (just incase)
    /*if (MessageBox(hwndDlg, WASABI_API_LNGSTRINGW(IDS_DO_YOU_ALSO_WANT_TO_REMOVE_SETTINGS),
                   (LPWSTR)plugin.description, MB_YESNO | MB_DEFBUTTON2) == IDYES)
    {
        SaveNativeIniString(WINAMP_INI, L"in_flac", 0, 0);
    }*/

    // we should be good to go now
    return IN_PLUGIN_UNINSTALL_NOW;
}

// return 1 if you want winamp to show it's own file info dialogue, 0 if you want to show your own (via In_Module.InfoBox)
// if returning 1, remember to implement winampGetExtendedFileInfo("formatinformation")!
extern "C" __declspec(dllexport) int winampUseUnifiedFileInfoDlg(const wchar_t* fn)
{
    return 1;
}

// should return a child window of 513x271 pixels (341x164 in msvc dlg units), or return NULL for no tab.
// Fill in name (a buffer of namelen characters), this is the title of the tab (defaults to "Advanced").
// filename will be valid for the life of your window. n is the tab number. This function will first be 
// called with n == 0, then n == 1 and so on until you return NULL (so you can add as many tabs as you like).
// The window you return will recieve WM_COMMAND, IDOK/IDCANCEL messages when the user clicks OK or Cancel.
// when the user edits a field which is duplicated in another pane, do a SendMessage(GetParent(hwnd),WM_USER,(WPARAM)L"fieldname",(LPARAM)L"newvalue");
// this will be broadcast to all panes (including yours) as a WM_USER.
extern "C" __declspec(dllexport) HWND winampAddUnifiedFileInfoPane(int n, const wchar_t* filename, HWND parent, wchar_t* name, size_t namelen)
{
    /*if (n == 0)
    {
        // add first pane
        g_info = new FLACInfo;
        if (g_info)
        {
            SetProp(parent, L"INBUILT_NOWRITEINFO", (HANDLE)1);

            // TODO localise
            wcsncpy(name, L"Vorbis Comment Tag", namelen);

            g_info->filename = filename;
            g_info->metadata.Open(filename, true);
            return WASABI_API_CREATEDIALOGPARAMW(IDD_INFOCHILD_ADVANCED, parent,
                                                 ChildProc_Advanced, (LPARAM)g_info);
        }
    }*/
    return NULL;
}

extern "C" __declspec(dllexport) int winampGetExtendedFileInfoW(const wchar_t* fn, const char* data,
                                                                wchar_t* dest, size_t destlen)
{
    if (SameStrA(data, "type") ||
        SameStrA(data, "lossless") ||
        SameStrA(data, "streammetadata"))
    {
        dest[0] = L'0';
        dest[1] = L'\0';
        return 1;
    }
    else if (SameStrA(data, "streamgenre") ||
             SameStrA(data, "streamtype") ||
             SameStrA(data, "streamurl") ||
             SameStrA(data, "streamname") ||
             SameStrA(data, "reset"))
    {
        return 0;
    }

    if (!fn || !fn[0])
    {
        return 0;
    }

    if (SameStrA(data, "family"))
    {
        LPCWSTR e = FindPathExtension(fn);
        if (e != NULL)
        {
            int pID = -1;
            for (size_t i = 0; i < ARRAYSIZE(extension_list); i++)
            {
                if (SameStr(e, extension_list[i].ext))
                {
                    pID = extension_list[i].id;
                }
            }

            if (pID != -1)
            {
                WASABI_API_LNGSTRINGW_BUF(pID, dest, destlen);
                return 1;
            }
        }
        return 0;
    }

    /*create_player(false);

    AutoCharFn _fn(fn);
    return ((pPlugin != NULL) ? pPlugin->GetMetadata(_fn, data, dest, destlen) : 0);*/
    return 0;
}

extern "C" __declspec(dllimport) BOOL GetFileName(LPOPENFILENAMEW ofn);
extern "C" __declspec(dllimport) BOOL SaveFileName(LPOPENFILENAMEW ofn);

extern "C" int get_illfilename(HWND hWnd, char* buf, int max, int mode)
{
    OPENFILENAMEW ofn = { 0 };
    wchar_t bufw[_MAX_PATH] = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"ILL Files(*.ill)\0*.ill\0All Files(*.*)\0*.*\0\0";
    ofn.lpstrFile = bufw;
    ofn.nMaxFile = _MAX_PATH;

    if (mode)
    {
        ofn.Flags = OFN_FILEMUSTEXIST;
        const int ret = GetFileName(&ofn);
        if (ret)
        {
            ConvertUnicodeFn(buf, max, bufw, CP_ACP);
        }
        return ret;
    }
    else
    {
        ofn.Flags = OFN_OVERWRITEPROMPT;
        const int ret = SaveFileName(&ofn);
        if (ret)
        {
            ConvertUnicodeFn(buf, max, bufw, CP_ACP);
        }
        return ret;
    }
}

extern "C" int get_driver_filename(HWND hWnd, char* buf, int max)
{
    OPENFILENAME ofn = { 0 };
    wchar_t bufw[_MAX_PATH] = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"All Files(*.*)\0*.*\0\0";
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFile = bufw;
    ofn.nMaxFile = _MAX_PATH;

    const int ret = GetFileName(&ofn);
    if (ret)
    {
        ConvertUnicodeFn(buf, max, bufw, CP_ACP);
    }
    return ret;
}

extern "C" int get_filename(HWND hWnd, char* buf, int max)
{
    OPENFILENAME ofn = { 0 };
    wchar_t bufw[_MAX_PATH] = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"Playlist Files(*.pls)\0*.pls\0All Files(*.*)\0*.*\0\0";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrFile = bufw;
    ofn.nMaxFile = _MAX_PATH;

    const int ret = SaveFileName(&ofn);
    if (ret)
    {
        ConvertUnicodeFn(buf, max, bufw, CP_ACP);
    }
    return ret;
}

#include <../common/browse.h>
extern "C" UINT GetOpenFolderName(HWND hWnd, LPCSTR lpszDefaultFolder, char* buf, int buflen)
{
    wchar_t  szSelectedFolder[MAX_PATH] = { 0 };
    ConvertANSIFn(szSelectedFolder, ARRAYSIZE(szSelectedFolder), buf, CP_ACP);
    //Browse_Folders_SetStyle(BIF_USENEWUI | BIF_NONEWFOLDERBUTTON);
    if (Browse_Folders(hWnd, szSelectedFolder, ARRAYSIZE(szSelectedFolder), L"Select a folder"))
    {
        ConvertUnicodeFn(buf, buflen, szSelectedFolder, CP_ACP);
        return IDOK;
    }
    return IDCANCEL;
}