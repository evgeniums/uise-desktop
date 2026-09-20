/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/voicerecorderdialog.hpp
*
*  Declares VoiceRecorderDialog and FloatingVoiceRecorderDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_VOICERECORDERDIALOG_HPP
#define UISE_DESKTOP_VOICERECORDERDIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/floatingdialog.hpp>
#include <uise/desktop/abstractvoicerecorderdialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class VoiceRecorderDialog_p;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief Default AbstractVoiceRecorderDialog.
 *
 * Built in construct() rather than the constructor, so a factory-substituted implementation
 * participates the same way. Carries no dialog button row: its own buttons are IconTextButtons in
 * the content, icon above the text for Pause, Cancel and Send, icon before the text for Listen.
 *
 * QSS: uise--VoiceRecorderDialog, and by object name #recorderHeader, #recordDot,
 * #durationLabel, #targetsRow, #continueArea, #cancelArea, #buttonsRow, #pauseButton,
 * #cancelButton, #sendButton, #playerRow, #listenButton, #progressBar, #commentEdit. The two
 * areas carry hot="true" while the pointer is over them, the dot recording="true" while it blinks
 * and dim="true" on its off beat. Icons are the "VoiceRecorder" context of voicerecorder.json.
 */
class UISE_DESKTOP_EXPORT VoiceRecorderDialog : public Dialog<AbstractVoiceRecorderDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractVoiceRecorderDialog>;

        explicit VoiceRecorderDialog(QWidget* parent=nullptr);

        ~VoiceRecorderDialog();
        VoiceRecorderDialog(const VoiceRecorderDialog&)=delete;
        VoiceRecorderDialog(VoiceRecorderDialog&&)=delete;
        VoiceRecorderDialog& operator=(const VoiceRecorderDialog&)=delete;
        VoiceRecorderDialog& operator=(VoiceRecorderDialog&&)=delete;

        void construct() override;

        void setState(State state) override;
        State state() const override;

        void setElapsedMs(qint64 ms) override;
        void setPlaybackMs(qint64 ms) override;
        void setWaveform(const QByteArray& waveform) override;

        void setCropRange(qreal start, qreal end) override;
        qreal cropStart() const override;
        qreal cropEnd() const override;

        void setComment(const QString& comment) override;
        QString comment() const override;

        void pointerMoved(const QPoint& globalPos) override;
        void pointerReleased(const QPoint& globalPos) override;
        Target targetAt(const QPoint& globalPos) const override;

        //! Its content has a natural size only.
        bool isResizable() const override
        {
            return false;
        }

    protected:

        void changeEvent(QEvent* event) override;

    private:

        void applyState();
        void updateDuration();
        void setHot(Target target);
        void retranslate();

        std::unique_ptr<VoiceRecorderDialog_p> pimpl;
};

// Max width/height of 0 on purpose, as for the emoji gallery: the content's natural size rules.
using FloatingVoiceRecorderDialogType=FloatingDialog<AbstractVoiceRecorderDialog,VoiceRecorderDialog,0,0>;

/**
 * @brief Floating, draggable host for VoiceRecorderDialog.
 *
 * A named class so a message editor can forward-declare it. Its window type is Qt::Tool, like the
 * emoji gallery it is modelled on: kept above the composer's window by the platform, with no raise()
 * to race. The trade-off is the same one: on macOS a Qt::Tool window hides while the application is
 * not active, which for a recorder means a recording that goes on behind a window that has gone.
 */
class UISE_DESKTOP_EXPORT FloatingVoiceRecorderDialog : public FloatingVoiceRecorderDialogType
{
    Q_OBJECT

    public:

        explicit FloatingVoiceRecorderDialog(QWidget* parent=nullptr);
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_VOICERECORDERDIALOG_HPP
