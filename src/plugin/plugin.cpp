#include "core/pitch.hpp"
#include "core/sampler.hpp"
#include "gui/gui.hpp"
#include "gui/gui_window.hpp"

#include <clap/clap.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace one44 {

enum ParamId : clap_id {
	kParamSampleRate = 0,
	kParamBudgetKb = 1,
	kParamMono = 2,
	kParamLoopMode = 3,
	kParamSnap = 4,
	kParamRoot = 5,
	kParamAttack = 6,
	kParamDecay = 7,
	kParamSustain = 8,
	kParamRelease = 9,
	kParamPreviewLoop = 10,
	kParamCount = 11,
};

const char *features[] = {
	CLAP_PLUGIN_FEATURE_INSTRUMENT,
	CLAP_PLUGIN_FEATURE_SAMPLER,
	CLAP_PLUGIN_FEATURE_STEREO,
	nullptr,
};

const clap_plugin_descriptor_t descriptor = {
	CLAP_VERSION_INIT,
	"com.enazzzz.one-four-four",
	"1.44",
	"1.44",
	"https://github.com/Enazzzz/1.44",
	"https://github.com/Enazzzz/1.44",
	"https://github.com/Enazzzz/1.44",
	"1.0.0",
	"ASR-10-era constraint sampler. Load audio, select a region, keep only what fits.",
	features,
};

/// CLAP instrument instance: constraint sampler + embedded GUI.
struct Plugin {
	clap_plugin_t clap{};
	const clap_host_t *host = nullptr;
	Sampler sampler;
	std::unique_ptr<GuiView> gui;
	GuiWindow window;
	double host_sr = 44100.0;
	bool gui_created = false;
	bool gui_floating = false;
	clap_id timer_id = CLAP_INVALID_ID;
	int posix_fd = -1;
	const clap_host_timer_support_t *host_timer = nullptr;
	const clap_host_posix_fd_support_t *host_fd = nullptr;
	const clap_host_log_t *host_log = nullptr;

	static Plugin *self(const clap_plugin_t *plugin) {
		return static_cast<Plugin *>(plugin->plugin_data);
	}

	void apply_param(clap_id id, double value);
	double get_param(clap_id id) const;
	void sync_gui();
};

/// Writes a param's current value from sampler settings.
double Plugin::get_param(clap_id id) const {
	const SamplerSettings s = sampler.settings();
	switch (id) {
	case kParamSampleRate:
		return s.sample_rate == SampleRateOption::Rate29760 ? 1.0 : 0.0;
	case kParamBudgetKb:
		return static_cast<double>(s.budget_bytes / 1024ull);
	case kParamMono:
		return s.mono_downmix ? 1.0 : 0.0;
	case kParamLoopMode:
		return static_cast<double>(loop_mode_to_int(s.loop_mode));
	case kParamSnap:
		return s.snap_zero_crossing ? 1.0 : 0.0;
	case kParamRoot:
		return static_cast<double>(s.root_note);
	case kParamAttack:
		return s.adsr.attack;
	case kParamDecay:
		return s.adsr.decay;
	case kParamSustain:
		return s.adsr.sustain;
	case kParamRelease:
		return s.adsr.release;
	case kParamPreviewLoop:
		return s.preview_loop ? 1.0 : 0.0;
	default:
		return 0.0;
	}
}

