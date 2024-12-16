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

EXTERN_C
IMAGE_DOS_HEADER __ImageBase;

#ifdef _WINDLL
#define AOTAPI        __declspec(dllexport)
#else
#define AOTAPI        __declspec(dllimport)
#endif

#ifdef _WINDLL
#pragma data_seg(".shared")
static HHOOK     hCbtHook           = NULL;
static HINSTANCE hCbtHookModule     = NULL;
static HWND      hWndHookMarshaller = NULL;
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")
#endif

#define AOT_WINDOW_NAME            (TEXT("AOT"))
#define AOT_CLASS_NAME             (TEXT("AOTClass"))
#define AOT_INSTANCE_MUTEX         (TEXT("AOTInstanceMutex"))

#define AOT_MENU_ALWAYS_ON_TOP     (0x69)
#define AOT_MENU_EXIT              (0x69 + 1)

#define AOT_MENUTEXT_ALWAYS_ON_TOP (TEXT("Always On Top"))
#define AOT_MENUTEXT_EXIT          (TEXT("Exit"))

#define WM_AOTEXIT                 (WM_USER + 0x69)
#define WM_AOTHOOKINIT             (WM_USER + 0x69 + 1)
#define WM_AOTMOUSEHOOK            (WM_USER + 0x69 + 2)
#define WM_AOTCBTHOOK              (WM_USER + 0x69 + 3)

#define HANDLE_WM_AOTHOOKINIT(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (AOTHOOKTHREAD)(wParam), (DWORD)(lParam)), 0L)

#define HANDLE_WM_AOTMOUSEHOOK(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (DWORD)(wParam), MAKEPOINTS((lParam))), 0L)

#define HANDLE_WM_AOTCBTHOOK(hWnd, wParam, lParam, fn) \
        ((fn)((hWnd), (INT)(DWORD)(wParam)), 0L)

#define PADDING(x) __declspec(align(x)) unsigned __int8 reserved[x];

typedef struct _AOTUSERDATA {
  DWORD dwAotMouseHookThreadId;
  DWORD dwAotCbtHookThreadId;
  BOOL  fAlwaysOnTop;
#ifdef _M_X64
  PADDING(4)
#endif
} AOTUSERDATA, *LPAOTUSERDATA;

