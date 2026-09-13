#pragma once

#include "core/sampler.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace one44 {

/// Fixed editor size in physical pixels.
constexpr int kGuiWidth = 1000;
constexpr int kGuiHeight = 640;

/// Immediate-mode editor drawn into an ARGB32 framebuffer (0xAARRGGBB).
class GuiView {
public:
	explicit GuiView(Sampler *sampler);

	int width() const { return kGuiWidth; }
	int height() const { return kGuiHeight; }
	const uint32_t *pixels() const { return pixels_.data(); }

	/// Pointer / wheel input. Coordinates are in GUI pixels.
	void mouse_down(int x, int y, int button);
	void mouse_up(int x, int y, int button);
	void mouse_move(int x, int y);
	void mouse_wheel(int x, int y, int delta);

	/// Keyboard: space auditions, +/- zoom, Esc closes the file browser.
	void key_down(int keysym);

	/// Rebuilds the framebuffer from the current sampler state.
	void paint();

	bool consume_dirty();
	const std::string &status() const { return status_; }

private:
	enum class DragTarget {
		None,
		RegionStart,
		RegionEnd,
		LoopStart,
		LoopEnd,
		/// Left-click on empty waveform; becomes RegionSweep after a short drag, else scrub on release.
		WavePending,
		/// Click-drag on empty waveform: anchor is IN, current pointer is OUT (order swapped if needed).
		RegionSweep,
		Scrub,
		Pan,
		Budget,
		Attack,
		Decay,
		Sustain,
		Release,
		Root,
	};

	void fill_rect(int x, int y, int w, int h, uint32_t color);
	void blend_rect(int x, int y, int w, int h, uint32_t color, int alpha);
	void hline(int x0, int x1, int y, uint32_t color);
	void vline(int x, int y0, int y1, uint32_t color);
	void draw_text(int x, int y, const char *text, uint32_t color);
	void draw_waveform();
	void draw_controls();
	void draw_file_browser();
	struct MarkerGeom {
		int stem_x = 0;
		int tab_x = 0;
		int tab_y = 0;
		int tab_w = 0;
		int tab_h = 0;
	};
	/// Pixel layout for a handle. `pair_frame` is the other marker in the same band (IN↔OUT or LS↔LE).
	MarkerGeom marker_geom(uint64_t frame, uint64_t pair_frame, bool region, bool is_start) const;
	void draw_marker(const MarkerGeom &geom, uint32_t color, const char *label, bool region_handle, bool is_start);
	bool hit_button(int x, int y, int bx, int by, int bw, int bh) const;
	uint64_t x_to_frame(int x) const;
	int frame_to_x(uint64_t frame) const;
	void zoom_at(int x, double factor);
	void open_file_browser();
	void refresh_dir();
	void apply_budget_from_x(int x);
	void apply_region_sweep(int x);
	void begin_scrub(uint64_t frame);
	DragTarget hit_waveform_handle(int x, int y) const;
	void clamp_view();

	Sampler *sampler_;
	std::vector<uint32_t> pixels_;
	int mouse_x_ = 0;
	int mouse_y_ = 0;
	bool mouse_left_ = false;
	DragTarget drag_ = DragTarget::None;
	int drag_last_x_ = 0;
	int drag_down_x_ = 0;
	uint64_t region_anchor_frame_ = 0;
	double view_start_ = 0.0;
	double frames_per_pixel_ = 1.0;
	bool file_browser_ = false;
	std::string browse_dir_;
	std::vector<std::string> browse_names_;
	std::vector<bool> browse_is_dir_;
	int browse_scroll_ = 0;
	std::string status_ = "Load a WAV or MP3, drag on the waveform to set the clip, COMMIT if it fits.";
	bool dirty_ = true;
	int wave_x_ = 16;
	int wave_y_ = 108;
	int wave_w_ = kGuiWidth - 32;
	int wave_h_ = 300;
};

} // namespace one44