/// Applies a host (or GUI) parameter change onto the sampler.
void Plugin::apply_param(clap_id id, double value) {
	SamplerSettings s = sampler.settings();
	auto flag = [&](double v) { return v >= 0.5; };
	switch (id) {
	case kParamSampleRate:
		s.sample_rate = static_cast<int>(value) >= 1 ? SampleRateOption::Rate29760 : SampleRateOption::Rate44100;
		break;
	case kParamBudgetKb: {
		const double kb = std::clamp(value, 64.0, 8192.0);
		s.budget_bytes = static_cast<uint64_t>(std::lround(kb)) * 1024ull;
		break;
	}
	case kParamMono:
		s.mono_downmix = flag(value);
		break;
	case kParamLoopMode:
		s.loop_mode = loop_mode_from_int(static_cast<int>(value));
		break;
	case kParamSnap:
		s.snap_zero_crossing = flag(value);
		break;
	case kParamRoot:
		s.root_note = clamp_midi_note(static_cast<int>(std::lround(value)));
		break;
	case kParamAttack:
		s.adsr.attack = static_cast<float>(std::clamp(value, 0.0, 1.0));
		break;
	case kParamDecay:
		s.adsr.decay = static_cast<float>(std::clamp(value, 0.0, 1.0));
		break;
	case kParamSustain:
		s.adsr.sustain = static_cast<float>(std::clamp(value, 0.0, 1.0));
		break;
	case kParamRelease:
		s.adsr.release = static_cast<float>(std::clamp(value, 0.0, 2.0));
		break;
	case kParamPreviewLoop:
		s.preview_loop = flag(value);
		break;
	default:
		break;
	}
	sampler.set_settings(s);
}

void Plugin::sync_gui() {
	if (gui && window.valid()) {
		window.process_events();
		window.blit();
	}
}

bool plugin_init(const clap_plugin_t *plugin) {
	Plugin *p = Plugin::self(plugin);
	p->host_timer = static_cast<const clap_host_timer_support_t *>(
		p->host->get_extension(p->host, CLAP_EXT_TIMER_SUPPORT));
	p->host_fd = static_cast<const clap_host_posix_fd_support_t *>(
		p->host->get_extension(p->host, CLAP_EXT_POSIX_FD_SUPPORT));
	p->host_log = static_cast<const clap_host_log_t *>(p->host->get_extension(p->host, CLAP_EXT_LOG));
	return true;
}

void plugin_destroy(const clap_plugin_t *plugin) {
	delete Plugin::self(plugin);
}

bool plugin_activate(const clap_plugin_t *plugin, double sample_rate, uint32_t, uint32_t) {
	Plugin::self(plugin)->host_sr = sample_rate;
	return true;
}

void plugin_deactivate(const clap_plugin_t *) {}

bool plugin_start_processing(const clap_plugin_t *) {
	return true;
}

void plugin_stop_processing(const clap_plugin_t *) {}

void plugin_reset(const clap_plugin_t *plugin) {
	Plugin::self(plugin)->sampler.choke_all();
}

void handle_event(Plugin *p, const clap_event_header_t *hdr) {
	if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) {
		return;
	}
	switch (hdr->type) {
	case CLAP_EVENT_NOTE_ON: {
		const auto *ev = reinterpret_cast<const clap_event_note_t *>(hdr);
		p->sampler.note_on(
			ev->key,
			static_cast<float>(ev->velocity),
			ev->note_id,
			ev->channel,
			ev->port_index,
			p->host_sr);
		break;
	}
	case CLAP_EVENT_NOTE_OFF: {
		const auto *ev = reinterpret_cast<const clap_event_note_t *>(hdr);
		p->sampler.note_off(ev->key, ev->note_id, ev->channel, ev->port_index);
		break;
	}
	case CLAP_EVENT_NOTE_CHOKE: {
		p->sampler.choke_all();
		break;
	}
	case CLAP_EVENT_PARAM_VALUE: {
		const auto *ev = reinterpret_cast<const clap_event_param_value_t *>(hdr);
		p->apply_param(ev->param_id, ev->value);
		break;
	}
	case CLAP_EVENT_MIDI: {
		const auto *ev = reinterpret_cast<const clap_event_midi_t *>(hdr);
		const uint8_t status = ev->data[0] & 0xF0;
		const uint8_t chan = ev->data[0] & 0x0F;
		const uint8_t data1 = ev->data[1];
		const uint8_t data2 = ev->data[2];
		if (status == 0x90 && data2 > 0) {
			p->sampler.note_on(data1, data2 / 127.0f, -1, chan, ev->port_index, p->host_sr);
		} else if (status == 0x80 || (status == 0x90 && data2 == 0)) {
			p->sampler.note_off(data1, -1, chan, ev->port_index);
		}
		break;
	}
	default:
		break;
	}
}

