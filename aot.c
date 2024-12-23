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
#include <shellscalingapi.h>
#include <shlwapi.h>
#include <commctrl.h>

EXTERN_C
IMAGE_DOS_HEADER __ImageBase;

#ifdef _WINDLL
#define AOTAPI        __declspec(dllexport)
#else
#define AOTAPI        __declspec(dllimport)
#endif

#ifdef _M_X64
#define ARCH(identifier) identifier ## 64
#define SEGMENT ".shared64"
#else
#define ARCH(identifier) identifier ## 32
#define SEGMENT ".shared32"
#endif

#define STRINGIZE(x) #x

#ifdef _WINDLL
#pragma section(SEGMENT, read, write, shared)
#define ALLOC_SEGMENT(segment, type, identifier, assign)   \
        __declspec(allocate(SEGMENT)) __declspec(selectany) type ARCH(identifier) = assign;
ALLOC_SEGMENT(SEGMENT, HHOOK,     hCbtHook,           NULL);
ALLOC_SEGMENT(SEGMENT, HINSTANCE, hCbtHookModule,     NULL);
ALLOC_SEGMENT(SEGMENT, HWND,      hWndHookMarshaller, NULL);
ALLOC_SEGMENT(SEGMENT, DWORD,     dwAotProcessId,     0);
#endif

#define AOT_WINDOW_NAME            (TEXT("AOT"))
#define AOT_CLASS_NAME             (TEXT("AOTClass"))
#define AOT_INSTANCE_MUTEX         (TEXT(STRINGIZE(ARCH(AOTInstanceMutex))))

#define SYSCOMMAND(value)          (value & 0xFFF0)
#define AOT_MENU_ALWAYS_ON_TOP     (0x69)
#define AOT_MENU_EXIT              (0x69 + 1)
#define SC_AOT                     (SYSCOMMAND(AOT_MENU_ALWAYS_ON_TOP))

#define AOT_MENUTEXT_ALWAYS_ON_TOP (TEXT("Always On Top"))
#define AOT_MENUTEXT_EXIT          (TEXT("Exit"))

#define WM_AOTQUIT                 (WM_USER + 0x69)
#define WM_AOTHOOKINIT             (WM_USER + 0x69 + 1)
#define WM_AOTMOUSEHOOK            (WM_USER + 0x69 + 2)
#define WM_AOTCBTHOOK              (WM_USER + 0x69 + 3)
#define HANDLE_WM_AOTHOOKINIT(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (AOTHOOKTHREAD)(wParam), (DWORD)(lParam)), 0L)

#define HANDLE_WM_AOTMOUSEHOOK(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (DWORD)(wParam), MAKEPOINTS((lParam))), 0L)

#define HANDLE_WM_AOTCBTHOOK(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (INT)(DWORD)(wParam), (HWND)(LONG_PTR)(lParam)), 0L)

#define PADDING(x) __declspec(align(x)) unsigned __int8 reserved[x];

typedef struct _AOTUSERDATA {
  DWORD dwMouseHookThreadId;
  DWORD dwCbtHookThreadId;
  BOOL  fAlwaysOnTop;
#ifdef _M_X64
  PADDING(4)
#endif
} AOTUSERDATA, *LPAOTUSERDATA;

typedef enum _AOTHOOKTHREAD {
  AOTMOUSEHOOKTHREAD,
  AOTCBTHOOKTHREAD,
} AOTHOOKTHREAD;

EXTERN_C
AOTAPI
BOOL
__cdecl
SetCbtHook(
  HWND hWnd
);

EXTERN_C
AOTAPI
BOOL
__cdecl
UnsetCbtHook(
  VOID
);

static
CFORCEINLINE
BOOL
WINAPI
LogWindowText(
  HWND hWnd)
{
  if (hWnd)
  {
    TCHAR szText[256];
    GetClassName(hWnd, szText, ARRAYSIZE(szText));
    //GetWindowText(hWnd, szText, ARRAYSIZE(szText));
    wprintf(L"%s\n", szText);
    return TRUE;
  }
  return FALSE;
}