typedef enum _AOTHOOKTHREAD {
  AOTMOUSEHOOKTHREAD,
  AOTCBTHOOKTHREAD,
} AOTHOOKTHREAD;

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
    lpAotUserData->dwAotMouseHookThreadId = dwAotHookThreadId;
    break;
  }
  case AOTCBTHOOKTHREAD: {
    wprintf(L"AotCbtHookThreadInit %ld\n", dwAotHookThreadId);
    lpAotUserData->dwAotCbtHookThreadId = dwAotHookThreadId;
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
  HWND  hWndHit = NULL;
  POINT pt      = {0};
  POINTSTOPOINT(pt, MAKEPOINTS(pts));
  hWndHit = WindowFromPoint(pt);

  if (hWndHit && GetKeyState(VK_LCONTROL) < 0)
  {
    //HMENU hMenu = (HMENU)FindOpenMenuWindow();
    //if (hMenu)
    //{
    //  wprintf(L"Menu: %ld\n", (LONG)hMenu);
    //  return;
    //}
    TCHAR szText[256];
    GetWindowText(hWndHit, szText, ARRAYSIZE(szText));
    wprintf(
    //  L"AOT: %llu, %ld %ld %ld %s\n",
      L"AOT: %ld %s\n",
      //(LONG_PTR)hWndHit,
      //pt.x,
      //pt.y,
      mouseData,
      szText
    );

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
    DWORD CONST_SW_SHOW         = 5;
    LogWindowText(hWndForeground);
    AttachThreadInput(windowThreadProcessId, currentThreadId, TRUE);
    BringWindowToTop(hWnd);
    ShowWindow(hWnd, CONST_SW_SHOW);
    AttachThreadInput(windowThreadProcessId, currentThreadId, FALSE);
    INT nMenuId = (INT)TrackPopupMenu(
      hPopupMenu,
      TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
      pt.x,
      pt.y,
      0,
      hWnd,
      NULL
    );
    SetForegroundWindow(hWndForeground);
    if(nMenuId)
    {
      switch (nMenuId)
      {
      case AOT_MENU_ALWAYS_ON_TOP:
      {
        DWORD exStyle = (DWORD)GetWindowLong(hWndHit, GWL_EXSTYLE);
        (VOID)SetWindowPos(
          hWndHit,
          (exStyle & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST,
          0,
          0,
          0,
          0,
          SWP_NOMOVE | SWP_NOSIZE
        );
        break;
      }
      case AOT_MENU_EXIT:
      {
        LPAOTUSERDATA lpAot = (LPAOTUSERDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        PostThreadMessage(lpAot->dwAotMouseHookThreadId, WM_AOTEXIT, 0, 0);
        PostQuitMessage(0);
        break;
      }
      }
    }
    DestroyMenu(hPopupMenu);
  }
}

static
CFORCEINLINE
VOID
CALLBACK
OnAotCbtHook(
  HWND hWnd,
  int  nCode)
{
  static int i = 0;
  UNREFERENCED_PARAMETER(hWnd);
  if (nCode == HCBT_SETFOCUS) {

    wprintf(L"%d AotCbtHook HCBT_SETFOCUS\n", i++);
  }
  else if (nCode == HCBT_ACTIVATE) {

    wprintf(L"%d AotCbtHook HCBT_ACTIVATE\n", i++);
  }
  else if (nCode == HCBT_CREATEWND) {

    wprintf(L"%d AotCbtHook HCBT_CREATEWND\n", i++);
  }
  else {
    wprintf(L"%d AotCbtHook %d\n", i++, nCode);
  }
}

static
CFORCEINLINE
VOID
CALLBACK
OnDestroy(
  HWND hWnd)
{
  LPAOTUSERDATA lpAotUserData = (LPAOTUSERDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  if (lpAotUserData)
  {
    if (lpAotUserData->dwAotMouseHookThreadId)
    {
      PostThreadMessage(lpAotUserData->dwAotMouseHookThreadId, WM_AOTEXIT, 0, 0);
    }
    if (lpAotUserData->dwAotCbtHookThreadId)
    {
      PostThreadMessage(lpAotUserData->dwAotCbtHookThreadId, WM_AOTEXIT, 0, 0);
    }
    PostQuitMessage(0);
  }
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
  if (WM_RBUTTONUP == wParam) {
    PostThreadMessage(
      GetCurrentThreadId(),
      WM_AOTMOUSEHOOK,
      (WPARAM) (DWORD) HIWORD(((LPMSLLHOOKSTRUCT)lParam)->mouseData),
      (LPARAM) (DWORD) POINTTOPOINTS(((LPMSLLHOOKSTRUCT)lParam)->pt)
    );
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

#ifdef _WINDLL

static
CFORCEINLINE
LRESULT
CALLBACK
CBTProc(
  int    nCode,
  WPARAM wParam,
  LPARAM lParam)
{
  switch (nCode)
  {
  case HCBT_ACTIVATE:
  case HCBT_CREATEWND:
  case HCBT_DESTROYWND:
  case HCBT_MINMAX:
  case HCBT_MOVESIZE:
  case HCBT_SETFOCUS:
  case HCBT_CLICKSKIPPED:
  case HCBT_KEYSKIPPED:
  case HCBT_QS:
  case HCBT_SYSCOMMAND:
  {
    PostMessage(
      hWndHookMarshaller,
      WM_AOTCBTHOOK,
      (WPARAM)(DWORD)nCode,
      0
    );
    break;
  }
  }

  return CallNextHookEx(hCbtHook, nCode, wParam, lParam);
}
EXTERN_C
AOTAPI
BOOL
__cdecl
SetCbtHook(
  HWND hWnd)
{
  HHOOK hook = SetWindowsHookEx(WH_CBT, CBTProc, hCbtHookModule, 0);
  if (hook)
  {
    hCbtHook           = hook;
    hWndHookMarshaller = hWnd;
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
  if (hCbtHook)
  {
    UnhookWindowsHookEx(hCbtHook);
    hWndHookMarshaller = NULL;
    return TRUE;
  }
  return FALSE;
}
#else
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
#endif

static
FORCEINLINE
LRESULT
CALLBACK
WndProc(
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
    MSG msg = {0};

    PostMessage(
      *(HWND*)lphWnd,
      WM_AOTHOOKINIT,
      (WPARAM) (DWORD) AOTMOUSEHOOKTHREAD,
      (LPARAM) (DWORD) GetCurrentThreadId()
    );

    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (WM_AOTEXIT == msg.message) break;

      switch (msg.message) {
      case WM_AOTMOUSEHOOK:
        PostMessage(*(HWND*)lphWnd, msg.message, msg.wParam, msg.lParam);
      default:
        continue;
      }
    }

    UnhookWindowsHookEx(hook);
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
    MSG msg = {0};

    PostMessage(
      *(HWND*)lphWnd,
      WM_AOTHOOKINIT,
      (WPARAM) (DWORD) AOTCBTHOOKTHREAD,
      (LPARAM) (DWORD) GetCurrentThreadId()
    );

    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (WM_AOTEXIT == msg.message) break;
    }
    //UnsetCbtHook();
    return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
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

BOOL
APIENTRY
DllMain(
  HMODULE hModule,
  DWORD   ul_reason_for_call,
  LPVOID  lpReserved)
{
  UNREFERENCED_PARAMETER(hModule);
  UNREFERENCED_PARAMETER(lpReserved);
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
  {
    hCbtHookModule = hModule;
    DisableThreadLibraryCalls(hModule);
    break;
  }
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
  case DLL_PROCESS_DETACH:
    break;
  }
  return TRUE;
}
#else

int
_tmain(
  void)
{
  AOTUSERDATA aot  = {0};
  WNDCLASSEX  wcx  = { sizeof(WNDCLASSEX) };
  HWND        hWnd = NULL;

  if(S_OK != SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE))
  {
    return EXIT_FAILURE;
  }

  wcx.lpfnWndProc   = WndProc;
  wcx.hInstance     = (HINSTANCE)&__ImageBase;
  wcx.lpszClassName = AOT_CLASS_NAME;

  if (!(ATOM)RegisterClassEx(&wcx))
  {
    return EXIT_FAILURE;
  }

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
      for (;;)
      {
        MSG msg = {0};
        if (GetMessage(&msg, NULL, 0U, 0U)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
          continue;
        }
        break;
      }
      return EXIT_SUCCESS;
    }
  }
}
#endif
