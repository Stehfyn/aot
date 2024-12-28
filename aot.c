// -----------------------------------------------------
// resource.h : Definitions
// -----------------------------------------------------
#define AOT_HOOK_EXE_MANIFEST 1
#define AOT_HOOK_DLL_DATA     101
#define AOT_HOOK_EXE_DATA     102
#define AOT_HOST_DLL_DATA     103
#define AOT_SENTINEL_EXE_DATA 104
#define AOT_X86HOST_EXE_DATA  105
#define AOT_X64HOST_EXE_DATA  106
#define AOT_HOOK_DLL          "aot-hook.dll"
#define AOT_HOOK_EXE          "aot-hook.exe"
#define AOT_HOST_DLL          "aot-host.dll"
#define AOT_SENTINEL_EXE      "aot-sentinel.exe"
#define AOT_X86HOST_EXE       "aotx86-host.exe"
#define AOT_X64HOST_EXE       "aotx64-host.exe"

// -----------------------------------------------------
// aot-host.rc : Definitions
// -----------------------------------------------------

#if   (defined _HOSTRES)
AOT_EXE_MANIFEST      RT_MANIFEST     "aot.exe.manifest"
AOT_HOOK_DLL_DATA     RCDATA          AOT_HOOK_DLL
AOT_HOOK_EXE_DATA     RCDATA          AOT_HOOK_EXE
AOT_HOST_DLL_DATA     RCDATA          AOT_HOST_DLL
AOT_SENTINEL_EXE_DATA RCDATA          AOT_SENTINEL_EXE

// -----------------------------------------------------
// aot-release.rc : Definitions
// -----------------------------------------------------

#elif (defined _RELRES)
AOT_EXE_MANIFEST      RT_MANIFEST     "aot.exe.manifest"
AOT_X86HOST_EXE_DATA  RCDATA          AOT_X86HOST_EXE
AOT_X64HOST_EXE_DATA  RCDATA          AOT_X64HOST_EXE

#else
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_DISABLE_PERFCRIT_LOCKS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <psapi.h>
#include <winternl.h>
#include <shellapi.h>
EXTERN_C
IMAGE_DOS_HEADER __ImageBase;

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
      ALLOC_SEGMENT(SEGMENT, DWORD,     dwHostProcessId,    0);

    #elif defined _AOTHOOKDLL
      ALLOC_SEGMENT(SEGMENT, DWORD,     dwHookProcessId,    0);
      ALLOC_SEGMENT(SEGMENT, HHOOK,     hCbtHook,           NULL);
      ALLOC_SEGMENT(SEGMENT, HINSTANCE, hCbtHookModule,     NULL);
      ALLOC_SEGMENT(SEGMENT, HWND,      hWndHookMarshaller, NULL);
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

#define AOT_WINDOW_NAME            (TEXT("AOT"))
#define AOT_CLASS_NAME             (TEXT("AOTClass"))
#define AOT_INSTANCE_MUTEX         (TEXT("AOTInstanceMutex"SEGMENT))

#define AOT_MENUTEXT_ALWAYS_ON_TOP (TEXT("Always On Top"))
#define AOT_MENU_ALWAYS_ON_TOP     (0x69)
#define SYSCOMMAND(value)          (value & 0xFFF0)
#define SC_AOT                     (SYSCOMMAND(AOT_MENU_ALWAYS_ON_TOP))

#define WM_AOTQUIT                 (WM_USER + 0x69)
#define WM_AOTHOOKINIT             (WM_USER + 0x69 + 1)

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
    HANDLE   hInstanceMutex = CreateMutex(NULL, TRUE, AOT_INSTANCE_MUTEX);
    
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
OnAotInit(
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
      UnsetCbtHook();
      SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);

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

    PostQuitMessage(0);
}

static
LRESULT CFORCEINLINE CALLBACK
AotWndProc(
    HWND   hWnd,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message) {
    HANDLE_MSG(hWnd, WM_NCCREATE,     OnNcCreate);
    HANDLE_MSG(hWnd, WM_AOTHOOKINIT,  OnAotInit);
    HANDLE_MSG(hWnd, WM_CLOSE,        OnClose);
    HANDLE_MSG(hWnd, WM_DESTROY,      OnDestroy);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

static
VOID CFORCEINLINE APIPRIVATE
CbtHookThread(
    LPVOID lpvhWnd)
{
    DWORD dwExitCode = EXIT_FAILURE;

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
          SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
          break;
        default:
          continue;
        }
        break;
      }
      dwExitCode = EXIT_SUCCESS;
    }
    ExitThread(dwExitCode);
}

