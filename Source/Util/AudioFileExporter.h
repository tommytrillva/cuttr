#pragma once

#include "../Common.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <vector>

//==============================================================================
/**
 * AudioFileExporter writes AudioBuffer data to disk in WAV or FLAC format.
 *
 * Two export operations are provided:
 *  - exportBuffer  : write the entire buffer as a single file.
 *  - exportSlices  : write each slice (defined by a vector of SlicePoints) as
 *                    a separate numbered file placed alongside the output file.
 *
 * Normalisation, bit depth and sample rate are configurable per export.
 */
class AudioFileExporter
{
public:
    //==========================================================================
    enum class Format
    {
        WAV,   ///< Uncompressed PCM wave file (.wav)
        FLAC   ///< Losslessly compressed FLAC file (.flac)
    };

    enum class BitDepth
    {
        Bits16 = 16,   ///< 16-bit integer PCM
        Bits24 = 24,   ///< 24-bit integer PCM
        Bits32 = 32    ///< 32-bit integer PCM (or 32-bit float for WAV)
    };

    //==========================================================================
    struct ExportSettings
    {
        Format    format     = Format::WAV;         ///< Output container format.
        BitDepth  bitDepth   = BitDepth::Bits24;    ///< Bit depth of output samples.
        double    sampleRate = 44100.0;             ///< Output sample rate (Hz).

        /** Destination file.  For exportSlices() this acts as the template:
         *  slice files are written into the same directory with a numeric suffix
         *  appended before the extension, e.g. "drums_001.wav". */
        juce::File outputFile;

        /** When true the buffer is scaled so its peak level is 0 dBFS before
         *  writing.  Has no effect on silent buffers. */
        bool normalise = false;
    };

    //==========================================================================
    AudioFileExporter();

    /**
     * Export an entire buffer to a single file.
     *
     * @param buffer    Audio data to write.
     * @param settings  Export parameters (format, path, etc.).
     * @param errorMsg  Populated on failure with a human-readable description.
     * @return          true on success, false on any error.
     */
    bool exportBuffer (const juce::AudioBuffer<float>& buffer,
                       const ExportSettings&            settings,
                       juce::String&                    errorMsg);

    /**
     * Export each slice as an individual numbered file.
     *
     * Slices are bounded by consecutive SlicePoints; the last slice runs to
     * the end of the buffer.  The output filename template in @p settings is
     * used to generate per-slice filenames.
     *
     * @param buffer    Full source audio buffer.
     * @param slices    Ordered slice boundary points (at least one required).
     * @param settings  Export parameters.  outputFile is used as a naming template.
     * @param errorMsg  Populated on failure.
     * @return          true if all slices were written successfully.
     */
    bool exportSlices (const juce::AudioBuffer<float>&  buffer,
                       const std::vector<SlicePoint>&   slices,
                       const ExportSettings&             settings,
                       juce::String&                     errorMsg);

private:
    //==========================================================================
    /**
     * Create an AudioFormatWriter for @p file using the format and bit depth
     * specified in @p settings.  Returns nullptr on failure and sets @p errorMsg.
     */
    std::unique_ptr<juce::AudioFormatWriter> createWriter (const juce::File&     file,
                                                            const ExportSettings& settings,
                                                            int                   numChannels,
                                                            juce::String&         errorMsg);

    /**
     * Scan all channels of @p buffer and return the maximum absolute sample
     * value (0.0 if the buffer is empty or silent).
     */
    float getPeakLevel (const juce::AudioBuffer<float>& buffer) const;

    //==========================================================================
    juce::AudioFormatManager formatManager_;
};
