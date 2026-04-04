#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
// Shared types for the CHOPPR sampler plugin

enum class SliceMode
{
    Transient,  // Auto-detect transients
    Equal,      // Equal-length slices
    Beat,       // Slice on beat divisions
    Bar         // Slice on bar boundaries
};

enum class TimeStretchMode
{
    None,       // No stretching
    MatchBPM,   // Stretch all slices to match project BPM
    PerPad,     // Per-pad stretch settings
    Free        // Free-form stretching
};

enum class PitchMode
{
    Chromatic,   // Chromatic pitch shifting (rate-based)
    Tonal,       // Tonal (formant-preserving - future)
    Percussive   // Percussive (transient-focused - future)
};

enum class GridSize
{
    Grid4x4 = 16,
    Grid4x8 = 32
};

enum class PadPlayMode
{
    OneShot = 0,  // plays full slice to end regardless of note-off
    Gate,         // plays while MIDI note held, stops on note-off
    Loop          // loops the slice while held, stops on note-off
};

struct SlicePoint
{
    int samplePosition = 0;
    bool isLocked = false;  // User-locked slices won't be moved by auto-detect
};

//==============================================================================
/** Per-pad insert FX parameters. */
struct PadFxSettings
{
    // ── Filter ──────────────────────────────────────────────────────────────
    bool  filterEnabled   { false };
    int   filterType      { 0 };       // 0=LowPass 1=HighPass 2=BandPass 3=Notch
    float filterCutoff    { 8000.0f }; // Hz  (20 – 20000)
    float filterResonance { 0.707f };  // Q   (0.1 – 10)

    // ── Distortion ──────────────────────────────────────────────────────────
    bool  distortionEnabled { false };
    float distortionDrive   { 2.0f };  // 1 – 20
    float distortionMix     { 1.0f };  // 0 – 1  (wet/dry)

    // ── Reverb ──────────────────────────────────────────────────────────────
    bool  reverbEnabled  { false };
    float reverbRoomSize { 0.5f };     // 0 – 1
    float reverbDamping  { 0.5f };     // 0 – 1
    float reverbWidth    { 1.0f };     // 0 – 1
    float reverbMix      { 0.3f };     // 0 – 1  (wet/dry)

    // ── Delay ────────────────────────────────────────────────────────────────
    bool  delayEnabled   { false };
    float delayTimeMs    { 250.0f };   // 1 – 2000 ms
    float delayFeedback  { 0.4f };     // 0 – 0.95
    float delayMix       { 0.3f };     // 0 – 1  (wet/dry)
};

struct PadSettings
{
    int sliceIndex = -1;                // Which slice this pad plays (-1 = unassigned)
    float pitchOffsetSemitones = 0.0f;  // -24 to +24
    float fineTuneCents { 0.0f };       // -100 to +100
    float volume = 1.0f;                // 0..1
    float pan = 0.0f;                   // -1..1
    int chokeGroup = 0;                 // 0 = no choke group
    bool reverse = false;
    bool mute = false;
    bool solo = false;
    TimeStretchMode stretchMode = TimeStretchMode::None;
    float stretchRatio = 1.0f;          // Manual stretch ratio (PerPad/Free modes)
    PadPlayMode playMode { PadPlayMode::OneShot };
    PadFxSettings fx;
};

static constexpr int kMaxPads   = 32;
static constexpr int kMaxVoices = 16;
static constexpr int kMaxSlices = 64;
