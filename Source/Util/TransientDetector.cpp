#include "TransientDetector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

//==============================================================================
// Internal helpers
//==============================================================================
namespace
{
    /** Build a Hann window of length n. */
    static std::vector<float> makeHannWindow (int n)
    {
        std::vector<float> w (static_cast<size_t> (n));
        const double twoPiOverNm1 = juce::MathConstants<double>::twoPi / static_cast<double> (n - 1);
        for (int i = 0; i < n; ++i)
            w[static_cast<size_t> (i)] = static_cast<float> (0.5 * (1.0 - std::cos (twoPiOverNm1 * i)));
        return w;
    }

    /** Return the next power-of-two >= n (minimum 2). */
    static int nextPow2 (int n)
    {
        int p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    /** Return the log2 of a power-of-two value. */
    static int log2int (int n)
    {
        int log = 0;
        while (n > 1) { n >>= 1; ++log; }
        return log;
    }
} // namespace

//==============================================================================
std::vector<float> TransientDetector::computeSpectralFlux (const std::vector<float>& mono,
                                                            int fftSize,
                                                            int hopSize)
{
    jassert (fftSize > 0 && (fftSize & (fftSize - 1)) == 0); // must be power of two

    const int order       = log2int (fftSize);
    juce::dsp::FFT fft (order);

    const int   numSamples   = static_cast<int> (mono.size());
    const int   complexSize  = fftSize + 2;          // juce FFT interleaved: real/imag pairs, fftSize/2+1 bins
    const int   numBins      = fftSize / 2 + 1;

    auto hann = makeHannWindow (fftSize);

    // Temporary buffers reused each frame
    std::vector<float> frame      (static_cast<size_t> (fftSize * 2), 0.0f); // interleaved complex
    std::vector<float> prevMag    (static_cast<size_t> (numBins),     0.0f);
    std::vector<float> currMag    (static_cast<size_t> (numBins),     0.0f);

    std::vector<float> flux;

    for (int hop = 0; hop * hopSize < numSamples; ++hop)
    {
        const int start = hop * hopSize;

        // Fill and window the real part; zero imaginary (interleaved layout)
        std::fill (frame.begin(), frame.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
        {
            const int srcIdx = start + i;
            const float sample = (srcIdx < numSamples) ? mono[static_cast<size_t> (srcIdx)] : 0.0f;
            frame[static_cast<size_t> (i * 2)]     = sample * hann[static_cast<size_t> (i)];
            frame[static_cast<size_t> (i * 2 + 1)] = 0.0f;
        }

        fft.performRealOnlyForwardTransform (frame.data(), true);

        // Extract magnitude spectrum from interleaved complex output
        // JUCE's performRealOnlyForwardTransform stores bins as [Re0, Im0, Re1, Im1, ...]
        for (int k = 0; k < numBins; ++k)
        {
            const float re  = frame[static_cast<size_t> (k * 2)];
            const float im  = frame[static_cast<size_t> (k * 2 + 1)];
            currMag[static_cast<size_t> (k)] = std::sqrt (re * re + im * im);
        }

        // Spectral flux: sum of positive magnitude differences
        float f = 0.0f;
        for (int k = 0; k < numBins; ++k)
        {
            const float diff = currMag[static_cast<size_t> (k)] - prevMag[static_cast<size_t> (k)];
            if (diff > 0.0f) f += diff;
        }
        flux.push_back (f);

        std::swap (prevMag, currMag);
    }

    return flux;
}

//==============================================================================
std::vector<float> TransientDetector::computeHFC (const std::vector<float>& mono,
                                                   int fftSize,
                                                   int hopSize)
{
    jassert (fftSize > 0 && (fftSize & (fftSize - 1)) == 0);

    const int order   = log2int (fftSize);
    juce::dsp::FFT fft (order);

    const int numSamples = static_cast<int> (mono.size());
    const int numBins    = fftSize / 2 + 1;

    auto hann = makeHannWindow (fftSize);
    std::vector<float> frame (static_cast<size_t> (fftSize * 2), 0.0f);
    std::vector<float> hfc;

    for (int hop = 0; hop * hopSize < numSamples; ++hop)
    {
        const int start = hop * hopSize;

        std::fill (frame.begin(), frame.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
        {
            const int   srcIdx = start + i;
            const float sample = (srcIdx < numSamples) ? mono[static_cast<size_t> (srcIdx)] : 0.0f;
            frame[static_cast<size_t> (i * 2)]     = sample * hann[static_cast<size_t> (i)];
            frame[static_cast<size_t> (i * 2 + 1)] = 0.0f;
        }

        fft.performRealOnlyForwardTransform (frame.data(), true);

        // HFC = sum(k * |X[k]|^2)
        float h = 0.0f;
        for (int k = 0; k < numBins; ++k)
        {
            const float re  = frame[static_cast<size_t> (k * 2)];
            const float im  = frame[static_cast<size_t> (k * 2 + 1)];
            const float mag2 = re * re + im * im;
            h += static_cast<float> (k) * mag2;
        }
        hfc.push_back (h);
    }

    return hfc;
}

//==============================================================================
std::vector<float> TransientDetector::adaptiveThreshold (const std::vector<float>& onsetFn,
                                                          int   halfWin,
                                                          float multiplier)
{
    const int n = static_cast<int> (onsetFn.size());
    std::vector<float> thresh (static_cast<size_t> (n), 0.0f);
    std::vector<float> localWindow;
    localWindow.reserve (static_cast<size_t> (2 * halfWin + 1));

    for (int i = 0; i < n; ++i)
    {
        const int lo = std::max (0, i - halfWin);
        const int hi = std::min (n - 1, i + halfWin);

        localWindow.clear();
        for (int j = lo; j <= hi; ++j)
            localWindow.push_back (onsetFn[static_cast<size_t> (j)]);

        // Median via partial sort
        const size_t mid = localWindow.size() / 2;
        std::nth_element (localWindow.begin(),
                          localWindow.begin() + static_cast<std::ptrdiff_t> (mid),
                          localWindow.end());

        float median = localWindow[mid];
        if (localWindow.size() % 2 == 0 && mid > 0)
        {
            // Even-length: average of two middle elements
            auto it = std::max_element (localWindow.begin(),
                                        localWindow.begin() + static_cast<std::ptrdiff_t> (mid));
            median = (median + *it) * 0.5f;
        }

        thresh[static_cast<size_t> (i)] = median * multiplier;
    }

    return thresh;
}

//==============================================================================
std::vector<int> TransientDetector::detectTransients (const juce::AudioBuffer<float>& buffer,
                                                       double                           sampleRate,
                                                       const Settings&                  settings)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return { 0 };

    // Ensure fftSize is a valid power-of-two >= 64
    const int fftSize = nextPow2 (std::max (64, settings.fftSize));
    const int hopSize = std::max (1, settings.hopSize);

    //--------------------------------------------------------------------------
    // 1. Mix to mono
    //--------------------------------------------------------------------------
    std::vector<float> mono (static_cast<size_t> (numSamples), 0.0f);
    const float invChannels = 1.0f / static_cast<float> (numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            mono[static_cast<size_t> (i)] += src[i] * invChannels;
    }

    //--------------------------------------------------------------------------
    // 2. Compute onset function(s)
    //--------------------------------------------------------------------------
    std::vector<float> onsetFn;

    if (settings.useSpectralFlux && settings.useHFC)
    {
        auto flux = computeSpectralFlux (mono, fftSize, hopSize);
        auto hfc  = computeHFC          (mono, fftSize, hopSize);

        // Normalise each to [0,1] then average
        auto normalise = [] (std::vector<float>& v)
        {
            const float mx = *std::max_element (v.begin(), v.end());
            if (mx > 0.0f)
                for (auto& x : v) x /= mx;
        };
        normalise (flux);
        normalise (hfc);

        onsetFn.resize (std::min (flux.size(), hfc.size()));
        for (size_t i = 0; i < onsetFn.size(); ++i)
            onsetFn[i] = (flux[i] + hfc[i]) * 0.5f;
    }
    else if (settings.useHFC)
    {
        onsetFn = computeHFC (mono, fftSize, hopSize);
    }
    else
    {
        // Default: spectral flux (also used when both flags are false)
        onsetFn = computeSpectralFlux (mono, fftSize, hopSize);
    }

    if (onsetFn.empty())
        return { 0 };

    //--------------------------------------------------------------------------
    // 3. Adaptive threshold
    //--------------------------------------------------------------------------
    auto thresh = adaptiveThreshold (onsetFn, 8, settings.threshold);

    //--------------------------------------------------------------------------
    // 4. Peak-picking with minimum gap enforcement
    //--------------------------------------------------------------------------
    const int minSampleGap = static_cast<int> (settings.minSliceSeconds * sampleRate);
    const int minFrameGap  = std::max (1, minSampleGap / hopSize);

    std::vector<int> positions;
    positions.push_back (0); // always include the start

    int lastAcceptedFrame = -minFrameGap; // allow first hit immediately

    const int numFrames = static_cast<int> (onsetFn.size());
    for (int f = 1; f < numFrames; ++f) // skip frame 0 (already added as 0)
    {
        // Must exceed threshold
        if (onsetFn[static_cast<size_t> (f)] <= thresh[static_cast<size_t> (f)])
            continue;

        // Must be a local peak
        const float prev = (f > 0)            ? onsetFn[static_cast<size_t> (f - 1)] : 0.0f;
        const float next = (f < numFrames - 1) ? onsetFn[static_cast<size_t> (f + 1)] : 0.0f;
        if (onsetFn[static_cast<size_t> (f)] < prev || onsetFn[static_cast<size_t> (f)] < next)
            continue;

        // Enforce minimum gap
        if (f - lastAcceptedFrame < minFrameGap)
            continue;

        const int samplePos = f * hopSize;
        if (samplePos < numSamples)
        {
            positions.push_back (samplePos);
            lastAcceptedFrame = f;
        }
    }

    std::sort (positions.begin(), positions.end());
    // Remove duplicates that might arise from the forced 0
    positions.erase (std::unique (positions.begin(), positions.end()), positions.end());

    return positions;
}
