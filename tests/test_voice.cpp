#include "check.hpp"
#include "core/sampler.hpp"
#include "core/zero_crossing.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace one44;

TEST(one_shot_stops_at_end_without_wrapping) {
	CommittedSample sample;
	sample.channels = 1;
	sample.sample_rate = 8.0;
	sample.frame_count = 4;
	sample.pcm = {1000, 2000, 3000, 4000};
	sample.loop_mode = LoopMode::OneShot;
	sample.loop_start = 0;
	sample.loop_end = 4;

	Voice voice;
	ADSR adsr;
	adsr.attack = 0.0f;
	adsr.decay = 0.0f;
	adsr.sustain = 1.0f;
	adsr.release = 0.0f;
	voice.start(60, 60, 0, 0, 0, 1.0f, sample, 8.0);

	int live = 0;
	for (int i = 0; i < 16; ++i) {
		float l = 0, r = 0;
		if (voice.render_frame(sample, adsr, 8.0, &l, &r)) {
			++live;
		}
	}
	CHECK(live > 0);
	CHECK(live <= 5);
	CHECK(!voice.in_use);
}

TEST(forward_loop_wraps_without_crossfade) {
	CommittedSample sample;
	sample.channels = 1;
	sample.sample_rate = 8.0;
	sample.frame_count = 4;
	sample.pcm = {1000, 2000, 3000, 4000};
	sample.loop_mode = LoopMode::Forward;
	sample.loop_start = 0;
	sample.loop_end = 4;

	Voice voice;
	ADSR adsr;
	adsr.attack = 0.0f;
	adsr.decay = 0.0f;
	adsr.sustain = 1.0f;
	adsr.release = 1.0f;
	voice.start(60, 60, 0, 0, 0, 1.0f, sample, 8.0);

	for (int i = 0; i < 12; ++i) {
		float l = 0, r = 0;
		CHECK(voice.render_frame(sample, adsr, 8.0, &l, &r));
	}
	CHECK(voice.in_use);
	CHECK(voice.pos < 4.0);
}

TEST(ping_pong_reverses_direction) {
	CommittedSample sample;
	sample.channels = 1;
	sample.sample_rate = 8.0;
	sample.frame_count = 4;
	sample.pcm = {1000, 2000, 3000, 4000};
	sample.loop_mode = LoopMode::PingPong;
	sample.loop_start = 0;
	sample.loop_end = 4;

	Voice voice;
	ADSR adsr;
	adsr.attack = 0.0f;
	adsr.decay = 0.0f;
	adsr.sustain = 1.0f;
	adsr.release = 1.0f;
	voice.start(60, 60, 0, 0, 0, 1.0f, sample, 8.0);

	for (int i = 0; i < 6; ++i) {
		float l = 0, r = 0;
		voice.render_frame(sample, adsr, 8.0, &l, &r);
	}
	CHECK_EQ(voice.ping_dir, -1);
}

TEST(snap_finds_nearest_zero_crossing) {
	std::vector<float> pcm = {-0.5f, -0.2f, 0.01f, 0.4f, 0.8f};
	const uint64_t snapped = snap_to_zero_crossing(pcm.data(), pcm.size(), 1, 4, 0, 4);
	CHECK_EQ(snapped, 2ull);
}

TEST(midi_render_is_silent_until_commit) {
	Sampler sampler;
	AudioBuffer buf;
	buf.channels = 1;
	buf.sample_rate = 44100.0;
	buf.frame_count = 64;
	buf.interleaved.assign(64, 0.5f);
	sampler.load_buffer(buf);
	sampler.note_on(60, 1.0f, 1, 0, 0, 44100.0);
	std::vector<float> l(32, 99.0f), r(32, 99.0f);
	sampler.render(l.data(), r.data(), 32, 44100.0);
	for (float s : l) {
		CHECK_NEAR(s, 0.0, 1e-9);
	}
}

TEST(midi_render_makes_sound_after_commit) {
	Sampler sampler;
	AudioBuffer buf;
	buf.channels = 1;
	buf.sample_rate = 44100.0;
	buf.frame_count = 2048;
	buf.interleaved.resize(2048);
	for (int i = 0; i < 2048; ++i) {
		buf.interleaved[static_cast<size_t>(i)] = (i % 32) < 16 ? 0.8f : -0.8f;
	}
	sampler.load_buffer(buf);
	SamplerSettings s = sampler.settings();
	s.adsr.attack = 0.0f;
	s.adsr.decay = 0.0f;
	s.adsr.sustain = 1.0f;
	s.adsr.release = 0.05f;
	s.mono_downmix = true;
	sampler.set_settings(s);
	sampler.set_region(0, 2048);
	CHECK(sampler.commit().accepted);
	sampler.note_on(60, 1.0f, 1, 0, 0, 44100.0);
	std::vector<float> l(256, 0.0f), r(256, 0.0f);
	sampler.render(l.data(), r.data(), 256, 44100.0);
	double peak = 0.0;
	for (float x : l) {
		peak = std::max(peak, static_cast<double>(std::fabs(x)));
	}
	CHECK(peak > 0.1);
}

TEST(preview_render_is_audible_after_load_without_commit) {
	Sampler sampler;
	AudioBuffer buf;
	buf.channels = 1;
	buf.sample_rate = 44100.0;
	buf.frame_count = 2048;
	buf.interleaved.resize(2048);
	for (int i = 0; i < 2048; ++i) {
		buf.interleaved[static_cast<size_t>(i)] = (i % 32) < 16 ? 0.8f : -0.8f;
	}
	sampler.load_buffer(buf);
	CHECK(!sampler.has_committed());
	sampler.start_preview();
	CHECK(sampler.preview_playing());
	CHECK(sampler.wants_process());
	std::vector<float> l(256, 0.0f), r(256, 0.0f);
	sampler.render(l.data(), r.data(), 256, 44100.0);
	double peak = 0.0;
	for (float x : l) {
		peak = std::max(peak, static_cast<double>(std::fabs(x)));
	}
	CHECK(peak > 0.1);
}