// -----------------------------------------------------
// aot-hook.dll : Implementation
// -----------------------------------------------------

#ifdef _AOTHOOKDLL

static 
BOOL CFORCEINLINE CALLBACK
UpdateSystemMenu(
    HWND hWnd)
{
    HMENU hSysMenu = NULL;
    GetSystemMenu(hWnd, TRUE);
    hSysMenu = GetSystemMenu(hWnd, FALSE);
    
    if(hSysMenu)
    {
      MENUITEMINFO mii;
      DWORD        exStyle; 
      RtlSecureZeroMemory(&mii, sizeof(MENUITEMINFO));

      mii.cbSize = sizeof(MENUITEMINFO);
      mii.fMask  = MIIM_ID;
      exStyle    = (DWORD)GetWindowLong(hWnd, GWL_EXSTYLE);
      
      if (!GetMenuItemInfo(hSysMenu, AOT_MENU_ALWAYS_ON_TOP, FALSE, &mii))
        return AppendMenu(
          hSysMenu,
          MF_BYPOSITION | MF_STRING | ((exStyle & WS_EX_TOPMOST) ? MF_CHECKED : MF_UNCHECKED),
          AOT_MENU_ALWAYS_ON_TOP,
          AOT_MENUTEXT_ALWAYS_ON_TOP
        );
      else
        return ModifyMenu(
          hSysMenu,
          AOT_MENU_ALWAYS_ON_TOP,
          MF_BYCOMMAND | MF_STRING | ((exStyle & WS_EX_TOPMOST) ? MF_CHECKED : MF_UNCHECKED),
          AOT_MENU_ALWAYS_ON_TOP,
          NULL
        );
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
    case HCBT_SYSCOMMAND:
    {
      HWND hWnd = GetForegroundWindow();
      if(IsWindowVisible(hWnd))
      {
        switch(wParam) {
        case SC_AOT:
        {
          DWORD exStyle = (DWORD)GetWindowLong(hWnd, GWL_EXSTYLE);
          SetWindowPos(
            hWnd,
            (exStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST,
            0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_FRAMECHANGED
          );
          break;
        }
        case SC_MOUSEMENU:
        case SC_KEYMENU:
          UpdateSystemMenu(GetForegroundWindow());
        default:
          break;
        }
      }
      break;
    }
    case HCBT_ACTIVATE:
    {
      if (lParam)
      {
        HWND hWndActive = ((LPCBTACTIVATESTRUCT)lParam)->hWndActive;
        if(hWndActive)
        {
          
          TCHAR szClassName[256];
          GetClassName((HWND)wParam, szClassName, sizeof(szClassName));
          if(_tccmp(szClassName, _T("#32768")) && 
             _tccmp(szClassName, _T("CLIPBRDWNDCLASS")) &&
             _tccmp(szClassName, _T("ApplicationFrameWindow")))
          {
            UpdateSystemMenu((HWND)wParam);
          }
        }
      }
      break;
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
    HHOOK hook = SetWindowsHookEx(WH_CBT, CBTProc, ARCH(hCbtHookModule), 0);
    if (hook)
    {
      ARCH(hCbtHook)           = hook;
      ARCH(hWndHookMarshaller) = hWnd;
      return TRUE;
    }
    return FALSE;
}

EXTERN_C
AOTAPI BOOL AOTAPIV
UnsetCbtHook(
    VOID)
{
    DWORD dwExitCode = EXIT_FAILURE;
    if (ARCH(hCbtHook))
    {
      UnhookWindowsHookEx(ARCH(hCbtHook));
      ARCH(hWndHookMarshaller) = NULL;
      dwExitCode               = EXIT_SUCCESS;
    }
    return (BOOL)dwExitCode;
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

#if defined _AOTHOSTDLL
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

#if defined _WINDLL

static
BOOL CFORCEINLINE APIPRIVATE
IsServiceHost(
    HANDLE hProcess)
{
    TCHAR szPath[MAX_PATH];
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
    case DLL_PROCESS_ATTACH: {
      // Tell any reject ass services to fuck off
      if(IsServiceHost(GetCurrentProcess()))
        return FALSE;
#if defined   _AOTHOSTDLL
      if (!ARCH(dwHostProcessId)) ARCH(dwHostProcessId) = GetCurrentProcessId();

#elif defined _AOTHOOKDLL
      if(!ARCH(dwHookProcessId)) ARCH(dwHookProcessId) = GetCurrentProcessId();
      ARCH(hCbtHookModule) = hModule;

#endif
      DisableThreadLibraryCalls(hModule);
    }
    default:
      SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
      break;
    }
    return TRUE;
}

#else

// -----------------------------------------------------
// aot-host.exe : Implementation
// -----------------------------------------------------

typedef enum _AOTHOST {
  AOT_X86HOST,
  AOT_X64HOST,
} AOTHOST;

typedef struct _AOTMANAGEDHOST {
  TCHAR               szPath[_MAX_ENV];
  HANDLE              hKillcordThread;
  STARTUPINFO         si;
  PROCESS_INFORMATION pi;

} AOTMANAGEDHOST;

typedef struct _AOTINSTANCE {
  TCHAR          szPath[MAX_PATH];
  AOTMANAGEDHOST x86ManagedHost;
  AOTMANAGEDHOST x64ManagedHost;
  HANDLE         hJob;
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
HANDLE CFORCEINLINE APIPRIVATE
UnloadResource(
    HMODULE hModule, 
    UINT    nResourceId, 
    TCHAR   szName[])
{
    HANDLE hOut = NULL;
    HRSRC hResourceInfo = FindResource(hModule, MAKEINTRESOURCE(nResourceId), RT_RCDATA);
    if (hResourceInfo)
    {
      HGLOBAL hResourceData = LoadResource(hModule, hResourceInfo);
      if(hResourceData)
      {
        CONST LPVOID lpvResourceData = (CONST LPVOID)LockResource(hResourceData);
        if(lpvResourceData)
        {
          DWORD dwResourceSize = SizeofResource(hModule, hResourceInfo);
          if(dwResourceSize)
          {
            HANDLE hFile = CreateFile(
              szName,        
              GENERIC_WRITE,
              0,
              NULL,
              CREATE_ALWAYS,
              FILE_ATTRIBUTE_NORMAL,
              NULL
            );
            if(hFile)
            {
              DWORD _ = 0;
              if(!WriteFile(
                hFile,          
                lpvResourceData, 
                dwResourceSize, 
                &_,
                NULL            
                ))
              {
                CloseHandle(hFile);
              }
              hOut = hFile;
            }
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
Killcord2(
  LPVOID dwProcessId)
{
  INT nRealityChecks = 100;
  HANDLE hProcessId = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)(DWORD_PTR)dwProcessId);
  if (hProcessId)
  {
    WaitForSingleObject(hProcessId, INFINITE);
  }
  CloseHandle(hProcessId);
  do SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0); while (0 < --nRealityChecks);
  ExitProcess(0);
}

static
BOOL CFORCEINLINE APIPRIVATE
CreateSuspendedHost(
    HMODULE               hModule,
    AOTHOST               eHost,
    AOTMANAGEDHOST*       lpManagedHost)
{
    LPTSTR szPath = lpManagedHost->szPath;
    TCHAR WorkingDirectory[_MAX_ENV];
    GetCurrentDirectory(sizeof(WorkingDirectory), szPath);
    GetCurrentDirectory(sizeof(WorkingDirectory), WorkingDirectory);
    switch(eHost) {
    case AOT_X86HOST: {
      CreateDirectory(_T("x86"), NULL);
      PathAppend(szPath, _T("x86"));
      CreateDirectory(szPath, NULL);
      PathAppend(szPath, _T("\\"));
      PathAppend(szPath, _T(AOT_X86HOST_EXE));
      if(!CloseHandle(UnloadResource(hModule, AOT_X86HOST_EXE_DATA, szPath)))
        return FALSE;
      PathAppend(WorkingDirectory, _T("\\x86"));
      if(!CreateProcess(
        NULL, 
        szPath, 
        NULL,
        NULL,
        FALSE, 
        CREATE_SUSPENDED|CREATE_NO_WINDOW, 
        NULL,
        WorkingDirectory,
        &lpManagedHost->si,
        &lpManagedHost->pi))
        return FALSE;
      break;
    }
    case AOT_X64HOST: {
      CreateDirectory(_T("x64"), NULL);
      PathAppend(szPath, _T("x64"));
      CreateDirectory(szPath, NULL);
      PathAppend(szPath, _T("\\"));
      PathAppend(szPath, _T(AOT_X64HOST_EXE));
      if(!CloseHandle(UnloadResource(hModule, AOT_X64HOST_EXE_DATA, szPath)))
        return FALSE;
      PathAppend(WorkingDirectory, _T("\\x64"));
      if(!CreateProcess(
        NULL,
        szPath,
        NULL,
        NULL,
        FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW,
        NULL,
        WorkingDirectory,
        &lpManagedHost->si,
        &lpManagedHost->pi))
        return FALSE;
      break;
    }
    DEFAULT_UNREACHABLE;
    }
    
    lpManagedHost->hKillcordThread =
      CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Killcord2, (LPVOID)(DWORD_PTR)lpManagedHost->pi.dwProcessId, CREATE_SUSPENDED, 0);
     return FALSE;
}

static
VOID CFORCEINLINE APIPRIVATE
Killcord(
    LPVOID dwProcessId)
{
    HANDLE hProcessId = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)(DWORD_PTR)dwProcessId);
    if (hProcessId)
    {
      WaitForSingleObject(hProcessId, INFINITE);
    }
    UnsetCbtHook();
    SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
    CloseHandle(hProcessId);
    ExitProcess(0);
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

#define WM_TRAYICON (WM_USER + 1)
static
LRESULT CFORCEINLINE CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
  if (uMsg == WM_TRAYICON) 
  {
    if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
      PostQuitMessage(0); // Exit on click
    }
  }
  else if (uMsg == WM_DESTROY) {
    PostQuitMessage(0);
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static
BOOL CFORCEINLINE APIPRIVATE
CreateTrayIcon(
  NOTIFYICONDATA* nid)
{
#define AOT_TRAYWINDOW_NAME            (TEXT("AOT_TrayWnd"))
#define AOT_TRAYCLASS_NAME             (TEXT("AOT_TrayWndClass"))
  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = AOT_TRAYCLASS_NAME;
  RegisterClass(&wc);

  HWND hwnd = CreateWindow(AOT_TRAYCLASS_NAME, AOT_TRAYWINDOW_NAME, 0, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);

  nid->cbSize = sizeof(NOTIFYICONDATA);
  nid->hWnd = hwnd;
  nid->uID = 1;
  nid->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid->uCallbackMessage = WM_TRAYICON;
  nid->hIcon = LoadIcon(NULL, IDI_APPLICATION);
  lstrcpy(nid->szTip, _T("Spencer is Cool"));
  Shell_NotifyIcon(NIM_ADD, nid);
return TRUE;
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

#ifdef _RELEASE
    AOTINSTANCE AlwaysOnTop;
    RtlSecureZeroMemory(&AlwaysOnTop, sizeof(AOTINSTANCE));
    
    AlwaysOnTop.hJob = CreateJobObject(NULL, NULL);
    if(!SetJobInformation(AlwaysOnTop.hJob))
    {
      CloseHandle(AlwaysOnTop.hJob);
      ExitProcess(EXIT_FAILURE);
    }

    if(!SetCurrentDirectory(GetModulePath(GetCurrentProcess(), AlwaysOnTop.szPath)))
      ExitProcess(EXIT_FAILURE);

    CreateSuspendedHost((HMODULE)&__ImageBase, AOT_X86HOST, &AlwaysOnTop.x86ManagedHost);
    AssignProcessToJobObject(AlwaysOnTop.hJob, AlwaysOnTop.x86ManagedHost.pi.hProcess);
    CloseHandle(AlwaysOnTop.x86ManagedHost.pi.hProcess);

    CreateSuspendedHost((HMODULE)&__ImageBase, AOT_X64HOST, &AlwaysOnTop.x64ManagedHost);
    AssignProcessToJobObject(AlwaysOnTop.hJob, AlwaysOnTop.x64ManagedHost.pi.hProcess);
    CloseHandle(AlwaysOnTop.x64ManagedHost.pi.hProcess);

    ResumeThread(AlwaysOnTop.x86ManagedHost.pi.hThread);
    ResumeThread(AlwaysOnTop.x64ManagedHost.pi.hThread);
    CloseHandle(AlwaysOnTop.x86ManagedHost.pi.hThread);
    CloseHandle(AlwaysOnTop.x64ManagedHost.pi.hThread);

    ResumeThread(AlwaysOnTop.x86ManagedHost.hKillcordThread);
    ResumeThread(AlwaysOnTop.x64ManagedHost.hKillcordThread);
    CloseHandle(AlwaysOnTop.x86ManagedHost.hKillcordThread);
    CloseHandle(AlwaysOnTop.x64ManagedHost.hKillcordThread);
    NOTIFYICONDATA nid = { 0 };
    CreateTrayIcon(&nid);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &nid);

    CloseHandle(AlwaysOnTop.hJob);

    ExitProcess(EXIT_SUCCESS);
}
#elif defined _HOST
    HMODULE hHostInstance = NULL;
    hHostInstance = BootstrapHost((HMODULE)&__ImageBase);
    
    if(hHostInstance)
    {
      STARTUPINFOA        si;
      PROCESS_INFORMATION pi;
      HANDLE              lpKillcords[2] = { NULL, NULL };
      CloseHandle(UnloadResource((HMODULE)&__ImageBase, AOT_HOOK_DLL_DATA,     _T(AOT_HOOK_DLL)));
      CloseHandle(UnloadResource((HMODULE)&__ImageBase, AOT_HOOK_EXE_DATA,     _T(AOT_HOOK_EXE)));
      CloseHandle(UnloadResource((HMODULE)&__ImageBase, AOT_SENTINEL_EXE_DATA, _T(AOT_SENTINEL_EXE)));

      RtlSecureZeroMemory(&si, sizeof(STARTUPINFOA));
      RtlSecureZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
      if (!CreateProcessA(NULL, AOT_HOOK_EXE, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
      {
        CloseHandle(hHostInstance);
        ExitProcess(EXIT_FAILURE);
      }
      
      lpKillcords[0] = pi.hProcess;
      CloseHandle(pi.hThread);

      RtlSecureZeroMemory(&si, sizeof(STARTUPINFOA));
      RtlSecureZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
      if (!CreateProcessA(NULL, AOT_SENTINEL_EXE, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
      {
        CloseHandle(lpKillcords[0]);
        CloseHandle(hHostInstance);
        ExitProcess(EXIT_FAILURE);
      }
      lpKillcords[1] = pi.hProcess;
      CloseHandle(pi.hThread);

      WaitForMultipleObjects(ARRAYSIZE(lpKillcords), lpKillcords, FALSE, INFINITE);
      CloseHandle(lpKillcords[0]);
      CloseHandle(lpKillcords[1]);
      CloseHandle(hHostInstance);
      ExitProcess(EXIT_SUCCESS);
    }
    ExitProcess(EXIT_FAILURE);
}
#elif defined _SENTINEL
  DWORD dwExitCode = EXIT_FAILURE;
  HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, GetHookProcessId());
  if (hProcess)
  {
    INT nRealityChecks = 1000;

    if (GetCurrentProcessId() != GetHookProcessId())
    {
      WaitForSingleObject(hProcess, INFINITE);
      dwExitCode = EXIT_SUCCESS;
    }
    UnsetCbtHook();
    // Wake up any bum ass services who still have an instance of our .dlls loaded
    do SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0); while (0 < --nRealityChecks);
    CloseHandle(hProcess);
  }
  ExitProcess(dwExitCode);
}
#else
    AOTUSERDATA aot;
    WNDCLASSEX  wcex;
    HWND        hWnd;
    
    if(!CloseHandle(
          CreateThread(
            0, 0, (LPTHREAD_START_ROUTINE)Killcord, (LPVOID)(DWORD_PTR)GetHostProcessId(), 0, 0)))
      ExitProcess(EXIT_FAILURE);

    RtlSecureZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.hInstance     = (HINSTANCE)&__ImageBase;
    wcex.lpszClassName = AOT_CLASS_NAME;
    wcex.lpfnWndProc   = AotWndProc;

    if (!RegisterClassEx(&wcex))
      ExitProcess(EXIT_FAILURE);

    RtlSecureZeroMemory(&aot,  sizeof(AOTUSERDATA));
    RtlSecureZeroMemory(&hWnd, sizeof(HWND));
    hWnd = CreateWindowEx(
      WS_EX_TOOLWINDOW,
      AOT_CLASS_NAME,
      AOT_WINDOW_NAME,
      WS_POPUP,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      NULL,
      NULL,
      (HINSTANCE)&__ImageBase,
      (LPVOID)&aot
    );

    if (!hWnd)
      ExitProcess(EXIT_FAILURE);

    if (!CloseHandle(
          CreateThread(
            0, 0, (LPTHREAD_START_ROUTINE)CbtHookThread, (LPVOID)&hWnd, 0, 0)))
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
}

#endif
#endif
#endif