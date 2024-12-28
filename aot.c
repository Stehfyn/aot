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

//__declspec(align(16)) 
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
      DWORD exStyle = (DWORD)GetWindowLong(hWnd, GWL_EXSTYLE);
      RtlSecureZeroMemory(&mii, sizeof(MENUITEMINFO));
      mii.cbSize = sizeof(MENUITEMINFO);
      mii.fMask  = MIIM_ID;
      
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
BOOL CFORCEINLINE APIPRIVATE
IsShellWindow(HWND Window)
{
  if (!IsWindow(Window) || !IsWindowVisible(Window))
  {
    return 1;
  }
  //if (GetAncestor(Window, GA_PARENT) != GetDesktopWindow())
  //{
  //  return 0;
  //}

  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  if (ClientRect.right - ClientRect.left <= 1 || ClientRect.bottom - ClientRect.top <= 1)
  {
    return 1;
  }

  TCHAR cDescription[256];
  GetWindowText(Window, cDescription, sizeof(cDescription));
  
  if (!_tccmp(cDescription, _T("Start")))
  {
    return 1;
  }

  // Start at the root owner
  /*HWND hwndWalk = GetAncestor(Window, GA_ROOTOWNER);

  // See if we are the last active visible popup
  HWND hwndTry;
  while ((hwndTry = GetLastActivePopup(hwndWalk)) != hwndTry)
  {
      if (IsWindowVisible(hwndTry)) break;
      hwndWalk = hwndTry;
  }
  return (hwndWalk == Window);*/
  return 0;
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
      HWND  hWnd    = GetForegroundWindow();
      if(!IsShellWindow(hWnd)){
      switch(wParam) {
      case SC_AOT:{
        
        DWORD exStyle = (DWORD)GetWindowLong(hWnd, GWL_EXSTYLE);
        SetWindowPos(
          hWnd,
          (exStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST,
          0,0,0,0,
          SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_FRAMECHANGED
        );
      }
      case SC_MOUSEMENU:
      case SC_KEYMENU:
        {}
        UpdateSystemMenu(GetForegroundWindow());
      default:
        break;
      }
      break;
    }
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
          if(_tccmp(szClassName, _T("#32768")) && _tccmp(szClassName, _T("OleMainThreadWndClass")))
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
AOTAPI DWORD AOTAPIV
GetHookProcessId(
    VOID)
{
    return ARCH(dwHookProcessId);
}

#endif

// -----------------------------------------------------
// aot.dll : Interface
// -----------------------------------------------------

EXTERN_C
AOTAPI DWORD AOTAPIV
GetHostProcessId(
    VOID
    );

// -----------------------------------------------------
// aot.dll : Implementation
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

#if defined _WINDLL

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
void
_tmain(
    void)
{
#ifdef _REL
  HMODULE hModule = GetModuleHandle(NULL);
  CreateDirectory(_T(".\\x86"), NULL);
  CreateDirectory(_T(".\\x64"), NULL);
  STARTUPINFOA        si = {0};
  PROCESS_INFORMATION pi = {0};
  SetCurrentDirectory(_T(".\\x86"));
  HANDLE  x86Host = UnloadResource(hModule, AOT_X86HOST_EXE_DATA, _T(AOT_X86HOST_EXE));
  CloseHandle(x86Host);
  CreateProcessA(NULL, "aotx86-host", NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
  PROCESS_INFORMATION pi2 ={0};
  SetCurrentDirectory(_T(".\\..\\x64"));
  HANDLE  x64Host = UnloadResource(hModule, AOT_X64HOST_EXE_DATA, _T(AOT_X64HOST_EXE));
  CloseHandle(x64Host);
  CreateProcessA(NULL, "aotx64-host", NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &si, &pi2);
  SetCurrentDirectory(_T(".\\.."));
  HANDLE hJob = CreateJobObject(NULL, NULL);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {0};
  jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  //JOBOBJECT_BASIC_UI_RESTRICTIONS uiRestrictions = {0};
  //uiRestrictions.UIRestrictionsClass = JOB_OBJECT_UILIMIT_READCLIPBOARD | JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
  if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo))) {
    CloseHandle(hJob);
  }
  //if (!SetInformationJobObject(hJob, JobObjectBasicLimitInformation, &uiRestrictions, sizeof(uiRestrictions))) {
  //  CloseHandle(hJob);
  //}
  AssignProcessToJobObject(hJob, pi.hProcess);
  AssignProcessToJobObject(hJob, pi2.hProcess);
  ResumeThread(pi.hThread);
  ResumeThread(pi2.hThread);
  if (!CloseHandle(
    CreateThread(
      0, 0, (LPTHREAD_START_ROUTINE)Killcord2, (LPVOID)(DWORD_PTR)pi.dwProcessId, 0, 0)))
    ExitProcess(1);
  if (!CloseHandle(
    CreateThread(
      0, 0, (LPTHREAD_START_ROUTINE)Killcord2, (LPVOID)(DWORD_PTR)pi2.dwProcessId, 0, 0)))
    ExitProcess(1);
  SuspendThread(GetCurrentThread());
  ExitProcess(0);
}
#elif defined _HOST
    HMODULE hModule   = GetModuleHandle(NULL);
    HANDLE  hDllHost  = UnloadResource(hModule, AOT_HOST_DLL_DATA, _T(AOT_HOST_DLL));
    CloseHandle(hDllHost);
    HMODULE hModHost = LoadLibrary(_T(AOT_HOST_DLL));
    if(hModHost)
    {
      CHAR    szPath    [MAX_PATH];
      GetModuleFileNameA(NULL, szPath, sizeof(szPath));
      PathRemoveFileSpecA(szPath);
      PathAppendA(szPath, "aotx64.exe");
      HANDLE  hDll = UnloadResource(hModule, AOT_HOOK_DLL_DATA, _T(AOT_HOOK_DLL));
      HANDLE  hExe = UnloadResource(hModule, AOT_HOOK_EXE_DATA, _T(AOT_HOOK_EXE));
      HANDLE  hSentinel = UnloadResource(hModule, AOT_SENTINEL_EXE_DATA, _T(AOT_SENTINEL_EXE));
      STARTUPINFOA        si;
      PROCESS_INFORMATION pi;
      PROCESS_INFORMATION pi2;

      RtlSecureZeroMemory(szPath, sizeof(szPath));
      printf("%s\n", szPath);
      printf("%s\n", szPath);
      printf("%s\n", szPath);

      RtlSecureZeroMemory(&si, sizeof(STARTUPINFOA));
      si.dwFlags = STARTF_USESHOWWINDOW;
      si.wShowWindow = SW_HIDE;
      RtlSecureZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
      RtlSecureZeroMemory(&pi2, sizeof(PROCESS_INFORMATION));
      CloseHandle(hExe);
      CloseHandle(hDll);
      CloseHandle(hSentinel);
      if (CreateProcessA(NULL, "aot-hook.exe", NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
      {
        CloseHandle(pi.hThread);
      }
      else
      {
        printf("lasterr %ld\n", GetLastError());
      }
      if (CreateProcessA(NULL, "aot-sentinel.exe", NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi2))
      {
        CloseHandle(pi.hThread);
      }
      else
      {
        printf("lasterr %ld\n", GetLastError());
      }
      HANDLE h[2] = {pi.hProcess, pi2.hProcess};
      WaitForMultipleObjects(2, h, FALSE, INFINITE);
      CloseHandle(pi.hProcess);
      CloseHandle(pi2.hProcess);
      CloseHandle(hModHost);
    }
    ExitProcess(0);
}
#elif defined _SENTINEL
  DWORD dwExitCode = EXIT_FAILURE;
  HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetHookProcessId());
  if (hProcess)
  {
    INT nRealityChecks = 1000;

    if (GetCurrentProcessId() != GetHookProcessId())
    {
      WaitForSingleObject(hProcess, INFINITE);
      dwExitCode = EXIT_SUCCESS;
    }
    WaitForSingleObject(hProcess, INFINITE);
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
    UINT        uExitCode  = EXIT_FAILURE;

    if(!CloseHandle(
          CreateThread(
            0, 0, (LPTHREAD_START_ROUTINE)Killcord, (LPVOID)(DWORD_PTR)GetHostProcessId(), 0, 0)))
      ExitProcess(uExitCode);

    RtlSecureZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.hInstance     = (HINSTANCE)&__ImageBase;
    wcex.lpszClassName = AOT_CLASS_NAME;
    wcex.lpfnWndProc   = AotWndProc;

    if (!RegisterClassEx(&wcex))
      ExitProcess(uExitCode);

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
      ExitProcess(uExitCode);

    if (CloseHandle(
          CreateThread(
            0, 0, (LPTHREAD_START_ROUTINE)CbtHookThread, (LPVOID)&hWnd, 0, 0)))
    {
      MSG msg;
      RtlSecureZeroMemory(&msg, sizeof(MSG));
      while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      uExitCode = EXIT_SUCCESS;
    }
    ExitProcess(uExitCode);
}

#endif
#endif
#endif