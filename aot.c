// -----------------------------------------------------
// resource.h : Definitions
// -----------------------------------------------------
#define _WIN32_WINNT            0x601
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#define AOT_HOOK_EXE_MANIFEST   1
#define AOT_ICON				        2
#define AOT_HOOK_DLL_DATA       101
#define AOT_HOOK_EXE_DATA       102
#define AOT_HOST_DLL_DATA       103
#define AOT_SENTINEL_EXE_DATA   104
#define AOT_X86HOST_EXE_DATA    105
#define AOT_X64HOST_EXE_DATA    106
#define AOT_ICON_ICO            "aot.ico"
#define AOT_HOOK_DLL            "aot-hook.dll"
#define AOT_HOOK_EXE            "aot-hook.exe"
#define AOT_HOST_DLL            "aot-host.dll"
#define AOT_X86HOST_EXE         "aotx86-host.exe"
#define AOT_X64HOST_EXE         "aotx64-host.exe"

// -----------------------------------------------------
// Version Resources : Definitions
// -----------------------------------------------------
#if (defined _VERRES)
AOT_EXE_MANIFEST   RT_MANIFEST     "aot.exe.manifest"
AOT_ICON           ICON            AOT_ICON_ICO

VS_VERSION_INFO    VERSIONINFO
    FILEVERSION    VERCSV
    PRODUCTVERSION VERCSV
    FILEFLAGSMASK  0x17L
    FILEFLAGS      0x0L
    FILEOS         VOS_NT_WINDOWS32
    FILETYPE       VFT_APP
    FILESUBTYPE    VFT2_UNKNOWN
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "FileDescription",  "AlwaysOnTop"
            VALUE "FileVersion",      VERDOT
            VALUE "InternalName",     "AOT"
            VALUE "LegalCopyright",   "Copyright (C) 2025"
            VALUE "OriginalFilename", "AlwaysOnTop.exe"
            VALUE "ProductName",      "AlwaysOnTop"
            VALUE "ProductVersion",   VERDOT
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END

// -----------------------------------------------------
// Resources : Definitions
// -----------------------------------------------------
#elif   (defined _HOSTRES)
AOT_HOOK_DLL_DATA     RCDATA          AOT_HOOK_DLL
AOT_HOOK_EXE_DATA     RCDATA          AOT_HOOK_EXE
AOT_HOST_DLL_DATA     RCDATA          AOT_HOST_DLL

#elif (defined _RELRES)
AOT_X86HOST_EXE_DATA  RCDATA          AOT_X86HOST_EXE
AOT_X64HOST_EXE_DATA  RCDATA          AOT_X64HOST_EXE

#else
// -----------------------------------------------------
// Shared Data Segments : Declarations & Definitions
// -----------------------------------------------------
#include <tchar.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <psapi.h>
#include <shellapi.h>
#pragma warning(disable : 4820)
#include <Shobjidl.h>
#pragma warning(default : 4820)

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

#if (defined __i386__) || (defined _M_IX86)
  #define ARCH(identifier) identifier ## 32
  #define SEGMENT ".shared32"
#elif   (defined __x86_64__) || (defined _M_X64)
  #define ARCH(identifier) identifier ## 64
  #define SEGMENT ".shared64"
#endif

#if defined _WINDLL
  #define ALLOC_SEGMENT(segment, type, identifier, assignment)   \
          __declspec(allocate(SEGMENT)) __declspec(selectany) type ARCH(identifier) = assignment;

  #pragma section(SEGMENT, read, write, shared)
    #if defined   _AOTHOSTDLL
      ALLOC_SEGMENT(SEGMENT, DWORD,            dwHostProcessId,    0);

    #elif defined _AOTHOOKDLL
      ALLOC_SEGMENT(SEGMENT, DWORD,            dwHookProcessId,    0);
      ALLOC_SEGMENT(SEGMENT, HHOOK,            hCbtHook,           NULL);
      ALLOC_SEGMENT(SEGMENT, HINSTANCE,        hCbtHookModule,     NULL);
      ALLOC_SEGMENT(SEGMENT, HWND,             hWndHookMarshaller, NULL);
      ALLOC_SEGMENT(SEGMENT, BOOL,             fBounceLoads,       FALSE);
      ALLOC_SEGMENT(SEGMENT, CRITICAL_SECTION, csHookLoaderLock,  {0});
    #endif

  #define AOTAPI __declspec(dllexport)
