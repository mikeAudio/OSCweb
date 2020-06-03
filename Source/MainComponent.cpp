
#include "MainComponent.h"


MainComponent::MainComponent()
{

    setSize(800, 600);

    
    if (RuntimePermissions::isRequired (RuntimePermissions::recordAudio)
        && ! RuntimePermissions::isGranted (RuntimePermissions::recordAudio))
    {
        RuntimePermissions::request (RuntimePermissions::recordAudio,
                                     [&] (bool granted) { if (granted)  setAudioChannels (2, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }
    
    frequencySlider.setRange(20.0, 20000.0);
    frequencySlider.setValue(440.0);
    frequencySlider.setSkewFactorFromMidPoint(2000.0);
    addAndMakeVisible(frequencySlider);
    
    amplitudeSlider.setRange(0.0, 4.0);
    addAndMakeVisible(amplitudeSlider);
    
    oscSlider.setRange(1.0, 500.0);
    addAndMakeVisible(oscSlider);
    
    webSlider.setRange(1.0, 2.0);
    addAndMakeVisible(webSlider);
    
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::updateFrequency(float f, int index)
{
    frequency = f;
    increment = frequency * waveTableSize / currentSampleRate;
    phaseVector[index] = fmod((phase + increment), waveTableSize);
}


void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    frequency = 20;
    phase = 0;
    increment = frequency * waveTableSize / sampleRate;
    currentSampleRate = sampleRate;
    
    
    for(int i = 0; i < waveTableSize; i++){waveTable[i] = sin(2.f * double_Pi * i / waveTableSize);}
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    
    
    gain = amplitudeSlider.getValue();
    numOSC = static_cast<int>(oscSlider.getValue());
    webDensity = webSlider.getValue();
    
    bool numOscChanged = oldNumOSC == numOSC ? true : false;
    
    float subFrequency = 20;
    
    auto const buffer = bufferToFill.buffer;
    
    buffer->clear();
    if(numOscChanged){phaseVector.clear();}
    
    for(int i = 0; i < numOSC; i++){
        
        
        float freqStep   = 19980.f / static_cast<float>(numOSC);
        float freqFactor = 19980.f / (19980.f - freqStep);
        
        if (numOscChanged) {
            phaseVector.push_back(0.f);
        }
        
        if(i == 0)
        {
            subFrequency = 20.f;
        }
        else
        {
            subFrequency *= freqFactor;
        }

        updateFrequency(subFrequency, i);
        
        for(int sample = 0; sample < buffer->getNumSamples(); sample++)
        {
            
            
            for(int channel = 0; channel < 2; channel++)
            {
                buffer->addSample(channel, sample, waveTable[static_cast<int>(phaseVector[i])] * gain);
            }
        }
    }
    
    oldNumOSC = numOSC;
    
}

void MainComponent::releaseResources()
{

}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto const area = getLocalBounds();
    
    auto const heightForth = area.getHeight() / 4;
    
    amplitudeSlider.setBounds(0, 0, getWidth(), heightForth);
    frequencySlider.setBounds(0, heightForth, getWidth(), heightForth);
    oscSlider.setBounds(0, heightForth * 2, getWidth(), heightForth);
    webSlider.setBounds(0, heightForth * 3, getWidth(), heightForth);
    
    
    
    
    
    
}