static CFORCEINLINE
BOOL CALLBACK
EnumWindowsCallback(
  HWND   hwnd,
  LPARAM lParam) {
  char className[256];

  // Get the class name of the window
  if (GetClassNameA(hwnd, className, sizeof(className))) {
    // Check if the class name matches the popup menu class (#32768)
    if (strcmp(className, "#32768") == 0) {
      *(HWND*)lParam = hwnd;
      return FALSE; // Stop enumeration
    }
  }

  return TRUE; // Continue enumeration
}

static CFORCEINLINE
HWND WINAPI
FindOpenMenuWindow(
  VOID) {
  HWND hWnd; // Reset the global handle
  EnumWindows(EnumWindowsCallback, (LPARAM)(LONG_PTR)&hWnd); // Enumerate all top-level windows
  return hWnd; // Return the found handle (if any)
}

static
CFORCEINLINE
BOOL
CALLBACK
OnNcCreate(
  HWND           hWnd,
  LPCREATESTRUCT lpCreateStruct)
{
  HANDLE hInstanceMutex = CreateMutex(NULL, TRUE, AOT_INSTANCE_MUTEX);
  if (!(hInstanceMutex && (GetLastError() == ERROR_ALREADY_EXISTS)))
  {
    LONG_PTR offset = 0;
    SetLastError(0);
    offset = SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lpCreateStruct->lpCreateParams);
    if (!(0 == offset && (NO_ERROR != GetLastError())))
    {
      return FORWARD_WM_NCCREATE(hWnd, lpCreateStruct, DefWindowProc);
    }
  }
  else
  {
    printf("AOT already running\n");
  }

  WaitForSingleObjectEx(hInstanceMutex, 1000, FALSE);
  ReleaseMutex(hInstanceMutex);
  CloseHandle(hInstanceMutex);
  return FALSE;
}