#else
  #define AOTAPI __declspec(dllimport)
#endif

#define AOTAPIV __cdecl

// -----------------------------------------------------
// aot-hook.dll : Interface
// -----------------------------------------------------
EXTERN_C
BOOL AOTAPI AOTAPIV
SetCbtHook(
    HWND hWnd
    );

EXTERN_C
BOOL AOTAPI AOTAPIV
UnsetCbtHook(
    VOID
    );

EXTERN_C
DWORD AOTAPI AOTAPIV
GetHookProcessId(
    VOID
    );

// -----------------------------------------------------
// aot-hook.exe : Implementation
// -----------------------------------------------------
#define AOT_INSTANCE_NAME          (TEXT("AlwaysOnTop"))
#define AOT_TRAY_NAME              (TEXT("AOT_TrayWnd"))
#define AOT_HOOK_NAME              (TEXT("AOT_HookWnd"))
#define AOT_TRAY_CLASS_NAME        (TEXT("AOT_TrayWndClass"))
#define AOT_HOOK_CLASS_NAME        (TEXT("AOT_HookWndClass"))
#define AOT_HOOK_INSTANCE_MUTEX    (TEXT("AOT_HookWndMutex"SEGMENT))

#define AOT_MENUTEXT_ALWAYS_ON_TOP (TEXT("Always On Top"))
#define AOT_MENU_ALWAYS_ON_TOP     (0x69 + 0x1f)
#define SYSCOMMAND(value)          (value & 0xFFF0)
#define SC_AOT                     (SYSCOMMAND(AOT_MENU_ALWAYS_ON_TOP))

#define WM_AOTQUIT                 (WM_USER + 0x69)
#define WM_AOTHOOKINIT             (WM_USER + 0x69 + 1)
#define WM_AOTTRAYICON             (WM_USER + 0x69 + 2)

#define HANDLE_WM_AOTHOOKINIT(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (AOTHOOKTHREAD)(wParam), (DWORD)(lParam)), 0L)

typedef struct _AOTUSERDATA {
    DWORD dwCbtHookThreadId;
#if   (defined __i386__) || (defined _M_IX86)
    __declspec(align(4)) unsigned __int8 reserved[4];
#endif
} AOTUSERDATA, * LPAOTUSERDATA;

typedef enum _AOTHOOKTHREAD {
    AOTCBTHOOKTHREAD,
} AOTHOOKTHREAD;

static
BOOL CFORCEINLINE CALLBACK
OnNcCreate(
    HWND           hWnd,
    LPCREATESTRUCT lpCreateStruct)
{
    LONG_PTR offset = 0;
    HANDLE   hInstanceMutex = CreateMutex(NULL, TRUE, AOT_HOOK_INSTANCE_MUTEX);
    
    if (hInstanceMutex && (GetLastError() == ERROR_ALREADY_EXISTS))
    {
      WaitForSingleObjectEx(hInstanceMutex, 100, FALSE);
      ReleaseMutex(hInstanceMutex);
      CloseHandle(hInstanceMutex);
      return FALSE;
    }

    SetLastError(0);
    offset = SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lpCreateStruct->lpCreateParams);
    
    if ((0 == offset) && (NO_ERROR != GetLastError()))
      return FALSE;

    return FORWARD_WM_NCCREATE(hWnd, lpCreateStruct, DefWindowProc);
}

