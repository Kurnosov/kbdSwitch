#define OEMRESOURCE
#include <windows.h>
#include "kbdSwitch.h"
#include "../res/resource.h"


static UINT WM_TASKBAR_CREATED;

HINSTANCE   g_instance;
HHOOK		g_hook;
HCURSOR     g_hc_ibeam;
UINT_PTR    g_timer = NULL;
DWORD       g_layout = 0;

static HWND g_hwndMainWindow;
static HWND g_hwndForMenu;
static bool g_isShifted = false;

void CALLBACK UpdateTimer(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    int layout = (int) GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), NULL)) & 0xFFFF;
    if (g_layout != layout)
    {
        HCURSOR hc_new = LoadCursor(g_instance, MAKEINTRESOURCE(layout));

        if (hc_new)
        {
            SetSystemCursor(hc_new, OCR_IBEAM);
        }
        else
        {
            SetSystemCursor(CopyCursor(g_hc_ibeam), OCR_IBEAM);
        }
    }
}

LRESULT CALLBACK LowLevelKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT *ks = (KBDLLHOOKSTRUCT*)lParam;
        if (ks->vkCode == VK_LSHIFT) {
            if (wParam == WM_KEYDOWN) {
                g_isShifted = true;
            }
            if (g_isShifted && wParam == WM_KEYUP) {
                g_isShifted = false;
            }
        }
        
        if (ks->vkCode==VK_CAPITAL && !g_isShifted)
        {
            if (wParam == WM_KEYDOWN)
            {
                int layout = (int) GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), NULL)) & 0xFFFF;
                switch (layout) {
                case 0x409:
                    ActivateKeyboardLayout((HKL)0x419, 0);
                    PostMessage(GetForegroundWindow(), WM_INPUTLANGCHANGEREQUEST, 1, 0x419);
                    break;
                case 0x419:
                default:
                    ActivateKeyboardLayout((HKL)0x409, 0);
                    PostMessage(GetForegroundWindow(), WM_INPUTLANGCHANGEREQUEST, 1, 0x409);
                }

                //if ((GetKeyState(VK_CAPITAL) & 0x0001) !=0 && layout == 0x409) {
                //    keybd_event(VK_CAPITAL, 0, 0, 0);
                //    keybd_event(VK_CAPITAL, 0, VK_UP, 0);
                //}

                //SendMessage(GetForegroundWindow(), WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_FORWARD, 0);
            }
            else if (wParam == WM_KEYUP)
            {

            }

            return -1;
        }
        if (ks->vkCode == VK_CAPITAL && g_isShifted) {
            if (wParam == WM_KEYDOWN) {
                keybd_event(VK_CAPITAL, 0, KEYEVENTF_EXTENDEDKEY, 0);
                keybd_event(VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0);
            } else if (wParam == WM_KEYUP) {
            }
            return -1;
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

HICON GetWindowIcon(HWND hwnd) {
	HICON icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0)) return icon;
	if (icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM)) return icon;
	if (icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON)) return icon;
	return LoadIcon(NULL, IDI_WINLOGO);
}

