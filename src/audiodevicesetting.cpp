/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/audiodevicesetting.cpp
*
*  Defines AudioDeviceSetting.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QComboBox>
#include <QProgressBar>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/audiodevicesetting.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** AudioDeviceSetting ******************************/

class AudioDeviceSetting_p
{
    public:

        AudioDeviceType type=AudioDeviceType::Capture;

        PushButton* typeButton=nullptr;
        QComboBox* combo=nullptr;
        QLabel* defaultName=nullptr;
        QLabel* commentLabel=nullptr;
        PushButton* testButton=nullptr;
        QProgressBar* volumeBar=nullptr;

        QString defaultDeviceName;

        std::shared_ptr<SvgIcon> micIcon;
        std::shared_ptr<SvgIcon> speakerIcon;
        std::shared_ptr<SvgIcon> playIcon;
        std::shared_ptr<SvgIcon> stopIcon;

        QString defaultText() const
        {
            return defaultDeviceName.isEmpty() ? QObject::tr("Default") : defaultDeviceName;
        }
};

//--------------------------------------------------------------------------

AudioDeviceSetting::AudioDeviceSetting(AudioDeviceType type, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<AudioDeviceSetting_p>())
{
    pimpl->type=type;

    pimpl->micIcon=Style::instance().svgIconLocator().icon("AudioDeviceSetting::capture",this);
    pimpl->speakerIcon=Style::instance().svgIconLocator().icon("AudioDeviceSetting::playback",this);
    pimpl->playIcon=Style::instance().svgIconLocator().icon("AudioDeviceSetting::test",this);
    pimpl->stopIcon=Style::instance().svgIconLocator().icon("AudioDeviceSetting::stop",this);

    auto mainL=Layout::vertical(this);

    // row 1 - device type indicator + device combo box
    auto deviceRow=new QFrame(this);
    deviceRow->setObjectName("deviceRow");
    auto deviceL=Layout::horizontal(deviceRow);
    mainL->addWidget(deviceRow);

    pimpl->typeButton=new PushButton(deviceRow);
    pimpl->typeButton->setObjectName("typeButton");
    deviceL->addWidget(pimpl->typeButton);

    pimpl->combo=new QComboBox(deviceRow);
    pimpl->combo->setObjectName("deviceCombo");
    deviceL->addWidget(pimpl->combo,1);

    // implicit Default entry always kept at index 0
    pimpl->combo->addItem(pimpl->defaultText(),QString{});

    connect(
        pimpl->combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int)
        {
            pimpl->defaultName->setVisible(isDefaultSelected());
            emit deviceChanged(currentDevice());
        }
    );

    // row 2 - default device name
    auto defaultRow=new QFrame(this);
    defaultRow->setObjectName("defaultRow");
    auto defaultL=Layout::horizontal(defaultRow);
    mainL->addWidget(defaultRow);

    pimpl->defaultName=new QLabel(defaultRow);
    pimpl->defaultName->setObjectName("defaultName");
    pimpl->defaultName->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->defaultName->setWordWrap(true);
    defaultL->addWidget(pimpl->defaultName,1);

    // row between device and test rows - status comment (hidden by default)
    pimpl->commentLabel=new QLabel(this);
    pimpl->commentLabel->setObjectName("commentLabel");
    pimpl->commentLabel->setWordWrap(true);
    pimpl->commentLabel->setVisible(false);
    mainL->addWidget(pimpl->commentLabel);

    // row 3 - test button + volume meter
    auto testRow=new QFrame(this);
    testRow->setObjectName("testRow");
    auto testL=Layout::horizontal(testRow);
    mainL->addWidget(testRow);

    pimpl->testButton=new PushButton(pimpl->playIcon,testRow);
    pimpl->testButton->setObjectName("testButton");
    pimpl->testButton->setCheckable(true);
    pimpl->testButton->setText(tr("Test"));
    testL->addWidget(pimpl->testButton);

    connect(
        pimpl->testButton,
        &PushButton::toggled,
        this,
        [this](bool checked)
        {
            pimpl->testButton->setSvgIcon(checked ? pimpl->stopIcon : pimpl->playIcon);
            pimpl->testButton->setText(checked ? tr("Stop") : tr("Test"));
            emit testToggled(checked);
        }
    );

    pimpl->volumeBar=new QProgressBar(testRow);
    pimpl->volumeBar->setObjectName("volumeBar");
    pimpl->volumeBar->setRange(0,100);
    pimpl->volumeBar->setValue(0);
    pimpl->volumeBar->setTextVisible(false);
    testL->addWidget(pimpl->volumeBar,1);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    setDeviceType(type);
    pimpl->defaultName->setVisible(isDefaultSelected());
}

