#include "pitch.hpp"

#include <algorithm>
#include <cmath>

namespace one44 {

/// Equal-tempered playback-rate mapping. Octave up doubles rate; octave down halves it.
double playback_rate_for_note(int midi_note, int root_note) {
	const int note = clamp_midi_note(midi_note);
	const int root = clamp_midi_note(root_note);
	const double semitones = static_cast<double>(note - root);
	return std::pow(2.0, semitones / 12.0);
}

/// MIDI notes are 7-bit; keep mapping defined for out-of-range host events.
int clamp_midi_note(int note) {
	return std::max(0, std::min(127, note));
}

} // namespace one44
