#include <format>
#include <new>
#include <prism.h>
#include <richedit.h>
#include <string>
#include <tchar.h>
#include <windows.h>

static constexpr int IDC_RICHEDIT = 1001;
static constexpr int IDC_SAY = 1002;
using tstring = std::basic_string<TCHAR>;
static HINSTANCE g_hInstance = nullptr;
static HMODULE g_hRichDll = nullptr;
static HWND g_hEdit = nullptr;
static HWND g_hButton = nullptr;
static WNDPROC g_EditOrigProc = nullptr;

static void TrimRightWhitespace(tstring &s) {
  while (!s.empty()) {
    const TCHAR ch = s.back();
    if (ch == _T('\r') || ch == _T('\n') || ch == _T(' ') || ch == _T('\t'))
      s.pop_back();
    else
      break;
  }
}

static tstring GetWin32ErrorMessage(DWORD errorCode) {
  LPTSTR buffer = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
  const DWORD len =
      FormatMessage(flags, nullptr, errorCode, langId,
                    reinterpret_cast<LPTSTR>(&buffer), 0, nullptr);
  if (len == 0 || buffer == nullptr) {
    return std::format(_T("Unknown error (code {})."), errorCode);
  }
  tstring msg(buffer, buffer + len);
  LocalFree(buffer);
  TrimRightWhitespace(msg);
  if (msg.empty())
    msg = std::format(_T("Unknown error (code {})."), errorCode);
  return msg;
}

static void ShowProblem(HWND owner, LPCTSTR title, LPCTSTR description) {
  if (title == nullptr)
    title = _T("Error");
  if (description == nullptr)
    description = _T("An unspecified problem occurred.");
  MessageBoxEx(owner, description, title, MB_ICONERROR | MB_OK, 0);
}

static void ShowWin32Error(HWND owner, LPCTSTR whatFailed) {
  const DWORD err = GetLastError();
  const tstring sysMsg = GetWin32ErrorMessage(err);
  const tstring what = (whatFailed != nullptr)
                           ? tstring(whatFailed)
                           : tstring(_T("A Win32 API call"));
  const tstring text =
      std::format(_T("{} failed.\n\nWin32 error {}: {}"), what, err, sysMsg);
  MessageBoxEx(owner, text.c_str(), _T("Win32 Error"), MB_ICONERROR | MB_OK, 0);
}

static void LayoutControls(HWND hwnd) {
  RECT rc{};
  if (!GetClientRect(hwnd, &rc)) {
    ShowWin32Error(hwnd, _T("GetClientRect"));
    return;
  }
  const int padding = 12;
  const int gap = 10;
  const int btnW = 90;
  const int h = 28;
  const int width = (rc.right - rc.left);
  const int height = (rc.bottom - rc.top);
  const int y = padding;
  const int xEdit = padding;
  const int xBtn = width - padding - btnW;
  const int editW = (xBtn - gap) - xEdit;
  if (editW < 40 || height < (padding + h + padding)) {
    // Non-fatal: window is just too small.
    return;
  }
  if (g_hEdit) {
    if (!MoveWindow(g_hEdit, xEdit, y, editW, h, TRUE))
      ShowWin32Error(hwnd, _T("MoveWindow(edit)"));
  }
  if (g_hButton) {
    if (!MoveWindow(g_hButton, xBtn, y, btnW, h, TRUE))
      ShowWin32Error(hwnd, _T("MoveWindow(button)"));
  }
}

