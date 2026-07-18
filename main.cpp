#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <cstdio>
#include <string>
#include "config.h"
#include "pawnio_ec.h"

static const wchar_t* WND_CLASS = L"MSI_FBWnd";
static const wchar_t* WND_TITLE = L"MSI_FB";
static const UINT     WM_TRAYICON = WM_USER + 1;
static const UINT     IDI_TRAY    = 1;
static const UINT     IDM_EXIT    = 1001;

static HHOOK    g_hook      = nullptr;
static HWND     g_hwnd      = nullptr;
static NOTIFYICONDATAW g_nid = {};
static HICON    g_iconOff   = nullptr;
static HICON    g_iconOn    = nullptr;

static PawnIoEc g_pawnio;
static bool     g_pawnioInitTried = false;
static bool     g_pawnioInitOk    = false;
static bool     g_fullBlastOn     = false;
static DWORD    g_lastToggleTick  = 0;

static HICON MakeTrayIcon(COLORREF accent) {
	const int S = 15;
	HDC hdcScreen = GetDC(nullptr);
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = S;
	bi.bmiHeader.biHeight = -S;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP hBmp = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
	auto hOld = (HBITMAP)SelectObject(hdcMem, hBmp);

	const uint16_t grid[S] = {0x7FFF, 0x4001, 0x4001, 0x4001, 0x4081, 0x42A1, 0x4491, 0x4411, 0x4411, 0x4291, 0x41C1, 0x4001, 0x4001, 0x4001, 0x7FFF};

	DWORD rgb = (accent & 0xFF) << 16 | accent & 0xFF00 | (accent >> 16) & 0xFF;
	auto* px = static_cast<DWORD*>(bits);
	for (int y = 0; y < S; ++y)
		for (int x = 0; x < S; ++x)
			px[y * S + x] = (grid[y] >> (S - 1 - x)) & 1 ? rgb : 0x00000000;

	SelectObject(hdcMem, hOld);

	HBITMAP hMask = CreateBitmap(S, S, 1, 1, nullptr);

	ICONINFO ii = {};
	ii.fIcon = TRUE;
	ii.hbmMask = hMask;
	ii.hbmColor = hBmp;
	HICON hIcon = CreateIconIndirect(&ii);

	DeleteObject(hMask);
	DeleteObject(hBmp);
	DeleteDC(hdcMem);
	ReleaseDC(nullptr, hdcScreen);
	return hIcon;
}

static bool IsTargetKey(const KBDLLHOOKSTRUCT* k) {
	return k->scanCode == cfg::TARGET_SCANCODE && k->flags & LLKHF_EXTENDED;
}

