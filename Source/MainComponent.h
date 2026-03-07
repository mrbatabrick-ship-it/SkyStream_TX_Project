#pragma once
#include <JuceHeader.h>
#include <atomic>

// Fix for Socket Constants & MicroSleep
#if JUCE_WINDOWS
 #include <winsock2.h>
 #include <ws2tcpip.h>
 #include <windows.h>
#else
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include <time.h>
#endif

// --- HELPER: MICRO SLEEP (HIGH PRECISION) ---
// Digunakan untuk memberi jeda antar paket segmen (pacing)
static inline void microSleep(int microseconds)
{
#if JUCE_WINDOWS
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    do {
        QueryPerformanceCounter(&now);
    } while ((now.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart < microseconds);
#else
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = microseconds * 1000;
    nanosleep(&ts, nullptr);
#endif
}

// --- CRC32 HELPER ---
class CRC32 {
public:
    static uint32_t calculate(const void* data, size_t length) {
        static uint32_t table[256];
        static bool tableComputed = false;
        if (!tableComputed) {
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int j = 0; j < 8; j++) {
                    if (c & 1) c = 0xedb88320L ^ (c >> 1);
                    else c = c >> 1;
                }
                table[i] = c;
            }
            tableComputed = true;
        }
        uint32_t crc = 0xffffffffL;
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < length; i++) {
            crc = table[(crc ^ bytes[i]) & 0xff] ^ (crc >> 8);
        }
        return crc ^ 0xffffffffL;
    }
};

// --- ARCHITECTURE ---
#pragma pack(push, 1)
struct MultiChannelHeader {
    uint32_t sequenceID;
    uint16_t segmentIndex;
    uint16_t totalSegments;
    uint64_t audioTimestamp;
    float sampleRate;
    uint16_t numChannels;
    uint16_t numSamples;
    uint16_t channelStartIndex;
    uint16_t channelCount;
    uint16_t hasMetadata;
    uint32_t crc32;
};

struct ChannelMetadata {
    char names[64][20];
};

struct HealthPacket {
    uint32_t magic = 0xDEADBEEF;
    uint32_t frameCounter;
    uint16_t channelCount;
    uint16_t mode;
    float cpuLoad;
    float networkMbps;
    uint32_t networkBytesPerSec;
    uint32_t droppedPackets;
    float avgJitterMs;
};
#pragma pack(pop)

enum class NetworkMode { Unicast = 0, MulticastSingle, MulticastLayered };

struct AudioTrack {
    int id;
    juce::String name;
    std::atomic<float> currentLevelDb { -100.0f };
    std::atomic<int> routedInputIndex { -1 };
    std::atomic<bool> sendEnabled { true };
    std::atomic<bool> soloEnabled { false };
    float smoothGain = 0.0f;
    juce::AbstractFifo fifo { 131072 }; // Ukuran diperbesar untuk stabilitas
    juce::AudioBuffer<float> fifoBuffer { 1, 131072 };

    AudioTrack(int trackId, const juce::String& trackName) : id(trackId), name(trackName) {
        fifoBuffer.clear();
    }
};