clap_process_status plugin_process(const clap_plugin_t *plugin, const clap_process_t *process) {
	Plugin *p = Plugin::self(plugin);
	const uint32_t nframes = process->frames_count;
	if (process->audio_outputs_count < 1 || process->audio_outputs == nullptr ||
		process->audio_outputs[0].data32 == nullptr || process->audio_outputs[0].channel_count < 2) {
		return CLAP_PROCESS_ERROR;
	}
	float *left = process->audio_outputs[0].data32[0];
	float *right = process->audio_outputs[0].data32[1];
	if (left == nullptr || right == nullptr) {
		return CLAP_PROCESS_ERROR;
	}

	const uint32_t nev = process->in_events ? process->in_events->size(process->in_events) : 0;
	uint32_t ev_index = 0;
	uint32_t next_ev_frame = nev > 0 ? 0 : nframes;

	uint32_t i = 0;
	while (i < nframes) {
		while (ev_index < nev && next_ev_frame == i) {
			const clap_event_header_t *hdr = process->in_events->get(process->in_events, ev_index);
			if (hdr->time != i) {
				next_ev_frame = hdr->time;
				break;
			}
			handle_event(p, hdr);
			++ev_index;
			if (ev_index == nev) {
				next_ev_frame = nframes;
				break;
			}
		}

		const uint32_t chunk = next_ev_frame - i;
		p->sampler.render(left + i, right + i, chunk, p->host_sr);
		i += chunk;
	}
	return CLAP_PROCESS_CONTINUE;
}

uint32_t audio_ports_count(const clap_plugin_t *, bool is_input) {
	return is_input ? 0 : 1;
}

bool audio_ports_get(const clap_plugin_t *, uint32_t index, bool is_input, clap_audio_port_info_t *info) {
	if (is_input || index > 0) {
		return false;
	}
	info->id = 0;
	std::snprintf(info->name, sizeof(info->name), "Output");
	info->channel_count = 2;
	info->flags = CLAP_AUDIO_PORT_IS_MAIN;
	info->port_type = CLAP_PORT_STEREO;
	info->in_place_pair = CLAP_INVALID_ID;
	return true;
}

const clap_plugin_audio_ports_t audio_ports = {
	audio_ports_count,
	audio_ports_get,
};

uint32_t note_ports_count(const clap_plugin_t *, bool is_input) {
	return is_input ? 1 : 0;
}

bool note_ports_get(const clap_plugin_t *, uint32_t index, bool is_input, clap_note_port_info_t *info) {
	if (!is_input || index > 0) {
		return false;
	}
	info->id = 0;
	info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
	info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
	std::snprintf(info->name, sizeof(info->name), "Notes");
	return true;
}

const clap_plugin_note_ports_t note_ports = {
	note_ports_count,
	note_ports_get,
};

uint32_t params_count(const clap_plugin_t *) {
	return kParamCount;
}

bool params_get_info(const clap_plugin_t *, uint32_t index, clap_param_info_t *info) {
	if (index >= kParamCount) {
		return false;
	}
	std::memset(info, 0, sizeof(*info));
	info->id = index;
	info->cookie = nullptr;
	std::snprintf(info->module, sizeof(info->module), "Sampler");
	switch (index) {
	case kParamSampleRate:
		info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Sample Rate");
		info->min_value = 0;
		info->max_value = 1;
		info->default_value = 0;
		break;
	case kParamBudgetKb:
		info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Memory Budget (KB)");
		info->min_value = 64;
		info->max_value = 8192;
		info->default_value = 1440;
		break;
	case kParamMono:
		info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Mono Downmix");
		info->min_value = 0;
		info->max_value = 1;
		info->default_value = 0;
		break;
	case kParamLoopMode:
		info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Loop Mode");
		info->min_value = 0;
		info->max_value = 2;
		info->default_value = 1;
		break;
	case kParamSnap:
		info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Snap Zero Crossing");
		info->min_value = 0;
		info->max_value = 1;
		info->default_value = 0;
		break;
	case kParamRoot:
		info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Root Note");
		info->min_value = 0;
		info->max_value = 127;
		info->default_value = 60;
		break;
	case kParamAttack:
		info->flags = CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Attack");
		info->min_value = 0;
		info->max_value = 1;
		info->default_value = 0.005;
		break;
	case kParamDecay:
		info->flags = CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Decay");
		info->min_value = 0;
		info->max_value = 1;
		info->default_value = 0.080;
		break;
	case kParamSustain:
		info->flags = CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Sustain");
		info->min_value = 0;
		info->max_value = 1;
		info->default_value = 0.850;
		break;
	case kParamRelease:
		info->flags = CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Release");
		info->min_value = 0;
		info->max_value = 2;
		info->default_value = 0.120;
		break;
	case kParamPreviewLoop:
		info->flags = CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_AUTOMATABLE;
		std::snprintf(info->name, sizeof(info->name), "Preview Loop");
		info->min_value = 0;
		info->max_value = 1;
		info->default_value = 1;
		break;
	default:
		return false;
	}
	return true;
}

