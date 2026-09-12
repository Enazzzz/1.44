#pragma once

#include "constraint.hpp"
#include "decoder.hpp"
#include "envelope.hpp"
#include "voice.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace one44 {

constexpr int kMaxVoices = 16;

/// GUI/engine settings that are not the waveform itself.
struct SamplerSettings {
	SampleRateOption sample_rate = SampleRateOption::Rate44100;
	uint64_t budget_bytes = kDefaultBudgetBytes;
	bool mono_downmix = false;
	bool snap_zero_crossing = false;
	bool preview_loop = true;
	LoopMode loop_mode = LoopMode::Forward;
	int root_note = 60;
	ADSR adsr{};
};

/// Full sampler: source editor + constraint commit + MIDI/preview playback.
class Sampler {
public:
	Sampler() = default;

	/// Loads WAV or MP3 and resets region markers to the full file.
	DecodeResult load_path(const std::string &path);

	/// Loads already-decoded PCM (used by tests and state restore).
	void load_buffer(AudioBuffer buffer);

	/// Current source audio, or nullptr if nothing is loaded.
	const AudioBuffer *source() const;

	SamplerSettings settings() const;
	void set_settings(const SamplerSettings &s);

	uint64_t region_start() const;
	uint64_t region_end() const;
	uint64_t loop_start() const;
	uint64_t loop_end() const;

	/// Sets region markers in source frames; end is exclusive.
	void set_region(uint64_t start, uint64_t end);

	/// Sets loop markers in source frames, clamped to the region.
	void set_loop(uint64_t start, uint64_t end);

	/// Live byte-size of the current region at the chosen rate/depth/channels.
	uint64_t current_selection_bytes() const;

	/// Channel count that would be stored on commit (1 if downmix, else min(2, source)).
	int commit_channel_count() const;

	/// Byte-size query used by the live budget display.
	ByteSizeInput current_byte_size_input() const;

	/// Attempts to commit the region. Over-budget selections are rejected and
	/// the previous committed sample is left unchanged.
	CommitDecision commit();

	bool has_committed() const;
	CommittedSample committed_copy() const;

	/// Preview transport: plays the selected *source* region, looping when enabled.
	void start_preview();
	void stop_preview();
	void scrub_preview(uint64_t source_frame);
	bool preview_playing() const;
	double preview_position() const;

	/// MIDI voice control (audio thread).
	void note_on(int note, float velocity, int note_id, int channel, int port, double host_sr);
	void note_off(int note, int note_id, int channel, int port);
	void choke_all();

	/// Mixes `frames` of stereo audio at `host_sr` into non-interleaved buffers.
	void render(float *left, float *right, uint32_t frames, double host_sr);

	/// Serializes editor + committed sample for clap.state.
	std::vector<uint8_t> save_state() const;
	bool load_state(const uint8_t *data, size_t size);

private:
	void clamp_markers();
	int find_voice(int note, int note_id, int channel, int port);
	int steal_voice();
	void render_preview_frame(float *left, float *right, double host_sr);

	mutable std::mutex mutex_;
	AudioBuffer source_;
	SamplerSettings settings_{};
	uint64_t region_start_ = 0;
	uint64_t region_end_ = 0;
	uint64_t loop_start_ = 0;
	uint64_t loop_end_ = 0;
	CommittedSample committed_{};
	bool has_committed_ = false;

	bool preview_playing_ = false;
	double preview_pos_ = 0.0;

	std::array<Voice, kMaxVoices> voices_{};
};

} // namespace one44
