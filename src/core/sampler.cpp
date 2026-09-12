#include "sampler.hpp"

#include "pitch.hpp"
#include "resampler.hpp"
#include "zero_crossing.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace one44 {
namespace {

constexpr uint32_t kStateMagic = 0x53344131; // "14AS" little-endian-ish
constexpr uint32_t kStateVersion = 1;

/// Writes a POD value onto a byte vector.
template <typename T>
void append_pod(std::vector<uint8_t> *out, const T &value) {
	const size_t off = out->size();
	out->resize(off + sizeof(T));
	std::memcpy(out->data() + off, &value, sizeof(T));
}

/// Reads a POD value; returns false on underrun.
template <typename T>
bool read_pod(const uint8_t *data, size_t size, size_t *offset, T *out) {
	if (*offset + sizeof(T) > size) {
		return false;
	}
	std::memcpy(out, data + *offset, sizeof(T));
	*offset += sizeof(T);
	return true;
}

} // namespace

/// Loads a file and selects the whole duration as the initial region.
DecodeResult Sampler::load_path(const std::string &path) {
	DecodeResult result = decode_file(path);
	if (result.error == DecodeError::None) {
		load_buffer(result.audio);
	}
	return result;
}

/// Replaces source audio and resets markers; committed sample is cleared.
void Sampler::load_buffer(AudioBuffer buffer) {
	std::lock_guard<std::mutex> lock(mutex_);
	source_ = std::move(buffer);
	region_start_ = 0;
	region_end_ = source_.frame_count;
	loop_start_ = 0;
	loop_end_ = source_.frame_count;
	has_committed_ = false;
	committed_ = {};
	preview_playing_ = false;
	preview_pos_ = 0.0;
	for (auto &v : voices_) {
		v.in_use = false;
		v.env.choke();
	}
}

/// Source accessor for the GUI waveform.
const AudioBuffer *Sampler::source() const {
	return source_.frame_count > 0 ? &source_ : nullptr;
}

SamplerSettings Sampler::settings() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return settings_;
}

void Sampler::set_settings(const SamplerSettings &s) {
	std::lock_guard<std::mutex> lock(mutex_);
	settings_ = s;
	if (settings_.budget_bytes < 2) {
		settings_.budget_bytes = 2;
	}
	settings_.root_note = clamp_midi_note(settings_.root_note);
	clamp_markers();
}

uint64_t Sampler::region_start() const {
	return region_start_;
}

uint64_t Sampler::region_end() const {
	return region_end_;
}

uint64_t Sampler::loop_start() const {
	return loop_start_;
}

uint64_t Sampler::loop_end() const {
	return loop_end_;
}

/// Region end is exclusive; a zero-length region is allowed until commit rejects it.
void Sampler::set_region(uint64_t start, uint64_t end) {
	std::lock_guard<std::mutex> lock(mutex_);
	region_start_ = start;
	region_end_ = end;
	clamp_markers();
}

void Sampler::set_loop(uint64_t start, uint64_t end) {
	std::lock_guard<std::mutex> lock(mutex_);
	loop_start_ = start;
	loop_end_ = end;
	if (settings_.snap_zero_crossing && source_.frame_count > 0) {
		loop_start_ = snap_to_zero_crossing(
			source_.interleaved.data(),
			source_.frame_count,
			static_cast<int>(source_.channels),
			loop_start_,
			region_start_,
			region_end_ > 0 ? region_end_ - 1 : 0);
		loop_end_ = snap_to_zero_crossing(
			source_.interleaved.data(),
			source_.frame_count,
			static_cast<int>(source_.channels),
			loop_end_,
			region_start_,
			region_end_ > 0 ? region_end_ - 1 : 0);
	}
	clamp_markers();
}

uint64_t Sampler::current_selection_bytes() const {
	return selection_byte_size(current_byte_size_input());
}

int Sampler::commit_channel_count() const {
	if (source_.channels <= 0) {
		return 0;
	}
	if (settings_.mono_downmix || source_.channels == 1) {
		return 1;
	}
	return 2;
}

ByteSizeInput Sampler::current_byte_size_input() const {
	ByteSizeInput in;
	in.region_frames = region_end_ > region_start_ ? region_end_ - region_start_ : 0;
	in.source_sample_rate = source_.sample_rate > 0.0 ? source_.sample_rate : 44100.0;
	in.target_rate = settings_.sample_rate;
	in.channels = commit_channel_count();
	return in;
}