static void AddToTray() {
	NOTIFYICONDATA nid;
	nid.cbSize           = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd             = g_hwndMainWindow;
	nid.uID              = 0;
	nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYCMD;
	nid.hIcon            = GetWindowIcon(g_hwndMainWindow);
	GetWindowText(g_hwndMainWindow, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	nid.uVersion         = NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_ADD, &nid);
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

static void MinimizeWindowToTray() {
	// Don't minimize MDI child windows
	if ((UINT)GetWindowLongPtr(g_hwndMainWindow, GWL_EXSTYLE) & WS_EX_MDICHILD) return;

	ShowWindow(g_hwndMainWindow, SW_FORCEMINIMIZE);
	ShowWindow(g_hwndMainWindow, SW_HIDE);
	AddToTray();
	// Workaround for some programs that reshow themselves
	Sleep(100);
	ShowWindow(g_hwndMainWindow, SW_HIDE);
}

static void RemoveFromTray() {
	NOTIFYICONDATA nid;
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd   = g_hwndMainWindow;
	nid.uID    = (UINT)0;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

void RefreshWindowInTray(HWND hwnd) {
	NOTIFYICONDATA nid;
	nid.cbSize = NOTIFYICONDATA_V2_SIZE;
	nid.hWnd   = g_hwndMainWindow;
	nid.uID    = (UINT)0;
	nid.uFlags = NIF_TIP;
	GetWindowText(hwnd, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void ExecuteMenu() {
	HMENU hMenu;
	POINT point;

	hMenu = CreatePopupMenu();
	if (!hMenu) {
		MessageBox(NULL, "Error creating menu.", "kbdSwitch", MB_OK | MB_ICONERROR);
		return;
	}
	AppendMenu(hMenu, MF_STRING, IDM_ABOUT,   "About kbdSwitch");
	AppendMenu(hMenu, MF_STRING, IDM_EXIT,    "Exit kbdSwitch");
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); //--------------

	GetCursorPos(&point);
	SetForegroundWindow(g_hwndMainWindow);

	TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN, point.x, point.y, 0, g_hwndMainWindow, NULL);

	PostMessage(g_hwndMainWindow, WM_USER, 0, 0);
	DestroyMenu(hMenu);
}

BOOL CALLBACK AboutDlgProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch (Msg) {
		case WM_CLOSE:
			PostMessage(hWnd, WM_COMMAND, IDCANCEL, 0);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(hWnd, TRUE);
					break;
				case IDCANCEL:
					EndDialog(hWnd, FALSE);
					break;
			}
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

LRESULT CALLBACK HookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDM_ABOUT:
					DialogBox(g_instance, MAKEINTRESOURCE(IDD_ABOUT), g_hwndMainWindow, (DLGPROC)AboutDlgProc);
					break;
				case IDM_EXIT:
					SendMessage(g_hwndMainWindow, WM_DESTROY, 0, 0);
					break;
			}
			break;
        case WM_FOCUSCHNG:
            break;
		case WM_REFRTRAY:
			RefreshWindowInTray((HWND)lParam);
			break;
		case WM_TRAYCMD:
			switch ((UINT)lParam) {
				case NIN_SELECT:
					//RestoreWindowFromTray(_hwndItems[wParam]);
					break;
				case WM_CONTEXTMENU:
					g_hwndForMenu = g_hwndMainWindow;
					ExecuteMenu();
					break;
				case WM_MOUSEMOVE:
					RefreshWindowInTray(g_hwndMainWindow);
					break;
			}
			break;
		case WM_DESTROY:
            RemoveFromTray();
			PostQuitMessage(0);
            UnhookWindowsHookEx(g_hook);
            SetSystemCursor(CopyCursor(g_hc_ibeam), OCR_IBEAM);
            DestroyCursor(g_hc_ibeam);
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}


int Main() {
	WNDCLASS wc;
	MSG msg;
    
    HANDLE mutex = CreateMutex(NULL, FALSE, "LangCursor");
    if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED) return 1;

    g_hc_ibeam = CopyCursor(LoadCursor(NULL, IDC_IBEAM));
    if (!g_hc_ibeam) return 1;

    g_instance = GetModuleHandle(NULL);
    

	g_hwndMainWindow = FindWindow(NAME, NAME);
	if (g_hwndMainWindow) {
		MessageBox(NULL, "kbdSwitch is already running.", "kbdSwitch", MB_OK | MB_ICONINFORMATION);
		return 0;
	}
    
	wc.style         = 0;
	wc.lpfnWndProc   = HookWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = g_instance;
    wc.hIcon         = LoadIcon(g_instance,MAKEINTRESOURCE(IDI_KBDSWITCH));
	wc.hCursor       = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = NAME;
	if (!RegisterClass(&wc)) {
		MessageBox(NULL, "Error creating window class", "kbdSwitch", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (!(g_hwndMainWindow = CreateWindow(NAME, NAME, WS_OVERLAPPED, 0, 0, 0, 0, (HWND)NULL, (HMENU)NULL, (HINSTANCE)g_instance, (LPVOID)NULL))) {
		MessageBox(NULL, "Error creating window", "kbdSwitch", MB_OK | MB_ICONERROR);
		return 0;
	}
    
    WM_TASKBAR_CREATED = RegisterWindowMessage("TaskbarCreated");
    
    g_timer = SetTimer(g_hwndMainWindow, g_timer, 200, UpdateTimer);
    if (!g_timer) return 1;

    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardHook, g_instance, 0);
    if (!g_hook) return 1;

    ShowWindow(g_hwndMainWindow, SW_HIDE);
    UpdateWindow(g_hwndMainWindow);
    MinimizeWindowToTray();

	while (IsWindow(g_hwndMainWindow) && GetMessage(&msg, g_hwndMainWindow, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
    
	return 0;
}

EXTERN_C void WINAPI WinMainCRTStartup()
{
    ExitProcess(Main());
}