//--------------------------------------------------------------------------

AudioDeviceSetting::~AudioDeviceSetting()
{}

//--------------------------------------------------------------------------

AudioDeviceType AudioDeviceSetting::deviceType() const
{
    return pimpl->type;
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::setDeviceType(AudioDeviceType type)
{
    pimpl->type=type;
    pimpl->typeButton->setSvgIcon(type==AudioDeviceType::Capture ? pimpl->micIcon : pimpl->speakerIcon);
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::setDevices(const QVector<AudioDevice>& devices)
{
    const QSignalBlocker blocker{pimpl->combo};
    const auto previousId=currentDevice().id;

    pimpl->combo->clear();
    pimpl->combo->addItem(pimpl->defaultText(),QString{});
    for (const auto& device : devices)
    {
        if (device.id.isEmpty())
        {
            continue;
        }
        pimpl->combo->addItem(device.name,device.id);
    }

    setCurrentDevice(previousId);
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::addDevice(const AudioDevice& device)
{
    if (device.id.isEmpty())
    {
        return;
    }
    pimpl->combo->addItem(device.name,device.id);
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::clearDevices()
{
    const QSignalBlocker blocker{pimpl->combo};
    pimpl->combo->clear();
    pimpl->combo->addItem(pimpl->defaultText(),QString{});
    pimpl->combo->setCurrentIndex(0);
    pimpl->defaultName->setVisible(isDefaultSelected());
    emit deviceChanged(currentDevice());
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::setCurrentDevice(const QString& id)
{
    int index=0;
    if (!id.isEmpty())
    {
        auto found=pimpl->combo->findData(id);
        if (found>=0)
        {
            index=found;
        }
    }
    pimpl->combo->setCurrentIndex(index);
}

//--------------------------------------------------------------------------

AudioDevice AudioDeviceSetting::currentDevice() const
{
    return AudioDevice{pimpl->combo->currentData().toString(),pimpl->combo->currentText()};
}

//--------------------------------------------------------------------------

bool AudioDeviceSetting::isDefaultSelected() const
{
    return pimpl->combo->currentIndex()==0;
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::setDefaultDeviceName(const QString& name)
{
    pimpl->defaultDeviceName=name;
    pimpl->defaultName->setText(name);
    if (pimpl->combo->count()>0)
    {
        pimpl->combo->setItemText(0,pimpl->defaultText());
    }
}

//--------------------------------------------------------------------------

QString AudioDeviceSetting::defaultDeviceName() const
{
    return pimpl->defaultDeviceName;
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::setVolume(int value)
{
    pimpl->volumeBar->setValue(value);
}

//--------------------------------------------------------------------------

int AudioDeviceSetting::volume() const
{
    return pimpl->volumeBar->value();
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::setTesting(bool enable)
{
    pimpl->testButton->setChecked(enable);
}

//--------------------------------------------------------------------------

bool AudioDeviceSetting::isTesting() const
{
    return pimpl->testButton->isChecked();
}

//--------------------------------------------------------------------------

void AudioDeviceSetting::setComment(const QString& text)
{
    pimpl->commentLabel->setText(text);
    pimpl->commentLabel->setVisible(!text.isEmpty());
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
