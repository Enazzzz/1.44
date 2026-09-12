#pragma once

namespace one44 {

/// Playback-rate pitch mapping: the root note plays at captured speed, and
/// other MIDI notes change rate (not time-stretch). Chipmunk/demonic by design.
///
/// `rate = 2^((midi_note - root_note) / 12)`
double playback_rate_for_note(int midi_note, int root_note);

/// Clamps a MIDI note number into 0..127.
int clamp_midi_note(int note);

} // namespace one44
