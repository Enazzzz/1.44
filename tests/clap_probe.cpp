#include <clap/clap.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct DummyHost {
	bool process_requested = false;
};

const void *host_get_extension(const clap_host_t *, const char *) {
	return nullptr;
}

void host_request_restart(const clap_host_t *) {}
void host_request_process(const clap_host_t *) {}
void host_request_callback(const clap_host_t *) {}

DummyHost g_dummy;

const clap_host_t g_host = {
	CLAP_VERSION_INIT,
	&g_dummy,
	"1.44-clap-probe",
	"1.44",
	"https://github.com/Enazzzz/1.44",
	"1.0.0",
	host_get_extension,
	host_request_restart,
	host_request_process,
	host_request_callback,
};

struct EventList {
	std::vector<clap_event_note_t> notes;
};

uint32_t in_size(const clap_input_events_t *list) {
	return static_cast<uint32_t>(static_cast<EventList *>(list->ctx)->notes.size());
}

const clap_event_header_t *in_get(const clap_input_events_t *list, uint32_t index) {
	auto *events = static_cast<EventList *>(list->ctx);
	if (index >= events->notes.size()) {
		return nullptr;
	}
	return &events->notes[index].header;
}

bool out_try_push(const clap_output_events_t *, const clap_event_header_t *) {
	return true;
}

int fail(const char *msg) {
	std::fprintf(stderr, "FAIL: %s\n", msg);
	return 1;
}

#ifdef _WIN32
void *load_plugin(const char *path) {
	return static_cast<void *>(LoadLibraryA(path));
}

