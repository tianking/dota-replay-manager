#include "window.h"

#include "core/app.h"
#include "frameui/fontsys.h"

#include <stdio.h>

struct Window::TTData
{
	HWND hTip;
	RECT tipRect;
	int hitCode;
};
static LRESULT CALLBACK TooltipWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_PAINT)
	{
		char text[256];
		GetWindowText(hWnd, text, sizeof text);

		RECT rc;
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);

		SelectObject(hDC, FontSys::getSysFont());
		SetBkColor(hDC, 0xE1FFFF);
		SetTextColor(hDC, 0x000000);

		HPEN hPen = CreatePen(PS_SOLID, 0, 0x000000);
		HPEN hPrev = (HPEN) SelectObject(hDC, hPen);
		Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
		rc.left++;
		rc.top++;
		rc.bottom--;
		rc.right--;
		DrawText(hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
		SelectObject(hDC, hPrev);
		DeleteObject(hPen);

		EndPaint(hWnd, &ps);
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);;
}


ATOM Window::windowClass = NULL;
IntDictionary Window::handleMap;

Window* Window::fromHandle(HWND hWnd)
{
	if (handleMap.has((uint32) hWnd))
		return (Window*) handleMap.get((uint32) hWnd);
	return NULL;
}

WNDCLASSEX* Window::createclass(String wndClass)
{
	regClass = wndClass;

	WNDCLASSEX* wcx = new WNDCLASSEX;
	HINSTANCE hInstance = getInstance();
	if (!GetClassInfoEx(hInstance, regClass, wcx))
	{
		memset(wcx, 0, sizeof(WNDCLASSEX));
		wcx->cbSize = sizeof(WNDCLASSEX);
		wcx->lpfnWndProc = WindowProc;
		wcx->hInstance = hInstance;
		wcx->lpszClassName = regClass;
		wcx->hCursor = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
		return wcx;
	}
	delete wcx;
	return NULL;
}
void Window::create(int x, int y, int width, int height, String text, uint32 style, uint32 exStyle,
	HWND parent)
{
	if (!windowClass && regClass.isEmpty())
	{
		WNDCLASSEX wcex;
		memset(&wcex, 0, sizeof wcex);
		wcex.cbSize = sizeof wcex;
		wcex.lpfnWndProc = WindowProc;
		wcex.hInstance = getInstance();
		wcex.lpszClassName = "WUTILSWINDOW";
		wcex.hCursor = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
		windowClass = RegisterClassEx(&wcex);
	}
	hWnd = CreateWindowEx(exStyle, regClass.isEmpty() ? "WUTILSWINDOW" : regClass, text, style,
		x, y, width, height, parent, NULL, getInstance(), this);
	handleMap.set((uint32) hWnd, (uint32) this);
}
void Window::create(String wndClass, int x, int y, int width, int height, String text, uint32 style,
	uint32 exStyle, HWND parent)
{
	hWnd = CreateWindowEx(exStyle, wndClass, text, style, x, y, width, height,
		parent, NULL, getInstance(), NULL);
	handleMap.set((uint32) hWnd, (uint32) this);
}
void Window::subclass(String wndClass, int x, int y, int width, int height, String text, uint32 style,
	uint32 exStyle, HWND parent)
{
	hWnd = CreateWindowEx(exStyle, wndClass, text, style, x, y, width, height,
		parent, NULL, getInstance(), NULL);
	handleMap.set((uint32) hWnd, (uint32) this);
	origProc = (WNDPROC) SetWindowLong(hWnd, GWL_WNDPROC, (uint32) WindowProc);
}

LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Window* wnd = (Window*) (handleMap.has((uint32) hWnd)
		? handleMap.get((uint32) hWnd) : 0);
	if (wnd == NULL && uMsg == WM_CREATE)
	{
		CREATESTRUCT* cs = (CREATESTRUCT*) lParam;
		wnd = (Window*) cs->lpCreateParams;
		if (wnd)
			wnd->hWnd = hWnd;
	}
	if (wnd)
	{
		bool send = true;
		if (wnd->ttData)
		{
			TRACKMOUSEEVENT tme;
			memset(&tme, 0, sizeof tme);
			tme.cbSize = sizeof tme;
			tme.hwndTrack = wnd->hWnd;
			if (uMsg == WM_MOUSEMOVE)
			{
				POINT pt;
				pt.x = GET_X_LPARAM(lParam);
				pt.y = GET_Y_LPARAM(lParam);
				if (wnd->ttData->hitCode >= 0)
				{
					if (pt.x < wnd->ttData->tipRect.left || pt.x >= wnd->ttData->tipRect.right ||
						pt.y < wnd->ttData->tipRect.top || pt.y >= wnd->ttData->tipRect.bottom)
						wnd->ttData->hitCode = -1;
				}
				if (wnd->ttData->hitCode < 0)
				{
					ToolInfo ti;
					wnd->ttData->hitCode = wnd->toolHitTest(pt, &ti);
					if (wnd->ttData->hitCode >= 0)
					{
						tme.dwFlags = TME_HOVER | TME_LEAVE;
						tme.dwHoverTime = HOVER_DEFAULT;
						TrackMouseEvent(&tme);

						wnd->ttData->tipRect = ti.rc;
						SetWindowText(wnd->ttData->hTip, ti.text);

						SIZE sz;
						HDC hDC = GetDC(wnd->ttData->hTip);
						SelectObject(hDC, FontSys::getSysFont());
						GetTextExtentPoint32(hDC, ti.text, ti.text.length(), &sz);
						ReleaseDC(wnd->ttData->hTip, hDC);

						POINT ptTL;
						ptTL.x = wnd->ttData->tipRect.left;
						ptTL.y = wnd->ttData->tipRect.bottom + 5;
						ClientToScreen(wnd->hWnd, &ptTL);

						InvalidateRect(wnd->ttData->hTip, NULL, TRUE);
						SetWindowPos(wnd->ttData->hTip, HWND_TOPMOST,
							ptTL.x, ptTL.y, sz.cx + 10, sz.cy + 5,
							SWP_NOACTIVATE);
					}
					else
					{
						tme.dwFlags = TME_CANCEL;
						TrackMouseEvent(&tme);
						ShowWindow(wnd->ttData->hTip, SW_HIDE);
					}
				}
			}
			else if (uMsg == WM_MOUSELEAVE)
			{
				ShowWindow(wnd->ttData->hTip, SW_HIDE);
				wnd->ttData->hitCode = -1;
				send = false;
			}
			else if (uMsg == WM_MOUSEHOVER)
			{
				tme.dwFlags = TME_CANCEL;
				TrackMouseEvent(&tme);

				if (wnd->ttData->hitCode >= 0)
					ShowWindow(wnd->ttData->hTip, SW_SHOWNA);
				send = false;
			}
			else if (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MBUTTONDOWN ||
				uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL || uMsg == WM_KEYDOWN)
			{
				ShowWindow(wnd->ttData->hTip, SW_HIDE);
				wnd->ttData->hitCode = -1;
			}
		}
		if (send)
		{
			uint32 result = wnd->onWndMessage(uMsg, wParam, lParam);
			if (uMsg == WM_DESTROY)
				wnd->hWnd = NULL;
			return result;
		}
		else
			return 0;
	}
	else
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
uint32 Window::onWndMessage(uint32 message, uint32 wParam, uint32 lParam)
{
	if (origProc)
		return CallWindowProc(origProc, hWnd, message, wParam, lParam);
	else
		return DefWindowProc(hWnd, message, wParam, lParam);
}