bool params_get_value(const clap_plugin_t *plugin, clap_id id, double *out) {
	if (id >= kParamCount) {
		return false;
	}
	*out = Plugin::self(plugin)->get_param(id);
	return true;
}

bool params_value_to_text(const clap_plugin_t *, clap_id id, double value, char *out, uint32_t cap) {
	if (cap == 0) {
		return false;
	}
	switch (id) {
	case kParamSampleRate:
		std::snprintf(out, cap, "%s", value >= 1.0 ? "29.76 kHz" : "44.1 kHz");
		return true;
	case kParamBudgetKb:
		std::snprintf(out, cap, "%d KB", static_cast<int>(value));
		return true;
	case kParamMono:
	case kParamSnap:
	case kParamPreviewLoop:
		std::snprintf(out, cap, "%s", value >= 0.5 ? "On" : "Off");
		return true;
	case kParamLoopMode: {
		const int m = static_cast<int>(value);
		const char *name = m == 2 ? "Ping-Pong" : (m == 1 ? "Forward" : "One-shot");
		std::snprintf(out, cap, "%s", name);
		return true;
	}
	case kParamRoot:
		std::snprintf(out, cap, "%d", static_cast<int>(value));
		return true;
	case kParamAttack:
	case kParamDecay:
	case kParamRelease:
		std::snprintf(out, cap, "%.3f s", value);
		return true;
	case kParamSustain:
		std::snprintf(out, cap, "%.2f", value);
		return true;
	default:
		return false;
	}
}

bool params_text_to_value(const clap_plugin_t *, clap_id id, const char *text, double *out) {
	if (!text || !out) {
		return false;
	}
	*out = std::atof(text);
	(void)id;
	return true;
}

void params_flush(const clap_plugin_t *plugin, const clap_input_events_t *in, const clap_output_events_t *) {
	Plugin *p = Plugin::self(plugin);
	const uint32_t n = in ? in->size(in) : 0;
	for (uint32_t i = 0; i < n; ++i) {
		handle_event(p, in->get(in, i));
	}
}

const clap_plugin_params_t params_ext = {
	params_count,
	params_get_info,
	params_get_value,
	params_value_to_text,
	params_text_to_value,
	params_flush,
};

bool state_save(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
	const std::vector<uint8_t> blob = Plugin::self(plugin)->sampler.save_state();
	size_t written = 0;
	while (written < blob.size()) {
		const int64_t n = stream->write(stream, blob.data() + written, blob.size() - written);
		if (n <= 0) {
			return false;
		}
		written += static_cast<size_t>(n);
	}
	return true;
}

bool state_load(const clap_plugin_t *plugin, const clap_istream_t *stream) {
	std::vector<uint8_t> blob;
	uint8_t buf[4096];
	for (;;) {
		const int64_t n = stream->read(stream, buf, sizeof(buf));
		if (n < 0) {
			return false;
		}
		if (n == 0) {
			break;
		}
		blob.insert(blob.end(), buf, buf + n);
	}
	return Plugin::self(plugin)->sampler.load_state(blob.data(), blob.size());
}

const clap_plugin_state_t state_ext = {
	state_save,
	state_load,
};

