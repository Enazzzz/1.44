#include "gui.hpp"

#include "core/pitch.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace one44 {
namespace {

constexpr uint32_t kBg = 0xFF140F0C;
constexpr uint32_t kPanel = 0xFF221A14;
constexpr uint32_t kAmber = 0xFFE8A020;
constexpr uint32_t kAmberDim = 0xFF8A5A10;
constexpr uint32_t kText = 0xFFE8DCC8;
constexpr uint32_t kMuted = 0xFF8A7A68;
constexpr uint32_t kRegion = 0xFFFF7A1A;
constexpr uint32_t kLoop = 0xFF3EC8E0;
constexpr uint32_t kPlay = 0xFFF0F0F0;
constexpr uint32_t kGood = 0xFF3CB86A;
constexpr uint32_t kBad = 0xFFE05040;
constexpr uint32_t kButton = 0xFF3A2C20;
constexpr uint32_t kButtonOn = 0xFF6A4018;

/// 5x7 packed glyphs for printable ASCII (32..126). Each column is 7 bits, LSB at the top.
const uint8_t kFont[95][5] = {
	{0,0,0,0,0}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
	{0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x56,0x20,0x50}, {0x00,0x08,0x07,0x03,0x00},
	{0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x2A,0x1C,0x7F,0x1C,0x2A}, {0x08,0x08,0x3E,0x08,0x08},
	{0x00,0x80,0x70,0x30,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x00,0x60,0x60,0x00}, {0x20,0x10,0x08,0x04,0x02},
	{0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x72,0x49,0x49,0x49,0x46}, {0x21,0x41,0x49,0x4D,0x33},
	{0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x31}, {0x41,0x21,0x11,0x09,0x07},
	{0x36,0x49,0x49,0x49,0x36}, {0x46,0x49,0x49,0x29,0x1E}, {0x00,0x00,0x14,0x00,0x00}, {0x00,0x40,0x34,0x00,0x00},
	{0x00,0x08,0x14,0x22,0x41}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x59,0x09,0x06},
	{0x3E,0x41,0x5D,0x59,0x4E}, {0x7C,0x12,0x11,0x12,0x7C}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
	{0x7F,0x41,0x41,0x41,0x3E}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x41,0x51,0x73},
	{0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
	{0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x1C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
	{0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x26,0x49,0x49,0x49,0x32},
	{0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
	{0x63,0x14,0x08,0x14,0x63}, {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x41},
	{0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x41,0x7F}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
	{0x00,0x03,0x07,0x08,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
	{0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x18,0xA4,0xA4,0xA4,0x7C},
	{0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x40,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
	{0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x78,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
	{0xFC,0x18,0x24,0x24,0x18}, {0x18,0x24,0x24,0x18,0xFC}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x24},
	{0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
	{0x44,0x28,0x10,0x28,0x44}, {0x1C,0xA0,0xA0,0xA0,0x7C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
	{0x00,0x00,0x77,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x02,0x01,0x02,0x04,0x02},
};

/// Formats a byte count for the budget readout.
std::string format_bytes(uint64_t bytes) {
	char buf[64];
	if (bytes >= 1024ull * 1024ull) {
		std::snprintf(buf, sizeof(buf), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
	} else if (bytes >= 1024ull) {
		std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
	} else {
		std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
	}
	return buf;
}

/// MIDI note name for the root-note slider.
std::string note_name(int note) {
	static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	note = clamp_midi_note(note);
	char buf[16];
	std::snprintf(buf, sizeof(buf), "%s%d", names[note % 12], (note / 12) - 1);
	return buf;
}

/// True if the name looks like a supported audio file.
bool is_audio_name(const std::string &name) {
	auto lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return lower.size() >= 4 &&
		(lower.rfind(".wav") == lower.size() - 4 || lower.rfind(".mp3") == lower.size() - 4);
}

/// Starting folder for the in-GUI file browser (user home, with a platform fallback).
std::string default_browse_dir() {
#ifdef _WIN32
	if (const char *profile = std::getenv("USERPROFILE")) {
		return profile;
	}
	const char *drive = std::getenv("HOMEDRIVE");
	const char *path = std::getenv("HOMEPATH");
	if (drive && path) {
		return std::string(drive) + path;
	}
	return "C:\\";
#else
	if (const char *home = std::getenv("HOME")) {
		return home;
	}
	return "/tmp";
#endif
}

/// Parent of `dir`, or `dir` itself at a filesystem root.
std::string parent_browse_dir(const std::string &dir) {
	const std::filesystem::path parent = std::filesystem::path(dir).parent_path();
	if (parent.empty()) {
		return dir;
	}
	std::string out = parent.string();
	return out.empty() ? dir : out;
}

/// Joins a directory and a child name with the host path separator.
std::string join_browse_path(const std::string &dir, const std::string &name) {
	return (std::filesystem::path(dir) / name).string();
}

} // namespace

/// Constructs a dark ASR-styled editor bound to `sampler`.
GuiView::GuiView(Sampler *sampler) : sampler_(sampler) {
	pixels_.assign(static_cast<size_t>(kGuiWidth * kGuiHeight), kBg);
}

/// Marks the view as consumed so the platform window can skip redundant blits.
bool GuiView::consume_dirty() {
	const bool d = dirty_;
	dirty_ = false;
	return d;
}

void GuiView::fill_rect(int x, int y, int w, int h, uint32_t color) {
	if (w <= 0 || h <= 0) {
		return;
	}
	const int x1 = std::min(kGuiWidth, x + w);
	const int y1 = std::min(kGuiHeight, y + h);
	x = std::max(0, x);
	y = std::max(0, y);
	for (int yy = y; yy < y1; ++yy) {
		uint32_t *row = pixels_.data() + yy * kGuiWidth;
		for (int xx = x; xx < x1; ++xx) {
			row[xx] = color;
		}
	}
}

void GuiView::hline(int x0, int x1, int y, uint32_t color) {
	if (y < 0 || y >= kGuiHeight) {
		return;
	}
	if (x0 > x1) {
		std::swap(x0, x1);
	}
	x0 = std::max(0, x0);
	x1 = std::min(kGuiWidth - 1, x1);
	uint32_t *row = pixels_.data() + y * kGuiWidth;
	for (int x = x0; x <= x1; ++x) {
		row[x] = color;
	}
}

void GuiView::vline(int x, int y0, int y1, uint32_t color) {
	if (x < 0 || x >= kGuiWidth) {
		return;
	}
	if (y0 > y1) {
		std::swap(y0, y1);
	}
	y0 = std::max(0, y0);
	y1 = std::min(kGuiHeight - 1, y1);
	for (int y = y0; y <= y1; ++y) {
		pixels_[static_cast<size_t>(y * kGuiWidth + x)] = color;
	}
}

void GuiView::draw_text(int x, int y, const char *text, uint32_t color) {
	int cx = x;
	for (const char *p = text; p && *p; ++p) {
		unsigned char ch = static_cast<unsigned char>(*p);
		if (ch == '\n') {
			y += 10;
			cx = x;
			continue;
		}
		if (ch < 32 || ch > 126) {
			ch = '?';
		}
		const uint8_t *g = kFont[ch - 32];
		for (int col = 0; col < 5; ++col) {
			uint8_t bits = g[col];
			for (int row = 0; row < 7; ++row) {
				if (bits & (1u << row)) {
					const int px = cx + col;
					const int py = y + row;
					if (px >= 0 && py >= 0 && px < kGuiWidth && py < kGuiHeight) {
						pixels_[static_cast<size_t>(py * kGuiWidth + px)] = color;
					}
				}
			}
		}
		cx += 6;
	}
}

bool GuiView::hit_button(int x, int y, int bx, int by, int bw, int bh) const {
	return x >= bx && y >= by && x < bx + bw && y < by + bh;
}

uint64_t GuiView::x_to_frame(int x) const {
	const double rel = static_cast<double>(x - wave_x_) * frames_per_pixel_ + view_start_;
	if (rel < 0.0) {
		return 0;
	}
	return static_cast<uint64_t>(rel);
}

int GuiView::frame_to_x(uint64_t frame) const {
	if (frames_per_pixel_ <= 0.0) {
		return wave_x_;
	}
	const double x = wave_x_ + (static_cast<double>(frame) - view_start_) / frames_per_pixel_;
	return static_cast<int>(std::lround(x));
}

void GuiView::clamp_view() {
	const AudioBuffer *src = sampler_->source();
	const double frames = src ? static_cast<double>(src->frame_count) : 1.0;
	const double min_fpp = std::max(frames / static_cast<double>(wave_w_), 0.05);
	if (frames_per_pixel_ < 0.05) {
		frames_per_pixel_ = 0.05;
	}
	if (frames_per_pixel_ > min_fpp) {
		frames_per_pixel_ = min_fpp;
	}
	const double view_len = frames_per_pixel_ * static_cast<double>(wave_w_);
	if (view_start_ < 0.0) {
		view_start_ = 0.0;
	}
	if (view_start_ + view_len > frames) {
		view_start_ = std::max(0.0, frames - view_len);
	}
}

void GuiView::zoom_at(int x, double factor) {
	const uint64_t anchor = x_to_frame(x);
	frames_per_pixel_ *= factor;
	clamp_view();
	const double new_x = frame_to_x(anchor);
	view_start_ += (new_x - x) * frames_per_pixel_;
	clamp_view();
	dirty_ = true;
}

void GuiView::open_file_browser() {
	file_browser_ = true;
	if (browse_dir_.empty()) {
		browse_dir_ = default_browse_dir();
	}
	browse_scroll_ = 0;
	refresh_dir();
	dirty_ = true;
}

void GuiView::refresh_dir() {
	browse_names_.clear();
	browse_is_dir_.clear();
	browse_names_.push_back("..");
	browse_is_dir_.push_back(true);
	std::error_code ec;
	const std::filesystem::path dir(browse_dir_);
	if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
		status_ = "Cannot open directory";
		return;
	}
	std::vector<std::string> dirs;
	std::vector<std::string> files;
	std::filesystem::directory_iterator it(dir, ec);
	for (; it != std::filesystem::directory_iterator() && !ec; it.increment(ec)) {
		const std::filesystem::path child = it->path();
		const std::string name = child.filename().string();
		if (name == "." || name == "..") {
			continue;
		}
		std::error_code type_ec;
		if (std::filesystem::is_directory(child, type_ec)) {
			dirs.push_back(name);
		} else if (is_audio_name(name)) {
			files.push_back(name);
		}
	}
	std::sort(dirs.begin(), dirs.end());
	std::sort(files.begin(), files.end());
	for (const auto &d : dirs) {
		browse_names_.push_back(d);
		browse_is_dir_.push_back(true);
	}
	for (const auto &f : files) {
		browse_names_.push_back(f);
		browse_is_dir_.push_back(false);
	}
}

void GuiView::apply_budget_from_x(int x) {
	const int slx = 16;
	const int slw = 280;
	double t = static_cast<double>(x - slx) / static_cast<double>(slw);
	t = std::max(0.0, std::min(1.0, t));
	const double kb = 64.0 + t * (8192.0 - 64.0);
	SamplerSettings s = sampler_->settings();
	s.budget_bytes = static_cast<uint64_t>(std::lround(kb)) * 1024ull;
	sampler_->set_settings(s);
	dirty_ = true;
}

void GuiView::mouse_down(int x, int y, int button) {
	mouse_x_ = x;
	mouse_y_ = y;
	dirty_ = true;
	if (button == 4) {
		mouse_wheel(x, y, 1);
		return;
	}
	if (button == 5) {
		mouse_wheel(x, y, -1);
		return;
	}
	if (button == 2) {
		drag_ = DragTarget::Pan;
		drag_last_x_ = x;
		return;
	}
	if (button != 1) {
		return;
	}
	mouse_left_ = true;

	if (file_browser_) {
		const int list_y = 80;
		const int row_h = 16;
		if (hit_button(x, y, kGuiWidth - 90, 16, 74, 22)) {
			file_browser_ = false;
			return;
		}
		if (y >= list_y) {
			const int row = (y - list_y) / row_h + browse_scroll_;
			if (row >= 0 && row < static_cast<int>(browse_names_.size())) {
				const std::string name = browse_names_[static_cast<size_t>(row)];
				if (browse_is_dir_[static_cast<size_t>(row)]) {
					if (name == "..") {
						browse_dir_ = parent_browse_dir(browse_dir_);
					} else {
						browse_dir_ = join_browse_path(browse_dir_, name);
					}
					browse_scroll_ = 0;
					refresh_dir();
				} else {
					const std::string path = join_browse_path(browse_dir_, name);
					const DecodeResult result = sampler_->load_path(path);
					if (result.error == DecodeError::None) {
						status_ = "Loaded " + name;
						file_browser_ = false;
						view_start_ = 0.0;
						if (sampler_->source()) {
							frames_per_pixel_ =
								static_cast<double>(sampler_->source()->frame_count) / static_cast<double>(wave_w_);
						}
					} else {
						status_ = "Load failed: " + result.message;
					}
				}
			}
		}
		return;
	}

	auto toggle_rate = [&](SampleRateOption opt) {
		SamplerSettings s = sampler_->settings();
		s.sample_rate = opt;
		sampler_->set_settings(s);
	};

	if (hit_button(x, y, 16, 12, 88, 24)) {
		open_file_browser();
		return;
	}
	if (hit_button(x, y, 112, 12, 88, 24)) {
		const CommitDecision d = sampler_->commit();
		if (d.accepted) {
			status_ = "Committed " + format_bytes(d.byte_size) + " at 16-bit.";
		} else {
			status_ = "Rejected: " + format_bytes(d.byte_size) + " exceeds " + format_bytes(d.budget) +
				". Shorten the region.";
		}
		return;
	}
	if (hit_button(x, y, 208, 12, 70, 24)) {
		toggle_rate(SampleRateOption::Rate44100);
		return;
	}
	if (hit_button(x, y, 282, 12, 78, 24)) {
		toggle_rate(SampleRateOption::Rate29760);
		return;
	}
	if (hit_button(x, y, 16, 44, 88, 22)) {
		if (sampler_->preview_playing()) {
			sampler_->stop_preview();
			status_ = "Preview stopped.";
		} else {
			sampler_->start_preview();
			status_ = "Previewing selection (looped if enabled).";
		}
		return;
	}
	if (hit_button(x, y, 112, 44, 100, 22)) {
		SamplerSettings s = sampler_->settings();
		s.preview_loop = !s.preview_loop;
		sampler_->set_settings(s);
		return;
	}
	if (hit_button(x, y, 220, 44, 72, 22)) {
		SamplerSettings s = sampler_->settings();
		s.mono_downmix = !s.mono_downmix;
		sampler_->set_settings(s);
		return;
	}
	if (hit_button(x, y, 300, 44, 88, 22)) {
		SamplerSettings s = sampler_->settings();
		s.snap_zero_crossing = !s.snap_zero_crossing;
		sampler_->set_settings(s);
		return;
	}

	auto cycle_loop = [&](LoopMode mode) {
		SamplerSettings s = sampler_->settings();
		s.loop_mode = mode;
		sampler_->set_settings(s);
	};
	if (hit_button(x, y, 400, 44, 70, 22)) {
		cycle_loop(LoopMode::OneShot);
		return;
	}
	if (hit_button(x, y, 474, 44, 78, 22)) {
		cycle_loop(LoopMode::Forward);
		return;
	}
	if (hit_button(x, y, 556, 44, 88, 22)) {
		cycle_loop(LoopMode::PingPong);
		return;
	}

	if (hit_button(x, y, 16, 74, 280, 18)) {
		drag_ = DragTarget::Budget;
		apply_budget_from_x(x);
		return;
	}

	auto slider_hit = [&](int sx, int sy, DragTarget t) {
		if (hit_button(x, y, sx, sy, 160, 16)) {
			drag_ = t;
			mouse_move(x, y);
			return true;
		}
		return false;
	};
	if (slider_hit(16, 430, DragTarget::Attack) || slider_hit(200, 430, DragTarget::Decay) ||
		slider_hit(384, 430, DragTarget::Sustain) || slider_hit(568, 430, DragTarget::Release) ||
		slider_hit(752, 430, DragTarget::Root)) {
		return;
	}

	if (x >= wave_x_ && x < wave_x_ + wave_w_ && y >= wave_y_ && y < wave_y_ + wave_h_) {
		const int rs = frame_to_x(sampler_->region_start());
		const int re = frame_to_x(sampler_->region_end());
		const int ls = frame_to_x(sampler_->loop_start());
		const int le = frame_to_x(sampler_->loop_end());
		auto near = [&](int mx) { return std::abs(x - mx) <= 6; };
		if (near(rs)) {
			drag_ = DragTarget::RegionStart;
		} else if (near(re)) {
			drag_ = DragTarget::RegionEnd;
		} else if (near(ls)) {
			drag_ = DragTarget::LoopStart;
		} else if (near(le)) {
			drag_ = DragTarget::LoopEnd;
		} else {
			drag_ = DragTarget::Scrub;
			sampler_->scrub_preview(x_to_frame(x));
			if (!sampler_->preview_playing()) {
				sampler_->start_preview();
			}
		}
	}
}

void GuiView::mouse_up(int x, int y, int button) {
	(void)x;
	(void)y;
	if (button == 1) {
		mouse_left_ = false;
		drag_ = DragTarget::None;
	}
	if (button == 2) {
		drag_ = DragTarget::None;
	}
	dirty_ = true;
}

void GuiView::mouse_move(int x, int y) {
	mouse_x_ = x;
	mouse_y_ = y;
	if (drag_ == DragTarget::Pan) {
		const int dx = x - drag_last_x_;
		view_start_ -= static_cast<double>(dx) * frames_per_pixel_;
		drag_last_x_ = x;
		clamp_view();
		dirty_ = true;
		return;
	}
	if (drag_ == DragTarget::Budget) {
		apply_budget_from_x(x);
		return;
	}
	auto set_time = [&](float *dst, float min_v, float max_v, int sx) {
		double t = static_cast<double>(x - sx) / 160.0;
		t = std::max(0.0, std::min(1.0, t));
		*dst = static_cast<float>(min_v + t * (max_v - min_v));
	};
	if (drag_ == DragTarget::Attack || drag_ == DragTarget::Decay || drag_ == DragTarget::Sustain ||
		drag_ == DragTarget::Release || drag_ == DragTarget::Root) {
		SamplerSettings s = sampler_->settings();
		switch (drag_) {
		case DragTarget::Attack:
			set_time(&s.adsr.attack, 0.0f, 1.0f, 16);
			break;
		case DragTarget::Decay:
			set_time(&s.adsr.decay, 0.0f, 1.0f, 200);
			break;
		case DragTarget::Sustain:
			set_time(&s.adsr.sustain, 0.0f, 1.0f, 384);
			break;
		case DragTarget::Release:
			set_time(&s.adsr.release, 0.0f, 2.0f, 568);
			break;
		case DragTarget::Root: {
			double t = static_cast<double>(x - 752) / 160.0;
			t = std::max(0.0, std::min(1.0, t));
			s.root_note = static_cast<int>(std::lround(t * 127.0));
			break;
		}
		case DragTarget::None:
		case DragTarget::RegionStart:
		case DragTarget::RegionEnd:
		case DragTarget::LoopStart:
		case DragTarget::LoopEnd:
		case DragTarget::Scrub:
		case DragTarget::Pan:
		case DragTarget::Budget:
			break;
		}
		sampler_->set_settings(s);
		dirty_ = true;
		return;
	}

	if (drag_ == DragTarget::None || sampler_->source() == nullptr) {
		return;
	}
	uint64_t frame = x_to_frame(x);
	const uint64_t n = sampler_->source()->frame_count;
	if (frame > n) {
		frame = n;
	}
	switch (drag_) {
	case DragTarget::RegionStart:
		sampler_->set_region(frame, sampler_->region_end());
		break;
	case DragTarget::RegionEnd:
		sampler_->set_region(sampler_->region_start(), frame);
		break;
	case DragTarget::LoopStart:
		sampler_->set_loop(frame, sampler_->loop_end());
		break;
	case DragTarget::LoopEnd:
		sampler_->set_loop(sampler_->loop_start(), frame);
		break;
	case DragTarget::Scrub:
		sampler_->scrub_preview(frame);
		break;
	default:
		break;
	}
	dirty_ = true;
}

void GuiView::mouse_wheel(int x, int y, int delta) {
	if (file_browser_) {
		browse_scroll_ = std::max(0, browse_scroll_ - delta * 3);
		dirty_ = true;
		return;
	}
	if (x >= wave_x_ && x < wave_x_ + wave_w_ && y >= wave_y_ && y < wave_y_ + wave_h_) {
		zoom_at(x, delta > 0 ? 0.85 : 1.18);
	}
}

void GuiView::key_down(int keysym) {
	// XK_space = 0x0020, XK_Escape = 0xff1b, XK_plus = 0x002b, XK_minus = 0x002d
	if (keysym == 0xff1b) {
		file_browser_ = false;
		dirty_ = true;
		return;
	}
	if (keysym == 0x0020) {
		if (sampler_->preview_playing()) {
			sampler_->stop_preview();
		} else {
			sampler_->start_preview();
		}
		dirty_ = true;
		return;
	}
	if (keysym == 0x002b || keysym == 0xffab) {
		zoom_at(wave_x_ + wave_w_ / 2, 0.85);
	}
	if (keysym == 0x002d || keysym == 0xffad) {
		zoom_at(wave_x_ + wave_w_ / 2, 1.18);
	}
}

void GuiView::draw_waveform() {
	fill_rect(wave_x_, wave_y_, wave_w_, wave_h_, 0xFF1A140F);
	hline(wave_x_, wave_x_ + wave_w_, wave_y_ + wave_h_ / 2, 0xFF3A2A1C);

	const AudioBuffer *src = sampler_->source();
	if (src == nullptr || src->frame_count == 0) {
		draw_text(wave_x_ + 12, wave_y_ + wave_h_ / 2 - 4, "No sample loaded", kMuted);
		return;
	}
	clamp_view();
	const int ch = static_cast<int>(src->channels);
	for (int x = 0; x < wave_w_; ++x) {
		const double f0 = view_start_ + static_cast<double>(x) * frames_per_pixel_;
		const double f1 = f0 + std::max(1.0, frames_per_pixel_);
		const uint64_t i0 = static_cast<uint64_t>(std::max(0.0, f0));
		uint64_t i1 = static_cast<uint64_t>(std::min(static_cast<double>(src->frame_count), f1));
		if (i1 <= i0) {
			i1 = i0 + 1;
		}
		if (i0 >= src->frame_count) {
			continue;
		}
		float mn = 1.0f;
		float mx = -1.0f;
		for (uint64_t i = i0; i < i1 && i < src->frame_count; ++i) {
			float mix = 0.0f;
			for (int c = 0; c < ch; ++c) {
				mix += src->interleaved[i * static_cast<uint64_t>(ch) + static_cast<uint64_t>(c)];
			}
			mix /= static_cast<float>(ch);
			mn = std::min(mn, mix);
			mx = std::max(mx, mix);
		}
		const int mid = wave_y_ + wave_h_ / 2;
		const int y0 = mid - static_cast<int>(mx * (wave_h_ / 2 - 4));
		const int y1 = mid - static_cast<int>(mn * (wave_h_ / 2 - 4));
		vline(wave_x_ + x, y0, y1, kAmberDim);
	}

	auto marker = [&](uint64_t frame, uint32_t color, const char *label, bool tall) {
		const int x = frame_to_x(frame);
		if (x < wave_x_ || x >= wave_x_ + wave_w_) {
			return;
		}
		vline(x, wave_y_, wave_y_ + (tall ? wave_h_ : wave_h_ * 2 / 3), color);
		fill_rect(x - 3, wave_y_, 7, 10, color);
		draw_text(x + 6, wave_y_ + 2, label, color);
	};
	marker(sampler_->region_start(), kRegion, "IN", true);
	marker(sampler_->region_end(), kRegion, "OUT", true);
	marker(sampler_->loop_start(), kLoop, "LS", false);
	marker(sampler_->loop_end(), kLoop, "LE", false);

	if (sampler_->preview_playing()) {
		const int x = frame_to_x(static_cast<uint64_t>(sampler_->preview_position()));
		if (x >= wave_x_ && x < wave_x_ + wave_w_) {
			vline(x, wave_y_, wave_y_ + wave_h_, kPlay);
		}
	}

	// Horizontal scrollbar / view window.
	fill_rect(wave_x_, wave_y_ + wave_h_ + 4, wave_w_, 8, 0xFF2A2018);
	if (src->frame_count > 0) {
		const double span = frames_per_pixel_ * wave_w_;
		const int thumb_x = wave_x_ + static_cast<int>(view_start_ / static_cast<double>(src->frame_count) * wave_w_);
		const int thumb_w = std::max(8, static_cast<int>(span / static_cast<double>(src->frame_count) * wave_w_));
		fill_rect(thumb_x, wave_y_ + wave_h_ + 4, thumb_w, 8, kAmberDim);
	}
}

void GuiView::draw_controls() {
	auto button = [&](int x, int y, int w, int h, const char *label, bool on) {
		fill_rect(x, y, w, h, on ? kButtonOn : kButton);
		hline(x, x + w, y, kAmberDim);
		draw_text(x + 8, y + 8, label, kText);
	};

	const SamplerSettings s = sampler_->settings();
	button(16, 12, 88, 24, "LOAD", false);
	button(112, 12, 88, 24, "COMMIT", sampler_->has_committed());
	button(208, 12, 70, 24, "44.1k", s.sample_rate == SampleRateOption::Rate44100);
	button(282, 12, 78, 24, "29.76k", s.sample_rate == SampleRateOption::Rate29760);
	button(16, 44, 88, 22, sampler_->preview_playing() ? "STOP" : "AUDITION", sampler_->preview_playing());
	button(112, 44, 100, 22, "PREV LOOP", s.preview_loop);
	button(220, 44, 72, 22, "MONO", s.mono_downmix);
	button(300, 44, 88, 22, "SNAP ZX", s.snap_zero_crossing);
	button(400, 44, 70, 22, "ONE-SHOT", s.loop_mode == LoopMode::OneShot);
	button(474, 44, 78, 22, "FORWARD", s.loop_mode == LoopMode::Forward);
	button(556, 44, 88, 22, "PINGPONG", s.loop_mode == LoopMode::PingPong);

	const uint64_t used = sampler_->current_selection_bytes();
	const uint64_t budget = s.budget_bytes;
	const bool ok = fits_budget(used, budget);
	char memline[160];
	std::snprintf(
		memline,
		sizeof(memline),
		"MEMORY  %s / %s   %s",
		format_bytes(used).c_str(),
		format_bytes(budget).c_str(),
		ok ? "FITS" : "OVER BUDGET");
	draw_text(370, 18, memline, ok ? kGood : kBad);

	fill_rect(16, 74, 280, 18, 0xFF2A2018);
	const double t = std::max(0.0, std::min(1.0, (static_cast<double>(budget) / 1024.0 - 64.0) / (8192.0 - 64.0)));
	fill_rect(16, 74, std::max(2, static_cast<int>(t * 280.0)), 18, ok ? kGood : kBad);
	draw_text(304, 78, "BUDGET (64KB-8MB)", kMuted);

	auto slider = [&](int x, int y, const char *label, double t01, const char *value) {
		draw_text(x, y - 12, label, kMuted);
		fill_rect(x, y, 160, 12, 0xFF2A2018);
		fill_rect(x, y, std::max(2, static_cast<int>(t01 * 160.0)), 12, kAmber);
		draw_text(x + 168, y + 2, value, kText);
	};
	char vbuf[32];
	std::snprintf(vbuf, sizeof(vbuf), "%.3fs", s.adsr.attack);
	slider(16, 430, "ATTACK", s.adsr.attack / 1.0, vbuf);
	std::snprintf(vbuf, sizeof(vbuf), "%.3fs", s.adsr.decay);
	slider(200, 430, "DECAY", s.adsr.decay / 1.0, vbuf);
	std::snprintf(vbuf, sizeof(vbuf), "%.2f", s.adsr.sustain);
	slider(384, 430, "SUSTAIN", s.adsr.sustain, vbuf);
	std::snprintf(vbuf, sizeof(vbuf), "%.3fs", s.adsr.release);
	slider(568, 430, "RELEASE", s.adsr.release / 2.0, vbuf);
	std::snprintf(vbuf, sizeof(vbuf), "%s", note_name(s.root_note).c_str());
	slider(752, 430, "ROOT", s.root_note / 127.0, vbuf);

	draw_text(16, 470, "Wheel zooms waveform. Middle-drag pans. Drag IN/OUT (region) and LS/LE (loop). No loop crossfade.", kMuted);
	draw_text(16, 486, "Root note plays captured pitch. Other keys change playback rate. 16-bit / budget applied on COMMIT.", kMuted);
	draw_text(16, 510, status_.c_str(), kText);

	if (sampler_->source()) {
		char info[192];
		std::snprintf(
			info,
			sizeof(info),
			"SOURCE  %.0f Hz  %u ch  %llu frames   REGION %llu-%llu",
			sampler_->source()->sample_rate,
			sampler_->source()->channels,
			static_cast<unsigned long long>(sampler_->source()->frame_count),
			static_cast<unsigned long long>(sampler_->region_start()),
			static_cast<unsigned long long>(sampler_->region_end()));
		draw_text(16, 530, info, kMuted);
	}
}

void GuiView::draw_file_browser() {
	fill_rect(20, 20, kGuiWidth - 40, kGuiHeight - 40, kPanel);
	draw_text(36, 32, ("LOAD WAV / MP3  —  " + browse_dir_).c_str(), kAmber);
	fill_rect(kGuiWidth - 90, 16, 74, 22, kButton);
	draw_text(kGuiWidth - 78, 22, "CLOSE", kText);
	const int list_y = 80;
	const int row_h = 16;
	const int visible = (kGuiHeight - 120) / row_h;
	for (int i = 0; i < visible; ++i) {
		const int idx = i + browse_scroll_;
		if (idx < 0 || idx >= static_cast<int>(browse_names_.size())) {
			break;
		}
		const int y = list_y + i * row_h;
		const bool is_dir = browse_is_dir_[static_cast<size_t>(idx)];
		if (is_dir) {
			fill_rect(36, y - 2, kGuiWidth - 80, row_h, 0xFF2A2218);
		}
		draw_text(44, y, browse_names_[static_cast<size_t>(idx)].c_str(), is_dir ? kAmber : kText);
	}
}

void GuiView::paint() {
	std::fill(pixels_.begin(), pixels_.end(), kBg);
	fill_rect(0, 0, kGuiWidth, 100, kPanel);
	fill_rect(0, 416, kGuiWidth, kGuiHeight - 416, kPanel);
	draw_text(370, 40, "1.44  ASR-10 CONSTRAINT SAMPLER", kAmber);
	draw_waveform();
	draw_controls();
	if (file_browser_) {
		draw_file_browser();
	}
	dirty_ = true;
}

} // namespace one44