// --- UI: LOOK AND FEEL ---
class CustomLNF : public juce::LookAndFeel_V4 {
public:
    CustomLNF() {
        setDefaultSansSerifTypefaceName ("Courier New");
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.2f));
        setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
        setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.withAlpha(0.3f));
    }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider& slider) override {
        auto trackWidth = 3.0f;
        auto slotBounds = slider.getLocalBounds().withSizeKeepingCentre (trackWidth, height - 30);
        g.setColour (juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle (slotBounds.toFloat(), 1.0f);
        auto knobWidth = (float)width * 0.6f;
        auto knobHeight = 15.0f;
        juce::Rectangle<float> knob ((float)slider.getLocalBounds().getCentreX() - (knobWidth * 0.5f),
                                     sliderPos - (knobHeight * 0.5f), knobWidth, knobHeight);
        juce::ColourGradient grad (juce::Colours::lightgrey, knob.getX(), knob.getY(),
                                   juce::Colours::darkgrey, knob.getX(), knob.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (knob, 2.0f);
        g.setColour (juce::Colours::black);
        g.drawLine (knob.getX() + 3, knob.getCentreY(), knob.getRight() - 3, knob.getCentreY(), 1.0f);
    }
};

class MixerContainer : public juce::Component {
public:
    MixerContainer (juce::OwnedArray<AudioTrack>& t, juce::OwnedArray<juce::Slider>& f)
    : tracks(t), faders(f) {}
    void paint (juce::Graphics& g) override {
        int spacing = 125;
        for (int i = 0; i < tracks.size(); ++i) {
            int x = (i * spacing) + 5;
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle ((float)x + 2, 2.0f, (float)spacing - 10, (float)getHeight() - 4, 6.0f);
            if (i < faders.size()) {
                auto fBounds = faders[i]->getBounds();
                int meterW = 6;
                int meterX = fBounds.getCentreX() - (meterW / 2);
                int meterH = fBounds.getHeight() - 45;
                int meterY = fBounds.getY();
                g.setColour (juce::Colours::black);
                g.fillRect (meterX, meterY, meterW, meterH);
                float db = tracks[i]->currentLevelDb.load();
                float visualLevel = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 6.0f, 0.0f, 1.0f));
                int fillH = (int)(visualLevel * (float)meterH);
                if (fillH > 0) {
                    juce::Colour meterCol = (db > 0.0f) ? juce::Colours::red :
                                            (db > -10.0f) ? juce::Colours::yellow : juce::Colours::cyan;
                    g.setColour (meterCol);
                    g.fillRect (meterX, (meterY + meterH) - fillH, meterW, fillH);
                }
            }
        }
    }
private:
    juce::OwnedArray<AudioTrack>& tracks;
    juce::OwnedArray<juce::Slider>& faders;
};

class MainComponent : public juce::AudioAppComponent, public juce::Timer, public juce::Thread
{
public:
    MainComponent() : juce::Thread ("NetworkSenderThread"),
                      deviceSelector (deviceManager, 0, 64, 2, 64, false, false, true, false) {
        networkBuffer.ensureSize (16384);
        frameBuffer.setSize(64, 512, false, false, true);
        setLookAndFeel(&customLNF);
        addAndMakeVisible (headerArea);
        headerArea.addAndMakeVisible (titleLabel);
        titleLabel.setText ("SkyStream (TX)", juce::dontSendNotification);
        titleLabel.setFont (juce::Font(20.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
        headerArea.addAndMakeVisible (ioButton);
        ioButton.setButtonText("I/O");
        ioButton.onClick = [this] { showDeviceSettings(); };
        headerArea.addAndMakeVisible (modeSelector);
        modeSelector.addItem("UNICAST", 1);
        modeSelector.addItem("MULTICAST SINGLE", 2);
        modeSelector.addItem("MULTICAST LAYERED", 3);
        modeSelector.setSelectedId(1);
        modeSelector.onChange = [this] {
            auto id = modeSelector.getSelectedId();
            if (id == 1) currentMode.store(NetworkMode::Unicast);
            else if (id == 2) currentMode.store(NetworkMode::MulticastSingle);
            else currentMode.store(NetworkMode::MulticastLayered);
            targetIpLabel.setEnabled(id == 1);
            targetIpLabel.setAlpha(id == 1 ? 1.0f : 0.4f);
        };
        headerArea.addAndMakeVisible (targetIpLabel);
        targetIpLabel.setText ("192.168.1.100", juce::dontSendNotification);
        targetIpLabel.setEditable(true);
        targetIpLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.3f));
        cachedTargetIp = targetIpLabel.getText();
        targetIpLabel.onTextChange = [this] { cachedTargetIp = targetIpLabel.getText(); };
        headerArea.addAndMakeVisible (addTrackButton);
        addTrackButton.setButtonText("+ TRACK");
        addTrackButton.onClick = [this] { addTrack(); };
        addAndMakeVisible (viewport);
        mixerContainer = std::make_unique<MixerContainer> (tracks, faders);
        viewport.setViewedComponent (mixerContainer.get(), false);
        setSize (1000, 700);
        setAudioChannels (64, 2);
        for (int i = 0; i < 8; ++i) addTrack();
        udpSocket.bindToPort(0);
        configureSocket();
        startThread (juce::Thread::Priority::highest); // Prioritas tertinggi untuk pengiriman audio
        startTimerHz (30);
    }