/// Resamples, quantizes to 16-bit, and stores the region only when it fits.
CommitDecision Sampler::commit() {
	std::lock_guard<std::mutex> lock(mutex_);
	const ByteSizeInput in = current_byte_size_input();
	const CommitDecision decision = evaluate_commit(in, settings_.budget_bytes);
	if (!decision.accepted || source_.frame_count == 0) {
		return decision;
	}

	const double target_sr = sample_rate_hz(settings_.sample_rate);
	const uint64_t out_frames = resampled_frame_count(in.region_frames, in.source_sample_rate, target_sr);
	const int out_ch = in.channels;
	CommittedSample next;
	next.channels = out_ch;
	next.sample_rate = target_sr;
	next.frame_count = out_frames;
	next.loop_mode = settings_.loop_mode;
	next.pcm.resize(out_frames * static_cast<uint64_t>(out_ch));

	const double src_start = static_cast<double>(region_start_);
	const double step = in.region_frames == 0 || out_frames == 0
		? 0.0
		: static_cast<double>(in.region_frames) / static_cast<double>(out_frames);

	for (uint64_t i = 0; i < out_frames; ++i) {
		const double src_pos = src_start + static_cast<double>(i) * step;
		if (out_ch == 1) {
			next.pcm[i] = quantize_i16(lerp_mono(
				source_.interleaved.data(),
				source_.frame_count,
				static_cast<int>(source_.channels),
				src_pos));
		} else {
			next.pcm[i * 2 + 0] = quantize_i16(lerp_sample(
				source_.interleaved.data(),
				source_.frame_count,
				static_cast<int>(source_.channels),
				0,
				src_pos));
			next.pcm[i * 2 + 1] = quantize_i16(lerp_sample(
				source_.interleaved.data(),
				source_.frame_count,
				static_cast<int>(source_.channels),
				1,
				src_pos));
		}
	}

	const double map = target_sr / in.source_sample_rate;
	auto map_loop = [&](uint64_t src_frame) -> uint64_t {
		if (src_frame < region_start_) {
			src_frame = region_start_;
		}
		if (src_frame > region_end_) {
			src_frame = region_end_;
		}
		const double rel = static_cast<double>(src_frame - region_start_) * map;
		uint64_t dst = static_cast<uint64_t>(std::llround(rel));
		if (dst > out_frames) {
			dst = out_frames;
		}
		return dst;
	};
	next.loop_start = map_loop(loop_start_);
	next.loop_end = map_loop(loop_end_);
	if (next.loop_end <= next.loop_start) {
		next.loop_start = 0;
		next.loop_end = out_frames;
	}

	committed_ = std::move(next);
	has_committed_ = true;
	return decision;
}

bool Sampler::has_committed() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return has_committed_;
}

CommittedSample Sampler::committed_copy() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return committed_;
}

void Sampler::start_preview() {
	std::lock_guard<std::mutex> lock(mutex_);
	if (source_.frame_count == 0 || region_end_ <= region_start_) {
		preview_playing_ = false;
		return;
	}
	preview_playing_ = true;
	preview_pos_ = static_cast<double>(region_start_);
}

void Sampler::stop_preview() {
	std::lock_guard<std::mutex> lock(mutex_);
	preview_playing_ = false;
}

void Sampler::scrub_preview(uint64_t source_frame) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (source_frame < region_start_) {
		source_frame = region_start_;
	}
	if (source_frame >= region_end_ && region_end_ > 0) {
		source_frame = region_end_ - 1;
	}
	preview_pos_ = static_cast<double>(source_frame);
}

bool Sampler::preview_playing() const {
	return preview_playing_;
}

double Sampler::preview_position() const {
	return preview_pos_;
}

void Sampler::note_on(int note, float velocity, int note_id, int channel, int port, double host_sr) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (!has_committed_) {
		return;
	}
	int slot = find_voice(note, note_id, channel, port);
	if (slot < 0) {
		slot = steal_voice();
	}
	voices_[static_cast<size_t>(slot)].start(
		note,
		settings_.root_note,
		note_id,
		channel,
		port,
		velocity,
		committed_,
		host_sr);
}

void Sampler::note_off(int note, int note_id, int channel, int port) {
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto &v : voices_) {
		if (!v.in_use) {
			continue;
		}
		const bool note_ok = note < 0 || v.midi_note == note;
		const bool id_ok = note_id < 0 || v.note_id == note_id || v.note_id < 0;
		const bool ch_ok = channel < 0 || v.channel == channel;
		const bool port_ok = port < 0 || v.port == port;
		if (note_ok && id_ok && ch_ok && port_ok) {
			v.env.note_off();
		}
	}
}

void Sampler::choke_all() {
	std::lock_guard<std::mutex> lock(mutex_);
	preview_playing_ = false;
	for (auto &v : voices_) {
		v.in_use = false;
		v.env.choke();
	}
}