static void RemoveNewlinesFromEdit(HWND hEdit) {
  if (!hEdit)
    return;
  const int len = GetWindowTextLength(hEdit);
  if (len < 0) {
    ShowProblem(GetParent(hEdit), _T("Error"),
                _T("GetWindowTextLength returned a negative length."));
    return;
  }
  if (len == 0)
    return;
  TCHAR *buf = new (std::nothrow) TCHAR[static_cast<size_t>(len) + 1];
  if (!buf) {
    ShowProblem(GetParent(hEdit), _T("Out of memory"),
                _T("Failed to allocate memory to sanitize pasted text."));
    return;
  }
  const int got = GetWindowText(hEdit, buf, len + 1);
  if (got <= 0) {
    delete[] buf;
    ShowWin32Error(GetParent(hEdit), _T("GetWindowText"));
    return;
  }
  DWORD selStart = 0, selEnd = 0;
  SendMessage(hEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart),
              reinterpret_cast<LPARAM>(&selEnd));
  int write = 0;
  for (int read = 0; read < got; ++read) {
    const TCHAR ch = buf[read];
    if (ch == _T('\r') || ch == _T('\n'))
      continue;
    buf[write++] = ch;
  }
  buf[write] = _T('\0');
  if (write != got) {
    if (!SetWindowText(hEdit, buf)) {
      delete[] buf;
      ShowWin32Error(GetParent(hEdit), _T("SetWindowText"));
      return;
    }
    if (selStart > static_cast<DWORD>(write))
      selStart = static_cast<DWORD>(write);
    if (selEnd > static_cast<DWORD>(write))
      selEnd = static_cast<DWORD>(write);
    SendMessage(hEdit, EM_SETSEL, selStart, selEnd);
  }
  delete[] buf;
}

static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam) {
  switch (msg) {
  case WM_KEYDOWN:
    if (wParam == VK_RETURN) {
      if (HWND parent = GetParent(hwnd)) {
        if (g_hButton)
          SendMessage(g_hButton, BM_CLICK, 0, 0);
        else
          ShowProblem(parent, _T("Error"), _T("Say button handle is missing."));
      }
      return 0;
    }
    break;
  case WM_CHAR:
    if (wParam == _T('\r') || wParam == _T('\n'))
      return 0;
    break;
  case WM_PASTE: {
    const LRESULT r = CallWindowProc(g_EditOrigProc, hwnd, msg, wParam, lParam);
    RemoveNewlinesFromEdit(hwnd);
    return r;
  }
  case WM_GETDLGCODE: {
    LRESULT code = CallWindowProc(g_EditOrigProc, hwnd, msg, wParam, lParam);
    if (lParam) {
      const MSG *pMsg = reinterpret_cast<const MSG *>(lParam);
      if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB) {
        return code & ~(DLGC_WANTTAB | DLGC_WANTALLKEYS);
      }
    }
    return code & ~DLGC_WANTTAB;
  }
  }
  return CallWindowProc(g_EditOrigProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  switch (msg) {
  case WM_CREATE: {
    g_hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS, // RichEdit 4.1+ (from msftedit.dll)
        _T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RICHEDIT)),
        g_hInstance, nullptr);
    if (!g_hEdit) {
      ShowWin32Error(hwnd, _T("CreateWindowEx(MSFTEDIT_CLASS)"));
      return -1; // fail creation
    }
    SetLastError(0);
    g_EditOrigProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
        g_hEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditSubclassProc)));
    if (!g_EditOrigProc) {
      const DWORD e = GetLastError();
      if (e != 0) {
        ShowWin32Error(hwnd, _T("SetWindowLongPtr(subclass RichEdit)"));
        return -1;
      }
    }
    g_hButton = CreateWindowEx(
        0, _T("BUTTON"), _T("&Speak"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SAY)), g_hInstance,
        nullptr);
    if (!g_hButton) {
      ShowWin32Error(hwnd, _T("CreateWindowEx(BUTTON)"));
      return -1;
    }
    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    if (!hFont) {
      ShowWin32Error(hwnd, _T("GetStockObject(DEFAULT_GUI_FONT)"));
    } else {
      SendMessage(g_hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
      SendMessage(g_hButton, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    }
    LayoutControls(hwnd);
    return 0;
  }
  case WM_SIZE:
    LayoutControls(hwnd);
    return 0;
  case WM_COMMAND: {
    const int id = LOWORD(wParam);
    const int code = HIWORD(wParam);
    if (id == IDC_SAY && code == BN_CLICKED) {
      auto *backend = reinterpret_cast<PrismBackend *>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
      if (!backend) {
        ShowProblem(hwnd, _T("Prism error"),
                    _T("Backend pointer is null (window user data not set)."));
        return 0;
      }
      if (!g_hEdit) {
        ShowProblem(hwnd, _T("Error"), _T("Edit control handle is missing."));
        return 0;
      }
      GETTEXTLENGTHEX gtl{};
      gtl.flags = GTL_DEFAULT;
      gtl.codepage = 1200; // UTF-16
      LRESULT cch = SendMessage(g_hEdit, EM_GETTEXTLENGTHEX,
                                reinterpret_cast<WPARAM>(&gtl), 0);
      if (cch < 0) {
        ShowProblem(hwnd, _T("Error"),
                    _T("Failed to query RichEdit text length."));
        return 0;
      }
      if (cch == 0)
        return 0;
      std::wstring w;
      w.resize(static_cast<size_t>(cch) + 1);
      GETTEXTEX gte{};
      gte.cb = static_cast<DWORD>(w.size() * sizeof(wchar_t));
      gte.flags = GT_DEFAULT;
      gte.codepage = 1200; // UTF-16 out
      LRESULT copied =
          SendMessage(g_hEdit, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&gte),
                      reinterpret_cast<LPARAM>(w.data()));
      if (copied < 0) {
        ShowProblem(hwnd, _T("Error"), _T("Failed to read RichEdit text."));
        return 0;
      }
      w.resize(static_cast<size_t>(copied));
      int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.c_str(),
                                       -1, nullptr, 0, nullptr, nullptr);
      if (needed == 0) {
        ShowWin32Error(hwnd, _T("WideCharToMultiByte(size query)"));
        return 0;
      }
      std::string text;
      text.resize(static_cast<size_t>(needed)); // includes NUL
      int written =
          WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.c_str(), -1,
                              text.data(), needed, nullptr, nullptr);
      if (written == 0) {
        ShowWin32Error(hwnd, _T("WideCharToMultiByte(convert)"));
        return 0;
      }
      if (!text.empty() && text.back() == '\0')
        text.pop_back();
      const char *ctext = text.c_str();
      if (const auto res = prism_backend_speak(backend, ctext, true);
          res != PRISM_OK) {
        const char *err8 = prism_error_string(res);
        if (!err8)
          err8 = "unknown";
        int wneeded = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, err8,
                                          -1, nullptr, 0);
        if (wneeded == 0) {
          ShowWin32Error(
              hwnd,
              _T("MultiByteToWideChar(size query for prism_error_string)"));
          return 0;
        }
        std::wstring werr;
        werr.resize(static_cast<size_t>(wneeded));
        int wwritten = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, err8,
                                           -1, werr.data(), wneeded);
        if (wwritten == 0) {
          ShowWin32Error(
              hwnd, _T("MultiByteToWideChar(convert for prism_error_string)"));
          return 0;
        }
        if (!werr.empty() && werr.back() == L'\0')
          werr.pop_back();