/// Native embedded window API for this build (X11 on Linux, Win32 HWND on Windows).
const char *native_gui_api() {
#ifdef _WIN32
	return CLAP_WINDOW_API_WIN32;
#elif defined(__linux__)
	return CLAP_WINDOW_API_X11;
#else
	return nullptr;
#endif
}

/// Extracts the host parent pointer from a CLAP window of the native API.
void *parent_from_clap_window(const clap_window_t *window) {
	if (window == nullptr || window->api == nullptr) {
		return nullptr;
	}
	const char *api = native_gui_api();
	if (api == nullptr || std::strcmp(window->api, api) != 0) {
		return nullptr;
	}
#ifdef _WIN32
	return window->win32;
#elif defined(__linux__)
	return reinterpret_cast<void *>(static_cast<uintptr_t>(window->x11));
#else
	return nullptr;
#endif
}

bool gui_is_api_supported(const clap_plugin_t *, const char *api, bool is_floating) {
	(void)is_floating;
	const char *native = native_gui_api();
	return native != nullptr && api != nullptr && std::strcmp(api, native) == 0;
}

bool gui_get_preferred_api(const clap_plugin_t *, const char **api, bool *is_floating) {
	const char *native = native_gui_api();
	if (native == nullptr || api == nullptr || is_floating == nullptr) {
		return false;
	}
	*api = native;
	*is_floating = false;
	return true;
}

bool gui_create(const clap_plugin_t *plugin, const char *api, bool is_floating) {
	if (!gui_is_api_supported(plugin, api, is_floating)) {
		return false;
	}
	Plugin *p = Plugin::self(plugin);
	p->gui = std::make_unique<GuiView>(&p->sampler);
	p->gui_created = true;
	p->gui_floating = is_floating;
	return true;
}

void gui_destroy(const clap_plugin_t *plugin) {
	Plugin *p = Plugin::self(plugin);
	if (p->host_timer && p->timer_id != CLAP_INVALID_ID) {
		p->host_timer->unregister_timer(p->host, p->timer_id);
		p->timer_id = CLAP_INVALID_ID;
	}
	if (p->host_fd && p->posix_fd >= 0) {
		p->host_fd->unregister_fd(p->host, p->posix_fd);
		p->posix_fd = -1;
	}
	p->window.destroy();
	p->gui.reset();
	p->gui_created = false;
}

bool gui_set_scale(const clap_plugin_t *, double) {
	return false;
}

bool gui_get_size(const clap_plugin_t *, uint32_t *width, uint32_t *height) {
	*width = kGuiWidth;
	*height = kGuiHeight;
	return true;
}

bool gui_can_resize(const clap_plugin_t *) {
	return false;
}

bool gui_get_resize_hints(const clap_plugin_t *, clap_gui_resize_hints_t *hints) {
	hints->can_resize_horizontally = false;
	hints->can_resize_vertically = false;
	hints->preserve_aspect_ratio = true;
	hints->aspect_ratio_width = kGuiWidth;
	hints->aspect_ratio_height = kGuiHeight;
	return true;
}

bool gui_adjust_size(const clap_plugin_t *, uint32_t *width, uint32_t *height) {
	*width = kGuiWidth;
	*height = kGuiHeight;
	return true;
}

bool gui_set_size(const clap_plugin_t *plugin, uint32_t, uint32_t) {
	return Plugin::self(plugin)->window.set_size(kGuiWidth, kGuiHeight);
}

bool gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window) {
	Plugin *p = Plugin::self(plugin);
	if (!p->gui) {
		return false;
	}
	void *parent = parent_from_clap_window(window);
	if (!p->window.create_embedded(parent, p->gui.get())) {
		return false;
	}
	p->posix_fd = p->window.posix_fd();
	if (p->host_fd && p->posix_fd >= 0) {
		p->host_fd->register_fd(p->host, p->posix_fd, CLAP_POSIX_FD_READ | CLAP_POSIX_FD_ERROR);
	}
	if (p->host_timer && p->timer_id == CLAP_INVALID_ID) {
		p->host_timer->register_timer(p->host, 33, &p->timer_id);
	}
	return true;
}

