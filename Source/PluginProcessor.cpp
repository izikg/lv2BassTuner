// BassTuner — Bass Guitar Pitch Detector
// LV2 plugin for Ubuntu and Darkglass Anagram
// SPDX-License-Identifier: ISC

#include "PluginProcessor.h"

#include <cmath>
#include <cstring>
#include <algorithm>

//==============================================================================
// YinDetector implementation
//==============================================================================

YinDetector::YinDetector()
{
    reset();
}

void YinDetector::reset()
{
    std::memset (buffer, 0, sizeof (buffer));
    writePos = 0;
    filled   = 0;
}

float YinDetector::process (float sample, double sampleRate)
{
    // Fill circular buffer
    buffer[writePos] = sample;
    writePos = (writePos + 1) % kBufferSize;
    if (filled < kBufferSize)
        ++filled;

    // Only attempt detection once we have a full buffer
    if (filled < kBufferSize)
        return -1.f;

    const int tauMin = static_cast<int> (std::floor (sampleRate / kMaxFreq));
    const int tauMax = static_cast<int> (std::ceil  (sampleRate / kMinFreq));

    // Guard against bad tau range
    if (tauMin < 1 || tauMax >= kBufferSize / 2)
        return -1.f;

    // Temporary difference buffer
    float d[kBufferSize / 2] = {};

    computeDifference (d, tauMax);
    normaliseCMND     (d, tauMax);

    const int tau = findPitch (d, tauMin, tauMax);
    if (tau < 0)
        return -1.f;

    const float refinedTau = parabolicInterp (d, tau);
    if (refinedTau <= 0.f)
        return -1.f;

    return static_cast<float> (sampleRate) / refinedTau;
}

void YinDetector::computeDifference (float* d, int tauMax) const
{
    // d[tau] = sum over j of (x[j] - x[j+tau])^2
    // We read from the circular buffer in chronological order.
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < tauMax; ++j)
        {
            const int idx0 = (writePos + j)       & (kBufferSize - 1);
            const int idx1 = (writePos + j + tau)  & (kBufferSize - 1);
            const float delta = buffer[idx0] - buffer[idx1];
            sum += delta * delta;
        }
        d[tau] = static_cast<float> (sum);
    }
}

void YinDetector::normaliseCMND (float* d, int tauMax) const
{
    d[0] = 1.f;
    double runningSum = 0.0;

    for (int tau = 1; tau <= tauMax; ++tau)
    {
        runningSum += d[tau];
        if (runningSum > 0.0)
            d[tau] = static_cast<float> (d[tau] * tau / runningSum);
        else
            d[tau] = 1.f;
    }
}

int YinDetector::findPitch (const float* d, int tauMin, int tauMax) const
{
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (d[tau] < kThreshold)
        {
            // Find local minimum
            while (tau + 1 <= tauMax && d[tau + 1] < d[tau])
                ++tau;
            return tau;
        }
    }

    // Fallback: return global minimum in range
    int   bestTau = tauMin;
    float bestVal = d[tauMin];
    for (int tau = tauMin + 1; tau <= tauMax; ++tau)
    {
        if (d[tau] < bestVal)
        {
            bestVal = d[tau];
            bestTau = tau;
        }
    }
    // Only accept if reasonably confident
    return (bestVal < 0.35f) ? bestTau : -1;
}

float YinDetector::parabolicInterp (const float* d, int tau) const
{
    if (tau <= 0 || tau >= kBufferSize / 2 - 1)
        return static_cast<float> (tau);

    const float s0 = d[tau - 1];
    const float s1 = d[tau];
    const float s2 = d[tau + 1];

    const float denom = 2.f * (2.f * s1 - s0 - s2);
    if (std::abs (denom) < 1e-10f)
        return static_cast<float> (tau);

    return static_cast<float> (tau) + (s0 - s2) / denom;
}

//==============================================================================
// NoteUtil implementation
//==============================================================================

