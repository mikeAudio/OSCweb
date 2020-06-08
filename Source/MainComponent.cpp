
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
    
    frequencyLabel.attachToComponent(&frequencySlider, true);
    addAndMakeVisible(frequencyLabel);
    
    highcutSlider.setRange(20.0, 20000.0);
    highcutSlider.setValue(20000.0);
    highcutSlider.setSkewFactorFromMidPoint(2000.0);
    addAndMakeVisible(highcutSlider);
    
    
    amplitudeSlider.setRange(0.0, 1.0);
    addAndMakeVisible(amplitudeSlider);
    
    oscSlider.setRange(1.0, 20000.0);
    oscSlider.setSkewFactorFromMidPoint(2000);
    addAndMakeVisible(oscSlider);
    
    webSlider.setRange(1.0, 1.4);
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
    phaseVector[index] = fmod((phaseVector[index] + increment), waveTableSize);
}


void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    phaseVector.reserve(oscSlider.getMaximum());
    
    phaseVector[0] = 0;
    frequency = 440;
    increment = frequency * waveTableSize / sampleRate;
    currentSampleRate = sampleRate;
    
    
    
    fillBuffer.setSize(2, samplesPerBlockExpected);
    
    for(int i = 0; i < waveTableSize; i++){waveTable[i] = sin(2.f * double_Pi * i / waveTableSize);}
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    auto const buffer = bufferToFill.buffer;
    buffer->clear();
    
    gain = amplitudeSlider.getValue();
    numOSC = static_cast<int>(oscSlider.getValue());
    webDensity = webSlider.getValue();
    float subFrequency = std::floor(frequencySlider.getValue());
    
    bool numOscChanged = oldNumOSC != numOSC ? true : false;
    
    
    if(numOscChanged){phaseVector.clear();}
    
   
    for(int i = 0; i < numOSC; i++)
    {
        if (numOscChanged)
        {
            float rndNum = fmod(rand(), waveTableSize);
            phaseVector.push_back(rndNum);
        }
        

        if(subFrequency < highcutSlider.getValue())
        {
            for(int sample = 0; sample < buffer->getNumSamples(); sample++)
            {
                for(int channel = 0; channel < 2; channel++)
                {
                    buffer->addSample(channel, sample, waveTable[static_cast<int>(phaseVector[i])] * gain);
                }
                
                updateFrequency(subFrequency, i);
            }
        }
     
        float freqStep   = 19980.f / static_cast<float>(numOSC);
        float freqFactor = 19980.f / (19980.f - freqStep) * webDensity;
        subFrequency *= freqFactor;
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
    
    auto const heightForth = area.getHeight() / 5;
    auto const leftBorder  = area.getWidth() / 10;
    
    amplitudeSlider.setBounds(leftBorder, 0, getWidth(), heightForth);
    frequencySlider.setBounds(leftBorder, heightForth, getWidth() - leftBorder, heightForth);
    highcutSlider.setBounds(leftBorder, heightForth * 2, getWidth() - leftBorder, heightForth);
    oscSlider.setBounds(leftBorder, heightForth * 3, getWidth() - leftBorder, heightForth);
    webSlider.setBounds(leftBorder, heightForth * 4, getWidth() - leftBorder, heightForth);
    
    
    
    
    
    
}
