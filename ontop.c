
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shlobj.h>

#include <dwmapi.h>

#include <stdbool.h>

const size_t PSEUDO_BORDER_WIDTH = 5;
RECT g_rect;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static RECT border_thickness;
	static bool over = false;
	switch(uMsg)
	{
	case WM_ACTIVATE:
		SetRectEmpty(&border_thickness);
		if(GetWindowLongPtr(hWnd, GWL_STYLE) & WS_BORDER)
			SetRect(&border_thickness, 1, 1, 1, 1);

		{
			MARGINS m = { 0 };
			DwmExtendFrameIntoClientArea(hWnd, &m);
		}
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
				SWP_SHOWWINDOW|SWP_NOSIZE|SWP_NOMOVE|SWP_FRAMECHANGED);
	case WM_GETMINMAXINFO:
		if(lParam)
		{
			MINMAXINFO *mmi = (MINMAXINFO*)lParam;
			mmi->ptMinTrackSize.x = PSEUDO_BORDER_WIDTH*3;
			mmi->ptMinTrackSize.y = PSEUDO_BORDER_WIDTH*3;
		}
		return 0;
	case WM_NCMOUSEMOVE:
	case WM_MOUSEMOVE:
		if(over == false)
		{
			TRACKMOUSEEVENT tm;
			tm.cbSize = sizeof(tm);
			tm.dwFlags = TME_HOVER | TME_NONCLIENT;
			tm.hwndTrack = hWnd;
			tm.dwHoverTime = HOVER_DEFAULT;
			TrackMouseEvent(&tm);
			over = true;
		}
		break;
	case WM_MOUSEHOVER:
	case WM_NCMOUSEHOVER:
		{
			DWM_BLURBEHIND bb = { 0 };
			bb.dwFlags = DWM_BB_ENABLE;
			bb.fEnable = TRUE;
			bb.hRgnBlur = NULL;
			DwmEnableBlurBehindWindow(hWnd, &bb);
		}
		break;
	case WM_MOUSELEAVE:
	case WM_NCMOUSELEAVE:
		{
			over = false;
			DWM_BLURBEHIND bb = { 0 };
			bb.dwFlags = DWM_BB_ENABLE;
			bb.fEnable = FALSE;
			bb.hRgnBlur = NULL;
			DwmEnableBlurBehindWindow(hWnd, &bb);
		}
		break;
	case WM_NCACTIVATE:
		return 0;
	case WM_NCHITTEST:
		{
			LRESULT hit = DefWindowProc(hWnd, uMsg, wParam, lParam);
			if(hit == HTCLIENT) hit = HTCAPTION;
			POINT p = { .x = GET_X_LPARAM(lParam),
				.y = GET_Y_LPARAM(lParam) };
			ScreenToClient(hWnd, &p);
			RECT r;
			GetClientRect(hWnd, &r);

			// check if we're in the pseudo border so the window
			// can be sized
			if(p.x < PSEUDO_BORDER_WIDTH)
			{ // we're on the left border somewhere
				if(p.y < PSEUDO_BORDER_WIDTH) // is it the top left?
					hit = HTTOPLEFT;
				else if(p.y > (r.bottom - PSEUDO_BORDER_WIDTH)) // or the bottom left
					hit = HTBOTTOMLEFT;
				else
					hit = HTLEFT;
			}
			else if(p.x > (r.right - PSEUDO_BORDER_WIDTH))
			{ // right border
				if(p.y < PSEUDO_BORDER_WIDTH)
					hit = HTTOPRIGHT;
				else if(p.y > (r.bottom - PSEUDO_BORDER_WIDTH))
					hit = HTBOTTOMRIGHT;
				else
					hit = HTRIGHT;
			}
			else if(p.y < PSEUDO_BORDER_WIDTH)
				hit = HTTOP;
			else if(p.y > (r.bottom - PSEUDO_BORDER_WIDTH))
				hit = HTBOTTOM;
				
			return hit;
		}
	case WM_KEYUP:
		if(wParam != VK_ESCAPE)
			break;
	case WM_DESTROY:
		GetWindowRect(hWnd, &g_rect);
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			HBRUSH brush = CreateSolidBrush(RGB(0, 128, 0));
			FillRect(hdc, &ps.rcPaint, brush);
			EndPaint(hWnd, &ps);
			DeleteObject(brush);
		}
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
	HWND hWnd;
	WNDCLASS wc;
	MSG msg;
	RECT pos;
	CHAR szPath[MAX_PATH];
	HANDLE hFile;
	DWORD dwIO;

	pos.left = pos.right = pos.top = pos.bottom = CW_USEDEFAULT;
	if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szPath)))
		PathAppend(szPath, "\\OnTopBlocker\\");
	if(!PathFileExists(szPath))
		SHCreateDirectoryEx(NULL, szPath, NULL);
	PathAppend(szPath, "config.bin");

	if(PathFileExists(szPath))
	{
		dwIO = 0;
		hFile = CreateFile(szPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if(!ReadFile(hFile, &pos, sizeof(pos), &dwIO, NULL) || dwIO != sizeof(pos))
			pos.left = pos.right = pos.top = pos.bottom = CW_USEDEFAULT;
		CloseHandle(hFile);
	}

	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "OnTopBlockerClass";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClass(&wc);

	hWnd = CreateWindowEx(0, "OnTopBlockerClass", "On-Top Blocker",
			WS_VISIBLE,
			pos.left, pos.top, pos.right, pos.bottom,
			NULL, NULL, hInstance, NULL);
	SetForegroundWindow(hWnd);
	SetWindowLong(hWnd, GWL_STYLE, 0);
	ShowWindow(hWnd, nCmdShow);

	ZeroMemory(&msg, sizeof(msg));
	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	g_rect.right -= g_rect.left;
	g_rect.bottom -= g_rect.top;
	dwIO = 0;
	hFile = CreateFile(szPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	WriteFile(hFile, &g_rect, sizeof(g_rect), &dwIO, NULL);
	CloseHandle(hFile);

	return 0;
}