static 
VOID CFORCEINLINE CALLBACK
OnAotHookInit(
    HWND          hWnd,
    AOTHOOKTHREAD dwHookThread,
    DWORD         dwAotHookThreadId)
{
    LPAOTUSERDATA lpAotUserData = (LPAOTUSERDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if(lpAotUserData)
    {
      switch (dwHookThread) {
      case AOTCBTHOOKTHREAD:
        lpAotUserData->dwCbtHookThreadId = dwAotHookThreadId;
        return;
      DEFAULT_UNREACHABLE;
      }
    }
    
    UnsetCbtHook();
    CloseWindow(hWnd);
}

static
VOID CFORCEINLINE CALLBACK
OnClose(
    HWND hWnd)
{
    UnsetCbtHook();
    DestroyWindow(hWnd);
}

static
VOID CFORCEINLINE CALLBACK
OnDestroy(
    HWND hWnd)
{
    LPAOTUSERDATA lpAotUserData = (LPAOTUSERDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if(lpAotUserData)
    {
      if (lpAotUserData->dwCbtHookThreadId)
      {
        HANDLE hCbtHookThread = OpenThread(SYNCHRONIZE, FALSE, lpAotUserData->dwCbtHookThreadId);
        PostThreadMessage(lpAotUserData->dwCbtHookThreadId, WM_AOTQUIT, 0, 0);

        if (hCbtHookThread)
        {
          WaitForSingleObject(hCbtHookThread, INFINITE);
          CloseHandle(hCbtHookThread);
        }
      }
    }
    
    UnsetCbtHook();
    PostQuitMessage(EXIT_SUCCESS);
}

static
LRESULT CFORCEINLINE CALLBACK
HookWndProc(
    HWND   hWnd,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message) {
    HANDLE_MSG(hWnd, WM_NCCREATE,    OnNcCreate);
    HANDLE_MSG(hWnd, WM_AOTHOOKINIT, OnAotHookInit);
    HANDLE_MSG(hWnd, WM_CLOSE,       OnClose);
    HANDLE_MSG(hWnd, WM_DESTROY,     OnDestroy); 
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

static
VOID CFORCEINLINE APIPRIVATE
CbtHookThread(
    LPVOID lpvhWnd)
{
    if (SetCbtHook(*(HWND*)lpvhWnd))
    {
      MSG msg;

      PostMessage(
        *(HWND*)lpvhWnd,
        WM_AOTHOOKINIT,
        (WPARAM)(DWORD)AOTCBTHOOKTHREAD,
        (LPARAM)(DWORD)GetCurrentThreadId()
      );

      RtlSecureZeroMemory(&msg, sizeof(MSG));
      while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        switch (msg.message) {
        case WM_QUIT:
        case WM_AOTQUIT:
          UnsetCbtHook();
          break;
        default:
          continue;
        }
        break;
      }

      ExitThread(EXIT_SUCCESS);
    }

    ExitThread(EXIT_FAILURE);
}

// -----------------------------------------------------
// aot-hook.dll : Implementation
// -----------------------------------------------------
#if (defined _AOTHOOKDLL)

static 
BOOL CFORCEINLINE CALLBACK
UpdateSystemMenu(
    HWND hWnd)
{
    HMENU hSysMenu = NULL;
    hSysMenu = GetSystemMenu(hWnd, FALSE);
    if(hSysMenu)
    {
      MENUITEMINFO mii;
      LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
      RtlSecureZeroMemory(&mii, sizeof(MENUITEMINFO));

      mii.cbSize = sizeof(MENUITEMINFO);
      mii.fMask  = MIIM_ID;

      if (!GetMenuItemInfo(hSysMenu, AOT_MENU_ALWAYS_ON_TOP, FALSE, &mii))
      {
        AppendMenu(
          hSysMenu,
          MF_BYPOSITION | MF_STRING | ((exStyle & WS_EX_TOPMOST) ? MF_CHECKED : MF_UNCHECKED),
          AOT_MENU_ALWAYS_ON_TOP,
          AOT_MENUTEXT_ALWAYS_ON_TOP
        );
      }
      else
      {
        ModifyMenu(
          hSysMenu,
          AOT_MENU_ALWAYS_ON_TOP,
          MF_BYCOMMAND | MF_STRING | ((exStyle & WS_EX_TOPMOST) ? MF_CHECKED : MF_UNCHECKED),
          AOT_MENU_ALWAYS_ON_TOP,
          AOT_MENUTEXT_ALWAYS_ON_TOP
        );
      }
    }
    return FALSE;
}

static
LRESULT CFORCEINLINE CALLBACK
CBTProc(
    int    nCode,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (nCode) {
    case HCBT_ACTIVATE:
    {
      TCHAR szClassName[256];
      HWND hWnd = (HWND)wParam;
      RtlSecureZeroMemory(szClassName, sizeof(szClassName));
      if(GetClassName(hWnd, szClassName, sizeof(szClassName)))
        if(_tccmp(szClassName, _T("#32768")))
          if(IsWindowVisible(hWnd))
            UpdateSystemMenu(hWnd);
      break;
    }
    case HCBT_SYSCOMMAND:
    {
      switch(wParam) {
      case SC_AOT:
      {
        HWND hWnd    = GetForegroundWindow();
        LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
        UINT uFlags  = SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED;
        HWND hWndInsertAfter = (exStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST;
        SetWindowPos(hWnd, hWndInsertAfter, 0, 0, 0, 0, uFlags);
        UpdateSystemMenu(hWnd);
        break;
      }
      case SC_MOUSEMENU:
      case SC_KEYMENU:
        UpdateSystemMenu(GetForegroundWindow());
      default:
        break;
      }
    }
    default:
      break;
    }

    return CallNextHookEx(ARCH(hCbtHook), nCode, wParam, lParam);
}

EXTERN_C
AOTAPI BOOL AOTAPIV
SetCbtHook(
    HWND hWnd)
{
    InitializeCriticalSection(&ARCH(csHookLoaderLock));
    HHOOK hook = SetWindowsHookEx(WH_CBT, CBTProc, ARCH(hCbtHookModule), 0);

    if (hook)
    {
      ARCH(hCbtHook)           = hook;
      ARCH(hWndHookMarshaller) = hWnd;
    }

    return !!ARCH(hCbtHook);
}

EXTERN_C
BOOL AOTAPI CFORCEINLINE AOTAPIV
UnsetCbtHook(
    VOID)
{
    EnterCriticalSection(&ARCH(csHookLoaderLock));
    if (ARCH(hCbtHook))
    {
      UnhookWindowsHookEx(ARCH(hCbtHook));
      ARCH(hCbtHook)           = NULL;
      ARCH(fBounceLoads)       = TRUE;
    }
    LeaveCriticalSection(&ARCH(csHookLoaderLock));
    SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);

    return !!ARCH(hCbtHook);
}

EXTERN_C
DWORD AOTAPI AOTAPIV
GetHookProcessId(
    VOID)
{
    return ARCH(dwHookProcessId);
}

#endif
// -----------------------------------------------------
// aot-host.dll : Interface
// -----------------------------------------------------
EXTERN_C
DWORD AOTAPI AOTAPIV
GetHostProcessId(
    VOID
    );

// -----------------------------------------------------
// aot-host.dll : Implementation
// -----------------------------------------------------
#if (defined _AOTHOSTDLL)

EXTERN_C
AOTAPI DWORD AOTAPIV
GetHostProcessId(
    VOID)
{
    return ARCH(dwHostProcessId);
}

#endif

// -----------------------------------------------------
// aot-host.dll, aot-hook.dll : Implementation
// -----------------------------------------------------
#if (defined _WINDLL)

static
BOOL CFORCEINLINE APIPRIVATE
IsServiceHost(
    HANDLE hProcess)
{
    TCHAR szPath[MAX_PATH];
    RtlSecureZeroMemory(szPath, sizeof(szPath));
    GetFinalPathNameByHandle(hProcess, szPath, sizeof(szPath), 0);
    PathStripPath(szPath);
    return !_tccmp(szPath, _T("svchost.exe"));
}

BOOL
APIENTRY
DllMain(
    HMODULE hModule,
    DWORD   ul_reason_for_call,
    LPVOID  lpReserved)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(lpReserved);
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: 
    {
#if defined   _AOTHOSTDLL
      if (!ARCH(dwHostProcessId))
        ARCH(dwHostProcessId) = GetCurrentProcessId();

#elif defined _AOTHOOKDLL
      if (!ARCH(dwHookProcessId))
        ARCH(dwHookProcessId) = GetCurrentProcessId();
      else
      {
        EnterCriticalSection(&ARCH(csHookLoaderLock));
        if (ARCH(fBounceLoads))
        {
          LeaveCriticalSection(&ARCH(csHookLoaderLock));
          return FALSE;
        }
        LeaveCriticalSection(&ARCH(csHookLoaderLock));
      }
      ARCH(hCbtHookModule) = hModule;
#endif
      // Tell any services to fuck off 
      if (IsServiceHost(GetCurrentProcess()))
        return FALSE;

      DisableThreadLibraryCalls(hModule);
    }
    default:
      SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
      return TRUE;
    }
}
#else
// -----------------------------------------------------
// Shared functions : Inline Implementation
// -----------------------------------------------------
static 
HANDLE CFORCEINLINE APIPRIVATE
UnloadFile(
    LPTSTR  lpszName,
    LPCVOID lpBuffer,
    DWORD   nNumberOfBytesToWrite)
{
    HANDLE hFile = CreateFile(lpszName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if(hFile)
    {
      DWORD nNumberOfBytesWritten = 0;
      if (!WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &nNumberOfBytesWritten, 0))
      {
        CloseHandle(hFile);
        hFile = NULL;
      }
    }

    return hFile;
}

