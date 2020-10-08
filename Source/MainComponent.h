
#pragma once

#include "readerwriterqueue.h"
#include <JuceHeader.h>
#include <cstring>
#include <thread>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
enum class MessageType : uint8_t
{
    Performance = 0,
    Initialisation,
    InitialisationContent,
    Unknown,
};
struct PerformanceMessage
{
    MessageType type;
    uint16_t index;
};
static_assert(sizeof(PerformanceMessage) == 4, "");

struct InitialisationMessage
{
    MessageType type;
    uint16_t numFrequencies;
    uint16_t chunkSize;
};

struct InitialisationContentMessage
{
    MessageType type;
    float frequency;
};

class ExponentialDecay
{
public:
    ExponentialDecay() { reset(); }

    void reset() { gains.fill(defaultGain); }

    void trigger(int index)
    {
        gains[index] += addGain;

        if (gains[index] > gainLimit)
        {
            gains[index] = gainLimit;
            DBG("Neuron peaked");
        }
    }

    void tick(int index)
    {
        float g = gains[index];

        if (g > defaultGain + 0.01f)  //
        { g *= decayFactor; }
        if (g < defaultGain + 0.01f && g > defaultGain)  //
        { g = defaultGain; }

        gains[index] = g;
    }

    float getGain(int index) { return gains[index]; }

    float defaultGain {0.f};
    float addGain {1.3f};
    float decayFactor {0.99996f};

private:
    float gainLimit {12.f};
    std::array<float, 20000> gains {};
};

class MainComponent : public AudioAppComponent

{
public:
    //==============================================================================
    MainComponent();

    ~MainComponent();

    void updateFrequency(float f, int index);
    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint(Graphics& g) override;
    void resized() override;

private:
    static int const waveTableSize = 1024;
    static int const maxNumOsc     = 20000;
    float waveTable[waveTableSize];

    std::vector<int> phaseVector;
    std::vector<float> gainVector;
    juce::AudioBuffer<float> fillBuffer;

    float phase {};
    float increment {};
    float frequency {};
    double currentSampleRate;

    int oldNumOsc {};

    ExponentialDecay env {};

    std::array<float, 10000> envelopeValues {};
    std::vector<float> listOfFrequencies {20000};

    juce::Slider frequencySlider;
    juce::Label frequencyLabel;
    juce::Slider highcutSlider;
    juce::Slider amplitudeSlider;
    juce::Slider oscSlider;
    juce::Slider webSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider noiseGainSlider;
    juce::TextButton algoButton;
    juce::TextButton udpModeButton;
    juce::TextEditor portNumberEditor;

    bool oldToggleState = false;

    moodycamel::ReaderWriterQueue<int> queue {200};
    const int maxIndexToReadUdpMessage = 1;

    juce::DatagramSocket udp {};
    constexpr static int portNumber = 4001;
    std::atomic<bool> doneFlag {false};

    std::thread udpThread;
    std::atomic<bool> systemIsInInitMode {};
    uint16_t numFrequenciesReceived;
    uint16_t chunkSize;

    std::array<int, 10000> spikingFrequencies {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
