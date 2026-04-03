#pragma once

#include "../Common.h"
#include <juce_dsp/juce_dsp.h>

#include <vector>

//==============================================================================
/**
 * TransientDetector analyses an AudioBuffer and returns a sorted list of
 * sample positions that correspond to transient onsets.
 *
 * Two onset functions are supported and can be combined:
 *   - Spectral Flux  : positive-only sum of per-bin magnitude differences
 *   - High Frequency Content (HFC) : sum(k * |X[k]|^2)
 *
 * An adaptive threshold (local median * multiplier) is applied before
 * peak-picking, and a minimum slice gap (minSliceSeconds) is enforced.
 */
class TransientDetector
{
public:
    //==========================================================================
    struct Settings
    {
        /** Detection sensitivity: onset function values above this scaled
         *  threshold are accepted as transients.  Acts as the multiplier
         *  applied to the local median in adaptiveThreshold(). */
        float threshold        = 1.5f;

        /** FFT window size (must be a power of two). */
        int   fftSize          = 1024;

        /** Hop size in samples between successive analysis frames. */
        int   hopSize          = 256;

        /** Minimum gap between consecutive slices in seconds. */
        float minSliceSeconds  = 0.05f;

        /** Enable spectral-flux onset detector. */
        bool  useSpectralFlux  = true;

        /** Enable high-frequency-content onset detector. */
        bool  useHFC           = false;
    };

    //==========================================================================
    TransientDetector() = default;

    /**
     * Detect transient onset positions in @p buffer.
     *
     * @param buffer      Source audio (any channel count; will be mixed to mono).
     * @param sampleRate  Sample rate of the buffer (Hz).
     * @param settings    Detection parameters.
     * @return            Sorted vector of sample positions (in frames of the
     *                    original buffer) where transients were detected.
     *                    The first position (0) is always included so that the
     *                    caller can create a complete set of slices.
     */
    std::vector<int> detectTransients (const juce::AudioBuffer<float>& buffer,
                                       double                           sampleRate,
                                       const Settings&                  settings = {});

private:
    //==========================================================================
    /** Compute per-frame spectral flux from a mono onset signal.
     *  @param mono       Mono audio data.
     *  @param fftSize    FFT window length (power of two).
     *  @param hopSize    Hop in samples.
     *  @return           One non-negative flux value per analysis frame.
     */
    std::vector<float> computeSpectralFlux (const std::vector<float>& mono,
                                            int fftSize,
                                            int hopSize);

    /** Compute per-frame High Frequency Content from a mono onset signal.
     *  @param mono       Mono audio data.
     *  @param fftSize    FFT window length (power of two).
     *  @param hopSize    Hop in samples.
     *  @return           One HFC value per analysis frame.
     */
    std::vector<float> computeHFC (const std::vector<float>& mono,
                                   int fftSize,
                                   int hopSize);

    /** Compute an adaptive threshold curve from an onset function.
     *
     *  For each frame i the threshold is:
     *      median(onsetFn[i-halfWin .. i+halfWin]) * multiplier
     *
     *  @param onsetFn    Input onset function values.
     *  @param halfWin    Half-width (in frames) of the local median window.
     *  @param multiplier Scales the median; use Settings::threshold.
     *  @return           Per-frame threshold values (same length as onsetFn).
     */
    std::vector<float> adaptiveThreshold (const std::vector<float>& onsetFn,
                                          int   halfWin    = 8,
                                          float multiplier = 1.5f);
};