bool gui_set_transient(const clap_plugin_t *, const clap_window_t *) {
	return true;
}

void gui_suggest_title(const clap_plugin_t *, const char *) {}

bool gui_show(const clap_plugin_t *plugin) {
	Plugin *p = Plugin::self(plugin);
	p->window.show(true);
	p->window.blit();
	return p->window.valid();
}

bool gui_hide(const clap_plugin_t *plugin) {
	Plugin::self(plugin)->window.show(false);
	return true;
}

const clap_plugin_gui_t gui_ext = {
	gui_is_api_supported,
	gui_get_preferred_api,
	gui_create,
	gui_destroy,
	gui_set_scale,
	gui_get_size,
	gui_can_resize,
	gui_get_resize_hints,
	gui_adjust_size,
	gui_set_size,
	gui_set_parent,
	gui_set_transient,
	gui_suggest_title,
	gui_show,
	gui_hide,
};

void on_timer(const clap_plugin_t *plugin, clap_id) {
	Plugin::self(plugin)->sync_gui();
}

const clap_plugin_timer_support_t timer_ext = {on_timer};

#ifdef __linux__
void on_fd(const clap_plugin_t *plugin, int, clap_posix_fd_flags_t) {
	Plugin::self(plugin)->sync_gui();
}

const clap_plugin_posix_fd_support_t fd_ext = {on_fd};
#endif

const void *plugin_get_extension(const clap_plugin_t *, const char *id) {
	if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
		return &audio_ports;
	}
	if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) {
		return &note_ports;
	}
	if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
		return &params_ext;
	}
	if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
		return &state_ext;
	}
	if (std::strcmp(id, CLAP_EXT_GUI) == 0) {
		return &gui_ext;
	}
	if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) {
		return &timer_ext;
	}
#ifdef __linux__
	if (std::strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT) == 0) {
		return &fd_ext;
	}
#endif
	return nullptr;
}

void plugin_on_main_thread(const clap_plugin_t *plugin) {
	Plugin::self(plugin)->sync_gui();
}

clap_plugin_t *create_plugin(const clap_host_t *host) {
	auto *p = new Plugin();
	p->host = host;
	p->clap.desc = &descriptor;
	p->clap.plugin_data = p;
	p->clap.init = plugin_init;
	p->clap.destroy = plugin_destroy;
	p->clap.activate = plugin_activate;
	p->clap.deactivate = plugin_deactivate;
	p->clap.start_processing = plugin_start_processing;
	p->clap.stop_processing = plugin_stop_processing;
	p->clap.reset = plugin_reset;
	p->clap.process = plugin_process;
	p->clap.get_extension = plugin_get_extension;
	p->clap.on_main_thread = plugin_on_main_thread;
	return &p->clap;
}

uint32_t factory_get_plugin_count(const clap_plugin_factory *) {
	return 1;
}

const clap_plugin_descriptor_t *factory_get_plugin_descriptor(const clap_plugin_factory *, uint32_t index) {
	return index == 0 ? &descriptor : nullptr;
}

const clap_plugin_t *factory_create_plugin(
	const clap_plugin_factory *,
	const clap_host_t *host,
	const char *plugin_id) {
	if (!clap_version_is_compatible(host->clap_version)) {
		return nullptr;
	}
	if (std::strcmp(plugin_id, descriptor.id) != 0) {
		return nullptr;
	}
	return create_plugin(host);
}

const clap_plugin_factory_t factory = {
	factory_get_plugin_count,
	factory_get_plugin_descriptor,
	factory_create_plugin,
};

bool entry_init(const char *) {
	return true;
}

void entry_deinit() {}

const void *entry_get_factory(const char *factory_id) {
	if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
		return &factory;
	}
	return nullptr;
}

} // namespace one44

extern "C" {

extern const clap_plugin_entry_t clap_entry;

CLAP_EXPORT extern const clap_plugin_entry_t clap_entry = {
	CLAP_VERSION_INIT,
	one44::entry_init,
	one44::entry_deinit,
	one44::entry_get_factory,
};

}
