#include "constraint.hpp"

#include <cmath>

namespace one44 {

/// Returns Hz for a documented ASR-10 rate switch.
double sample_rate_hz(SampleRateOption option) {
	switch (option) {
	case SampleRateOption::Rate44100:
		return 44100.0;
	case SampleRateOption::Rate29760:
		return 29760.0;
	}
	// Exhaustive for known enumerators; defensive fallback.
	return 44100.0;
}

/// Accepts the two hardware rates only.
bool sample_rate_option_from_hz(double hz, SampleRateOption *out) {
	if (out == nullptr) {
		return false;
	}
	if (std::fabs(hz - 44100.0) < 0.01) {
		*out = SampleRateOption::Rate44100;
		return true;
	}
	if (std::fabs(hz - 29760.0) < 0.01) {
		*out = SampleRateOption::Rate29760;
		return true;
	}
	return false;
}

/// Converts a source-rate region length into a target-rate frame count.
uint64_t resampled_frame_count(uint64_t region_frames, double source_sr, double target_sr) {
	if (region_frames == 0 || source_sr <= 0.0 || target_sr <= 0.0) {
		return 0;
	}
	const double frames = static_cast<double>(region_frames) * (target_sr / source_sr);
	if (frames <= 0.0) {
		return 0;
	}
	return static_cast<uint64_t>(std::llround(frames));
}

/// 16-bit interleaved PCM size: frames × channels × 2 bytes.
uint64_t selection_byte_size(const ByteSizeInput &in) {
	if (in.channels < 1) {
		return 0;
	}
	const double target_sr = sample_rate_hz(in.target_rate);
	const uint64_t frames = resampled_frame_count(in.region_frames, in.source_sample_rate, target_sr);
	const int channels = in.channels > 2 ? 2 : in.channels;
	return frames * static_cast<uint64_t>(channels) * static_cast<uint64_t>(kBytesPerSample);
}

/// Zero-length selections are not a fit even if the budget is large.
bool fits_budget(uint64_t bytes, uint64_t budget) {
	return bytes > 0 && bytes <= budget;
}

/// Rejects over-budget and empty selections; never shortens the region.
CommitDecision evaluate_commit(const ByteSizeInput &in, uint64_t budget) {
	CommitDecision decision;
	decision.budget = budget;
	decision.byte_size = selection_byte_size(in);
	decision.accepted = fits_budget(decision.byte_size, budget);
	return decision;
}

/// Floppy-sized default: 1,474,560 bytes.
uint64_t default_budget_bytes() {
	return kDefaultBudgetBytes;
}

} // namespace one44
