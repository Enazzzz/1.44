#include "check.hpp"
#include "core/pitch.hpp"
#include "core/voice.hpp"

#include <cmath>

using namespace one44;

TEST(root_note_plays_at_unity_rate) {
	CHECK_NEAR(playback_rate_for_note(60, 60), 1.0, 1e-12);
	CHECK_NEAR(playback_rate_for_note(0, 0), 1.0, 1e-12);
}

TEST(octave_up_doubles_playback_rate) {
	CHECK_NEAR(playback_rate_for_note(72, 60), 2.0, 1e-12);
	CHECK_NEAR(playback_rate_for_note(84, 60), 4.0, 1e-12);
}

TEST(octave_down_halves_playback_rate) {
	CHECK_NEAR(playback_rate_for_note(48, 60), 0.5, 1e-12);
	CHECK_NEAR(playback_rate_for_note(36, 60), 0.25, 1e-12);
}

TEST(semitone_uses_equal_temperament) {
	CHECK_NEAR(playback_rate_for_note(61, 60), std::pow(2.0, 1.0 / 12.0), 1e-12);
	CHECK_NEAR(playback_rate_for_note(59, 60), std::pow(2.0, -1.0 / 12.0), 1e-12);
}

TEST(voice_increment_includes_pitch_and_rate_ratio) {
	CHECK_NEAR(voice_increment(60, 60, 44100.0, 44100.0), 1.0, 1e-12);
	CHECK_NEAR(voice_increment(72, 60, 44100.0, 44100.0), 2.0, 1e-12);
	CHECK_NEAR(voice_increment(60, 60, 29760.0, 44100.0), 29760.0 / 44100.0, 1e-12);
	CHECK_NEAR(voice_increment(72, 60, 29760.0, 48000.0), 2.0 * (29760.0 / 48000.0), 1e-12);
}

TEST(voice_start_stores_rate_mapped_increment) {
	CommittedSample sample;
	sample.frame_count = 100;
	sample.sample_rate = 44100.0;
	sample.channels = 1;
	sample.pcm.assign(100, 0);
	Voice voice;
	voice.start(72, 60, 1, 0, 0, 1.0f, sample, 44100.0);
	CHECK(voice.in_use);
	CHECK_NEAR(voice.increment, 2.0, 1e-12);
}