static
CFORCEINLINE
VOID
CALLBACK
OnAotInit(
  HWND          hWnd,
  AOTHOOKTHREAD dwHookThread,
  DWORD         dwAotHookThreadId)
{
  LPAOTUSERDATA lpAotUserData = (LPAOTUSERDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  switch (dwHookThread) {
  case AOTMOUSEHOOKTHREAD: {
    wprintf(L"AotMouseHookThreadInit %ld\n", dwAotHookThreadId);
    lpAotUserData->dwMouseHookThreadId = dwAotHookThreadId;
    break;
  }
  case AOTCBTHOOKTHREAD: {
    wprintf(L"AotCbtHookThreadInit %ld\n", dwAotHookThreadId);
    lpAotUserData->dwCbtHookThreadId = dwAotHookThreadId;
    break;
  }
  default:
    break;
  }
}

static
CFORCEINLINE
VOID
CALLBACK
OnAotMouseHook(
  HWND   hWnd,
  DWORD  mouseData,
  POINTS pts)
{
  UNREFERENCED_PARAMETER(hWnd);
  UNREFERENCED_PARAMETER(mouseData);

  POINT pt;
  HWND  hwndHovered;

  RtlSecureZeroMemory(&pt, sizeof(POINT));
  POINTSTOPOINT(pt, MAKEPOINTS(pts));

  RtlSecureZeroMemory(&hwndHovered, sizeof(HWND));
  hwndHovered = WindowFromPoint(pt);

  if(hwndHovered)
  {
    TCHAR   szClassName[256];
    //LRESULT result = SendMessage(hwndHovered, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
    RtlSecureZeroMemory(szClassName, sizeof(szClassName));
    if(GetClassName(hwndHovered, szClassName, sizeof(szClassName)))
    {
      TCHAR szWindowText[256];
      RtlSecureZeroMemory(szWindowText, sizeof(szWindowText));
      GetWindowText(hwndHovered, szWindowText, sizeof(szWindowText));
      //wprintf(L"%ld %ld HT: %ld: | %s %s\n", pt.x, pt.y, (LONG)result, szWindowText, szClassName);
    }
  }
}

static
CFORCEINLINE
LPTSTR
CALLBACK
GetCbtMessageName(
  int nCode)
{
  switch(nCode){
  case HCBT_ACTIVATE: return _T("HCBT_ACTIVATE");
  case HCBT_CREATEWND: return _T("HCBT_CREATEWND");
  case HCBT_DESTROYWND: return _T("HCBT_DESTROYWND");
  case HCBT_SYSCOMMAND: return _T("HCBT_SYSCOMMAND");
  default: return _T("");
  }
}

static
CFORCEINLINE
VOID
CALLBACK
OnAotCbtHook(
  HWND hWnd,
  int  nCode,
  HWND hWnd2)
{
  static int i = 0;
  UNREFERENCED_PARAMETER(hWnd);
  TCHAR szClassName[256];

  if(GetClassName(hWnd2, szClassName, ARRAYSIZE(szClassName)))
  {
    
    TCHAR szName[256];
    GetWindowText(hWnd2, szName, sizeof(szName));
    _tprintf(_T("%d AotCbtHook %s\n%s\n%s\n"), i++, GetCbtMessageName(nCode), szName, szClassName);
    if(GetKeyState(VK_LCONTROL) < 0)
    {
      POINT cursor = {0};
      GetCursorPos(&cursor);

      HMENU hPopupMenu = CreatePopupMenu();
      AppendMenu(
        hPopupMenu,
        MF_BYPOSITION | MF_STRING,
        AOT_MENU_ALWAYS_ON_TOP,
        AOT_MENUTEXT_ALWAYS_ON_TOP
      );

      AppendMenu(hPopupMenu, MF_BYPOSITION | MF_STRING, AOT_MENU_EXIT, AOT_MENUTEXT_EXIT);
      HWND  hWndForeground        = GetForegroundWindow();
      DWORD windowThreadProcessId = GetWindowThreadProcessId(hWndForeground, (LPDWORD)0);
      DWORD currentThreadId       = GetCurrentThreadId();
      LogWindowText(hWndForeground);
      AttachThreadInput(windowThreadProcessId, currentThreadId, TRUE);
      LockSetForegroundWindow(LSFW_UNLOCK);
      AllowSetForegroundWindow(ASFW_ANY);
      SetForegroundWindow(hWnd);
      LockSetForegroundWindow(LSFW_LOCK);
      ShowWindow(hWnd, SW_SHOW);
      AttachThreadInput(windowThreadProcessId, currentThreadId, FALSE);
      //AttachThreadInput(currentThreadId, windowThreadProcessId, TRUE);
      INT nMenuId = (INT)TrackPopupMenu(
        hPopupMenu,
        TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        cursor.x,
        cursor.y - 80,
        0,
        hWnd,
        NULL
      );
      LockSetForegroundWindow(LSFW_UNLOCK);
      if (nMenuId)
      {
        switch (nMenuId)
        {
        case AOT_MENU_ALWAYS_ON_TOP:
        {
          DWORD exStyle = (DWORD)GetWindowLong(hWnd2, GWL_EXSTYLE);
          (VOID)SetWindowPos(
            hWnd2,
            (exStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED
          );
          break;
        }
        case AOT_MENU_EXIT:
        {
          LPAOTUSERDATA lpAot = (LPAOTUSERDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
          PostThreadMessage(lpAot->dwMouseHookThreadId, WM_AOTQUIT, 0, 0);
          PostQuitMessage(0);
          break;
        }
        }
      }

      DestroyMenu(hPopupMenu);
    }
    //if(!_tccmp(szClassName, _T("32768")))
    //{
    //}
  }
}

static
CFORCEINLINE
VOID
CALLBACK
OnClose(
  HWND hWnd)
{
  UnsetCbtHook();
  DestroyWindow(hWnd);
}

static
CFORCEINLINE
VOID
CALLBACK
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
CFORCEINLINE
LRESULT
CALLBACK
LowLevelMouseProc(
  int    nCode,
  WPARAM wParam,
  LPARAM lParam)
{
  switch (wParam) {
  case WM_MOUSEMOVE:
    PostThreadMessage(
      GetCurrentThreadId(),
      WM_AOTMOUSEHOOK,
      (WPARAM)(DWORD)HIWORD(((LPMSLLHOOKSTRUCT)lParam)->mouseData),
      (LPARAM)(DWORD)POINTTOPOINTS(((LPMSLLHOOKSTRUCT)lParam)->pt)
    );
  default:
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }
}

#ifdef _WINDLL

static 
CFORCEINLINE
VOID
CALLBACK
AppendSystemMenu(HWND hWnd)
{
  HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
  if(hSysMenu)
  {
    MENUITEMINFO mii;
    RtlSecureZeroMemory(&mii, sizeof(MENUITEMINFO));
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask  = MIIM_ID;
    if (GetMenuItemInfo(hSysMenu, AOT_MENU_ALWAYS_ON_TOP, FALSE, &mii))
      return;
    AppendMenu(
      hSysMenu,
      MF_BYPOSITION | MF_STRING,
      AOT_MENU_ALWAYS_ON_TOP,
      AOT_MENUTEXT_ALWAYS_ON_TOP
    );
  }
}

static
CFORCEINLINE
VOID 
CALLBACK
Sentinel(LPVOID lpvHwnd) 
{
  UNREFERENCED_PARAMETER(lpvHwnd);
  HANDLE hProcessId = OpenProcess(SYNCHRONIZE, FALSE, ARCH(dwAotProcessId));
  if (hProcessId)
  {
    WaitForSingleObject(hProcessId, INFINITE);
  }
  SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
}

static 
CFORCEINLINE
LRESULT
CALLBACK
Subclassproc(
  HWND      hWnd,
  UINT      uMsg,
  WPARAM    wParam,
  LPARAM    lParam,
  UINT_PTR  uIdSubclass,
  DWORD_PTR dwRefData)
{
  UNREFERENCED_PARAMETER(dwRefData);
  switch (uMsg) {
  case WM_INITMENUPOPUP:
  {
    //CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Sentinel, NULL, 0, 0));
    
    AppendMenu(
      (HMENU)wParam,
      MF_BYPOSITION | MF_STRING,
      AOT_MENU_ALWAYS_ON_TOP,
      AOT_MENUTEXT_ALWAYS_ON_TOP
    );
  }
  case WM_NCDESTROY:
    RemoveWindowSubclass(hWnd, Subclassproc, uIdSubclass);
  default:
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
  }
}

static
CFORCEINLINE
LRESULT
CALLBACK
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
  //case HCBT_CREATEWND:
  case HCBT_ACTIVATE:
  {
    if (lParam)
    {
      //HWND hwndInsertAfter = ((LPCBT_CREATEWND)lParam)->hwndInsertAfter;
      HWND hWndActive = ((LPCBTACTIVATESTRUCT)lParam)->hWndActive;
      //if(hwndInsertAfter)
      if(hWndActive)
      {
        //if(hwndInsertAfter == GetActiveWindow())
        {
          TCHAR szClassName[256];
          GetClassName((HWND)wParam, szClassName, sizeof(szClassName));
          if(_tccmp(szClassName, _T("#32768")))
          {
            //(void)GetSystemMenu((HWND)wParam, FALSE);
            //SetWindowSubclass((HWND)wParam, Subclassproc, 0, (DWORD_PTR)GetCurrentProcessId());
            //(void)GetSystemMenu((HWND)wParam, FALSE);
            //HMENU hMenu = GetSystemMenu((HWND)wParam, FALSE);
            AppendSystemMenu((HWND)wParam);

            PostMessage(
              ARCH(hWndHookMarshaller),
              WM_AOTCBTHOOK,
              (WPARAM)(DWORD)nCode,
              (LPARAM)(LONG_PTR)wParam
            );
          }
        }
      }
    }
    break;
  }
  //case HCBT_ACTIVATE:
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
AOTAPI
BOOL
__cdecl
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
AOTAPI
BOOL
__cdecl
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
  //FreeLibraryAndExitThread(ARCH(hCbtHookModule), dwExitCode);
}


#endif

static
CFORCEINLINE
LRESULT
CALLBACK
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
CFORCEINLINE
DWORD
WINAPI
AotMouseHookThread(
  LPVOID lphWnd)
{
  HHOOK hook = SetWindowsHookEx(
    WH_MOUSE_LL,
    LowLevelMouseProc,
    (HINSTANCE)&__ImageBase,
    0
  );

  if (hook)
  {
    MSG msg;

    PostMessage(
      *(HWND*)lphWnd,
      WM_AOTHOOKINIT,
      (WPARAM)(DWORD)AOTMOUSEHOOKTHREAD,
      (LPARAM)(DWORD)GetCurrentThreadId()
    );

    RtlSecureZeroMemory(&msg, sizeof(MSG));
    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);

      switch (msg.message) {
      case WM_QUIT:
      case WM_AOTQUIT:
        UnhookWindowsHookEx(hook);
        SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
        break;
      case WM_AOTMOUSEHOOK:
        PostMessage(*(HWND*)lphWnd, msg.message, msg.wParam, msg.lParam);
      default:
        continue;
      }
      break;
    }
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

static
CFORCEINLINE
DWORD
WINAPI
AotCbtHookThread(
  LPVOID lphWnd)
{
  if (SetCbtHook(*(HWND*)lphWnd))
  {
    MSG msg;

    PostMessage(
      *(HWND*)lphWnd,
      WM_AOTHOOKINIT,
      (WPARAM)(DWORD)AOTCBTHOOKTHREAD,
      (LPARAM)(DWORD)GetCurrentThreadId()
    );

    RtlSecureZeroMemory(&msg, sizeof(MSG));
    while (GetMessage(&msg, NULL, 0, 0)) {
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
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

static CFORCEINLINE
BOOL WINAPI
HandlerRoutine(
  DWORD dwCtrlType)
{
  UNREFERENCED_PARAMETER(dwCtrlType);
  UnsetCbtHook();
  return TRUE;
}

static
CFORCEINLINE
BOOL
WINAPI
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
          0,
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

#ifdef _WINDLL

static
CFORCEINLINE
VOID
CALLBACK
Watchdog(
  VOID)
{
  HANDLE hProcessId = OpenProcess(SYNCHRONIZE, FALSE, ARCH(dwAotProcessId));
  if (hProcessId)
  {
    (void)WaitForSingleObject(hProcessId, INFINITE);
    //FreeLibrary(ARCH(hCbtHookModule));
    //FreeLibraryWhenCallbackReturns,
//Sleep(100);
    //FreeLibraryAndExitThread(ARCH(hCbtHookModule), EXIT_SUCCESS);
  }
  //FreeLibrary(GetModuleHandle(NULL));
  //FreeLibraryAndExitThread(GetModuleHandle(_T("aotx64.dll")), 0); //Update the refcount first
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
    if(!ARCH(dwAotProcessId)) ARCH(dwAotProcessId) = GetCurrentProcessId();
    ARCH(hCbtHookModule) = hModule;
    DisableThreadLibraryCalls(hModule);
  }
  default:
    SendNotifyMessage(HWND_BROADCAST, WM_NULL, 0, 0);
    break;
  }
  return TRUE;
}
#else

int
_tmain(
  void)
{
  AOTUSERDATA aot;
  WNDCLASSEX  wcx;
  HWND        hWnd;

  if(S_OK != SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE))
  {
    return EXIT_FAILURE;
  }

  RtlSecureZeroMemory(&wcx, sizeof(WNDCLASSEX));
  wcx.cbSize        = sizeof(WNDCLASSEX);
  wcx.hInstance     = (HINSTANCE)&__ImageBase;
  wcx.lpszClassName = AOT_CLASS_NAME;
  wcx.lpfnWndProc   = AotWndProc;

  if (!(ATOM)RegisterClassEx(&wcx))
  {
    return EXIT_FAILURE;
  }

  RtlSecureZeroMemory(&hWnd, sizeof(HWND));
  RtlSecureZeroMemory(&aot, sizeof(AOTUSERDATA));
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
  {
    return EXIT_FAILURE;
  }

  if (!CreateConsole(650, 150))
  {
    return EXIT_FAILURE;
  }

  if (CreateThread(0, 0, (LPTHREAD_START_ROUTINE)AotMouseHookThread, (LPVOID)&hWnd, 0, 0))
  {
    if (CreateThread(0, 0, (LPTHREAD_START_ROUTINE)AotCbtHookThread, (LPVOID)&hWnd, 0, 0))
    {
      MSG msg;
      RtlSecureZeroMemory(&msg, sizeof(MSG));
      while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        continue;
      }
      return EXIT_SUCCESS;
    }
  }
  return EXIT_FAILURE;
}

#endif