static
HANDLE CFORCEINLINE APIPRIVATE
UnloadResource(
    HMODULE hModule, 
    UINT    nResourceId, 
    LPTSTR  lpszName)
{
    HANDLE hOut = NULL;
    HRSRC  hResourceInfo = FindResource(hModule, MAKEINTRESOURCE(nResourceId), RT_RCDATA);

    if (hResourceInfo)
    {
      HGLOBAL hResourceData = LoadResource(hModule, hResourceInfo);
      if(hResourceData)
      {
        LPCVOID lpResourceData = (LPCVOID)LockResource(hResourceData);
        if(lpResourceData)
        {
          DWORD dwResourceSize = SizeofResource(hModule, hResourceInfo);
          if(dwResourceSize)
          {
            hOut = UnloadFile(lpszName, lpResourceData, dwResourceSize);
          }
          UnlockResource(hResourceData);
        }
        FreeResource(hResourceData);
      }
      CloseHandle(hResourceInfo);
    }

    return hOut;
}

static
VOID CFORCEINLINE APIPRIVATE
Killcord(
    LPVOID dwProcessId)
{
    HANDLE hProcessId = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)(DWORD_PTR)dwProcessId);

    if (hProcessId)
    {
      WaitForSingleObject(hProcessId, INFINITE);
      CloseHandle(hProcessId);
      ExitProcess(EXIT_SUCCESS);
    }

    ExitProcess(EXIT_FAILURE);
}