Window::Window()
{
	hWnd = NULL;
	origProc = NULL;
	ttData = NULL;
}
Window::~Window()
{
	if (hWnd)
	{
		if (origProc)
			SetWindowLong(hWnd, GWL_WNDPROC, (uint32) origProc);
		handleMap.del((uint32) hWnd);
		DestroyWindow(hWnd);
	}
	if (ttData)
		delete ttData;
}

void Window::setText(String text)
{
#ifdef UNICODE
	int wsize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), text.length(), NULL, 0);
	wchar_t* buf = new wchar_t[wsize + 1];
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), text.length(), buf, wsize);
	buf[wsize] = 0;
	SetWindowTextW(hWnd, buf);
	delete[] buf;
#else
	SetWindowTextA(hWnd, text);
#endif // !UNICODE
}
String Window::getText() const
{
	return getWindowText(hWnd);
}
String Window::getWindowText(HWND hWnd)
{
	int wsize = GetWindowTextLengthW(hWnd);
	wchar_t* buf = new wchar_t[wsize + 1];
	GetWindowTextW(hWnd, buf, wsize + 1);
	int size = WideCharToMultiByte(CP_UTF8, 0, buf, wsize, NULL, 0, NULL, NULL);
	String str;
	str.resize(size);
	WideCharToMultiByte(CP_UTF8, 0, buf, wsize, str.getBuffer(), size, NULL, NULL);
	str.setLength(size);
	delete[] buf;
	return str;
}

void Window::setFont(HFONT hFont)
{
	SendMessage(hWnd, WM_SETFONT, (WPARAM) hFont, TRUE);
}
HFONT Window::getFont() const
{
	return (HFONT) SendMessage(hWnd, WM_GETFONT, NULL, NULL);
}

void Window::enable(bool e)
{
	EnableWindow(hWnd, e);
}
void Window::showWindow(bool s)
{
	ShowWindow(hWnd, s ? SW_SHOW : SW_HIDE);
}

int Window::id() const
{
	return GetWindowLong(hWnd, GWL_ID);
}
void Window::setId(int id)
{
	SetWindowLong(hWnd, GWL_ID, id);
}

void Window::invalidate(bool erase)
{
	InvalidateRect(hWnd, NULL, erase ? TRUE : FALSE);
}

void Window::setRedraw(bool r)
{
	SendMessage(hWnd, WM_SETREDRAW, r ? TRUE : FALSE, 0);
	if (r)
		RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
}

int Window::toolHitTest(POINT pt, ToolInfo* ti)
{
	return -1;
}
void Window::enableTooltips(bool enable)
{
	if (hWnd == NULL)
		return;
	if (enable && ttData == NULL)
	{
		ttData = new TTData;
		WNDCLASSEX wcx;
		HINSTANCE hInstance = getInstance();
		if (!GetClassInfoEx(hInstance, "DRTooltip", &wcx))
		{
			memset(&wcx, 0, sizeof wcx);
			wcx.cbSize = sizeof wcx;
			wcx.lpfnWndProc = TooltipWindowProc;
			wcx.hInstance = hInstance;
			wcx.lpszClassName = "DRTooltip";
			wcx.hbrBackground = CreateSolidBrush(0xE1FFFF);
			RegisterClassEx(&wcx);
		}
		ttData->hTip = CreateWindowEx(WS_EX_TOPMOST, "DRTooltip",
			NULL, WS_POPUP, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, hWnd, NULL,
			getInstance(), NULL);

		ttData->hitCode = -1;
	}
	else if (!enable && ttData != NULL)
	{
		DestroyWindow(ttData->hTip);
		delete ttData;
		ttData = NULL;
	}
}