float NoteUtil::freqToMidi (float freq)
{
    // MIDI note 69 = A4 = 440 Hz
    return 69.f + 12.f * std::log2 (freq / 440.f);
}

juce::String NoteUtil::midiToName (int midiNote)
{
    static const char* names[] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };

    midiNote = std::clamp (midiNote, 0, 127);

    const int octave   = (midiNote / 12) - 1;
    const int noteIdx  = midiNote % 12;

    return juce::String (names[noteIdx]) + juce::String (octave);
}

float NoteUtil::centDeviation (float freq)
{
    const float midi        = freqToMidi (freq);
    const int   nearestNote = static_cast<int> (std::round (midi));
    return (midi - static_cast<float> (nearestNote)) * 100.f;
}

//==============================================================================
// BassTunerProcessor
//==============================================================================

BassTunerProcessor::BassTunerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono())
                          .withOutput ("Output", juce::AudioChannelSet::mono()))
{
    // --- Bypass (required by Anagram) ---
    bypass = new juce::AudioParameterBool (
        { "bypass", 1 }, "Bypass", false);
    addParameter (bypass);

    // --- Output: nearest MIDI note (0–127) ---
    // Anagram can display/bind this value — useful for automation
    outMidiNote = new juce::AudioParameterFloat (
        { "midi_note", 2 },
        "MIDI Note",
        juce::NormalisableRange<float> (0.f, 127.f, 1.f),
        0.f,
        juce::AudioParameterFloatAttributes().withLabel ("note"));
    addParameter (outMidiNote);

    // --- Output: cents deviation from equal temperament (-50..+50) ---
    outCents = new juce::AudioParameterFloat (
        { "cents", 3 },
        "Cents",
        juce::NormalisableRange<float> (-50.f, 50.f, 0.1f),
        0.f,
        juce::AudioParameterFloatAttributes().withLabel ("¢"));
    addParameter (outCents);

    // --- Output: raw detected frequency in Hz ---
    outFrequency = new juce::AudioParameterFloat (
        { "frequency", 4 },
        "Frequency",
        juce::NormalisableRange<float> (0.f, 500.f, 0.01f),
        0.f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz"));
    addParameter (outFrequency);
}

BassTunerProcessor::~BassTunerProcessor() {}

//==============================================================================

void BassTunerProcessor::prepareToPlay (double sampleRate, int /*maxSamplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    yin.reset();
}

void BassTunerProcessor::releaseResources() {}

//==============================================================================

void BassTunerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& /*midiMessages*/)
{
    // Pass audio through unmodified (this is an analyser, not an effect)
    // If bypassed, still pass through but freeze output values.
    if (bypass->get())
        return;

    const float* inputData = buffer.getReadPointer (0);
    const int    numSamples = buffer.getNumSamples();

    // Process each sample through YIN.
    // YIN only fires a result when the internal buffer is full (every ~4096
    // samples at 44.1 kHz ≈ 93 ms update rate — fast enough for a tuner).
    float detectedFreq = -1.f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float result = yin.process (inputData[i], currentSampleRate);
        if (result > 0.f)
            detectedFreq = result;
    }

    if (detectedFreq > 0.f)
    {
        const float midiFloat = NoteUtil::freqToMidi (detectedFreq);
        const int   midiNote  = static_cast<int> (std::round (midiFloat));
        const float cents     = NoteUtil::centDeviation (detectedFreq);

        // Write to output control ports — Anagram UI reads these
        *outFrequency = detectedFreq;
        *outMidiNote  = static_cast<float> (std::clamp (midiNote, 0, 127));
        *outCents     = std::clamp (cents, -50.f, 50.f);
    }
    else
    {
        // No pitch detected — zero out outputs
        *outFrequency = 0.f;
        *outMidiNote  = 0.f;
        *outCents     = 0.f;
    }
}

//==============================================================================
// Plugin entry point (required by JUCE)
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassTunerProcessor();
}