void Sampler::render(float *left, float *right, uint32_t frames, double host_sr) {
	std::lock_guard<std::mutex> lock(mutex_);
	for (uint32_t i = 0; i < frames; ++i) {
		float mix_l = 0.0f;
		float mix_r = 0.0f;
		if (preview_playing_) {
			float pl = 0.0f;
			float pr = 0.0f;
			render_preview_frame(&pl, &pr, host_sr);
			mix_l += pl;
			mix_r += pr;
		}
		if (has_committed_) {
			for (auto &v : voices_) {
				if (!v.in_use) {
					continue;
				}
				float vl = 0.0f;
				float vr = 0.0f;
				v.render_frame(committed_, settings_.adsr, host_sr, &vl, &vr);
				mix_l += vl;
				mix_r += vr;
			}
		}
		left[i] = mix_l;
		right[i] = mix_r;
	}
}

std::vector<uint8_t> Sampler::save_state() const {
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<uint8_t> out;
	append_pod(&out, kStateMagic);
	append_pod(&out, kStateVersion);
	const int32_t rate = settings_.sample_rate == SampleRateOption::Rate29760 ? 1 : 0;
	append_pod(&out, rate);
	append_pod(&out, settings_.budget_bytes);
	const uint8_t flags = static_cast<uint8_t>(
		(settings_.mono_downmix ? 1 : 0) |
		(settings_.snap_zero_crossing ? 2 : 0) |
		(settings_.preview_loop ? 4 : 0));
	append_pod(&out, flags);
	append_pod(&out, loop_mode_to_int(settings_.loop_mode));
	append_pod(&out, settings_.root_note);
	append_pod(&out, settings_.adsr.attack);
	append_pod(&out, settings_.adsr.decay);
	append_pod(&out, settings_.adsr.sustain);
	append_pod(&out, settings_.adsr.release);
	append_pod(&out, region_start_);
	append_pod(&out, region_end_);
	append_pod(&out, loop_start_);
	append_pod(&out, loop_end_);

	const uint8_t has_src = source_.frame_count > 0 ? 1 : 0;
	append_pod(&out, has_src);
	if (has_src) {
		append_pod(&out, source_.sample_rate);
		append_pod(&out, source_.channels);
		append_pod(&out, source_.frame_count);
		const uint64_t samples = source_.frame_count * source_.channels;
		append_pod(&out, samples);
		const auto *p = reinterpret_cast<const uint8_t *>(source_.interleaved.data());
		out.insert(out.end(), p, p + samples * sizeof(float));
	}

	const uint8_t has_c = has_committed_ ? 1 : 0;
	append_pod(&out, has_c);
	if (has_c) {
		append_pod(&out, committed_.sample_rate);
		append_pod(&out, committed_.channels);
		append_pod(&out, committed_.frame_count);
		append_pod(&out, committed_.loop_start);
		append_pod(&out, committed_.loop_end);
		append_pod(&out, loop_mode_to_int(committed_.loop_mode));
		const uint64_t samples = committed_.pcm.size();
		append_pod(&out, samples);
		const auto *p = reinterpret_cast<const uint8_t *>(committed_.pcm.data());
		out.insert(out.end(), p, p + samples * sizeof(int16_t));
	}
	return out;
}