void *lookup_symbol(void *handle, const char *name) {
	return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

void close_plugin(void *handle) {
	FreeLibrary(static_cast<HMODULE>(handle));
}

const char *load_error() {
	return "LoadLibrary failed";
}

const char *native_gui_api() {
	return CLAP_WINDOW_API_WIN32;
}
#else
void *load_plugin(const char *path) {
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void *lookup_symbol(void *handle, const char *name) {
	return dlsym(handle, name);
}

void close_plugin(void *handle) {
	dlclose(handle);
}

const char *load_error() {
	const char *err = dlerror();
	return err ? err : "dlopen failed";
}

const char *native_gui_api() {
	return CLAP_WINDOW_API_X11;
}
#endif

} // namespace

int main(int argc, char **argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: one44_clap_probe <path-to-one44.clap>\n");
		return 2;
	}

	void *handle = load_plugin(argv[1]);
	if (!handle) {
		std::fprintf(stderr, "load failed: %s\n", load_error());
		return 1;
	}

	const auto *entry = static_cast<const clap_plugin_entry_t *>(lookup_symbol(handle, "clap_entry"));
	if (!entry) {
		close_plugin(handle);
		return fail("clap_entry symbol not found");
	}
	if (!clap_version_is_compatible(entry->clap_version)) {
		close_plugin(handle);
		return fail("incompatible CLAP version");
	}
	if (!entry->init(argv[1])) {
		close_plugin(handle);
		return fail("clap_entry.init returned false");
	}

	const auto *factory =
		static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
	if (!factory) {
		entry->deinit();
		close_plugin(handle);
		return fail("plugin factory missing");
	}
	if (factory->get_plugin_count(factory) < 1) {
		entry->deinit();
		close_plugin(handle);
		return fail("factory reports zero plugins");
	}
	const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
	if (!desc || !desc->id || !desc->name) {
		entry->deinit();
		close_plugin(handle);
		return fail("descriptor missing");
	}
	std::printf("descriptor.id=%s\n", desc->id);
	std::printf("descriptor.name=%s\n", desc->name);
	std::printf("descriptor.vendor=%s\n", desc->vendor ? desc->vendor : "");
	bool is_instrument = false;
	if (desc->features) {
		for (int i = 0; desc->features[i]; ++i) {
			std::printf("feature=%s\n", desc->features[i]);
			if (std::strcmp(desc->features[i], CLAP_PLUGIN_FEATURE_INSTRUMENT) == 0) {
				is_instrument = true;
			}
		}
	}
	if (!is_instrument) {
		entry->deinit();
		close_plugin(handle);
		return fail("plugin is not tagged as a CLAP instrument");
	}

	const clap_plugin_t *plugin = factory->create_plugin(factory, &g_host, desc->id);
	if (!plugin) {
		entry->deinit();
		close_plugin(handle);
		return fail("create_plugin returned null");
	}
	if (!plugin->init(plugin)) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("plugin.init returned false");
	}

	const auto *audio_ports =
		static_cast<const clap_plugin_audio_ports_t *>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
	const auto *note_ports =
		static_cast<const clap_plugin_note_ports_t *>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
	const auto *gui = static_cast<const clap_plugin_gui_t *>(plugin->get_extension(plugin, CLAP_EXT_GUI));
	if (!audio_ports || audio_ports->count(plugin, false) < 1) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("no audio output port");
	}
	if (!note_ports || note_ports->count(plugin, true) < 1) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("no note input port");
	}
	if (!gui) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("GUI extension missing");
	}
	if (!gui->is_api_supported(plugin, native_gui_api(), false)) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("native GUI API not supported");
	}
	if (!gui->create(plugin, native_gui_api(), false)) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("gui.create failed");
	}
	uint32_t gui_w = 0;
	uint32_t gui_h = 0;
	if (!gui->get_size(plugin, &gui_w, &gui_h) || gui_w == 0 || gui_h == 0) {
		gui->destroy(plugin);
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("gui.get_size failed");
	}
	std::printf("gui.api=%s\n", native_gui_api());
	std::printf("gui.size=%ux%u\n", gui_w, gui_h);
	gui->destroy(plugin);

	clap_audio_port_info_t ap{};
	if (!audio_ports->get(plugin, 0, false, &ap) || ap.channel_count != 2) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("stereo output port expected");
	}
	clap_note_port_info_t np{};
	if (!note_ports->get(plugin, 0, true, &np)) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("note port info failed");
	}

	if (!plugin->activate(plugin, 44100.0, 32, 512)) {
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("activate failed");
	}
	if (!plugin->start_processing(plugin)) {
		plugin->deactivate(plugin);
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("start_processing failed");
	}

	std::vector<float> left(64, 0.0f);
	std::vector<float> right(64, 0.0f);
	float *channels[2] = {left.data(), right.data()};
	clap_audio_buffer_t out{};
	out.data32 = channels;
	out.data64 = nullptr;
	out.channel_count = 2;
	out.latency = 0;
	out.constant_mask = 0;

	EventList notes;
	clap_event_note_t note{};
	note.header.size = sizeof(note);
	note.header.time = 0;
	note.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
	note.header.type = CLAP_EVENT_NOTE_ON;
	note.header.flags = 0;
	note.note_id = 1;
	note.port_index = 0;
	note.channel = 0;
	note.key = 60;
	note.velocity = 1.0;
	notes.notes.push_back(note);

	clap_input_events_t in_events{&notes, in_size, in_get};
	clap_output_events_t out_events{nullptr, out_try_push};

	clap_process_t process{};
	process.steady_time = 0;
	process.frames_count = 64;
	process.transport = nullptr;
	process.audio_inputs = nullptr;
	process.audio_outputs = &out;
	process.audio_inputs_count = 0;
	process.audio_outputs_count = 1;
	process.in_events = &in_events;
	process.out_events = &out_events;

	const clap_process_status status = plugin->process(plugin, &process);
	if (status == CLAP_PROCESS_ERROR) {
		plugin->stop_processing(plugin);
		plugin->deactivate(plugin);
		plugin->destroy(plugin);
		entry->deinit();
		close_plugin(handle);
		return fail("process returned CLAP_PROCESS_ERROR");
	}

	plugin->stop_processing(plugin);
	plugin->deactivate(plugin);
	plugin->destroy(plugin);
	entry->deinit();
	close_plugin(handle);

	std::printf("instantiate=ok\n");
	std::printf("process=ok\n");
	std::printf("instrument=yes\n");
	std::printf("gui=ok\n");
	return 0;
}
