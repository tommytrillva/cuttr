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

struct SlicePoint
{
    int samplePosition = 0;
    bool isLocked = false;  // User-locked slices won't be moved by auto-detect
};

struct PadSettings
{
    int sliceIndex = -1;                // Which slice this pad plays (-1 = unassigned)
    float pitchOffsetSemitones = 0.0f;  // -24 to +24
    float volume = 1.0f;                // 0..1
    float pan = 0.0f;                   // -1..1
    int chokeGroup = 0;                 // 0 = no choke group
    bool reverse = false;
    bool mute = false;
    bool solo = false;
    TimeStretchMode stretchMode = TimeStretchMode::None;
    float stretchRatio = 1.0f;          // Manual stretch ratio (PerPad/Free modes)
};

static constexpr int kMaxPads   = 32;
static constexpr int kMaxVoices = 16;
static constexpr int kMaxSlices = 64;