// -----------------------------------------------------
// Executables : Entry points & Implementation
// -----------------------------------------------------

typedef enum _AOTHOST {
    AOT_X86HOST,
    AOT_X64HOST,
} AOTHOST;

typedef struct _AOTMANAGEDHOST {
    TCHAR               szPath            [MAX_PATH];
    TCHAR               szWorkingDirectory[MAX_PATH];
    HANDLE              hKillcord;
    LPSTARTUPINFO       lpsi;
    PROCESS_INFORMATION pi;
} AOTMANAGEDHOST;

typedef struct _AOTINSTANCE {
    STARTUPINFO    si;
    HANDLE         hJob;
    HANDLE         hObjects[4];
    AOTMANAGEDHOST x86ManagedHost;
    AOTMANAGEDHOST x64ManagedHost;
    TCHAR          szPath[MAX_PATH];
} AOTINSTANCE;

static
BOOL CFORCEINLINE APIPRIVATE
SetJobInformation(
    HANDLE hJob)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobLimitInfo;
    RtlSecureZeroMemory(&JobLimitInfo, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

    JobLimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    return SetInformationJobObject(
      hJob,
      JobObjectExtendedLimitInformation,
      &JobLimitInfo,
      sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)
    );
}

static
LPTSTR CFORCEINLINE APIPRIVATE
GetModulePath(
    HANDLE  hProcess,
    LPTSTR  szPath)
{
    DWORD _ = MAX_PATH;
    if (!QueryFullProcessImageName(hProcess, 0, szPath, &_))
      return NULL;
    if (!PathRemoveFileSpec(szPath))
      return NULL;
    return szPath;
}