#ifdef UNICODE
        const tstring msg =
            std::format(_T("Could not speak text to UIA: {}"), werr);
#else
        const tstring msg =
            std::format(_T("Could not speak text to UIA: {}"), err8);
#endif
        ShowProblem(hwnd, _T("Prism error"), msg.c_str());
        return 0;
      }
      return 0;
    }
    return 0;
  }
  case WM_DESTROY: {
    if (g_hEdit && g_EditOrigProc) {
      SetLastError(0);
      const LONG_PTR prev = SetWindowLongPtr(
          g_hEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_EditOrigProc));
      if (prev == 0) {
        const DWORD e = GetLastError();
        if (e != 0)
          ShowWin32Error(hwnd, _T("SetWindowLongPtr(restore RichEdit proc)"));
      }
    }
    g_hEdit = nullptr;
    g_hButton = nullptr;
    g_EditOrigProc = nullptr;
    PostQuitMessage(0);
    return 0;
  }
  case WM_SETFOCUS: {
    if (g_hEdit)
      SetFocus(g_hEdit);
    return 0;
  }
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow) {
  g_hInstance = hInstance;
  g_hRichDll = LoadLibraryEx(_T("Msftedit.dll"), nullptr, 0);
  if (!g_hRichDll) {
    ShowWin32Error(nullptr, _T("LoadLibraryEx(Msftedit.dll)"));
    return 1;
  }
  const TCHAR kClassName[] = _T("RichSayWindowClass");
  WNDCLASSEX wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = MainWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = kClassName;
  wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
  const ATOM a = RegisterClassEx(&wc);
  if (a == 0) {
    ShowWin32Error(nullptr, _T("RegisterClassEx"));
    FreeLibrary(g_hRichDll);
    g_hRichDll = nullptr;
    return 1;
  }
  HWND hwnd = CreateWindowEx(0, kClassName, _T("Prism UIA demo"),
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             560, 140, nullptr, nullptr, hInstance, nullptr);
  if (!hwnd) {
    ShowWin32Error(nullptr, _T("CreateWindowEx(main window)"));
    UnregisterClass(kClassName, hInstance);
    FreeLibrary(g_hRichDll);
    g_hRichDll = nullptr;
    return 1;
  }
  ShowWindow(hwnd, nCmdShow);
  if (!UpdateWindow(hwnd)) {
    ShowWin32Error(hwnd, _T("UpdateWindow"));
    DestroyWindow(hwnd); // triggers cleanup
    UnregisterClass(kClassName, hInstance);
    FreeLibrary(g_hRichDll);
    g_hRichDll = nullptr;
    return 1;
  }
  auto cfg = prism_config_init();
  cfg.hwnd = hwnd;
  PrismContext *ctx = prism_init(&cfg);
  PrismBackend *backend = prism_registry_create(ctx, PRISM_BACKEND_UIA);
  if (!backend) {
    MessageBoxEx(hwnd, _T("Could not create UIA backend!"), _T("Prism error"),
                 MB_ICONERROR | MB_OK, 0);
    prism_shutdown(ctx);
    DestroyWindow(hwnd); // triggers cleanup
    UnregisterClass(kClassName, hInstance);
    FreeLibrary(g_hRichDll);
    g_hRichDll = nullptr;
    return 1;
  }
  if (const auto res = prism_backend_initialize(backend); res != PRISM_OK) {
    const char *err8 = prism_error_string(res);
    if (!err8)
      err8 = "unknown";
    int wneeded = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, err8, -1,
                                      nullptr, 0);
    if (wneeded == 0) {
      ShowWin32Error(
          hwnd, _T("MultiByteToWideChar(size query for prism_error_string)"));
      prism_backend_free(backend);
      prism_shutdown(ctx);
      DestroyWindow(hwnd); // triggers cleanup
      UnregisterClass(kClassName, hInstance);
      FreeLibrary(g_hRichDll);
      g_hRichDll = nullptr;
      return 1;
    }
    std::wstring werr;
    werr.resize(static_cast<size_t>(wneeded));
    int wwritten = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, err8, -1,
                                       werr.data(), wneeded);
    if (wwritten == 0) {
      ShowWin32Error(hwnd,
                     _T("MultiByteToWideChar(convert for prism_error_string)"));
      prism_backend_free(backend);
      prism_shutdown(ctx);
      DestroyWindow(hwnd); // triggers cleanup
      UnregisterClass(kClassName, hInstance);
      FreeLibrary(g_hRichDll);
      g_hRichDll = nullptr;
      return 1;
    }
    if (!werr.empty() && werr.back() == L'\0')
      werr.pop_back();
    const tstring msg = std::format(_T("Could not initialize UIA: {}"), werr);
    ShowProblem(hwnd, _T("Prism error"), msg.c_str());
    prism_backend_free(backend);
    prism_shutdown(ctx);
    DestroyWindow(hwnd); // triggers cleanup
    UnregisterClass(kClassName, hInstance);
    FreeLibrary(g_hRichDll);
    g_hRichDll = nullptr;
    return 1;
  }
  SetLastError(0);
  SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(backend));
  if (GetLastError() != 0) {
    ShowWin32Error(hwnd, _T("SetWindowLongPtr"));
    prism_backend_free(backend);
    prism_shutdown(ctx);
    DestroyWindow(hwnd); // triggers cleanup
    UnregisterClass(kClassName, hInstance);
    FreeLibrary(g_hRichDll);
    g_hRichDll = nullptr;
    return 1;
  }
  MSG msg{};
  for (;;) {
    const BOOL gm = GetMessage(&msg, nullptr, 0, 0);
    if (gm == 0)
      break; // WM_QUIT
    if (gm == -1) {
      ShowWin32Error(hwnd, _T("GetMessage"));
      DestroyWindow(hwnd);
      break;
    }
    if (!IsDialogMessage(hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  UnregisterClass(kClassName, hInstance);
  if (g_hRichDll) {
    if (!FreeLibrary(g_hRichDll))
      ShowWin32Error(nullptr, _T("FreeLibrary(Msftedit.dll)"));
    g_hRichDll = nullptr;
  }
  prism_backend_free(backend);
  prism_shutdown(ctx);
  return static_cast<int>(msg.wParam);
}
