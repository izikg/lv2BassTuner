#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "juce_anagram.h"

//==============================================================================
// YIN pitch detector
// Optimised for bass guitar: detection range 30 Hz – 350 Hz
// Based on: de Cheveigné & Kawahara (2002), "YIN, a fundamental frequency
// estimator for speech and music", JASA 111(4).
//==============================================================================
class YinDetector
{
public:
    // bufferSize should be at least 2 * (sampleRate / minFreq)
    // For 44100 Hz and 30 Hz minimum: 2 * 1470 = 2940 samples — we use 4096.
    static constexpr int kBufferSize = 4096;
    static constexpr float kThreshold = 0.15f;   // confidence threshold
    static constexpr float kMinFreq   = 30.f;    // Hz — below low B string
    static constexpr float kMaxFreq   = 350.f;   // Hz — above open G string

    YinDetector();

    // Feed one sample at a time. Returns detected frequency in Hz,
    // or -1.f if no confident pitch was found this frame.
    float process (float sample, double sampleRate);

    // Reset internal state (call on prepareToPlay)
    void reset();

private:
    float buffer[kBufferSize];
    int   writePos { 0 };
    int   filled   { 0 };

    // YIN difference function
    void   computeDifference (float* d, int tauMax) const;
    // Cumulative mean normalised difference
    void   normaliseCMND (float* d, int tauMax) const;
    // Find first tau below threshold (absolute minimum)
    int    findPitch (const float* d, int tauMin, int tauMax) const;
    // Parabolic interpolation for sub-sample accuracy
    float  parabolicInterp (const float* d, int tau) const;
};

//==============================================================================
// Note utilities
//==============================================================================
namespace NoteUtil
{
    // Convert frequency to MIDI note number (float, for cents calculation)
    float  freqToMidi (float freq);

    // MIDI note number → note name with octave, e.g. "E2", "A2"
    // Uses standard equal temperament, A4 = 440 Hz
    juce::String midiToName (int midiNote);

    // Cents deviation from nearest semitone (-50..+50)
    float  centDeviation (float freq);
}

//==============================================================================
// AudioProcessor
//==============================================================================
class BassTunerProcessor : public juce::AudioProcessor
{
public:
    BassTunerProcessor();
    ~BassTunerProcessor() override;

    //==========================================================================
    // AudioProcessor overrides

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    // Anagram / LV2 requirements

    const juce::String   getName()               const override { return JucePlugin_Name; }
    juce::StringArray    getAlternateDisplayNames() const override { return { "BTN" }; }

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    bool acceptsMidi()     const override { return false; }
    bool producesMidi()    const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // No GUI on Anagram
    bool hasEditor()              const override { return false; }
    juce::AudioProcessorEditor* createEditor()  override { return nullptr; }

    // Programs unused on Anagram
    int  getNumPrograms()                          override { return 0; }
    int  getCurrentProgram()                       override { return 0; }
    void setCurrentProgram (int)                   override {}
    const juce::String getProgramName (int)        override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // State unused on Anagram (host manages it)
    void getStateInformation (juce::MemoryBlock&)       override {}
    void setStateInformation (const void*, int)         override {}

private:
    //==========================================================================
    // Parameters exposed as LV2 control ports

    // Input
    juce::AudioParameterBool* bypass;

    // Output ports — read by Anagram UI / automation
    juce::AudioParameterFloat* outMidiNote;   // 0–127, nearest semitone
    juce::AudioParameterFloat* outCents;      // -50..+50 cents deviation
    juce::AudioParameterFloat* outFrequency;  // raw Hz (0 = no signal)

    //==========================================================================
    // DSP state
    YinDetector  yin;
    double       currentSampleRate { 44100.0 };

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassTunerProcessor)
};