static
BOOL CFORCEINLINE APIPRIVATE
BuildPaths(
    HMODULE hModule,
    AOTHOST eHost,
    LPTSTR  lpszPath,
    LPTSTR  lpszWorkingDirectory)
{
    switch (eHost) {
    case AOT_X86HOST:
    {
      PathAppend(lpszWorkingDirectory, _T(".\\x86"));
      CreateDirectory(lpszWorkingDirectory, NULL);
      PathAppend(lpszPath, _T(".\\x86\\")_T(AOT_X86HOST_EXE));
      return CloseHandle(UnloadResource(hModule, AOT_X86HOST_EXE_DATA, lpszPath));
    }
    case AOT_X64HOST:
    {
      PathAppend(lpszWorkingDirectory, _T(".\\x64"));
      CreateDirectory(lpszWorkingDirectory, NULL);
      PathAppend(lpszPath, _T(".\\x64\\")_T(AOT_X64HOST_EXE));
      return CloseHandle(UnloadResource(hModule, AOT_X64HOST_EXE_DATA, lpszPath));
    }
    DEFAULT_UNREACHABLE;
    }
}

static
BOOL CFORCEINLINE APIPRIVATE
CreateSuspendedHost(
    AOTHOST               eHost,
    AOTMANAGEDHOST*       lpManagedHost,
    HMODULE               hModule)
{ 
    LPTSTR lpszPath              = lpManagedHost->szPath;
    LPTSTR lpszWorkingDirectory  = lpManagedHost->szWorkingDirectory;
    LPSTARTUPINFO          lpsi  = lpManagedHost->lpsi;
    LPPROCESS_INFORMATION  lppi  = &lpManagedHost->pi;

    if (!BuildPaths(hModule, eHost, lpszPath, lpszWorkingDirectory))
      return FALSE;

    if(!CreateProcess(0, lpszPath, 0, 0, 0, CREATE_SUSPENDED, 0, lpszWorkingDirectory, lpsi, lppi))
      return FALSE;
    
    else
    {
      lpManagedHost->hKillcord =
        CreateThread(
          0, 0, (LPTHREAD_START_ROUTINE)(LPVOID)Killcord, (LPVOID)(DWORD_PTR)lppi->dwProcessId, CREATE_SUSPENDED, 0);
       return !!lpManagedHost->hKillcord;
    }
}

