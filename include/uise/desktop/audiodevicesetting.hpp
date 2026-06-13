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

/** @file uise/desktop/audiodevicesetting.hpp
*
*  Declares AudioDeviceSetting.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AUDIO_DEVICE_SETTING_HPP
#define UISE_DESKTOP_AUDIO_DEVICE_SETTING_HPP

#include <memory>

#include <QFrame>
#include <QString>
#include <QVector>
#include <QMetaType>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Type of audio device managed by AudioDeviceSetting.
 */
enum class AudioDeviceType : int
{
    Capture,  //!< Input device (microphone).
    Playback  //!< Output device (speaker).
};

/**
 * @brief Descriptor of a single audio device.
 *
 * Used both to populate the device list and to report the current selection.
 * The implicit "Default" entry is represented by an empty id.
 */
struct AudioDevice
{
    QString id;
    QString name;
};

class AudioDeviceSetting_p;

/**
 * @brief Widget to configure a single audio capture or playback device.
 *
 * Layout (three rows):
 *  1. Device-type indicator button (microphone/speaker) + device combo box.
 *  2. Label showing the OS default device name (visible only while "Default" is selected).
 *  3. Checkable Test/Stop button + dashed volume progress bar.
 *
 * The combo box always keeps an implicit "Default" entry at index 0. That entry is never part of
 * the device list supplied via the API and is never returned by setDevices()/addDevice(); it is
 * always prepended by the widget and carries an empty id.
 */
class UISE_DESKTOP_EXPORT AudioDeviceSetting : public QFrame
{
    Q_OBJECT

    public:

        AudioDeviceSetting(AudioDeviceType type, QWidget* parent=nullptr);

        AudioDeviceSetting(QWidget* parent=nullptr) : AudioDeviceSetting(AudioDeviceType::Capture,parent)
        {}

        ~AudioDeviceSetting();

        AudioDeviceSetting(const AudioDeviceSetting&)=delete;
        AudioDeviceSetting(AudioDeviceSetting&&)=delete;
        AudioDeviceSetting& operator=(const AudioDeviceSetting&)=delete;
        AudioDeviceSetting& operator=(AudioDeviceSetting&&)=delete;

        AudioDeviceType deviceType() const;

        //! Currently selected device. For the implicit "Default" entry returns {"",<defaultName>}.
        AudioDevice currentDevice() const;

        //! Whether the implicit "Default" entry (index 0) is currently selected.
        bool isDefaultSelected() const;

        QString defaultDeviceName() const;

        int volume() const;

        bool isTesting() const;

        QString defaultText() const;

    signals:

        //! Emitted when the selected device changes.
        void deviceChanged(const uise::AudioDevice& device);

        //! Emitted when the Test button is toggled.
        void testToggled(bool testing);

    public slots:

        void setDeviceType(uise::AudioDeviceType type);

        //! Replace the device list. The supplied list must not contain the implicit "Default" entry.
        void setDevices(const QVector<uise::AudioDevice>& devices);

        //! Append a single device. An entry with an empty id is ignored.
        void addDevice(const uise::AudioDevice& device);

        //! Remove all devices (the implicit "Default" entry is kept).
        void clearDevices();

        //! Select the entry whose id matches; an empty id selects the "Default" entry.
        void setCurrentDevice(const QString& id);

        void setCurrentDevice(const uise::AudioDevice& device)
        {
            setCurrentDevice(device.id);
        }

        void updateDefaultName();

        //! Set the OS default device name shown in row 2 and on the "Default" combo entry.
        void setDefaultDeviceName(const QString& name);

        //! Set the volume meter value (0..100).
        void setVolume(int value);

        //! Set the Test button checked state.
        void setTesting(bool enable);

        /**
         * @brief Show or hide a status comment below the device row.
         *
         * Pass a non-empty string to display a note (e.g. "Selected device is not connected.
         * Currently using system default."). Pass an empty string to hide the comment.
         */
        void setComment(const QString& text);

    private:

        std::unique_ptr<AudioDeviceSetting_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::AudioDevice)

#endif // UISE_DESKTOP_AUDIO_DEVICE_SETTING_HPP
