#pragma once
#define MarkerFilesExt ".easymarkers"

#define ColorDefaultBkg Colours::darkgrey
#define ColorWaveThumbnailBkg Colours::black
#define ColorWaveThumbnailForm Colours::darkgrey
#define ColorWavePlayheadPlay Colours::orange
#define ColorWavePlayheadStop Colours::orange
#define ColorWaveMarker Colours::yellow
#define ColorText1 Colours::white
#include "DemoUtilities.h"

#define INTRO_BUTTON 'Q' //q
#define BUILDUP_BUTTON 'W' //w
#define DROP_BUTTON 'E' //e
#define BREAKDOWN_BUTTON 'R' //r
#define BRIDGE_BUTTON 'T' //t
#define OUTRO_BUTTON 'Y' //y


class MarkerInfo : public juce::Component, private juce::Button::Listener, private juce::TextEditor::Listener
{
public:
    MarkerInfo(double p, const juce::String &t) : pos(p)
    {
        setInterceptsMouseClicks(false, true);
        editMarker.setClickingTogglesState(true);
        editMarker.addListener(this);
        editTitle.setText(t);
        editTitle.addListener(this);

        addAndMakeVisible(editMarker);
        addAndMakeVisible(delMarker);
    }

    void buttonClicked(Button *btn) override
    {
        if (&editMarker == btn)
        {
            if (editMarker.getToggleState())
            {
                addAndMakeVisible(editTitle);
                editTitle.grabKeyboardFocus();
            }
            else
            {
                removeChildComponent(&editTitle);
            }
        }
    }

    virtual void textEditorReturnKeyPressed(TextEditor &e)
    {
        editMarker.setToggleState(false, juce::NotificationType::sendNotification);
    }

    virtual void textEditorEscapeKeyPressed(TextEditor &e)
    {
        editMarker.setToggleState(false, juce::NotificationType::sendNotification);
    }

    virtual void textEditorFocusLost(TextEditor &e)
    {
        editMarker.setToggleState(false, juce::NotificationType::sendNotification);
    }

    void resized() override
    {
        editMarker.setBounds(1, 15, 15, 15);
        delMarker.setBounds(1, 31, 15, 15);
        editTitle.setBounds(1, 0, getWidth(), 18);
    }

    void paint(juce::Graphics &g) override
    {
        g.setColour(ColorWaveMarker);
        if (!editMarker.getToggleState())
            g.drawText(editTitle.getText(), 1, 0, getWidth(), 20, juce::Justification::topLeft);
        g.drawRect(0, 0, 1, getHeight());
    }

    double pos;
    TextEditor editTitle;
    TextButton editMarker{"e"};
    TextButton delMarker{"-"};
};

struct PlayHead : public juce::Component
{
    PlayHead(AudioTransportSource &t) : transportSource(t)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics &g)
    {
        if (transportSource.isPlaying())
            g.setColour(ColorWavePlayheadPlay);
        else
            g.setColour(ColorWavePlayheadStop);
        g.fillRect(0, 0, 1, getHeight());
    }

private:
    AudioTransportSource &transportSource;
};