    ~MainComponent() override {
        stopThread(2000);
        shutdownAudio();
        setLookAndFeel(nullptr);
    }

    void configureSocket() {
        udpSocket.setEnablePortReuse(true);
        int handle = udpSocket.getRawSocketHandle();
        if (handle >= 0) {
            int sendBufSize = 64 * 1024;
            setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (const char*)&sendBufSize, sizeof(sendBufSize));
            int tos = 0xB8; // DSCP EF (Expedited Forwarding) untuk Low Latency
            setsockopt(handle, IPPROTO_IP, IP_TOS, (const char*)&tos, sizeof(tos));
            int ttl = 5;
            setsockopt(handle, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
        }
    }

    // ==============================================================================
    // NETWORK ENGINE - TRANSMITTER
    // ==============================================================================
    void run() override {
        const int packetSamples = 128;
        uint64_t bytesSentSinceLastHealth = 0;
        juce::Array<AudioTrack*> validTracks;
        uint64_t lastLoopTs = juce::Time::getHighResolutionTicks();

        while (!threadShouldExit())
        {
            dataReadyEvent.wait(1); // Tunggu max 1ms, lalu lanjut

            validTracks.clear();

            {
                const juce::ScopedLock sl(trackLock);
                for (auto* t : tracks)
                {
                    if (t->sendEnabled.load() && t->fifo.getNumReady() >= packetSamples)
                        validTracks.add(t);
                }
            }

            if (validTracks.isEmpty())
                continue;

            int totalActive = juce::jmin(validTracks.size(), 64);
            uint64_t highResTs = juce::Time::getHighResolutionTicks();
            float jitter = (float)juce::Time::highResolutionTicksToSeconds(highResTs - lastLoopTs) * 1000.0f;
            lastLoopTs = highResTs;

            // Persiapkan Buffer pengiriman
            frameBuffer.setSize(totalActive, packetSamples, false, false, true);
            for (int ch = 0; ch < totalActive; ++ch) {
                int s1, z1, s2, z2;
                validTracks[ch]->fifo.prepareToRead(packetSamples, s1, z1, s2, z2);
                if (z1 > 0) juce::FloatVectorOperations::copy(frameBuffer.getWritePointer(ch), validTracks[ch]->fifoBuffer.getReadPointer(0, s1), z1);
                if (z2 > 0) juce::FloatVectorOperations::copy(frameBuffer.getWritePointer(ch) + z1, validTracks[ch]->fifoBuffer.getReadPointer(0, s2), z2);
                validTracks[ch]->fifo.finishedRead(packetSamples);
            }

            int chPerPacket = 4;
            int totalSegments = (totalActive + chPerPacket - 1) / chPerPacket;
            uint32_t currentFrameID = packetSequenceCounter++;

            for (int segIdx = 0; segIdx < totalSegments; ++segIdx) {
                int startChIdx = segIdx * chPerPacket;
                int chCount = juce::jmin(chPerPacket, totalActive - startChIdx);
                size_t offset = 0;
                
                auto* h = reinterpret_cast<MultiChannelHeader*>((char*)networkBuffer.getData());
                h->sequenceID = currentFrameID;
                h->segmentIndex = (uint16_t)segIdx;
                h->totalSegments = (uint16_t)totalSegments;
                h->audioTimestamp = highResTs;
                h->sampleRate = currentSampleRate;
                h->numChannels = (uint16_t)totalActive;
                h->numSamples = (uint16_t)packetSamples;
                h->channelStartIndex = (uint16_t)startChIdx;
                h->channelCount = (uint16_t)chCount;
                h->hasMetadata = (segIdx == 0) ? 1 : 0;
                h->crc32 = 0;
                offset += sizeof(MultiChannelHeader);

                if (h->hasMetadata) {
                    auto* meta = reinterpret_cast<ChannelMetadata*>((char*)networkBuffer.getData() + offset);
                    std::memset(meta, 0, sizeof(ChannelMetadata));
                    for (int i = 0; i < totalActive; ++i)
                        std::strncpy(meta->names[i], validTracks[i]->name.toRawUTF8(), 19);
                    offset += sizeof(ChannelMetadata);
                }

                float* audioPtr = reinterpret_cast<float*>((char*)networkBuffer.getData() + offset);
                for (int i = 0; i < chCount; ++i) {
                    juce::FloatVectorOperations::copy(audioPtr + (i * packetSamples),
                                                     frameBuffer.getReadPointer(startChIdx + i),
                                                     packetSamples);
                }
                offset += (chCount * packetSamples * sizeof(float));
                h->crc32 = CRC32::calculate(networkBuffer.getData(), offset);

                juce::String targetIp;
                auto mode = currentMode.load();
                if (mode == NetworkMode::Unicast) {
                    targetIp = cachedTargetIp;
                } else if (mode == NetworkMode::MulticastSingle) {
                    targetIp = multicastBaseIp + "1";
                } else {
                    targetIp = multicastBaseIp + juce::String(segIdx + 1);
                }

                int sent = udpSocket.write(targetIp, 54321, networkBuffer.getData(), (int)offset);
                
                if (sent > 0) {
                    bytesSentSinceLastHealth += sent;
                } else {
                    droppedPacketCounter++;
                }

            }

            // HEALTH PACKET (Kirim status setiap 1 detik)
            auto now = juce::Time::getMillisecondCounter();
            if (now - lastHealthMs > 1000) {
                HealthPacket hp;
                hp.frameCounter = currentFrameID;
                hp.channelCount = (uint16_t)totalActive;
                hp.mode = (uint16_t)currentMode.load();
                hp.cpuLoad = (float)deviceManager.getCpuUsage();
                hp.networkMbps = (bytesSentSinceLastHealth * 8.0f) / (1024.0f * 1024.0f);
                hp.networkBytesPerSec = (uint32_t)bytesSentSinceLastHealth;
                hp.droppedPackets = droppedPacketCounter.load();
                hp.avgJitterMs = jitter;
                udpSocket.write("239.255.0.100", 54321, &hp, sizeof(hp));
                lastHealthMs = now;
                bytesSentSinceLastHealth = 0;
            }
        }
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override {
        currentSampleRate = (float)sampleRate;
        inputCopy.setSize(64, samplesPerBlockExpected);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override {
        auto* deviceBuffer = bufferToFill.buffer;
        int numSamples = bufferToFill.numSamples, startSample = bufferToFill.startSample;
        
        for (int i = 0; i < deviceBuffer->getNumChannels(); ++i)
            inputCopy.copyFrom(i, 0, *deviceBuffer, i, startSample, numSamples);
            
        for (int ch = 0; ch < deviceBuffer->getNumChannels(); ++ch)
            deviceBuffer->clear(ch, startSample, numSamples);

        const juce::ScopedLock sl(trackLock);
        bool dataWasWritten = false;
        for (int i = 0; i < tracks.size(); ++i) {
            auto* track = tracks[i];
            int inputIdx = track->routedInputIndex.load();
            
            if (inputIdx >= 0 && inputIdx < inputCopy.getNumChannels()) {
                const float* reader = inputCopy.getReadPointer(inputIdx, 0);
                float targetGain = juce::Decibels::decibelsToGain((float)faders[i]->getValue(), -70.0f);
                
                track->smoothGain = (track->smoothGain * 0.9f) + (targetGain * 0.1f);
                track->currentLevelDb.store(juce::Decibels::gainToDecibels(inputCopy.getRMSLevel(inputIdx, 0, numSamples)));
                
                if (track->soloEnabled.load()) {
                    deviceBuffer->addFrom(0, startSample, reader, numSamples, track->smoothGain);
                    deviceBuffer->addFrom(1, startSample, reader, numSamples, track->smoothGain);
                }
                
                if (track->sendEnabled.load()) {
                    int s1, z1, s2, z2;
                    track->fifo.prepareToWrite(numSamples, s1, z1, s2, z2);
                    if (z1 > 0) track->fifoBuffer.copyFrom(0, s1, reader, z1, track->smoothGain);
                    if (z2 > 0) track->fifoBuffer.copyFrom(0, s2, reader + z1, z2, track->smoothGain);
                    track->fifo.finishedWrite(numSamples);
                    dataWasWritten = true;
                }
            }
        }
        if (dataWasWritten)
               dataReadyEvent.signal(); // <--- INI YANG MEMBUAT LATENCY RENDAH
    }

    void releaseResources() override {}
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff141414)); }
    void resized() override {
        headerArea.setBounds (0, 0, getWidth(), 70);
        ioButton.setBounds (15, 18, 50, 32);
        modeSelector.setBounds (75, 18, 160, 32);
        targetIpLabel.setBounds (245, 18, 140, 32);
        titleLabel.setBounds (400, 15, 400, 40);
        addTrackButton.setBounds (getWidth() - 100, 18, 85, 32);
        viewport.setBounds (0, 70, getWidth(), getHeight() - 70);
        
        const juce::ScopedLock sl(trackLock);
        if (mixerContainer != nullptr) {
            int trackW = 125, h = viewport.getHeight() - 20;
            mixerContainer->setSize (juce::jmax ((int)tracks.size() * trackW + 20, viewport.getWidth()), h);
            for (int i = 0; i < tracks.size(); ++i) {
                int x = (i * trackW) + 5;
                if (i < removeButtons.size()) removeButtons[i]->setBounds (x + 102, 6, 18, 18);
                if (i < inputSelectors.size()) inputSelectors[i]->setBounds (x + 8, 12, 92, 22);
                if (i < faders.size()) faders[i]->setBounds (x + 5, 45, 115, h - 140);
                if (i < soloButtons.size()) soloButtons[i]->setBounds (x + 15, h - 85, 95, 24);
                if (i < sendButtons.size()) sendButtons[i]->setBounds (x + 15, h - 57, 95, 24);
                if (i < trackLabels.size()) trackLabels[i]->setBounds (x + 5, h - 28, 115, 22);
            }
        }
    }
    
    void timerCallback() override { if (mixerContainer) mixerContainer->repaint(); }

    void addTrack() {
        const juce::ScopedLock sl (trackLock);
        if (tracks.size() >= 64) return;
        int id = nextTrackId++;
        auto* t = tracks.add(new AudioTrack(id, "CH " + juce::String (tracks.size() + 1)));
        
        auto* cb = inputSelectors.add (new juce::ComboBox());
        cb->addItem ("None", 1);
        auto inputNames = deviceManager.getCurrentAudioDevice() ? deviceManager.getCurrentAudioDevice()->getInputChannelNames() : juce::StringArray();
        for(int i=0; i<inputNames.size(); ++i) cb->addItem(inputNames[i], i+2);
        cb->setSelectedId(tracks.size() + 1);
        cb->onChange = [this, id, cb] {
            const juce::ScopedLock sl(trackLock);
            for(auto* tr : tracks) if(tr->id == id) tr->routedInputIndex.store(cb->getSelectedId() - 2);
        };
        mixerContainer->addAndMakeVisible (cb);
        
        auto* s = faders.add (new juce::Slider());
        s->setLookAndFeel (&customLNF);
        s->setSliderStyle (juce::Slider::LinearVertical);
        s->setRange (-70.0, 12.0, 0.1); s->setValue (0.0);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
        mixerContainer->addAndMakeVisible (s);
        
        auto* bSolo = soloButtons.add (new juce::TextButton ("SOLO"));
        bSolo->setClickingTogglesState (true);
        bSolo->setColour (juce::TextButton::buttonOnColourId, juce::Colours::yellow.darker());
        bSolo->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        bSolo->onClick = [this, id, bSolo] {
            const juce::ScopedLock sl(trackLock);
            for(auto* tr : tracks) if(tr->id == id) tr->soloEnabled.store(bSolo->getToggleState());
        };
        mixerContainer->addAndMakeVisible (bSolo);
        
        auto* bSend = sendButtons.add (new juce::TextButton ("SEND"));
        bSend->setClickingTogglesState (true); bSend->setToggleState (true, juce::dontSendNotification);
        bSend->setColour (juce::TextButton::buttonOnColourId, juce::Colours::cyan.darker());
        bSend->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        bSend->onClick = [this, id, bSend] {
            const juce::ScopedLock sl(trackLock);
            for(auto* tr : tracks) if(tr->id == id) tr->sendEnabled.store(bSend->getToggleState());
        };
        mixerContainer->addAndMakeVisible (bSend);
        
        auto* lbl = trackLabels.add (new juce::Label());
        lbl->setText (t->name, juce::dontSendNotification); lbl->setJustificationType(juce::Justification::centred);
        lbl->setEditable(true);
        lbl->onTextChange = [this, id, lbl] {
            const juce::ScopedLock sl (trackLock);
            for (auto* tr : tracks) if (tr->id == id) { tr->name = lbl->getText(); break; }
        };
        mixerContainer->addAndMakeVisible (lbl);
        
        auto* bRem = removeButtons.add (new juce::TextButton ("X"));
        bRem->setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.5f));
        bRem->onClick = [this, id] { removeTrack (id); };
        mixerContainer->addAndMakeVisible (bRem);
        resized();
    }

    void removeTrack (int trackId) {
        const juce::ScopedLock sl (trackLock);
        int idx = -1;
        for (int i = 0; i < tracks.size(); ++i) if (tracks[i]->id == trackId) { idx = i; break; }
        if (idx != -1) {
            mixerContainer->removeChildComponent(removeButtons[idx]);
            mixerContainer->removeChildComponent(inputSelectors[idx]);
            mixerContainer->removeChildComponent(faders[idx]);
            mixerContainer->removeChildComponent(soloButtons[idx]);
            mixerContainer->removeChildComponent(sendButtons[idx]);
            mixerContainer->removeChildComponent(trackLabels[idx]);
            tracks.remove(idx); faders.remove(idx); soloButtons.remove(idx);
            sendButtons.remove(idx); inputSelectors.remove(idx); trackLabels.remove(idx);
            removeButtons.remove(idx); resized();
        }
    }

    void showDeviceSettings() {
        if (settingsWindow != nullptr) { settingsWindow->toFront(true); return; }
        deviceSelector.setSize (500, 500);
        juce::DialogWindow::LaunchOptions options;
        options.content.setNonOwned (&deviceSelector);
        options.dialogTitle = "Audio Settings";
        options.dialogBackgroundColour = juce::Colour (0xff141414);
        options.useNativeTitleBar = true;
        settingsWindow = options.launchAsync();
    }

