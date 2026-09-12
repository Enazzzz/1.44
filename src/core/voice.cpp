#include "voice.hpp"

#include "pitch.hpp"
#include "resampler.hpp"

#include <algorithm>
#include <cmath>

namespace one44 {

/// Stable encoding for CLAP params and state blobs.
int loop_mode_to_int(LoopMode mode) {
	switch (mode) {
	case LoopMode::OneShot:
		return 0;
	case LoopMode::Forward:
		return 1;
	case LoopMode::PingPong:
		return 2;
	}
	return 0;
}

/// Inverse of loop_mode_to_int; out-of-range values become one-shot.
LoopMode loop_mode_from_int(int value) {
	switch (value) {
	case 1:
		return LoopMode::Forward;
	case 2:
		return LoopMode::PingPong;
	case 0:
	default:
		return LoopMode::OneShot;
	}
}

/// `pitch_rate * (sample_sr / host_sr)` — an octave up doubles both pitch and speed.
double voice_increment(int midi_note, int root_note, double sample_rate, double host_sample_rate) {
	if (host_sample_rate <= 0.0) {
		return 0.0;
	}
	const double pitch = playback_rate_for_note(midi_note, root_note);
	return pitch * (sample_rate / host_sample_rate);
}

namespace {

/// Applies naive loop wrapping with no crossfade — the click is intentional.
void wrap_position(Voice *voice, const CommittedSample &sample) {
	if (sample.frame_count == 0) {
		voice->in_use = false;
		return;
	}
	const double end = static_cast<double>(sample.frame_count);
	double loop_a = static_cast<double>(std::min(sample.loop_start, sample.loop_end));
	double loop_b = static_cast<double>(std::max(sample.loop_start, sample.loop_end));
	if (loop_b <= loop_a + 1.0) {
		loop_a = 0.0;
		loop_b = end;
	}
	if (loop_b > end) {
		loop_b = end;
	}

	switch (sample.loop_mode) {
	case LoopMode::OneShot:
		if (voice->pos >= end || voice->pos < 0.0) {
			voice->pos = end;
			voice->env.choke();
			voice->in_use = false;
		}
		break;
	case LoopMode::Forward:
		if (voice->pos >= loop_b) {
			const double span = loop_b - loop_a;
			if (span > 0.0) {
				while (voice->pos >= loop_b) {
					voice->pos -= span;
				}
			} else {
				voice->pos = loop_a;
			}
			if (voice->pos < loop_a) {
				voice->pos = loop_a;
			}
		}
		break;
	case LoopMode::PingPong:
		if (voice->pos >= loop_b) {
			voice->pos = std::max(loop_a, loop_b - 1.0e-6);
			voice->ping_dir = -1;
		} else if (voice->pos < loop_a) {
			voice->pos = loop_a;
			voice->ping_dir = 1;
		}
		break;
	}
}

} // namespace

/// Starts a voice at the beginning of the committed sample.
void Voice::start(
	int note,
	int new_root_note,
	int new_note_id,
	int new_channel,
	int new_port,
	float vel,
	const CommittedSample &sample,
	double host_sample_rate) {
	midi_note = note;
	root_note = new_root_note;
	note_id = new_note_id;
	channel = new_channel;
	port = new_port;
	velocity = std::max(0.0f, std::min(1.0f, vel));
	pos = 0.0;
	ping_dir = 1;
	age = 0;
	increment = voice_increment(note, new_root_note, sample.sample_rate, host_sample_rate);
	env.choke();
	in_use = sample.frame_count > 0 && increment > 0.0;
	if (in_use) {
		env.note_on();
	}
}

/// Mixes one interpolated frame, then advances by the pitch-scaled increment.
bool Voice::render_frame(
	const CommittedSample &sample,
	const ADSR &adsr,
	double host_sample_rate,
	float *left,
	float *right) {
	if (!in_use || sample.frame_count == 0 || host_sample_rate <= 0.0) {
		in_use = false;
		*left = 0.0f;
		*right = 0.0f;
		return false;
	}

	const float amp = env.tick(adsr, host_sample_rate) * velocity;
	if (!env.active()) {
		in_use = false;
		*left = 0.0f;
		*right = 0.0f;
		return false;
	}

	wrap_position(this, sample);
	if (!in_use) {
		*left = 0.0f;
		*right = 0.0f;
		return false;
	}

	float l = 0.0f;
	float r = 0.0f;
	if (sample.channels <= 1) {
		l = r = lerp_i16(sample.pcm.data(), sample.frame_count, 1, 0, pos);
	} else {
		l = lerp_i16(sample.pcm.data(), sample.frame_count, sample.channels, 0, pos);
		r = lerp_i16(sample.pcm.data(), sample.frame_count, sample.channels, 1, pos);
	}

	*left = l * amp;
	*right = r * amp;

	pos += increment * static_cast<double>(ping_dir);
	wrap_position(this, sample);
	++age;
	return in_use;
}

} // namespace one44