static
LRESULT CFORCEINLINE CALLBACK
TrayWndProc(
    HWND hWnd, 
    UINT uMsg, 
    WPARAM wParam,
    LPARAM lParam) 
{
    switch(uMsg){
    case WM_CLOSE:
      DestroyWindow(hWnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(EXIT_SUCCESS);
      return 0;
    case WM_AOTTRAYICON:
    {
      if (lParam == WM_RBUTTONUP) 
      {
        PostQuitMessage(EXIT_SUCCESS);
        return 0;
      }
      else if (lParam == WM_LBUTTONUP) 
        SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
    }
    default:
      break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static
BOOL CFORCEINLINE APIPRIVATE
CreateTrayIcon(
    NOTIFYICONDATA* nid)
{
    WNDCLASS  wc;
    ATOM      atom;
    HWND      hWnd;

    RtlSecureZeroMemory(&wc, sizeof(WNDCLASS));
    wc.hInstance      = (HINSTANCE)&__ImageBase;
    wc.lpfnWndProc    = TrayWndProc;
    wc.lpszClassName  = AOT_TRAY_CLASS_NAME;
    atom              = RegisterClass(&wc);

    RtlSecureZeroMemory(&hWnd, sizeof(HWND));
    hWnd = CreateWindow(MAKEINTATOM(atom), AOT_TRAY_NAME, 0, 0, 0, 0, 0, 0, 0, (HINSTANCE)&__ImageBase, 0);
    if (!hWnd)
      return FALSE;

    nid->cbSize = sizeof(NOTIFYICONDATA);
    nid->hIcon  = LoadIcon((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(AOT_ICON));
    nid->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid->uCallbackMessage = WM_AOTTRAYICON;
    lstrcpy(nid->szTip, AOT_INSTANCE_NAME);
    nid->hWnd = hWnd;

    Shell_NotifyIcon(NIM_ADD, nid);

    return TRUE;
}

static
VOID CFORCEINLINE APIPRIVATE
TrayThread(
    LPVOID unused)
{
    UNREFERENCED_PARAMETER(unused);

    NOTIFYICONDATA nid;
    RtlSecureZeroMemory(&nid, sizeof(NOTIFYICONDATA));

    if (CreateTrayIcon(&nid))
    {
      MSG msg;
      RtlSecureZeroMemory(&msg, sizeof(MSG));
      while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    ExitProcess(EXIT_SUCCESS);
}

static
VOID CFORCEINLINE APIPRIVATE
HooksThread(
    LPVOID unused)
{
    UNREFERENCED_PARAMETER(unused);
    
    AOTINSTANCE AlwaysOnTop;
    RtlSecureZeroMemory(&AlwaysOnTop, sizeof(AOTINSTANCE));

    AlwaysOnTop.hJob = CreateJobObject(NULL, NULL);
    if (!SetJobInformation(AlwaysOnTop.hJob))
    {
      CloseHandle(AlwaysOnTop.hJob);
      ExitProcess(EXIT_FAILURE);
    }
    
    AlwaysOnTop.si.dwFlags          = STARTF_USESHOWWINDOW | STARTF_FORCEOFFFEEDBACK;
    AlwaysOnTop.si.wShowWindow      = SW_HIDE;
    AlwaysOnTop.x86ManagedHost.lpsi = &AlwaysOnTop.si;
    AlwaysOnTop.x64ManagedHost.lpsi = &AlwaysOnTop.si;

    if (!GetModulePath(GetCurrentProcess(), AlwaysOnTop.szPath))
      ExitProcess(EXIT_FAILURE);
    
    if (!SetCurrentDirectory(AlwaysOnTop.szPath))
      ExitProcess(EXIT_FAILURE);
    
    PathAppend(AlwaysOnTop.x86ManagedHost.szWorkingDirectory, AlwaysOnTop.szPath);
    PathAppend(AlwaysOnTop.x64ManagedHost.szWorkingDirectory, AlwaysOnTop.szPath);

    if (!CreateSuspendedHost(AOT_X86HOST, &AlwaysOnTop.x86ManagedHost, (HMODULE)&__ImageBase) ||
        !AssignProcessToJobObject(AlwaysOnTop.hJob, AlwaysOnTop.x86ManagedHost.pi.hProcess))
      ExitProcess(EXIT_FAILURE);

    if (!CreateSuspendedHost(AOT_X64HOST, &AlwaysOnTop.x64ManagedHost, (HMODULE)&__ImageBase) ||
        !AssignProcessToJobObject(AlwaysOnTop.hJob, AlwaysOnTop.x64ManagedHost.pi.hProcess))
      ExitProcess(EXIT_FAILURE);

    ResumeThread(AlwaysOnTop.x86ManagedHost.pi.hThread);
    CloseHandle(AlwaysOnTop.x86ManagedHost.pi.hThread);

    ResumeThread(AlwaysOnTop.x64ManagedHost.pi.hThread);
    CloseHandle(AlwaysOnTop.x64ManagedHost.pi.hThread);

    ResumeThread(AlwaysOnTop.x86ManagedHost.hKillcord);
    ResumeThread(AlwaysOnTop.x64ManagedHost.hKillcord);

    AlwaysOnTop.hObjects[0] = AlwaysOnTop.x86ManagedHost.pi.hProcess;
    AlwaysOnTop.hObjects[1] = AlwaysOnTop.x64ManagedHost.pi.hProcess;
    AlwaysOnTop.hObjects[2] = AlwaysOnTop.x86ManagedHost.hKillcord;
    AlwaysOnTop.hObjects[3] = AlwaysOnTop.x64ManagedHost.hKillcord;

    WaitForMultipleObjects(4, AlwaysOnTop.hObjects, FALSE, INFINITE);

    CloseHandle(AlwaysOnTop.x86ManagedHost.pi.hProcess);
    CloseHandle(AlwaysOnTop.x64ManagedHost.pi.hProcess);
    CloseHandle(AlwaysOnTop.x86ManagedHost.hKillcord);
    CloseHandle(AlwaysOnTop.x64ManagedHost.hKillcord);

    CloseHandle(AlwaysOnTop.hJob);

    ExitProcess(EXIT_SUCCESS);
}

static
HMODULE CFORCEINLINE APIPRIVATE
BootstrapHost(
    HMODULE hHostImage)
{
    if (!CloseHandle(UnloadResource(hHostImage, AOT_HOST_DLL_DATA, _T(AOT_HOST_DLL))))
      return NULL;

    return LoadLibrary(_T(AOT_HOST_DLL));
}

static
VOID CFORCEINLINE APIPRIVATE
Sentinel(
    LPVOID dwProcessId)
{
    HANDLE hProcessId = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)(DWORD_PTR)dwProcessId);

    if (hProcessId)
    {
      WaitForSingleObject(hProcessId, INFINITE);
      UnsetCbtHook();
      SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
      CloseHandle(hProcessId);

      ExitProcess(EXIT_SUCCESS);
    }

    ExitProcess(EXIT_FAILURE);
}

int
APIENTRY
_tWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR    lpCmdLine,
    int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    InitCommonControls();
    SetCurrentProcessExplicitAppUserModelID(AOT_INSTANCE_NAME);

#if   (defined _RELEASE)
    MSG msg;
    PostMessage(NULL, 0, 0, 0);
    GetMessage(&msg, 0, 0, 0);
    CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)(LPVOID)TrayThread,  NULL, 0, 0));
    CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)(LPVOID)HooksThread, NULL, 0, 0));
    SuspendThread(GetCurrentThread());

