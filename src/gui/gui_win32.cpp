#include "gui_window.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

namespace one44 {
namespace {

constexpr wchar_t kClassName[] = L"One44ClapEditor";
constexpr UINT_PTR kPaintTimerId = 1;
constexpr UINT kPaintPeriodMs = 33;

/// Returns the DLL/module that contains this translation unit (the .clap), not the host EXE.
HINSTANCE module_instance() {
	HMODULE module = nullptr;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&module_instance),
		&module);
	return static_cast<HINSTANCE>(module);
}

/// Maps Win32 virtual keys onto the X11 keysyms GuiView::key_down already understands.
int keysym_from_vk(WPARAM vk) {
	switch (vk) {
	case VK_ESCAPE:
		return 0xff1b;
	case VK_SPACE:
		return 0x0020;
	case VK_OEM_PLUS:
	case VK_ADD:
		return 0x002b;
	case VK_OEM_MINUS:
	case VK_SUBTRACT:
		return 0x002d;
	default:
		return 0;
	}
}

/// Paints the software framebuffer into `hdc` as a 32-bit top-down BGRA DIB.
void blit_dc(HDC hdc, GuiView *view) {
	if (view == nullptr) {
		return;
	}
	view->paint();
	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = kGuiWidth;
	info.bmiHeader.biHeight = -kGuiHeight;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	SetDIBitsToDevice(
		hdc,
		0,
		0,
		static_cast<DWORD>(kGuiWidth),
		static_cast<DWORD>(kGuiHeight),
		0,
		0,
		0,
		static_cast<UINT>(kGuiHeight),
		view->pixels(),
		&info,
		DIB_RGB_COLORS);
}

} // namespace

struct GuiWindow::Impl {
	HWND hwnd = nullptr;
	GuiView *view = nullptr;
	int width = kGuiWidth;
	int height = kGuiHeight;
	bool visible = false;

	static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};

