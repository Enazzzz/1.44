#include "gui_window.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace one44 {

struct GuiWindow::Impl {
	Display *dpy = nullptr;
	Window win = 0;
	GC gc = 0;
	XImage *image = nullptr;
	std::vector<uint8_t> native;
	GuiView *view = nullptr;
	int width = kGuiWidth;
	int height = kGuiHeight;
	bool visible = false;
};

/// Converts 0xAARRGGBB into the XImage's native 32-bit layout.
static void pack_native(XImage *image, const uint32_t *src, uint8_t *dst, int w, int h) {
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const uint32_t px = src[y * w + x];
			const uint8_t a = static_cast<uint8_t>((px >> 24) & 0xFF);
			const uint8_t r = static_cast<uint8_t>((px >> 16) & 0xFF);
			const uint8_t g = static_cast<uint8_t>((px >> 8) & 0xFF);
			const uint8_t b = static_cast<uint8_t>(px & 0xFF);
			uint8_t *p = dst + y * image->bytes_per_line + x * 4;
			if (image->byte_order == MSBFirst) {
				p[0] = a;
				p[1] = r;
				p[2] = g;
				p[3] = b;
			} else {
				p[0] = b;
				p[1] = g;
				p[2] = r;
				p[3] = a;
			}
		}
	}
}

GuiWindow::GuiWindow() = default;

GuiWindow::~GuiWindow() {
	destroy();
}

/// Creates a child X11 window inside the host's parent and binds it to `view`.
bool GuiWindow::create_embedded(void *parent, GuiView *view) {
	destroy();
	impl_ = std::make_unique<Impl>();
	impl_->view = view;
	impl_->dpy = XOpenDisplay(nullptr);
	if (impl_->dpy == nullptr) {
		impl_.reset();
		return false;
	}
	const Window parent_win = parent ? static_cast<Window>(reinterpret_cast<uintptr_t>(parent))
									 : DefaultRootWindow(impl_->dpy);
	XSetWindowAttributes attrs{};
	attrs.background_pixel = BlackPixel(impl_->dpy, DefaultScreen(impl_->dpy));
	attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
		KeyPressMask | StructureNotifyMask | ButtonMotionMask;
	impl_->win = XCreateWindow(
		impl_->dpy,
		parent_win,
		0,
		0,
		static_cast<unsigned>(kGuiWidth),
		static_cast<unsigned>(kGuiHeight),
		0,
		CopyFromParent,
		InputOutput,
		CopyFromParent,
		CWBackPixel | CWEventMask,
		&attrs);
	if (impl_->win == 0) {
		destroy();
		return false;
	}
	impl_->gc = XCreateGC(impl_->dpy, impl_->win, 0, nullptr);
	impl_->native.resize(static_cast<size_t>(kGuiWidth * kGuiHeight * 4));
	impl_->image = XCreateImage(
		impl_->dpy,
		DefaultVisual(impl_->dpy, DefaultScreen(impl_->dpy)),
		static_cast<unsigned>(DefaultDepth(impl_->dpy, DefaultScreen(impl_->dpy))),
		ZPixmap,
		0,
		reinterpret_cast<char *>(impl_->native.data()),
		static_cast<unsigned>(kGuiWidth),
		static_cast<unsigned>(kGuiHeight),
		32,
		kGuiWidth * 4);
	if (impl_->image == nullptr) {
		destroy();
		return false;
	}
	XFlush(impl_->dpy);
	return true;
}

void GuiWindow::destroy() {
	if (!impl_) {
		return;
	}
	if (impl_->image) {
		impl_->image->data = nullptr;
		XDestroyImage(impl_->image);
		impl_->image = nullptr;
	}
	if (impl_->gc && impl_->dpy) {
		XFreeGC(impl_->dpy, impl_->gc);
		impl_->gc = 0;
	}
	if (impl_->win && impl_->dpy) {
		XDestroyWindow(impl_->dpy, impl_->win);
		impl_->win = 0;
	}
	if (impl_->dpy) {
		XCloseDisplay(impl_->dpy);
		impl_->dpy = nullptr;
	}
	impl_.reset();
}

void GuiWindow::show(bool visible) {
	if (!impl_ || !impl_->dpy || !impl_->win) {
		return;
	}
	impl_->visible = visible;
	if (visible) {
		XMapRaised(impl_->dpy, impl_->win);
	} else {
		XUnmapWindow(impl_->dpy, impl_->win);
	}
	XFlush(impl_->dpy);
}

bool GuiWindow::set_size(int width, int height) {
	if (!impl_ || !impl_->dpy || !impl_->win) {
		return false;
	}
	impl_->width = width;
	impl_->height = height;
	XResizeWindow(impl_->dpy, impl_->win, static_cast<unsigned>(width), static_cast<unsigned>(height));
	XFlush(impl_->dpy);
	return true;
}

void GuiWindow::process_events() {
	if (!impl_ || !impl_->dpy || !impl_->view) {
		return;
	}
	XEvent ev;
	while (XPending(impl_->dpy) > 0) {
		XNextEvent(impl_->dpy, &ev);
		switch (ev.type) {
		case Expose:
			blit();
			break;
		case ButtonPress:
			impl_->view->mouse_down(ev.xbutton.x, ev.xbutton.y, static_cast<int>(ev.xbutton.button));
			blit();
			break;
		case ButtonRelease:
			impl_->view->mouse_up(ev.xbutton.x, ev.xbutton.y, static_cast<int>(ev.xbutton.button));
			blit();
			break;
		case MotionNotify:
			impl_->view->mouse_move(ev.xmotion.x, ev.xmotion.y);
			blit();
			break;
		case KeyPress: {
			KeySym sym = XLookupKeysym(&ev.xkey, 0);
			impl_->view->key_down(static_cast<int>(sym));
			blit();
			break;
		}
		default:
			break;
		}
	}
}

void GuiWindow::blit() {
	if (!impl_ || !impl_->dpy || !impl_->image || !impl_->view) {
		return;
	}
	impl_->view->paint();
	pack_native(impl_->image, impl_->view->pixels(), impl_->native.data(), kGuiWidth, kGuiHeight);
	XPutImage(
		impl_->dpy,
		impl_->win,
		impl_->gc,
		impl_->image,
		0,
		0,
		0,
		0,
		static_cast<unsigned>(kGuiWidth),
		static_cast<unsigned>(kGuiHeight));
	XFlush(impl_->dpy);
}

bool GuiWindow::valid() const {
	return impl_ && impl_->dpy && impl_->win;
}

int GuiWindow::posix_fd() const {
	if (!impl_ || !impl_->dpy) {
		return -1;
	}
	return ConnectionNumber(impl_->dpy);
}

} // namespace one44
