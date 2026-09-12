#pragma once

#include "constraint.hpp"
#include "envelope.hpp"

#include <cstdint>
#include <vector>

namespace one44 {

/// Loop modes. One-shot and forward-loop are required; ping-pong is the stretch.
enum class LoopMode {
	OneShot,
	Forward,
	PingPong,
};

/// 16-bit committed region that MIDI playback reads. Loop points are in
/// committed frames and are *not* crossfaded.
struct CommittedSample {
	std::vector<int16_t> pcm;
	int channels = 1;
	double sample_rate = 44100.0;
	uint64_t frame_count = 0;
	uint64_t loop_start = 0;
	uint64_t loop_end = 0;
	LoopMode loop_mode = LoopMode::OneShot;
};

/// One playing note. Pitch is applied as a playback-rate change.
struct Voice {
	bool in_use = false;
	int midi_note = 60;
	int root_note = 60;
	int note_id = -1;
	int channel = 0;
	int port = 0;
	float velocity = 1.0f;
	double pos = 0.0;
	/// Host-sample increment already including pitch and sample-rate ratio.
	double increment = 1.0;
	int ping_dir = 1;
	Envelope env;
	uint32_t age = 0;

	/// Prepares the voice for a note-on against `sample`.
	void start(
		int note,
		int new_root_note,
		int new_note_id,
		int new_channel,
		int new_port,
		float vel,
		const CommittedSample &sample,
		double host_sample_rate);

	/// Renders one stereo frame into `left`/`right`. Returns false when idle.
	bool render_frame(
		const CommittedSample &sample,
		const ADSR &adsr,
		double host_sample_rate,
		float *left,
		float *right);
};

/// Host-sample increment for a MIDI note against a committed sample.
double voice_increment(int midi_note, int root_note, double sample_rate, double host_sample_rate);

/// Converts a LoopMode to a stable integer for params/state.
int loop_mode_to_int(LoopMode mode);

/// Converts a stepped param value into a LoopMode.
LoopMode loop_mode_from_int(int value);

} // namespace one44