/// Child-window procedure: the host message loop delivers mouse/keys/paint here.
LRESULT CALLBACK GuiWindow::Impl::wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_NCCREATE) {
		auto *cs = reinterpret_cast<CREATESTRUCTW *>(lparam);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
		return TRUE;
	}

	auto *impl = reinterpret_cast<Impl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (impl == nullptr || impl->view == nullptr) {
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	auto client_point = [&](LPARAM lp) {
		return POINT{
			static_cast<LONG>(static_cast<short>(LOWORD(lp))),
			static_cast<LONG>(static_cast<short>(HIWORD(lp))),
		};
	};

	switch (msg) {
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT ps{};
		HDC hdc = BeginPaint(hwnd, &ps);
		blit_dc(hdc, impl->view);
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_TIMER:
		if (wparam == kPaintTimerId) {
			InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	case WM_LBUTTONDOWN: {
		SetCapture(hwnd);
		SetFocus(hwnd);
		const POINT p = client_point(lparam);
		impl->view->mouse_down(static_cast<int>(p.x), static_cast<int>(p.y), 1);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_LBUTTONUP: {
		ReleaseCapture();
		const POINT p = client_point(lparam);
		impl->view->mouse_up(static_cast<int>(p.x), static_cast<int>(p.y), 1);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_MBUTTONDOWN: {
		SetCapture(hwnd);
		const POINT p = client_point(lparam);
		impl->view->mouse_down(static_cast<int>(p.x), static_cast<int>(p.y), 2);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_MBUTTONUP: {
		ReleaseCapture();
		const POINT p = client_point(lparam);
		impl->view->mouse_up(static_cast<int>(p.x), static_cast<int>(p.y), 2);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_RBUTTONDOWN: {
		const POINT p = client_point(lparam);
		impl->view->mouse_down(static_cast<int>(p.x), static_cast<int>(p.y), 3);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_RBUTTONUP: {
		const POINT p = client_point(lparam);
		impl->view->mouse_up(static_cast<int>(p.x), static_cast<int>(p.y), 3);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_MOUSEMOVE: {
		const POINT p = client_point(lparam);
		impl->view->mouse_move(static_cast<int>(p.x), static_cast<int>(p.y));
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_MOUSEWHEEL: {
		POINT p{
			static_cast<LONG>(static_cast<short>(LOWORD(lparam))),
			static_cast<LONG>(static_cast<short>(HIWORD(lparam))),
		};
		ScreenToClient(hwnd, &p);
		const int delta = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1;
		impl->view->mouse_wheel(static_cast<int>(p.x), static_cast<int>(p.y), delta);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}
	case WM_KEYDOWN: {
		const int keysym = keysym_from_vk(wparam);
		if (keysym != 0) {
			impl->view->key_down(keysym);
			InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	}
	case WM_GETDLGCODE:
		return DLGC_WANTCHARS | DLGC_WANTARROWS | DLGC_WANTTAB;
	case WM_MOUSEACTIVATE:
		SetFocus(hwnd);
		return MA_ACTIVATE;
	default:
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}
}

/// Registers the editor class once per process. Safe to call again if it already exists.
static bool ensure_class(HINSTANCE inst, WNDPROC proc) {
	WNDCLASSEXW existing{};
	existing.cbSize = sizeof(existing);
	if (GetClassInfoExW(inst, kClassName, &existing)) {
		return true;
	}
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc = proc;
	wc.hInstance = inst;
	wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszClassName = kClassName;
	return RegisterClassExW(&wc) != 0;
}

GuiWindow::GuiWindow() = default;

GuiWindow::~GuiWindow() {
	destroy();
}

/// Creates a WS_CHILD HWND inside the host parent (CLAP win32 embed / SetParent).
bool GuiWindow::create_embedded(void *parent, GuiView *view) {
	destroy();
	if (parent == nullptr || view == nullptr) {
		return false;
	}
	HINSTANCE inst = module_instance();
	if (inst == nullptr || !ensure_class(inst, Impl::wndproc)) {
		return false;
	}
	impl_ = std::make_unique<Impl>();
	impl_->view = view;
	impl_->hwnd = CreateWindowExW(
		0,
		kClassName,
		L"1.44",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0,
		0,
		kGuiWidth,
		kGuiHeight,
		static_cast<HWND>(parent),
		nullptr,
		inst,
		impl_.get());
	if (impl_->hwnd == nullptr) {
		impl_.reset();
		return false;
	}
	SetTimer(impl_->hwnd, kPaintTimerId, kPaintPeriodMs, nullptr);
	return true;
}

void GuiWindow::destroy() {
	if (!impl_) {
		return;
	}
	if (impl_->hwnd) {
		KillTimer(impl_->hwnd, kPaintTimerId);
		DestroyWindow(impl_->hwnd);
		impl_->hwnd = nullptr;
	}
	impl_.reset();
}

void GuiWindow::show(bool visible) {
	if (!impl_ || impl_->hwnd == nullptr) {
		return;
	}
	impl_->visible = visible;
	ShowWindow(impl_->hwnd, visible ? SW_SHOW : SW_HIDE);
}

bool GuiWindow::set_size(int width, int height) {
	if (!impl_ || impl_->hwnd == nullptr) {
		return false;
	}
	impl_->width = width;
	impl_->height = height;
	return SetWindowPos(
			   impl_->hwnd,
			   nullptr,
			   0,
			   0,
			   width,
			   height,
			   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
}

void GuiWindow::process_events() {
	// The host owns the Win32 message loop and dispatches to Impl::wndproc.
}

void GuiWindow::blit() {
	if (!impl_ || impl_->hwnd == nullptr) {
		return;
	}
	InvalidateRect(impl_->hwnd, nullptr, FALSE);
	UpdateWindow(impl_->hwnd);
}

bool GuiWindow::valid() const {
	return impl_ && impl_->hwnd != nullptr;
}

int GuiWindow::posix_fd() const {
	return -1;
}

} // namespace one44
