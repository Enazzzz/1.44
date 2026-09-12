#include "check.hpp"
#include "core/constraint.hpp"
#include "core/sampler.hpp"

#include <vector>

using namespace one44;

TEST(default_budget_is_floppy_1440_kib) {
	CHECK_EQ(default_budget_bytes(), 1440ull * 1024ull);
	CHECK_EQ(kDefaultBudgetBytes, 1474560ull);
	CHECK_EQ(kBitDepth, 16);
	CHECK_EQ(kBytesPerSample, 2);
}

TEST(byte_size_one_second_44100_stereo) {
	ByteSizeInput in;
	in.region_frames = 44100;
	in.source_sample_rate = 44100.0;
	in.target_rate = SampleRateOption::Rate44100;
	in.channels = 2;
	CHECK_EQ(selection_byte_size(in), 44100ull * 2ull * 2ull);
}

TEST(byte_size_one_second_44100_mono) {
	ByteSizeInput in;
	in.region_frames = 44100;
	in.source_sample_rate = 44100.0;
	in.target_rate = SampleRateOption::Rate44100;
	in.channels = 1;
	CHECK_EQ(selection_byte_size(in), 44100ull * 1ull * 2ull);
}

TEST(byte_size_one_second_29760_stereo) {
	ByteSizeInput in;
	in.region_frames = 44100;
	in.source_sample_rate = 44100.0;
	in.target_rate = SampleRateOption::Rate29760;
	in.channels = 2;
	CHECK_EQ(sample_rate_hz(SampleRateOption::Rate29760), 29760.0);
	CHECK_EQ(selection_byte_size(in), 29760ull * 2ull * 2ull);
}

TEST(byte_size_one_second_29760_mono) {
	ByteSizeInput in;
	in.region_frames = 44100;
	in.source_sample_rate = 44100.0;
	in.target_rate = SampleRateOption::Rate29760;
	in.channels = 1;
	CHECK_EQ(selection_byte_size(in), 29760ull * 1ull * 2ull);
}

TEST(byte_size_resamples_when_source_rate_differs) {
	ByteSizeInput in;
	in.region_frames = 48000;
	in.source_sample_rate = 48000.0;
	in.target_rate = SampleRateOption::Rate44100;
	in.channels = 2;
	CHECK_EQ(selection_byte_size(in), 44100ull * 2ull * 2ull);
}

TEST(reject_over_budget_never_silent_truncate) {
	ByteSizeInput in;
	in.region_frames = 44100 * 20;
	in.source_sample_rate = 44100.0;
	in.target_rate = SampleRateOption::Rate44100;
	in.channels = 2;
	const uint64_t bytes = selection_byte_size(in);
	CHECK(bytes > default_budget_bytes());
	const CommitDecision d = evaluate_commit(in, default_budget_bytes());
	CHECK(!d.accepted);
	CHECK_EQ(d.byte_size, bytes);
	CHECK_EQ(d.budget, default_budget_bytes());
}

TEST(accept_when_exactly_on_budget) {
	ByteSizeInput in;
	in.region_frames = 100;
	in.source_sample_rate = 44100.0;
	in.target_rate = SampleRateOption::Rate44100;
	in.channels = 1;
	const uint64_t bytes = selection_byte_size(in);
	CHECK_EQ(bytes, 200ull);
	const CommitDecision d = evaluate_commit(in, 200);
	CHECK(d.accepted);
	CHECK_EQ(d.byte_size, 200ull);
}

TEST(empty_region_is_rejected) {
	ByteSizeInput in;
	in.region_frames = 0;
	in.channels = 2;
	const CommitDecision d = evaluate_commit(in, default_budget_bytes());
	CHECK(!d.accepted);
	CHECK_EQ(d.byte_size, 0ull);
}

TEST(commit_rejection_leaves_previous_sample_intact) {
	Sampler sampler;
	AudioBuffer buf;
	buf.channels = 1;
	buf.sample_rate = 44100.0;
	buf.frame_count = 44100;
	buf.interleaved.assign(44100, 0.25f);
	sampler.load_buffer(buf);

	SamplerSettings s = sampler.settings();
	s.budget_bytes = 4000;
	s.mono_downmix = true;
	s.sample_rate = SampleRateOption::Rate44100;
	sampler.set_settings(s);

	sampler.set_region(0, 1000);
	const CommitDecision ok = sampler.commit();
	CHECK(ok.accepted);
	CHECK(sampler.has_committed());
	const uint64_t committed_frames = sampler.committed_copy().frame_count;

	sampler.set_region(0, 44100);
	const CommitDecision bad = sampler.commit();
	CHECK(!bad.accepted);
	CHECK(sampler.has_committed());
	CHECK_EQ(sampler.committed_copy().frame_count, committed_frames);
	CHECK(bad.byte_size > s.budget_bytes);
}

TEST(live_budget_tracks_marker_drag_and_mono_toggle) {
	Sampler sampler;
	AudioBuffer buf;
	buf.channels = 2;
	buf.sample_rate = 44100.0;
	buf.frame_count = 44100;
	buf.interleaved.assign(44100 * 2, 0.1f);
	sampler.load_buffer(buf);
	sampler.set_region(0, 44100);

	CHECK_EQ(sampler.current_selection_bytes(), 44100ull * 2ull * 2ull);

	SamplerSettings s = sampler.settings();
	s.mono_downmix = true;
	sampler.set_settings(s);
	CHECK_EQ(sampler.current_selection_bytes(), 44100ull * 1ull * 2ull);

	s.sample_rate = SampleRateOption::Rate29760;
	sampler.set_settings(s);
	CHECK_EQ(sampler.current_selection_bytes(), 29760ull * 1ull * 2ull);
}