private:
    CustomLNF customLNF;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;
    juce::Viewport viewport;
    std::unique_ptr<MixerContainer> mixerContainer;
    juce::Component headerArea;
    juce::Label titleLabel, targetIpLabel;
    juce::ComboBox modeSelector;
    juce::TextButton addTrackButton, ioButton;
    juce::CriticalSection trackLock;
    juce::DatagramSocket udpSocket;
    juce::String cachedTargetIp;
    float currentSampleRate = 48000.0f;
    juce::AudioBuffer<float> inputCopy, frameBuffer;
    juce::MemoryBlock networkBuffer;
    uint32_t packetSequenceCounter = 0;
    std::atomic<NetworkMode> currentMode { NetworkMode::Unicast };
    juce::WaitableEvent dataReadyEvent;
    std::atomic<uint32_t> droppedPacketCounter { 0 };
    juce::String multicastBaseIp = "239.255.0.";
    juce::OwnedArray<AudioTrack> tracks;
    int nextTrackId = 0;
    juce::OwnedArray<juce::Slider> faders;
    juce::OwnedArray<juce::TextButton> soloButtons, sendButtons, removeButtons;
    juce::OwnedArray<juce::ComboBox> inputSelectors;
    juce::OwnedArray<juce::Label> trackLabels;
    uint32_t lastHealthMs = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