class DemoThumbnailComp : public Component,
                          public ChangeListener,
                          public FileDragAndDropTarget,
                          public ChangeBroadcaster,
                          private ScrollBar::Listener,
                          private Button::Listener,
                          private Timer
{
public:
    void setZoomFactor(double amount)
    {
        if (thumbnail.getTotalLength() > 0)
        {
            auto newScale = jmax(0.001, thumbnail.getTotalLength() * (1.0 - jlimit(0.0, 0.99, amount)));
            auto timeAtCentre = xToTime(getWidth() / 2.0f);

            auto timeAtCursor = xToTime(currentPositionMarker.getX());

            auto timeAtMouse = xToTime(lastMousePos.getX());

            setRange({juce::jmax(0., timeAtMouse - newScale * 0.5), timeAtMouse + newScale * 0.5});
        }
    }

    void setRange(Range<double> newRange)
    {
        visibleRange = newRange;
        scrollbar.setCurrentRange(visibleRange);
        updateCursorPosition();
        repaint();
    }

    void setFollowsTransport(bool shouldFollow)
    {
        isFollowingTransport = shouldFollow;
    }

    void paint(Graphics &g)
    {
        g.fillAll(ColorWaveThumbnailBkg);

        g.setColour(ColorText1);
        g.setFont(20.0f);
        juce::Time time(transportSource.getCurrentPosition() * 1000);
        juce::String timeStr = juce::String(time.getMinutes()) + juce::String(":") + juce::String(time.getSeconds()) + juce::String(".") + juce::String(time.getMilliseconds());
        g.drawText(timeStr, 0, 0, getWidth(), addMarker.getHeight(), juce::Justification::centred);

        if (thumbnail.getTotalLength() > 0.0)
        {
            g.setColour(ColorWaveThumbnailForm);
            // draw thumb
            auto thumbArea = getLocalBounds().removeFromBottom(getHeight() * 0.50);

            thumbArea.removeFromBottom(scrollbar.getHeight() + 4);
            thumbnail.drawChannels(g, thumbArea.reduced(2),
                                   visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
        }
        else
        {
            g.setFont(14.0f);
            g.drawFittedText("No audio file loaded", getLocalBounds(), Justification::centred, 2);
        }
    }

    void resized()
    {
        addMarker.setBounds(1, 1, 25, 25);
        scrollbar.setBounds(getLocalBounds().removeFromBottom(14).reduced(2));
        repaint();
    }

    void changeListenerCallback(ChangeBroadcaster *)
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }

    bool isInterestedInFileDrag(const StringArray & /*files*/)
    {
        return true;
    }

    void filesDropped(const StringArray &files, int /*x*/, int /*y*/)
    {
        lastFileDropped = URL(File(files[0]));
        sendChangeMessage();
    }

    void buttonClicked(juce::Button *btn)
    {
        if (&addMarker == btn)
        {
            addMarkerToList(transportSource.getCurrentPosition(), "NEW MARKER", true);
        }
        else
        {
            for (auto it = markers.begin(); it != markers.end(); ++it)
            {
                if (&(*it)->editMarker == btn)
                {
                    saveMarkers();
                }
                if (&(*it)->delMarker == btn)
                {
                    markers.erase(it);
                    break;
                }
            }
        }
        resized();
    }

    void addMarkerToList(double time, const juce::String &title, bool saveXML)
    {
        MarkerInfo *newMarker = new MarkerInfo(time, title);
        newMarker->delMarker.addListener(this);
        newMarker->editMarker.addListener(this);

        addChildComponent(newMarker);
        markers.push_back(newMarker);
        resized();
        if (saveXML)
            saveMarkers();
    }

    void saveMarkers()
    {
        juce::XmlElement root("Markers");
        for (auto &marker : markers)
        {
            auto m = root.createNewChildElement("Marker");
            m->setAttribute("Time", marker->pos);
            m->setAttribute("Title", marker->editTitle.getText());
        }
        bool res = root.writeToFile(markersLocation, "");
        jassert(res);
    }

    void loadMarkers()
    {
        std::unique_ptr<juce::XmlElement> root = XmlDocument::parse(markersLocation);
        if (!root)
            return;

        markers.clear();

        for (int i = 0; i < root->getNumChildElements(); ++i)
        {
            auto m = root->getChildElement(i);
            if (m->getTagName() == "Marker")
            {
                double time = m->getDoubleAttribute("Time");
                juce::String title = m->getStringAttribute("Title");
                addMarkerToList(time, title, true);
            }
        }
        root.release();
        resized();
    }

    void mouseDown(const MouseEvent &e)
    {
        mouseDrag(e);
    }

    void mouseDrag(const MouseEvent &e)
    {
        if (canMoveTransport())
            transportSource.setPosition(jmax(0.0, xToTime((float)e.x)));
    }

    void mouseMove(const MouseEvent &e)
    {
        lastMousePos = e.getPosition();
    }

    void mouseUp(const MouseEvent &)
    {
        // transportSource.start();
    }

    void mouseWheelMove(const MouseEvent &, const MouseWheelDetails &wheel)
    {
        if (thumbnail.getTotalLength() > 0.0)
        {
            auto newStart = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
            newStart = jlimit(0.0, jmax(0.0, thumbnail.getTotalLength() - (visibleRange.getLength())), newStart);

            if (canMoveTransport())
                setRange({newStart, newStart + visibleRange.getLength()});

            if (wheel.deltaY != 0.0f)
                zoomSlider.setValue(zoomSlider.getValue() - wheel.deltaY);

            repaint();
        }
    }

    DemoThumbnailComp(AudioFormatManager &formatManager,
                      AudioTransportSource &source,
                      Slider &slider)
        : transportSource(source),
          zoomSlider(slider),
          thumbnail(512, formatManager, thumbnailCache),
          currentPositionMarker(source)
    {
        thumbnail.addChangeListener(this);

        addAndMakeVisible(scrollbar);
        scrollbar.setRangeLimits(visibleRange);
        scrollbar.setAutoHide(false);
        scrollbar.addListener(this);

        addAndMakeVisible(currentPositionMarker);

        addAndMakeVisible(addMarker);
        addMarker.addListener(this);

        setOpaque(true);
    }

    ~DemoThumbnailComp()
    {
        scrollbar.removeListener(this);
        thumbnail.removeChangeListener(this);
    }

    void setURL(const URL &url)
    {
        markersLocation = File();
        InputSource *inputSource = nullptr;

        if (url.isLocalFile())
        {
            inputSource = new FileInputSource(url.getLocalFile());
            markersLocation = url.getLocalFile().getFullPathName() + MarkerFilesExt;
        }
        else
        {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Cannot open file", "Is not a local file");
            //   if (inputSource == nullptr)
            //     inputSource = new URLInputSource (url);
        }

        if (inputSource != nullptr)
        {
            thumbnail.setSource(inputSource);

            Range<double> newRange(0.0, thumbnail.getTotalLength());
            scrollbar.setRangeLimits(newRange);
            setRange(newRange);

            startTimerHz(40);

            loadMarkers();
        }
    }
    URL getLastDroppedFile() const noexcept { return lastFileDropped; }
    std::list<juce::ScopedPointer<MarkerInfo>> markers;

private:
    AudioTransportSource &transportSource;
    Slider &zoomSlider;
    ScrollBar scrollbar{false};
    TextButton addMarker{"+"};
    AudioThumbnailCache thumbnailCache{5};
    AudioThumbnail thumbnail;
    Range<double> visibleRange;
    bool isFollowingTransport = false;
    URL lastFileDropped;
    PlayHead currentPositionMarker;
    juce::File markersLocation;
    juce::Point<int> lastMousePos;

    float timeToX(const double time) const
    {
        if (visibleRange.getLength() <= 0)
            return 0;

        return getWidth() * (float)((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    double xToTime(const float x) const
    {
        return (x / getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    bool canMoveTransport() const noexcept
    {
        return !(isFollowingTransport && transportSource.isPlaying());
    }

    void scrollBarMoved(ScrollBar *scrollBarThatHasMoved, double newRangeStart)
    {
        if (scrollBarThatHasMoved == &scrollbar)
            if (!(isFollowingTransport && transportSource.isPlaying()))
                setRange(visibleRange.movedToStartAt(newRangeStart));
    }

    void timerCallback()
    {

        repaint(0, 0, getWidth(), addMarker.getHeight());

        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange(visibleRange.movedToStartAt(transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }

    void updateCursorPosition()
    {

        currentPositionMarker.setBounds(timeToX(transportSource.getCurrentPosition()) - 0.75f, addMarker.getBottom(),
                                        50.f, (float)(getHeight() - scrollbar.getHeight() - addMarker.getBottom()));

        if (thumbnail.getTotalLength() > 0.0)
        {
            for (auto &marker : markers)
            {
                if (marker->pos >= visibleRange.getStart() && marker->pos < visibleRange.getEnd())
                {
                    float curPos = timeToX(marker->pos) - 0.75f;
                    marker->setVisible(true);
                    marker->setBounds(curPos, addMarker.getBottom(), 200, getHeight() - scrollbar.getHeight() - addMarker.getBottom());
                }
                else
                    marker->setVisible(false);
            }
        }
    }
};

//==============================================================================
class MKS : public Component,
            private Button::Listener,
            private ChangeListener,
            private ListBoxModel,
            private KeyListener
{
public:
    MKS()
    {

        addKeyListener(this);

        addAndMakeVisible(audioBox);
        audioBox.setModel(this);

        addAndMakeVisible(zoomLabel);
        zoomLabel.setFont(Font(15.00f, Font::plain));
        zoomLabel.setJustificationType(Justification::centredRight);
        zoomLabel.setEditable(false, false, false);
        zoomLabel.setColour(TextEditor::textColourId, Colours::black);
        zoomLabel.setColour(TextEditor::backgroundColourId, Colour(0x00000000));

        addAndMakeVisible(followTransportButton);
        followTransportButton.onClick = [this]
        { updateFollowTransportState(); };

        addAndMakeVisible(chooseFileButton);
        chooseFileButton.addListener(this);

        addAndMakeVisible(zoomSlider);
        zoomSlider.setRange(0, 1, 0);
        zoomSlider.onValueChange = [this]
        { thumbnail->setZoomFactor(zoomSlider.getValue()); };
        zoomSlider.setSkewFactor(2);

        thumbnail = std::make_unique<DemoThumbnailComp>(formatManager, transportSource, zoomSlider);
        addAndMakeVisible(thumbnail.get());
        thumbnail->addChangeListener(this);

        addAndMakeVisible(startStopButton);
        startStopButton.setColour(TextButton::buttonColourId, Colour(0xff79ed7f));
        startStopButton.setColour(TextButton::textColourOffId, Colours::black);
        startStopButton.onClick = [this]
        { startOrStop(); };

        // audio setup
        formatManager.registerBasicFormats();

        thread.startThread(Thread::Priority::normal);

#ifndef JUCE_DEMO_RUNNER
        RuntimePermissions::request(RuntimePermissions::recordAudio,
                                    [this](bool granted)
                                    {
                                        int numInputChannels = granted ? 2 : 0;
                                        audioDeviceManager.initialise(numInputChannels, 2, nullptr, true, {}, nullptr);
                                    });
#endif

        audioDeviceManager.addAudioCallback(&audioSourcePlayer);
        audioSourcePlayer.setSource(&transportSource);

        setOpaque(true);
        setSize(500, 500);
    }

    ~MKS() override
    {
        transportSource.setSource(nullptr);
        audioSourcePlayer.setSource(nullptr);

        audioDeviceManager.removeAudioCallback(&audioSourcePlayer);

        chooseFileButton.removeListener(this);

        thumbnail->removeChangeListener(this);
    }
    int getNumRows()
    {

        return foundFiles.size();
    }

    void paintListBoxItem(int rowNumber, Graphics &g,
                          int width, int height, bool rowIsSelected)
    {

        if (rowIsSelected)
            g.fillAll(Colours::lightblue);

        g.setColour(Colours::black);
        g.setFont(height * 0.7f);

        g.drawText(foundFiles[rowNumber].getFileName(), 5, 0, width, height,
                   Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked(int row, const MouseEvent &)
    {
        thumbnail->markers.clear();
        // Logger::outputDebugString(foundFiles[row].getFileName());
        showAudioResource(std::move(URL(foundFiles[row])));
        // sendChangeMessage();
    }

    void paint(Graphics &g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
        if (!foundFiles.isEmpty())
        {
            audioBox.paint(g);
        }
    }

    bool keyPressed(const KeyPress &k, Component *c) override
    {

        switch (k.getKeyCode())
        {
        case (int)INTRO_BUTTON:
            thumbnail->addMarkerToList(transportSource.getCurrentPosition(), "INTRO", true);
            Logger::outputDebugString("pressed intro");
            break;
        case (int)BUILDUP_BUTTON:
            thumbnail->addMarkerToList(transportSource.getCurrentPosition(), "BUILDUP", true);
            Logger::outputDebugString("pressed buildup");
            break;
        case (int)DROP_BUTTON:
            thumbnail->addMarkerToList(transportSource.getCurrentPosition(), "DROP", true);
            Logger::outputDebugString("pressed drop");
            break;
        case (int)BREAKDOWN_BUTTON:
            thumbnail->addMarkerToList(transportSource.getCurrentPosition(), "BREAKDOWN", true);
            Logger::outputDebugString("pressed breakdown");
            break;
        case (int)BRIDGE_BUTTON:
            thumbnail->addMarkerToList(transportSource.getCurrentPosition(), "BRIDGE", true);
            Logger::outputDebugString("pressed bridge");
            break;
        case (int)OUTRO_BUTTON:
            thumbnail->addMarkerToList(transportSource.getCurrentPosition(), "OUTRO", true);
            Logger::outputDebugString("pressed outro");
            break;
        default:
            break;
        }
        return true;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(4);

        auto controls = r.removeFromBottom(90);

        auto controlRightBounds = controls.removeFromRight(controls.getWidth() / 3);

        chooseFileButton.setBounds(controlRightBounds.reduced(10));

        auto zoom = controls.removeFromTop(25);
        zoomLabel.setBounds(zoom.removeFromLeft(50));
        zoomSlider.setBounds(zoom);

        followTransportButton.setBounds(controls.removeFromTop(25));
        startStopButton.setBounds(controls);

        r.removeFromBottom(6);
        thumbnail->setBounds(r.removeFromBottom(700));

        r.removeFromBottom(6);
        audioBox.setBounds(r);
    }

private:
    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
    AudioDeviceManager audioDeviceManager;

    AudioFormatManager formatManager;
    TimeSliceThread thread{"audio file preview"};

    Array<File> foundFiles;

    std::unique_ptr<FileChooser> fileChooser;
    TextButton chooseFileButton{"Choose Audio File...", "Choose an audio file for playback"};

    ListBox audioBox{};

    URL currentAudioFile;
    AudioSourcePlayer audioSourcePlayer;
    AudioTransportSource transportSource;
    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;

    std::unique_ptr<DemoThumbnailComp> thumbnail;
    Label zoomLabel{{}, "zoom:"};
    Slider zoomSlider{Slider::LinearHorizontal, Slider::NoTextBox};
    ToggleButton followTransportButton{"Follow Transport"};
    TextButton startStopButton{"Play/Stop"};

    //==============================================================================
    void showAudioResource(URL resource)
    {
        if (loadURLIntoTransport(resource))
            currentAudioFile = std::move(resource);

        zoomSlider.setValue(0, dontSendNotification);
        thumbnail->setURL(currentAudioFile);
    }

    bool loadURLIntoTransport(const URL &audioURL)
    {
        transportSource.stop();
        transportSource.setSource(nullptr);
        currentAudioFileSource.reset();

        const auto source = makeInputSource(audioURL);

        if (source == nullptr)
            return false;

        auto stream = rawToUniquePtr(source->createInputStream());

        if (stream == nullptr)
            return false;

        auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

        if (reader == nullptr)
            return false;

        currentAudioFileSource = std::make_unique<AudioFormatReaderSource>(reader.release(), true);

        // ..and plug it into our transport source
        transportSource.setSource(currentAudioFileSource.get(),
                                  32768,                                                       // tells it to buffer this many samples ahead
                                  &thread,                                                     // this is the background thread to use for reading-ahead
                                  currentAudioFileSource->getAudioFormatReader()->sampleRate); // allows for sample rate correction

        return true;
    }

    void startOrStop()
    {
        if (transportSource.isPlaying())
        {
            transportSource.stop();
        }
        else
        {
            // transportSource.setPosition (0);
            transportSource.start();
        }
    }

    void updateFollowTransportState()
    {
        thumbnail->setFollowsTransport(followTransportButton.getToggleState());
    }

    void buttonClicked(Button *btn) override
    {
        if (btn == &chooseFileButton && fileChooser.get() == nullptr)
        {
            if (!RuntimePermissions::isGranted(RuntimePermissions::readExternalStorage))
            {
                SafePointer<MKS> safeThis(this);
                RuntimePermissions::request(RuntimePermissions::readExternalStorage,
                                            [safeThis](bool granted) mutable
                                            {
                                                if (safeThis != nullptr && granted)
                                                    safeThis->buttonClicked(&safeThis->chooseFileButton);
                                            });
                return;
            }

            if (FileChooser::isPlatformDialogAvailable())
            {
                fileChooser = std::make_unique<FileChooser>("Select an audio file...", File(), "*.wav;*.mp3;*.aif");

                fileChooser->launchAsync(
                    FileBrowserComponent::canSelectDirectories,
                    [this](const FileChooser &fc) mutable
                    {
                        if (fc.getURLResults().size() > 0)
                        {
                            foundFiles = fc.getResult().findChildFiles(2, false, "*.wav;*.mp3;*.aif");
                            // auto u = fc.getURLResult();
                            for (int i = 0; i < foundFiles.size(); i++)
                            {
                                Logger::outputDebugString(foundFiles[i].getFileName());
                            }
                            audioBox.updateContent();
                        }
                        fileChooser = nullptr;
                    },
                    nullptr);
            }
            else
            {
                NativeMessageBox::showAsync(MessageBoxOptions()
                                                .withIconType(MessageBoxIconType::WarningIcon)
                                                .withTitle("Enable Code Signing")
                                                .withMessage("You need to enable code-signing for your iOS project and enable \"iCloud Documents\" "
                                                             "permissions to be able to open audio files on your iDevice. See: "
                                                             "https://forum.juce.com/t/native-ios-android-file-choosers"),
                                            nullptr);
            }
        }
    }

    void changeListenerCallback(ChangeBroadcaster *source) override
    {
        if (source == thumbnail.get())
            showAudioResource(URL(thumbnail->getLastDroppedFile()));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MKS)
};
