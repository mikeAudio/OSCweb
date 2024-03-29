
#include "MainComponent.h"

#include <cstdint>

MainComponent::MainComponent()
    : udpThread([this]() {
        udp.bindToPort(portNumber, "0.0.0.0");

        // while(true){

        DBG("UDP Thread waiting for connection");

        auto status = udp.waitUntilReady(true, -1);

        if (status == 0) { DBG("Connection Time Out"); }
        if (status == -1) { DBG("Error connecting to UDP Port"); }

        if (status == 1)
        {
            while (true)
            {
                uint8_t buffer[5000] = {};
                auto const numBytes  = udp.read(static_cast<void*>(buffer), sizeof(buffer), false);

                if (numBytes > 0)
                {
                    MessageType type = MessageType::Unknown;
                    std::memcpy(&type, &buffer, sizeof(MessageType));

                    switch (type)
                    {
                        case MessageType::Performance:
                        {
                            auto msg = PerformanceMessage {};
                            std::memcpy(&msg.index, buffer + 1, sizeof(PerformanceMessage::index));
                            // DBG("UDP Thread:");
                            // DBG(msg.index);
                            queue.enqueue(msg.index);

                            break;
                        }

                        case MessageType::Initialisation:
                        {
                            systemIsInInitMode.store(true);
                            listOfFrequencies.clear();

                            std::memcpy(&numFrequenciesReceived, buffer + 1, 2);
                            std::memcpy(&chunkSize, buffer + 3, 2);

                            DBG("Initialisation Begin.\nNum Neurons:");
                            DBG(numFrequenciesReceived);
                            DBG("chunk Size:");
                            DBG(chunkSize);
                            break;
                        }

                        case MessageType::InitialisationContent:
                        {
                            auto msg = InitialisationContentMessage {};

                            for (int i = 1; i < chunkSize * 4; i = i + 4)
                            {
                                std::memcpy(&msg.frequency, buffer + i, 4);

                                if (msg.frequency == 0)
                                {
                                    DBG("Initialisation Succesfull - 0 reached");
                                    systemIsInInitMode.store(false);
                                    break;
                                }

                                listOfFrequencies.push_back(msg.frequency);
                                DBG(msg.frequency);
                            }

                            if (listOfFrequencies.size() == numFrequenciesReceived)
                            {
                                DBG("Initialisation Succesfull - vector filled");
                                systemIsInInitMode.store(false);
                                break;
                            }

                            if (listOfFrequencies.size() > numFrequenciesReceived)
                            {
                                DBG("Initialisation Overload");
                                break;
                            }
                            if (listOfFrequencies.size() < numFrequenciesReceived)
                            {
                                DBG("chunk done, waiting for next one");
                                DBG(listOfFrequencies.size());
                                break;
                            }
                        }

                        default: jassertfalse; break;
                    }
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

    attackSlider.setRange(0.5f, 2.f);
    addAndMakeVisible(attackSlider);

    decaySlider.setRange(0.999f, 0.99999f);
    addAndMakeVisible(decaySlider);

    noiseGainSlider.setRange(0.f, 1.f);
    addAndMakeVisible(noiseGainSlider);

    oscSlider.setRange(1, maxNumOsc);
    oscSlider.setSkewFactorFromMidPoint(2000);
    addAndMakeVisible(oscSlider);

    webSlider.setRange(1.0, 1.4);
    webSlider.setValue(1.1f);
    addAndMakeVisible(webSlider);

    algoButton.setButtonText("linear");
    algoButton.setClickingTogglesState(true);
    addAndMakeVisible(algoButton);

    udpModeButton.setButtonText("Activate UDP as Source");
    udpModeButton.setClickingTogglesState(true);
    addAndMakeVisible(udpModeButton);

    portNumberEditor.setMultiLine(false);
    portNumberEditor.setEscapeAndReturnKeysConsumed(true);
    portNumberEditor.setCaretVisible(true);
    addAndMakeVisible(portNumberEditor);
}

MainComponent::~MainComponent()
{
    udp.shutdown();

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
    bool udpMode        = udpModeButton.getToggleState();
    float masterGain    = amplitudeSlider.getValue();
    int numOSC          = udpMode ? numFrequenciesReceived : static_cast<int>(oscSlider.getValue());
    float webDensity    = webSlider.getValue();
    float highFrequency = highcutSlider.getValue();
    float subFrequency  = std::floor(frequencySlider.getValue());
    env.defaultGain     = noiseGainSlider.getValue();
    env.addGain         = attackSlider.getValue();
    env.decayFactor     = decaySlider.getValue();

    // UDP Receive
    spikingFrequencies.fill(-1);

    if (udpMode)
    {
        int udpReadIndex = 0;

        while (queue.peek() != nullptr && udpReadIndex < maxIndexToReadUdpMessage)
        {
            queue.try_dequeue(spikingFrequencies[udpReadIndex]);

            udpReadIndex++;
        }
    }
    else
    {
        int numCycles    = fmod(rand(), 10000);
        int rndmIndex    = -1;
        int indexCounter = 0;

        for (int i = 0; i < numCycles; i++)
        {
            rndmIndex = fmod(rand(), 20000);

            if (rndmIndex < numOSC)
            {
                spikingFrequencies[indexCounter] = rndmIndex;

                indexCounter++;
            }
        }
    }

    for (auto& f : spikingFrequencies)
    {
        if (f == -1) { break; }
        env.trigger(f);
    }

    // If the number of oscillators changed, delete old phases & envelope gains
    bool numOscChanged = oldNumOsc != numOSC ? true : false;
    if (numOscChanged)
    {
        phaseVector.clear();
        env.reset();
    }

    // oscillation:
    for (int i = 0; i < numOSC; i++)
    {
        // Generate new phases, when NumOSC-Slider was moved
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
                env.tick(i);

                for (int channel = 0; channel < 2; channel++)
                { buffer->addSample(channel, sample, waveTable[phaseVector[i]] * env.getGain(i)); }

                if (udpMode) { updateFrequency(listOfFrequencies[i], i); }
                else
                {
                    updateFrequency(subFrequency, i);
                }
            }
        }
        if (!udpMode)
        {
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
    }

    oldNumOsc = numOSC;
    buffer->applyGain(masterGain * 0.5f);
}

void MainComponent::releaseResources() { }

void MainComponent::paint(Graphics& g) { g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId)); }

void MainComponent::resized()
{
    auto const area = getLocalBounds();

    auto const heightForth = area.getHeight() / 5;
    auto const halfWidth   = area.getWidth() / 2;

    amplitudeSlider.setBounds(0, 0, halfWidth, heightForth);
    frequencySlider.setBounds(0, heightForth, halfWidth, heightForth);
    highcutSlider.setBounds(0, heightForth * 2, halfWidth, heightForth);
    oscSlider.setBounds(0, heightForth * 3, halfWidth, heightForth);
    webSlider.setBounds(0, heightForth * 4, halfWidth, heightForth / 2);

    noiseGainSlider.setBounds(halfWidth, 0, halfWidth, heightForth);
    attackSlider.setBounds(halfWidth, heightForth, halfWidth, heightForth);
    decaySlider.setBounds(halfWidth, heightForth * 2, halfWidth, heightForth);

    portNumberEditor.setBounds(0, heightForth * 4 + heightForth / 2, halfWidth, heightForth / 2);
    algoButton.setBounds(halfWidth, heightForth * 4, halfWidth, heightForth / 2);
    udpModeButton.setBounds(halfWidth, heightForth * 4 + heightForth / 2, halfWidth, heightForth / 2);
}