#elif (defined _HOST)
    HMODULE hHostInstance = BootstrapHost((HMODULE)&__ImageBase);
    
    if (hHostInstance)
    {
      STARTUPINFOA        si;
      PROCESS_INFORMATION pi;
      CloseHandle(UnloadResource((HMODULE)&__ImageBase, AOT_HOOK_DLL_DATA, _T(AOT_HOOK_DLL)));
      CloseHandle(UnloadResource((HMODULE)&__ImageBase, AOT_HOOK_EXE_DATA, _T(AOT_HOOK_EXE)));

      RtlSecureZeroMemory(&si, sizeof(STARTUPINFOA));
      si.cb      = sizeof(STARTUPINFOA);
      si.dwFlags = STARTF_FORCEOFFFEEDBACK;

      if (!CreateProcessA(0, AOT_HOOK_EXE, 0, 0, 0, CREATE_SUSPENDED, 0, 0, &si, &pi))
      {
        FreeLibrary(hHostInstance);
        ExitProcess(EXIT_FAILURE);
      }

      ResumeThread(pi.hThread);
      CloseHandle(pi.hThread);

      WaitForSingleObject(pi.hProcess, INFINITE);
      CloseHandle(pi.hProcess);

      FreeLibrary(hHostInstance);
      ExitProcess(EXIT_SUCCESS);
    }
    ExitProcess(EXIT_FAILURE);

#else
    AOTUSERDATA aot;
    WNDCLASS    wc;
    ATOM        atom;
    HWND        hWnd;

    if (!CloseHandle(
           CreateThread(
             0, 0, (LPTHREAD_START_ROUTINE)(LPVOID)Sentinel, (LPVOID)(DWORD_PTR)GetHostProcessId(), 0, 0)))
      ExitProcess(EXIT_FAILURE);

    RtlSecureZeroMemory(&wc, sizeof(WNDCLASS));
    wc.hInstance     = hInstance;
    wc.lpfnWndProc   = HookWndProc;
    wc.lpszClassName = AOT_HOOK_CLASS_NAME;
    atom             = RegisterClass(&wc);
    
    if (!atom)
      ExitProcess(EXIT_FAILURE);
    
    RtlSecureZeroMemory(&aot,  sizeof(AOTUSERDATA));
    hWnd = CreateWindow(MAKEINTATOM(atom), AOT_HOOK_NAME, 0, 0, 0, 0, 0, 0, 0, (HINSTANCE)&__ImageBase, &aot);

    if (!hWnd)
      ExitProcess(EXIT_FAILURE);

    if (!CloseHandle(
          CreateThread(
            0, 0, (LPTHREAD_START_ROUTINE)(LPVOID)CbtHookThread, (LPVOID)&hWnd, 0, 0)))
      ExitProcess(EXIT_FAILURE);

    else
    {
      MSG msg;
      RtlSecureZeroMemory(&msg, sizeof(MSG));
      while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      ExitProcess(EXIT_SUCCESS);
    }
#endif
}
#endif
#endif