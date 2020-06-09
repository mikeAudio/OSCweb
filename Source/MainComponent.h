
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
    float            waveTable[waveTableSize];
    
    std::vector<float> phaseVector;
    juce::AudioBuffer<float> fillBuffer;
    
    float phase{};
    float increment{};
    float frequency{};
    float gain{};
    double currentSampleRate;
    
    int numOSC;
    int oldNumOSC;
    float webDensity;
    
    juce::Slider frequencySlider;
    juce::Label  frequencyLabel;
    juce::Slider highcutSlider;
    juce::Slider amplitudeSlider;
    juce::Slider oscSlider;
    juce::Slider webSlider;
    juce::TextButton algoButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
