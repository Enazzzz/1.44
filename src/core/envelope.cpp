#include "envelope.hpp"

#include <algorithm>
#include <cmath>

namespace one44 {

/// Begins attack; keeps any residual level so retriggers don't click from zero only.
void Envelope::note_on() {
	stage = EnvStage::Attack;
}

/// Begins release from the current amplitude.
void Envelope::note_off() {
	if (stage == EnvStage::Idle) {
		return;
	}
	stage = EnvStage::Release;
}

/// Hard stop used for choke / voice steal.
void Envelope::choke() {
	stage = EnvStage::Idle;
	level = 0.0f;
}

/// Advances the ADSR one sample at `host_sample_rate`.
float Envelope::tick(const ADSR &params, double host_sample_rate) {
	if (host_sample_rate <= 0.0) {
		return 0.0f;
	}
	const float sr = static_cast<float>(host_sample_rate);

	auto increment = [&](float seconds, float from, float to) -> float {
		if (seconds <= 1.0e-5f) {
			return to;
		}
		const float delta = (to - from) / (seconds * sr);
		return from + delta;
	};

	switch (stage) {
	case EnvStage::Idle:
		level = 0.0f;
		break;
	case EnvStage::Attack: {
		level = increment(params.attack, level, 1.0f);
		if (params.attack <= 1.0e-5f || level >= 0.999f) {
			level = 1.0f;
			stage = EnvStage::Decay;
		}
		break;
	}
	case EnvStage::Decay: {
		const float sustain = std::max(0.0f, std::min(1.0f, params.sustain));
		level = increment(params.decay, level, sustain);
		if (params.decay <= 1.0e-5f || level <= sustain + 0.001f) {
			level = sustain;
			stage = EnvStage::Sustain;
		}
		break;
	}
	case EnvStage::Sustain:
		level = std::max(0.0f, std::min(1.0f, params.sustain));
		break;
	case EnvStage::Release: {
		level = increment(params.release, level, 0.0f);
		if (params.release <= 1.0e-5f || level <= 0.001f) {
			level = 0.0f;
			stage = EnvStage::Idle;
		}
		break;
	}
	}
	if (level < 0.0f) {
		level = 0.0f;
	}
	if (level > 1.0f) {
		level = 1.0f;
	}
	return level;
}

/// Idle means the voice can be reused.
bool Envelope::active() const {
	return stage != EnvStage::Idle;
}

} // namespace one44
