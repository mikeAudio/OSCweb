
#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent   : public AudioAppComponent

{
public:
    //==============================================================================
    MainComponent();
    
    ~MainComponent();

    void updateFrequency(float f, int index);
    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (Graphics& g) override;
    void resized() override;

private:
 
    static int const waveTableSize = 1024;
    static int const maxNumOsc = 20000;
    float            waveTable[waveTableSize];
    
    std::vector<int> phaseVector;
    std::vector<float> gainVector;
    juce::AudioBuffer<float> fillBuffer;
    
    float phase{};
    float increment{};
    float frequency{};
    double currentSampleRate;

    juce::ADSR::Parameters parameters_;
    juce::ADSR envelope;
    int envelopeIndex;
    int oldNumOSC;
    
    juce::Slider frequencySlider;
    juce::Label  frequencyLabel;
    juce::Slider highcutSlider;
    juce::Slider amplitudeSlider;
    juce::Slider oscSlider;
    juce::Slider webSlider;
    juce::TextButton algoButton;
    juce::TextButton toggleButton;
    
    bool oldToggleState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
