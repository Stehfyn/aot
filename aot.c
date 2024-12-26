// -----------------------------------------------------
// resource.h : Definitions
// -----------------------------------------------------

//#define AOTX86_DLL 101
//#define AOTX86_EXE 103
//#define AOTX86_SENTINEL_EXE 105
#define AOTX64_EXE_MANIFEST (1)
#define AOTX64_DLL          (102)
#define AOTX64_EXE          (104)
#define AOTX64_SENTINEL_EXE (106)

// -----------------------------------------------------
// aot-hook.rc : Definitions
// -----------------------------------------------------
#ifdef _RES
//AOTX86_DLL RCDATA "aotx86.dll"
//AOTX86_EXE RCDATA "aotx86.exe"
//AOTX86_SENTINEL_EXE RCDATA "aotx86-sentinel.exe"
AOTX64_EXE_MANIFEST RT_MANIFEST     "aot.exe.manifest"
AOTX64_DLL          RCDATA          "aotx64.dll"
AOTX64_EXE          RCDATA          "aotx64.exe"
AOTX64_SENTINEL_EXE RCDATA          "aotx64-sentinel.exe"
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

#if   (defined __x86_64__) || (defined _M_X64)
  #define ARCH(identifier) identifier ## 64
  #define SEGMENT ".shared64"
#elif (defined __i386__) || (defined _M_IX86)
  #define ARCH(identifier) identifier ## 32
  #define SEGMENT ".shared32"
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
AOTAPI BOOL AOTAPIV
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
#define AOT_MENUTEXT_EXIT          (TEXT("Exit"))
#define AOT_MENU_ALWAYS_ON_TOP     (0x69)
#define AOT_MENU_EXIT              (0x69 + 1)
#define SYSCOMMAND(value)          (value & 0xFFF0)
#define SC_AOT                     (SYSCOMMAND(AOT_MENU_ALWAYS_ON_TOP))

#define WM_AOTQUIT                 (WM_USER + 0x69)
#define WM_AOTHOOKINIT             (WM_USER + 0x69 + 1)
#define WM_AOTMOUSEHOOK            (WM_USER + 0x69 + 2)
#define WM_AOTCBTHOOK              (WM_USER + 0x69 + 3)

#define HANDLE_WM_AOTHOOKINIT(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (AOTHOOKTHREAD)(wParam), (DWORD)(lParam)), 0L)

#define HANDLE_WM_AOTMOUSEHOOK(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (LONG)(wParam), MAKEPOINTS((lParam))), 0L)

#define HANDLE_WM_AOTCBTHOOK(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (INT)(DWORD)(wParam), (HWND)(LONG_PTR)(lParam)), 0L)

__declspec(align(16)) typedef struct _AOTUSERDATA {
  DWORD dwMouseHookThreadId;
  DWORD dwCbtHookThreadId;
  BOOL  fAlwaysOnTop;
#if   (defined __x86_64__) || (defined _M_X64)
  __declspec(align(4)) unsigned __int8 reserved[4];
#endif
} AOTUSERDATA, * LPAOTUSERDATA;

typedef enum _AOTHOOKTHREAD {
  AOTMOUSEHOOKTHREAD,
  AOTCBTHOOKTHREAD,
} AOTHOOKTHREAD;

static
LPTSTR CFORCEINLINE APIPRIVATE
CbtHookCodeToString(
  int nCode)
{
#define HCBT_HC2STR(x) \
    case x:            \
        return _T(#x);
    switch (nCode) {
    HCBT_HC2STR(HCBT_ACTIVATE)
    HCBT_HC2STR(HCBT_CREATEWND)
    HCBT_HC2STR(HCBT_DESTROYWND)
    HCBT_HC2STR(HCBT_SYSCOMMAND)
    DEFAULT_UNREACHABLE;
    }
#undef HCBT_HC2STR
}

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

    switch (dwHookThread) {
    case AOTMOUSEHOOKTHREAD: 
      //wprintf(L"AotMouseHookThreadInit %ld\n", dwAotHookThreadId);
      lpAotUserData->dwMouseHookThreadId = dwAotHookThreadId;
      break;
    case AOTCBTHOOKTHREAD:
      //wprintf(L"AotCbtHookThreadInit %ld\n", dwAotHookThreadId);
      lpAotUserData->dwCbtHookThreadId = dwAotHookThreadId;
      break;
    DEFAULT_UNREACHABLE;
    }
}

