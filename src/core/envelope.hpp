#pragma once

namespace one44 {

/// ADSR times are in seconds; sustain is a 0..1 level.
struct ADSR {
	float attack = 0.005f;
	float decay = 0.080f;
	float sustain = 0.850f;
	float release = 0.120f;
};

/// Envelope stages. Notes enter Attack on note-on and Release on note-off.
enum class EnvStage {
	Idle,
	Attack,
	Decay,
	Sustain,
	Release,
};

/// Per-voice amplitude envelope. Prevents hard-cuts on note-off; does not
/// smooth loop points (those clicks are period-accurate).
struct Envelope {
	EnvStage stage = EnvStage::Idle;
	float level = 0.0f;

	/// Starts the attack from the current level (usually 0).
	void note_on();

	/// Starts the release from the current level.
	void note_off();

	/// Instantly silences the voice (note choke / reset).
	void choke();

	/// Advances one host sample and returns the amplitude in [0, 1].
	float tick(const ADSR &params, double host_sample_rate);

	/// True when the envelope can produce sound.
	bool active() const;
};

} // namespace one44