static void ShowBalloon(const wchar_t* text) {
	g_nid.uFlags |= NIF_INFO;
	g_nid.dwInfoFlags = NIIF_INFO;
	g_nid.uTimeout = 2000;
	StringCchCopyW(g_nid.szInfo, ARRAYSIZE(g_nid.szInfo), text);
	Shell_NotifyIconW(NIM_MODIFY, &g_nid);
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void UpdateTooltip() {
	StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
		g_fullBlastOn ? L"MSI_FB - Full Blast: ON" : L"MSI_FB - Full Blast: OFF");
	g_nid.hIcon = g_fullBlastOn ? g_iconOn : g_iconOff;
	g_nid.uFlags |= NIF_ICON;
	Shell_NotifyIconW(NIM_MODIFY, &g_nid);
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void DoToggle() {
	DWORD now = GetTickCount();
	if (now - g_lastToggleTick < 300) return;
	g_lastToggleTick = now;

	if (!g_pawnioInitTried) {
		g_pawnioInitTried = true;
		std::string err;
		g_pawnioInitOk = g_pawnio.Initialize(err);
	}
	if (!g_pawnioInitOk) return;

	std::string err;
	bool nowOn = false;
	if (g_pawnio.ToggleFullBlast(nowOn, err)) {
		g_fullBlastOn = nowOn;
		UpdateTooltip();
		ShowBalloon(nowOn ? L"Full Blast: ON" : L"Full Blast: OFF");
	}
}

static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		auto* k = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
		if (k->flags & LLKHF_INJECTED)
			return CallNextHookEx(g_hook, nCode, wParam, lParam);

		if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && IsTargetKey(k)) {
			DoToggle();
			return 1;
		}
		if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && IsTargetKey(k))
			return 1;
	}
	return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_TRAYICON) {
		if (lParam == WM_RBUTTONUP) {
			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(hwnd);
			HMENU menu = CreatePopupMenu();
			AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");
			TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
						   pt.x, pt.y, 0, hwnd, nullptr);
			DestroyMenu(menu);
			return 0;
		}
		if (lParam == WM_LBUTTONDBLCLK) {
			DoToggle();
			return 0;
		}
	}
	if (msg == WM_COMMAND && LOWORD(wParam) == IDM_EXIT) {
		PostQuitMessage(0);
		return 0;
	}
	if (msg == WM_DESTROY) {
		Shell_NotifyIconW(NIM_DELETE, &g_nid);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool SetRunRegistry(const wchar_t* exePath, bool add) {
	HKEY hKey;
	LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
		0, add ? KEY_SET_VALUE : KEY_ALL_ACCESS, &hKey);
	if (rc != ERROR_SUCCESS) return false;

	if (add) {
		wchar_t cmd[MAX_PATH + 8]{};
		StringCchPrintfW(cmd, ARRAYSIZE(cmd), L"\"%s\"", exePath);
		rc = RegSetValueExW(hKey, L"MSI_FB", 0, REG_SZ,
						   reinterpret_cast<const BYTE*>(cmd),
						   (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
	} else {
		rc = RegDeleteValueW(hKey, L"MSI_FB");
	}
	RegCloseKey(hKey);
	return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
}

static bool SetUninstallRegistry(const wchar_t* exePath, bool add) {
	static const wchar_t* subKey =
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MSI_FB";

	if (add) {
		HKEY hKey;
		LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr,
								  0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
		if (rc != ERROR_SUCCESS) return false;

		wchar_t dir[MAX_PATH]{};
		StringCchCopyW(dir, ARRAYSIZE(dir), exePath);
		wchar_t* lastSlash = wcsrchr(dir, L'\\');
		if (lastSlash) *lastSlash = L'\0';

		wchar_t uninstallCmd[MAX_PATH + 16]{};
		StringCchPrintfW(uninstallCmd, ARRAYSIZE(uninstallCmd), L"\"%s\" --uninstall", exePath);

		RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ,
					   (const BYTE*)L"MSI_FB", (DWORD)(wcslen(L"MSI_FB") + 1) * sizeof(wchar_t));
		RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ,
					   (const BYTE*)uninstallCmd, (DWORD)(wcslen(uninstallCmd) + 1) * sizeof(wchar_t));
		RegSetValueExW(hKey, L"QuietUninstallString", 0, REG_SZ,
					   (const BYTE*)uninstallCmd, (DWORD)(wcslen(uninstallCmd) + 1) * sizeof(wchar_t));
		RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ,
					   (const BYTE*)dir, (DWORD)(wcslen(dir) + 1) * sizeof(wchar_t));
		RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ,
					   (const BYTE*)exePath, (DWORD)(wcslen(exePath) + 1) * sizeof(wchar_t));
		DWORD one = 1;
		RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
		RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
		RegCloseKey(hKey);
		return true;
	} else {
		HKEY hKey;
		LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_ALL_ACCESS, &hKey);
		if (rc != ERROR_SUCCESS) return (rc == ERROR_FILE_NOT_FOUND);
		RegCloseKey(hKey);
		return RegDeleteKeyW(HKEY_CURRENT_USER, subKey) == ERROR_SUCCESS;
	}
}

static bool InstallTask() {
	wchar_t exePath[MAX_PATH]{};
	DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	if (n == 0 || n >= MAX_PATH) return false;

	wchar_t cmd[1024]{};
	StringCchPrintfW(cmd, ARRAYSIZE(cmd),
		L"schtasks /create /tn \"MSI_FB\" /tr \"\\\"%s\\\"\" /sc onlogon /rl highest /f",
		exePath);

	STARTUPINFOW si = { sizeof(si) };
	PROCESS_INFORMATION pi = {};
	wchar_t cmdBuf[1024];
	StringCchCopyW(cmdBuf, ARRAYSIZE(cmdBuf), cmd);
	if (!CreateProcessW(nullptr, cmdBuf, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
						nullptr, nullptr, &si, &pi))
		return false;
	WaitForSingleObject(pi.hProcess, 10000);
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	SetRunRegistry(exePath, true);
	SetUninstallRegistry(exePath, true);
	return exitCode == 0;
}

static bool UninstallTask() {
	wchar_t exePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);

	STARTUPINFOW si = { sizeof(si) };
	PROCESS_INFORMATION pi = {};
	wchar_t cmd[] = L"schtasks /delete /tn \"MSI_FB\" /f";
	if (!CreateProcessW(nullptr, cmd,
						nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
						nullptr, nullptr, &si, &pi))
		return false;
	WaitForSingleObject(pi.hProcess, 10000);
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	SetRunRegistry(exePath, false);
	SetUninstallRegistry(exePath, false);
	return exitCode == 0;
}

