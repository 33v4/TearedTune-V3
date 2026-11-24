#pragma once

// Parameter IDs and display names shared by the processor and the editor.
namespace pid
{
    constexpr const char* retune    = "retune";
    constexpr const char* amount    = "amount";
    constexpr const char* vibrato   = "vibrato";
    constexpr const char* flex      = "flex";
    constexpr const char* transpose = "transpose";
    constexpr const char* detune    = "detune";
    constexpr const char* refa      = "refa";
    constexpr const char* mix       = "mix";
    constexpr const char* ingain    = "ingain";
    constexpr const char* outgain   = "outgain";
    constexpr const char* range     = "range";
    constexpr const char* key       = "key";
    constexpr const char* scale     = "scale";
    constexpr const char* bypass    = "bypass";
}

inline const char* const kNoteIds[12]   = { "note_c", "note_cs", "note_d", "note_ds", "note_e", "note_f",
                                            "note_fs", "note_g", "note_gs", "note_a", "note_as", "note_b" };
inline const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline const char* const kScaleNames[10] = { "Chromatic", "Major", "Minor", "Harmonic Minor", "Major Pentatonic",
                                             "Minor Pentatonic", "Blues", "Dorian", "Mixolydian", "Custom" };
inline const char* const kRangeNames[4]  = { "Low voice (60-400 Hz)", "Mid voice (80-800 Hz)",
                                             "High voice (150-1400 Hz)", "Wide (50-1500 Hz)" };
constexpr int kCustomScale = 9;
