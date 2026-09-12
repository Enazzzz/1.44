#pragma once

#include <cstdint>

namespace one44 {

/// Floppy-era 1.44 MB = 1440 KiB = 1,474,560 bytes (80×18×512×2).
constexpr uint64_t kDefaultBudgetBytes = 1440ull * 1024ull;

/// 16-bit is the ASR-10 capture depth; not user-adjustable.
constexpr int kBitDepth = 16;
constexpr int kBytesPerSample = kBitDepth / 8;

/// Hardware sample-rate options actually offered by the ASR-10.
enum class SampleRateOption {
	Rate44100,
	Rate29760,
};

/// Returns the Hz value for a hardware rate option.
double sample_rate_hz(SampleRateOption option);

/// Maps Hz to a hardware option; unknown values are invalid (returns false).
bool sample_rate_option_from_hz(double hz, SampleRateOption *out);

/// Rounded frame count after resampling a region onto the target rate.
/// `frames_out = round(region_frames * target_sr / source_sr)`.
uint64_t resampled_frame_count(uint64_t region_frames, double source_sr, double target_sr);

/// Inputs for the 16-bit byte-size formula. `channels` is 1 (mono) or 2 (stereo)
/// after the optional downmix decision.
struct ByteSizeInput {
	uint64_t region_frames = 0;
	double source_sample_rate = 44100.0;
	SampleRateOption target_rate = SampleRateOption::Rate44100;
	int channels = 2;
};

/// Byte size of a selection stored as 16-bit PCM at the chosen rate.
/// Never truncates the region; this is a pure size query.
uint64_t selection_byte_size(const ByteSizeInput &in);

/// True when `bytes` is non-zero and does not exceed `budget`.
bool fits_budget(uint64_t bytes, uint64_t budget);

/// Result of a commit attempt. Rejection leaves any previously committed
/// sample untouched — the engine must never silent-truncate.
struct CommitDecision {
	bool accepted = false;
	uint64_t byte_size = 0;
	uint64_t budget = kDefaultBudgetBytes;
};

/// Accepts the selection only when it fits the budget and contains audio.
CommitDecision evaluate_commit(const ByteSizeInput &in, uint64_t budget);

/// Default memory budget (1.44 MB floppy).
uint64_t default_budget_bytes();

} // namespace one44