bool Sampler::load_state(const uint8_t *data, size_t size) {
	std::lock_guard<std::mutex> lock(mutex_);
	size_t off = 0;
	uint32_t magic = 0;
	uint32_t version = 0;
	if (!read_pod(data, size, &off, &magic) || magic != kStateMagic) {
		return false;
	}
	if (!read_pod(data, size, &off, &version) || version != kStateVersion) {
		return false;
	}
	int32_t rate = 0;
	if (!read_pod(data, size, &off, &rate)) {
		return false;
	}
	settings_.sample_rate = rate == 1 ? SampleRateOption::Rate29760 : SampleRateOption::Rate44100;
	if (!read_pod(data, size, &off, &settings_.budget_bytes)) {
		return false;
	}
	uint8_t flags = 0;
	if (!read_pod(data, size, &off, &flags)) {
		return false;
	}
	settings_.mono_downmix = (flags & 1) != 0;
	settings_.snap_zero_crossing = (flags & 2) != 0;
	settings_.preview_loop = (flags & 4) != 0;
	int mode = 0;
	if (!read_pod(data, size, &off, &mode)) {
		return false;
	}
	settings_.loop_mode = loop_mode_from_int(mode);
	if (!read_pod(data, size, &off, &settings_.root_note)) {
		return false;
	}
	if (!read_pod(data, size, &off, &settings_.adsr.attack) ||
		!read_pod(data, size, &off, &settings_.adsr.decay) ||
		!read_pod(data, size, &off, &settings_.adsr.sustain) ||
		!read_pod(data, size, &off, &settings_.adsr.release)) {
		return false;
	}
	if (!read_pod(data, size, &off, &region_start_) ||
		!read_pod(data, size, &off, &region_end_) ||
		!read_pod(data, size, &off, &loop_start_) ||
		!read_pod(data, size, &off, &loop_end_)) {
		return false;
	}

	uint8_t has_src = 0;
	if (!read_pod(data, size, &off, &has_src)) {
		return false;
	}
	source_ = {};
	if (has_src) {
		uint64_t samples = 0;
		if (!read_pod(data, size, &off, &source_.sample_rate) ||
			!read_pod(data, size, &off, &source_.channels) ||
			!read_pod(data, size, &off, &source_.frame_count) ||
			!read_pod(data, size, &off, &samples)) {
			return false;
		}
		if (off + samples * sizeof(float) > size) {
			return false;
		}
		source_.interleaved.resize(samples);
		std::memcpy(source_.interleaved.data(), data + off, samples * sizeof(float));
		off += samples * sizeof(float);
	}

	uint8_t has_c = 0;
	if (!read_pod(data, size, &off, &has_c)) {
		return false;
	}
	committed_ = {};
	has_committed_ = false;
	if (has_c) {
		int cmode = 0;
		uint64_t samples = 0;
		if (!read_pod(data, size, &off, &committed_.sample_rate) ||
			!read_pod(data, size, &off, &committed_.channels) ||
			!read_pod(data, size, &off, &committed_.frame_count) ||
			!read_pod(data, size, &off, &committed_.loop_start) ||
			!read_pod(data, size, &off, &committed_.loop_end) ||
			!read_pod(data, size, &off, &cmode) ||
			!read_pod(data, size, &off, &samples)) {
			return false;
		}
		committed_.loop_mode = loop_mode_from_int(cmode);
		if (off + samples * sizeof(int16_t) > size) {
			return false;
		}
		committed_.pcm.resize(samples);
		std::memcpy(committed_.pcm.data(), data + off, samples * sizeof(int16_t));
		has_committed_ = committed_.frame_count > 0;
	}
	clamp_markers();
	return true;
}

void Sampler::clamp_markers() {
	if (source_.frame_count == 0) {
		region_start_ = region_end_ = loop_start_ = loop_end_ = 0;
		return;
	}
	const uint64_t n = source_.frame_count;
	if (region_start_ > n) {
		region_start_ = n;
	}
	if (region_end_ > n) {
		region_end_ = n;
	}
	if (region_end_ < region_start_) {
		std::swap(region_start_, region_end_);
	}
	if (loop_start_ < region_start_) {
		loop_start_ = region_start_;
	}
	if (loop_end_ > region_end_) {
		loop_end_ = region_end_;
	}
	if (loop_end_ < loop_start_) {
		std::swap(loop_start_, loop_end_);
	}
}

int Sampler::find_voice(int note, int note_id, int channel, int port) {
	int free_slot = -1;
	for (int i = 0; i < kMaxVoices; ++i) {
		auto &v = voices_[static_cast<size_t>(i)];
		if (!v.in_use) {
			if (free_slot < 0) {
				free_slot = i;
			}
			continue;
		}
		if (v.midi_note == note && (note_id < 0 || v.note_id == note_id) &&
			(channel < 0 || v.channel == channel) && (port < 0 || v.port == port)) {
			return i;
		}
	}
	return free_slot;
}

int Sampler::steal_voice() {
	int oldest = 0;
	uint32_t age = 0;
	for (int i = 0; i < kMaxVoices; ++i) {
		auto &v = voices_[static_cast<size_t>(i)];
		if (!v.in_use) {
			return i;
		}
		if (v.age >= age) {
			age = v.age;
			oldest = i;
		}
	}
	voices_[static_cast<size_t>(oldest)].in_use = false;
	voices_[static_cast<size_t>(oldest)].env.choke();
	return oldest;
}

void Sampler::render_preview_frame(float *left, float *right, double host_sr) {
	if (source_.frame_count == 0 || region_end_ <= region_start_ || host_sr <= 0.0) {
		preview_playing_ = false;
		*left = *right = 0.0f;
		return;
	}
	const double pos = preview_pos_;
	if (source_.channels <= 1) {
		*left = *right = lerp_sample(
			source_.interleaved.data(), source_.frame_count, 1, 0, pos);
	} else {
		*left = lerp_sample(
			source_.interleaved.data(),
			source_.frame_count,
			static_cast<int>(source_.channels),
			0,
			pos);
		*right = lerp_sample(
			source_.interleaved.data(),
			source_.frame_count,
			static_cast<int>(source_.channels),
			1,
			pos);
	}
	preview_pos_ += source_.sample_rate / host_sr;
	if (preview_pos_ >= static_cast<double>(region_end_)) {
		if (settings_.preview_loop) {
			preview_pos_ = static_cast<double>(region_start_);
		} else {
			preview_playing_ = false;
			preview_pos_ = static_cast<double>(region_start_);
		}
	}
}

} // namespace one44
