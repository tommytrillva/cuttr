#include "AudioFileExporter.h"

#include <algorithm>
#include <cmath>

//==============================================================================
AudioFileExporter::AudioFileExporter()
{
    formatManager_.registerBasicFormats();
}

//==============================================================================
float AudioFileExporter::getPeakLevel (const juce::AudioBuffer<float>& buffer) const
{
    float peak = 0.0f;
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float magnitude = std::abs (data[i]);
            if (magnitude > peak)
                peak = magnitude;
        }
    }
    return peak;
}

//==============================================================================
std::unique_ptr<juce::AudioFormatWriter>
AudioFileExporter::createWriter (const juce::File&     file,
                                  const ExportSettings& settings,
                                  int                   numChannels,
                                  juce::String&         errorMsg)
{
    // Determine the format from the file extension (or the settings enum as a
    // fallback for extensionless paths).
    juce::AudioFormat* format = nullptr;

    const juce::String ext = file.getFileExtension().toLowerCase();

    if (ext == ".wav" || (ext.isEmpty() && settings.format == Format::WAV))
        format = formatManager_.findFormatForFileExtension ("wav");
    else if (ext == ".flac" || (ext.isEmpty() && settings.format == Format::FLAC))
        format = formatManager_.findFormatForFileExtension ("flac");
    else
        format = formatManager_.findFormatForFileExtension (ext.trimCharactersAtStart ("."));

    if (format == nullptr)
    {
        errorMsg = "No audio format found for extension: " + ext;
        return nullptr;
    }

    // Create the output file, overwriting any existing file.
    auto outputStream = std::make_unique<juce::FileOutputStream> (file);
    if (outputStream->failedToOpen())
    {
        errorMsg = "Could not open file for writing: " + file.getFullPathName();
        return nullptr;
    }
    outputStream->setPosition (0);
    outputStream->truncate();

    const int bitsPerSample = static_cast<int> (settings.bitDepth);

    // Build a quality options StringPairArray — used by some formats (e.g. FLAC).
    juce::StringPairArray qualityOptions;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        format->createWriterFor (outputStream.get(),
                                 settings.sampleRate,
                                 static_cast<unsigned int> (numChannels),
                                 bitsPerSample,
                                 qualityOptions,
                                 0 /* quality index */));

    if (writer != nullptr)
    {
        // The writer now owns the stream.
        outputStream.release(); // NOLINT (clang-analyzer-cplusplus.NewDeleteLeaks)
    }
    else
    {
        errorMsg = "AudioFormat::createWriterFor() failed for: " + file.getFullPathName();
    }

    return writer;
}

//==============================================================================
bool AudioFileExporter::exportBuffer (const juce::AudioBuffer<float>& buffer,
                                       const ExportSettings&            settings,
                                       juce::String&                    errorMsg)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
    {
        errorMsg = "Buffer is empty — nothing to export.";
        return false;
    }

    // Determine output file (append correct extension if missing)
    juce::File outputFile = settings.outputFile;
    {
        const juce::String ext = outputFile.getFileExtension().toLowerCase();
        const juce::String expectedExt = (settings.format == Format::FLAC) ? ".flac" : ".wav";
        if (ext != expectedExt)
            outputFile = outputFile.withFileExtension (expectedExt);
    }

    // Create parent directory if needed
    outputFile.getParentDirectory().createDirectory();

    // Build (potentially normalised) working copy
    juce::AudioBuffer<float> workBuffer;

    if (settings.normalise)
    {
        const float peak = getPeakLevel (buffer);
        if (peak > 0.0f)
        {
            workBuffer.makeCopyOf (buffer);
            const float gain = 1.0f / peak;
            for (int ch = 0; ch < workBuffer.getNumChannels(); ++ch)
                workBuffer.applyGain (ch, 0, workBuffer.getNumSamples(), gain);
        }
        else
        {
            // Silent buffer — copy as-is (no-op normalisation).
            workBuffer.makeCopyOf (buffer);
        }
    }

    const juce::AudioBuffer<float>& srcBuffer = settings.normalise ? workBuffer : buffer;

    // Create writer
    auto writer = createWriter (outputFile, settings, numChannels, errorMsg);
    if (writer == nullptr)
        return false;

    // Write all samples in one call
    if (! writer->writeFromAudioSampleBuffer (srcBuffer, 0, srcBuffer.getNumSamples()))
    {
        errorMsg = "Failed to write audio data to: " + outputFile.getFullPathName();
        return false;
    }

    return true;
}

//==============================================================================
bool AudioFileExporter::exportSlices (const juce::AudioBuffer<float>&  buffer,
                                       const std::vector<SlicePoint>&   slices,
                                       const ExportSettings&             settings,
                                       juce::String&                     errorMsg)
{
    const int numChannels = buffer.getNumChannels();
    const int totalSamples = buffer.getNumSamples();

    if (numChannels <= 0 || totalSamples <= 0)
    {
        errorMsg = "Buffer is empty — nothing to export.";
        return false;
    }

    if (slices.empty())
    {
        errorMsg = "No slices provided.";
        return false;
    }

    // Derive naming components from the template file in settings
    const juce::String expectedExt = (settings.format == Format::FLAC) ? ".flac" : ".wav";
    const juce::File   templateFile = settings.outputFile.withFileExtension (expectedExt);
    const juce::File   parentDir    = templateFile.getParentDirectory();
    const juce::String baseName     = templateFile.getFileNameWithoutExtension();

    parentDir.createDirectory();

    bool allSucceeded = true;

    const int numSlices = static_cast<int> (slices.size());
    for (int si = 0; si < numSlices; ++si)
    {
        // Determine slice sample boundaries
        const int sliceStart = slices[static_cast<size_t> (si)].samplePosition;
        const int sliceEnd   = (si + 1 < numSlices)
                                   ? slices[static_cast<size_t> (si + 1)].samplePosition
                                   : totalSamples;

        const int sliceLength = sliceEnd - sliceStart;
        if (sliceLength <= 0)
            continue; // Skip zero-length or inverted slices

        // Extract sub-buffer for this slice
        juce::AudioBuffer<float> sliceBuffer (numChannels, sliceLength);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            sliceBuffer.copyFrom (ch,           // destChannel
                                  0,             // destStartSample
                                  buffer,        // source
                                  ch,            // sourceChannel
                                  sliceStart,    // sourceStartSample
                                  sliceLength);  // numSamples
        }

        // Build per-slice filename: baseName_001.wav (3-digit, zero-padded)
        const juce::String sliceName = baseName
                                       + "_"
                                       + juce::String (si + 1).paddedLeft ('0', 3);
        const juce::File sliceFile = parentDir.getChildFile (sliceName + expectedExt);

        // Build per-slice export settings (same as template but with new path)
        ExportSettings sliceSettings = settings;
        sliceSettings.outputFile = sliceFile;

        juce::String sliceError;
        if (! exportBuffer (sliceBuffer, sliceSettings, sliceError))
        {
            errorMsg = "Slice " + juce::String (si + 1) + ": " + sliceError;
            allSucceeded = false;
            // Continue attempting remaining slices so caller gets partial results.
        }
    }

    return allSucceeded;
}
