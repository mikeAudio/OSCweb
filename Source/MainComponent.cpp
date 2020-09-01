
#include "MainComponent.h"

#include <cstdint>
namespace mc
{
namespace net
{
template<class T>
constexpr auto ntoh(T) -> T = delete;
constexpr auto ntoh(char v) noexcept -> char { return v; }
constexpr auto ntoh(uint8_t v) noexcept -> uint8_t { return v; }
constexpr auto ntoh(int8_t v) noexcept -> int8_t { return v; }
constexpr auto ntoh(uint16_t v) noexcept -> uint16_t
{
    return uint16_t(v << uint16_t {8}) | uint16_t(v >> uint16_t {8});
}
constexpr auto ntoh(uint32_t v) noexcept -> uint32_t
{
    auto const a = v << 24;
    auto const b = (v & 0x0000FF00) << 8;
    auto const c = (v & 0x00FF0000) >> 8;
    auto const d = v >> 24;
    return a | b | c | d;
}
template<class T>
constexpr auto hton(T) -> T = delete;
constexpr auto hton(char v) noexcept -> char { return v; }
constexpr auto hton(int8_t v) noexcept -> int8_t { return v; }
constexpr auto hton(uint8_t v) noexcept -> uint8_t { return v; }
constexpr auto hton(uint16_t v) noexcept -> uint16_t { return ntoh(v); }
constexpr auto hton(uint32_t v) noexcept -> uint32_t { return ntoh(v); }
}  // namespace net
}  // namespace mc

MainComponent::MainComponent()
    : udpThread([this]() {
        udp.bindToPort(portNumber, "0.0.0.0");

        auto status = udp.waitUntilReady(true, 30000);

        if (status <= 0) { DBG("Error connecting to UDP Port"); }

        if (status == 1)
        {
            int initCyclesCounter = 0;

            while (true)
            {
                uint8_t buffer[8]   = {};
                auto const numBytes = udp.read(static_cast<void*>(buffer), sizeof(buffer), false);

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
                            DBG("UDP Thread:");
                            DBG(msg.index);
                            queue.enqueue(msg.index);

                            break;
                        }

                        case MessageType::Initialisation:
                        {
                            if (initCyclesCounter == 0)
                            {
                                systemIsInInitMode.store(true);
                                listOfFrequencies.fill(0.f);
                            }
                            /*
                                                        if (initCyclesCounter + 2 > numFrequenciesReceived)  // 2
                               Toleranz
                                                        {
                                                            DBG("Initialisation Failed - not enough frequencies
                               received"); initCyclesCounter = 0; break;
                                                        }
                            */
                            auto msg = InitialisationMessage {};
                            std::memcpy(&msg.index, buffer + 1, sizeof(InitialisationMessage::index));
                            std::memcpy(&msg.frequency, buffer + 3, sizeof(InitialisationMessage::frequency));

                            DBG(msg.index);
                            DBG(msg.frequency);

                            if (msg.frequency == 0) { numFrequenciesReceived = msg.index; }
                            else
                            {
                                listOfFrequencies[msg.index] = msg.frequency;
                            }

                            initCyclesCounter++;

                            if (initCyclesCounter == numFrequenciesReceived + 1)  // 1 Offset for Initmessage
                            {
                                systemIsInInitMode = false;
                                DBG("Initialisation Done");
                                initCyclesCounter = 0;
                            }

                            break;
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

    attackSlider.setRange(1.0000001f, 1.01f);
    addAndMakeVisible(attackSlider);

    decaySlider.setRange(0.999f, 0.99999f);
    addAndMakeVisible(decaySlider);

    noiseGainSlider.setRange(0.f, 1.f);
    addAndMakeVisible(noiseGainSlider);

    oscSlider.setRange(1.0, maxNumOsc);
    oscSlider.setSkewFactorFromMidPoint(2000);
    addAndMakeVisible(oscSlider);

    webSlider.setRange(1.0, 1.4);
    webSlider.setValue(1.1f);
    addAndMakeVisible(webSlider);

    algoButton.setButtonText("linear");
    algoButton.setClickingTogglesState(true);
    addAndMakeVisible(algoButton);

    triggerFreqButton.setButtonText("Activate UDP");
    triggerFreqButton.setClickingTogglesState(true);
    addAndMakeVisible(triggerFreqButton);

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
    bool udpMode        = triggerFreqButton.getToggleState();
    float masterGain    = amplitudeSlider.getValue();
    int numOSC          = udpMode ? numFrequenciesReceived : static_cast<int>(oscSlider.getValue());
    float webDensity    = webSlider.getValue();
    float highFrequency = highcutSlider.getValue();
    float subFrequency  = std::floor(frequencySlider.getValue());
    env.defaultGain     = noiseGainSlider.getValue();
    env.attackFactor    = attackSlider.getValue();
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

        int numCycles = fmod(rand(), maxIndexToReadUdpMessage);

        for (int i = 0; i < numCycles; i++) { spikingFrequencies[i] = fmod(rand(), 20000); }

        for (auto& f : spikingFrequencies)
        {
            if (f == -1) { break; }
            env.trigger(f);
            // DBG("Audio Thread:");
            // DBG(f);
        }
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

void MainComponent::releaseResources() {}

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
    triggerFreqButton.setBounds(halfWidth, heightForth * 4 + heightForth / 2, halfWidth, heightForth / 2);
}