static void SelfTestInject() {
	INPUT in[2] = {};
	in[0].type = INPUT_KEYBOARD;
	in[0].ki.wScan = cfg::TARGET_SCANCODE;
	in[0].ki.dwFlags = KEYEVENTF_SCANCODE | (cfg::TARGET_EXTENDED ? KEYEVENTF_EXTENDEDKEY : 0);
	in[1] = in[0];
	in[1].ki.dwFlags |= KEYEVENTF_KEYUP;
	SendInput(2, in, sizeof(INPUT));
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	bool selfTest = false;
	bool pawnioTest = false;
	bool debug = false;

	for (int i = 1; i < argc; ++i) {
		if (wcscmp(argv[i], L"--help") == 0 || wcscmp(argv[i], L"-h") == 0) {
			MessageBoxW(nullptr,
				L"MSI_FB - MSI Full Blast toggle\n\n"
				L"Usage: MSI_FB.exe [options]\n\n"
				L"Options:\n"
				L"  --help, -h        Show this help message\n"
				L"  --install         Create scheduled task + startup entry\n"
				L"  --uninstall       Remove scheduled task + startup entry\n"
				L"  --selftest        Inject a test key press on startup\n"
				L"  --pawnio-test     Test PawnIO EC access\n"
				L"  --debug           Show debug console",
				WND_TITLE, MB_ICONINFORMATION);
			LocalFree(argv);
			return 0;
		}
		if (wcscmp(argv[i], L"--selftest") == 0) selfTest = true;
		if (wcscmp(argv[i], L"--pawnio-test") == 0) pawnioTest = true;
		if (wcscmp(argv[i], L"--debug") == 0) debug = true;
		if (wcscmp(argv[i], L"--install") == 0) {
			LocalFree(argv);
			return InstallTask() ? 0 : 1;
		}
		if (wcscmp(argv[i], L"--uninstall") == 0) {
			LocalFree(argv);
			return UninstallTask() ? 0 : 1;
		}
	}
	LocalFree(argv);

	if (pawnioTest) {
		AllocConsole();
		FILE* f = nullptr;
		freopen_s(&f, "CONOUT$", "w", stdout);
		setvbuf(stdout, nullptr, _IONBF, 0);
		std::string err;
		printf("PawnIO Initialize()...\n");
		if (!g_pawnio.Initialize(err)) {
			printf("PawnIO Initialize: FAIL %s\n", err.c_str());
			return 2;
		}
		uint8_t st = 0;
		if (g_pawnio.ReadPort66(st, err))
			printf("Read port 0x66 OK value=0x%02x\n", st);
		else
			printf("Read port 0x66 FAIL %s\n", err.c_str());
		bool fb = false;
		if (g_pawnio.GetFullBlast(fb, err))
			printf("Full Blast: %s\n", fb ? "ON" : "off");
		else
			printf("GetFullBlast FAIL %s\n", err.c_str());
		return 0;
	}

	if (debug) {
		AllocConsole();
		FILE* f = nullptr;
		freopen_s(&f, "CONOUT$", "w", stdout);
		setvbuf(stdout, nullptr, _IONBF, 0);
		printf("MSI_FB debug mode.\n");
	}

	WNDCLASSEXW wc = { sizeof(wc) };
	wc.lpfnWndProc   = WndProc;
	wc.hInstance      = hInst;
	wc.lpszClassName  = WND_CLASS;
	RegisterClassExW(&wc);

	g_hwnd = CreateWindowExW(0, WND_CLASS, WND_TITLE, 0,
							  0, 0, 0, 0, HWND_MESSAGE, nullptr, hInst, nullptr);

	g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, hInst, 0);
	if (!g_hook) {
		MessageBoxW(nullptr, L"Failed to install keyboard hook.", WND_TITLE, MB_ICONERROR);
		return 1;
	}

	g_iconOff = MakeTrayIcon(RGB(255, 0, 0));
	g_iconOn  = MakeTrayIcon(RGB(0, 255, 0));

	g_nid.cbSize = sizeof(g_nid);
	g_nid.hWnd   = g_hwnd;
	g_nid.uID    = IDI_TRAY;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAYICON;
	g_nid.hIcon  = g_iconOff;
	StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"MSI_FB - Full Blast: OFF");
	Shell_NotifyIconW(NIM_ADD, &g_nid);

	if (debug) printf("Keyboard hook installed. Tray icon active.\n");

	if (selfTest) {
		Sleep(200);
		SelfTestInject();
		Sleep(200);
	}

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	UnhookWindowsHookEx(g_hook);
	g_pawnio.Shutdown();
	if (g_iconOff) DestroyIcon(g_iconOff);
	if (g_iconOn)  DestroyIcon(g_iconOn);
	return 0;
}