static 
CFORCEINLINE VOID CALLBACK
OnAotMouseHook(
    HWND   hWnd,
    LONG  mouseData,
    POINTS pts)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(mouseData);
    static BOOL nc = 0;
    static POINT pt = {0};
    static HWND hwndHovered = 0;

    if(!mouseData)
    {
      RtlSecureZeroMemory(&pt, sizeof(POINT));
      POINTSTOPOINT(pt, MAKEPOINTS(pts));

      RtlSecureZeroMemory(&hwndHovered, sizeof(HWND));
      hwndHovered = WindowFromPoint(pt);
      if(hwndHovered)
      {
        LRESULT result = SendMessage(hwndHovered, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
        nc = result == HTCAPTION;
      }
    }
    else
    {
      if(hwndHovered)
      {
        DWORD exStyle = (DWORD)GetWindowLongPtr(hwndHovered, GWL_EXSTYLE);
        if(!(exStyle & WS_EX_LAYERED))
        {
          SetWindowLongPtr(hwndHovered, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        }
        static int div = 10;
        if(mouseData > 0)
        {
          if (div != 1)
            div -= 1;
        }
        else if(mouseData < 0)
        {
          if(div != 10)
            div += 1;
        }
        SetLayeredWindowAttributes(hwndHovered, 0, (BYTE)180, LWA_ALPHA);
        _tprintf(_T("yuh: %ld\n"), mouseData);
      }
      //TCHAR   szClassName[256];
      //RtlSecureZeroMemory(szClassName, sizeof(szClassName));
      //if(GetClassName(hwndHovered, szClassName, sizeof(szClassName)))
      //{
      //  TCHAR szWindowText[256];
      //  RtlSecureZeroMemory(szWindowText, sizeof(szWindowText));
      //  GetWindowText(hwndHovered, szWindowText, sizeof(szWindowText));
      //  //wprintf(L"%ld %ld HT: %ld: | %s %s\n", pt.x, pt.y, (LONG)result, szWindowText, szClassName);
      //}
    }
}

static
VOID CFORCEINLINE CALLBACK
OnAotCbtHook(
    HWND hWnd,
    int  nCode,
    HWND hWnd2)
{
    UNREFERENCED_PARAMETER(hWnd);

    TCHAR szClassName[256];
    RtlSecureZeroMemory(szClassName, sizeof(szClassName));
    if(GetClassName(hWnd2, szClassName, ARRAYSIZE(szClassName)))
    {
      TCHAR szName[256];
      RtlSecureZeroMemory(szName, sizeof(szName));
      if(GetWindowText(hWnd2, szName, sizeof(szName)))
      {
        static INT i = 0;
        _tprintf(_T("%d AotCbtHook %s\n%s\n%s\n"), i++, CbtHookCodeToString(nCode), szName, szClassName);
      }
    }
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

    if (lpAotUserData->dwMouseHookThreadId)
    {
      HANDLE hMouseHookThread = OpenThread(SYNCHRONIZE, FALSE, lpAotUserData->dwMouseHookThreadId);
      PostThreadMessage(lpAotUserData->dwMouseHookThreadId, WM_AOTQUIT, 0, 0);

      if (hMouseHookThread)
      {
        WaitForSingleObject(hMouseHookThread, INFINITE);
        CloseHandle(hMouseHookThread);
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
    HANDLE_MSG(hWnd, WM_AOTMOUSEHOOK, OnAotMouseHook);
    HANDLE_MSG(hWnd, WM_AOTCBTHOOK,   OnAotCbtHook);
    HANDLE_MSG(hWnd, WM_CLOSE,        OnClose);
    HANDLE_MSG(hWnd, WM_DESTROY,      OnDestroy);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

static
LRESULT CFORCEINLINE CALLBACK
LowLevelMouseProc(
    int    nCode,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (wParam) {
    case WM_MOUSEWHEEL:
    case WM_MOUSEMOVE:
      PostThreadMessage(
        GetCurrentThreadId(),
        WM_AOTMOUSEHOOK,
        (WPARAM)(LONG)HIWORD(((LPMSLLHOOKSTRUCT)lParam)->mouseData),
        (LPARAM)(DWORD)POINTTOPOINTS(((LPMSLLHOOKSTRUCT)lParam)->pt)
      );
    default:
      return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
}

static
VOID CFORCEINLINE APIPRIVATE
MouseHookThread(
    LPVOID lpvhWnd)
{
    DWORD dwExitCode = EXIT_FAILURE;
    HHOOK hook       = SetWindowsHookEx(
      WH_MOUSE_LL,
      LowLevelMouseProc,
      (HINSTANCE)&__ImageBase,
      0
    );

    if (hook)
    {
      MSG msg;

      PostMessage(
        *(HWND*)lpvhWnd,
        WM_AOTHOOKINIT,
        (WPARAM)(DWORD)AOTMOUSEHOOKTHREAD,
        (LPARAM)(DWORD)GetCurrentThreadId()
      );

      RtlSecureZeroMemory(&msg, sizeof(MSG));
      while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        switch (msg.message) {
        case WM_QUIT:
        case WM_AOTQUIT:
          UnhookWindowsHookEx(hook);
          SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
          break;
        case WM_AOTMOUSEHOOK:
          PostMessage(*(HWND*)lpvhWnd, msg.message, msg.wParam, msg.lParam);
        default:
          continue;
        }
        break;
      }
      dwExitCode = EXIT_SUCCESS;
    }
    ExitThread(dwExitCode);
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
AppendSystemMenu(
    HWND hWnd)
{
    HMENU hSysMenu = NULL;
    GetSystemMenu(hWnd, TRUE);
    hSysMenu = GetSystemMenu(hWnd, FALSE);
    
    if(hSysMenu)
    {
      MENUITEMINFO mii;
      RtlSecureZeroMemory(&mii, sizeof(MENUITEMINFO));
      mii.cbSize = sizeof(MENUITEMINFO);
      mii.fMask  = MIIM_ID;

      if (!GetMenuItemInfo(hSysMenu, AOT_MENU_ALWAYS_ON_TOP, FALSE, &mii))
        return AppendMenu(
          hSysMenu,
          MF_BYPOSITION | MF_STRING,
          AOT_MENU_ALWAYS_ON_TOP,
          AOT_MENUTEXT_ALWAYS_ON_TOP
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
      switch(wParam) {
      case SC_AOT:
      {
        HWND  hWnd    = GetForegroundWindow();
        DWORD exStyle = (DWORD)GetWindowLong(hWnd, GWL_EXSTYLE);
        (VOID)SetWindowPos(
          hWnd,
          (exStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST,
          0,
          0,
          0,
          0,
          SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_FRAMECHANGED
        );
        PostMessage(
          ARCH(hWndHookMarshaller),
          WM_AOTCBTHOOK,
          (WPARAM)(DWORD)nCode,
          (LPARAM)(LONG_PTR)hWnd
        );
      }
      default:
        break;
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
          if(_tccmp(szClassName, _T("#32768")))
          {
            if(AppendSystemMenu((HWND)wParam))
              PostMessage(
                ARCH(hWndHookMarshaller),
                WM_AOTCBTHOOK,
                (WPARAM)(DWORD)nCode,
                (LPARAM)(LONG_PTR)wParam
              );
          }
        }
      }
      break;
    }
    case HCBT_CREATEWND:
    case HCBT_DESTROYWND:
    {
      PostMessage(
        ARCH(hWndHookMarshaller),
        WM_AOTCBTHOOK,
        (WPARAM)(DWORD)nCode,
        (LPARAM)(LONG_PTR)wParam
      );
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

static 
CFORCEINLINE BOOL WINAPI
HandlerRoutine(
  DWORD dwCtrlType)
{
  UNREFERENCED_PARAMETER(dwCtrlType);
  UnsetCbtHook();
  return TRUE;
}

static
CFORCEINLINE BOOL WINAPI
CreateConsole(
  INT nWidth,
  INT nHeight)
{
  if (!FreeConsole())
  {
    return FALSE;
  }
  if (!AllocConsole())
  {
    return FALSE;
  }
  //if(!SetConsoleCtrlHandler((PHANDLER_ROUTINE)HandlerRoutine, TRUE))
  //{
  //  return FALSE;
  //}
  if (!SetConsoleTitle(_T("AOT Console")))
  {
    return FALSE;
  }
  if (!SetConsoleCP(CP_UTF8))
  {
    return FALSE;
  }
  if (!SetConsoleOutputCP(CP_UTF8))
  {
    return FALSE;
  }
  if (!SetConsoleMode(
        GetStdHandle(STD_INPUT_HANDLE),
        ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS
  ))
  {
    return FALSE;
  }
  if (!SetConsoleMode(
        GetStdHandle(STD_OUTPUT_HANDLE),
        ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT
  ))
  {
    return FALSE;
  }
  if (SetConsoleMode(
        GetStdHandle(STD_ERROR_HANDLE),
        ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT
  ))
  {
    HWND hWnd = GetConsoleWindow();
    if (hWnd)
    {
      MONITORINFO mi = { sizeof(mi) };
      if (GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi))
      {
        AnimateWindow(hWnd, 0, AW_HIDE | AW_SLIDE);
        SetWindowPos(
          hWnd,
          NULL,
#ifdef _M_X64
          nWidth,
#else
          0,
#endif
          mi.rcWork.bottom - nHeight,
          nWidth,
          nHeight,
          SWP_NOZORDER | SWP_NOACTIVATE
        );
        AnimateWindow(hWnd, 150, AW_ACTIVATE | AW_SLIDE);
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      }
    }
    return TRUE;
  }
  return FALSE;
}

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
    int nResourceId, 
    TCHAR szName[])
{
    HANDLE hOut = NULL;
    HRSRC hResourceInfo = FindResource(hModule, MAKEINTRESOURCE(nResourceId), RT_RCDATA);
    if (hResourceInfo)
    {
      HGLOBAL hResourceData = LoadResource(hModule, hResourceInfo);
      if(hResourceData)
      {
        CONST LPVOID pvResourceData = (CONST LPVOID)LockResource(hResourceData);
        if(pvResourceData)
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
              DWORD dwBytesWritten = 0;
              if(!WriteFile(
                hFile,          
                pvResourceData, 
                dwResourceSize, 
                &dwBytesWritten,
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
    ExitProcess(0);
}

void
_tmain(
    void)
{
#ifdef _HOST
    if(GetHostProcessId())
    {
      CHAR    szPath    [MAX_PATH];
      HMODULE hModule   = GetModuleHandle(NULL);
      GetModuleFileNameA(NULL, szPath, sizeof(szPath));
      PathRemoveFileSpecA(szPath);
      PathAppendA(szPath, "aotx64.exe");
      HANDLE  hDll      = UnloadResource(hModule, AOTX64_DLL,          _T("aotx64.dll"));
      HANDLE  hSentinel = UnloadResource(hModule, AOTX64_SENTINEL_EXE, _T("aotx64-sentinel.exe"));
      HANDLE  hExe      = UnloadResource(hModule, AOTX64_EXE,          _T("aotx64.exe"));

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
      CloseHandle(hSentinel);
      CloseHandle(hDll);
      if (CreateProcessA(NULL, "aotx64.exe", NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
      {
        CloseHandle(pi.hThread);
      }
      else
      {
        printf("lasterr %ld\n", GetLastError());
      }
      if (CreateProcessA(NULL, "aotx64-sentinel.exe", NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi2))
      {
        CloseHandle(pi.hThread);
      }
      HANDLE h[2] = {pi.hProcess, pi2.hProcess};
      WaitForMultipleObjects(2, h, FALSE, INFINITE);
      CloseHandle(pi.hProcess);
      CloseHandle(pi2.hProcess);
    }
    ExitProcess(0);
}
#elif defined _SENTINEL
    DWORD dwExitCode = EXIT_FAILURE;
    HANDLE hProcess  = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetHookProcessId());
    if (hProcess)
    {
      INT nRealityChecks = 1000;
      
      if(GetCurrentProcessId() != GetHookProcessId())
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
    LPVOID      dwCatalyst = (LPVOID)(DWORD_PTR)GetHostProcessId();

    if(!CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Killcord, dwCatalyst, 0, 0)))
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

    if (!CreateConsole(650, 150))
      ExitProcess(uExitCode);

    if (CloseHandle(
          CreateThread(
            0, 0, (LPTHREAD_START_ROUTINE)MouseHookThread, (LPVOID)&hWnd, 0, 0)))
    {
      if (CloseHandle(
            CreateThread(
              0, 0, (LPTHREAD_START_ROUTINE)CbtHookThread, (LPVOID)&hWnd, 0, 0)))
      {
        MSG msg;
        RtlSecureZeroMemory(&msg, sizeof(MSG));
        while (GetMessage(&msg, NULL, 0, 0) > 0) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
          continue;
        }
        uExitCode = EXIT_SUCCESS;
      }
    }
    ExitProcess(uExitCode);
}
#endif
#endif
#endif