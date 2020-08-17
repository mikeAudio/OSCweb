
#include "MainComponent.h"

MainComponent::MainComponent()
    : udpThread([this]() {
        udp.bindToPort(portNumber);

        if (udp.waitUntilReady(true, 1000) == 1)
        {
            while (true)
            {
                // auto buffer = std::array<std::uint8_t, 1024>{};
                uint8_t buffer[1024] = {};
                uint8_t* position    = nullptr;

                auto numBytes = udp.read(static_cast<void*>(buffer), sizeof(buffer), false);
                position      = buffer;

                jassert(numBytes > 0);

                while (numBytes > 0)
                {
                    auto data = *reinterpret_cast<const uint16_t*>(position);
                    position++;
                    position++;
                    numBytes -= 2;

                    queue.emplace(data);
                }
            }
        }
    })
{
    setSize(800, 600);

    if (RuntimePermissions::isRequired(RuntimePermissions::recordAudio)
        && !RuntimePermissions::isGranted(RuntimePermissions::recordAudio))
    {
        RuntimePermissions::request(RuntimePermissions::recordAudio, [&](bool granted) {
            if (granted) setAudioChannels(2, 2);
        });
    }
    else
    {
        setAudioChannels(2, 2);
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

    oscSlider.setRange(1.0, maxNumOsc);
    oscSlider.setSkewFactorFromMidPoint(2000);
    addAndMakeVisible(oscSlider);

    webSlider.setRange(1.0, 1.4);
    addAndMakeVisible(webSlider);

    algoButton.setButtonText("linear");
    algoButton.setClickingTogglesState(true);
    addAndMakeVisible(algoButton);

    toggleButton.setButtonText("Toggle Random Frequency");
    toggleButton.setClickingTogglesState(false);
    addAndMakeVisible(toggleButton);

    portNumberEditor.setMultiLine(false);
    portNumberEditor.setEscapeAndReturnKeysConsumed(true);
    portNumberEditor.setCaretVisible(true);
    addAndMakeVisible(portNumberEditor);
}

MainComponent::~MainComponent()
{
    if (udpThread.joinable()) { udpThread.join(); }

    shutdownAudio();
}

void MainComponent::updateFrequency(float f, int index)
{
    frequency          = f;
    increment          = frequency * waveTableSize / currentSampleRate;
    phaseVector[index] = fmod((phaseVector[index] + increment), waveTableSize);
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    phaseVector.reserve(maxNumOsc);
    gainVector.reserve(1000);

    phaseVector[0]    = 0;
    frequency         = 440;
    increment         = frequency * waveTableSize / sampleRate;
    currentSampleRate = sampleRate;

    fillBuffer.setSize(2, samplesPerBlockExpected);

    for (int i = 0; i < waveTableSize; i++) { waveTable[i] = sin(2.f * double_Pi * i / waveTableSize); }
}

void MainComponent::getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill)
{
    // prepare buffer
    auto const buffer = bufferToFill.buffer;
    buffer->clear();

    // get Slider values
    float masterGain    = amplitudeSlider.getValue();
    int numOSC          = static_cast<int>(oscSlider.getValue());
    float webDensity    = webSlider.getValue();
    float highFrequency = highcutSlider.getValue();
    float subFrequency  = std::floor(frequencySlider.getValue());
    bool toggleState    = toggleButton.isDown();

    
    //UDP Receive
    activeFrequencies.fill(0);
    
    auto data = -1;
    int udpReadIndex = 0;
    
    while(queue.peek() != nullptr && udpReadIndex < maxIndexToReadUdpMessage)
    {
        queue.try_dequeue(activeFrequencies[udpReadIndex]);
        udpReadIndex++;
    }
    

    //If the number of oscillators changed, delete old phases
    bool numOscChanged = oldNumOsc != numOSC ? true : false;
    if (numOscChanged) { phaseVector.clear(); }



    // oscillation:
    for (int i = 0; i < numOSC; i++)
    {
        // Generate new phases, when NumOSC-Slider is moved
        if (numOscChanged)
        {
            int rndPhaseNum = fmod(rand(), waveTableSize);
            phaseVector.push_back(rndPhaseNum);
        }

        // Oscillator, that fills the whole buffer at once
        if (subFrequency < highFrequency)
        {
            for (int sample = 0; sample < buffer->getNumSamples(); sample++)
            {
                float gain = 1.f;

                for (int channel = 0; channel < 2; channel++)
                { buffer->addSample(channel, sample, waveTable[phaseVector[i]] * gain); }

                updateFrequency(subFrequency, i);
            }
        }

        // Calculating next frequency
        if (algoButton.getToggleState())
        {
            float add = (webDensity - 1.f) * 50.f;
            subFrequency += add;
        }
        else
        {
            float freqStep   = 19980.f / static_cast<float>(numOSC);
            float freqFactor = 19980.f / (19980.f - freqStep) * webDensity;
            subFrequency *= freqFactor;
        }
    }

    oldNumOsc = numOSC;
    buffer->applyGain(masterGain);
}

void MainComponent::releaseResources() {}

void MainComponent::paint(Graphics& g) { g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId)); }

void MainComponent::resized()
{
    auto const area = getLocalBounds();

    auto const heightForth = area.getHeight() / 5;
    auto const leftBorder  = area.getWidth() / 10;

    amplitudeSlider.setBounds(leftBorder, 0, getWidth(), heightForth);
    frequencySlider.setBounds(leftBorder, heightForth, getWidth() - leftBorder, heightForth);
    highcutSlider.setBounds(leftBorder, heightForth * 2, getWidth() - leftBorder, heightForth);
    oscSlider.setBounds(leftBorder, heightForth * 3, getWidth() - leftBorder, heightForth);
    webSlider.setBounds(0, heightForth * 4, getWidth() / 2, heightForth / 2);
    portNumberEditor.setBounds(0, heightForth * 4 + heightForth / 2, getWidth() / 2, heightForth / 2);
    algoButton.setBounds(getWidth() / 2, heightForth * 4, getWidth() / 2, heightForth / 2);
    toggleButton.setBounds(getWidth() / 2, heightForth / 2 * 9, getWidth() / 2, heightForth / 2);
}
