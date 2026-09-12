#pragma once

#include "gui.hpp"

#include <memory>

namespace one44 {

/// Host-embedded window. Linux uses X11; Windows uses a child HWND (CLAP win32 API).
class GuiWindow {
public:
	GuiWindow();
	~GuiWindow();

	GuiWindow(const GuiWindow &) = delete;
	GuiWindow &operator=(const GuiWindow &) = delete;

	bool create_embedded(void *parent, GuiView *view);
	void destroy();
	void show(bool visible);
	bool set_size(int width, int height);
	void process_events();
	void blit();
	bool valid() const;
	int posix_fd() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace one44
