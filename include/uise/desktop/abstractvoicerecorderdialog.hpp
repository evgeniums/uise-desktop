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

/** @file uise/desktop/abstractvoicerecorderdialog.hpp
*
*  Declares AbstractVoiceRecorderDialog, the interface of the voice message recorder popup.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTVOICERECORDERDIALOG_HPP
#define UISE_DESKTOP_ABSTRACTVOICERECORDERDIALOG_HPP

#include <QByteArray>
#include <QPoint>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractdialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief The popup a message editor opens above its microphone button while a voice message is
 *  being recorded.
 *
 * PRESENTATION ONLY. The dialog records nothing and plays nothing: it shows what the host tells it
 * (the length so far, the waveform, the playback position) and reports what the user did. Wiring it
 * to a recorder, and to a player for the pre-listen, is the host's job.
 *
 * The states, and how the user moves between them:
 *
 *   Held ......... the user holds the mouse button on the microphone button. The dialog shows a
 *                  blinking dot, the length, and two areas to drag onto: "continue" and "cancel".
 *                  Releasing over "continue" pins the dialog (Pinned), over "cancel" cancels, and
 *                  anywhere else -- the microphone button itself -- sends.
 *   Pinned ....... recording goes on with the button let go: dot, length, and Pause, Cancel and
 *                  Send buttons.
 *   Paused ....... after Pause: the dot stops blinking, Pause reads Resume, and a review area
 *                  appears with a Listen button, the waveform (seekable, and with two handles to trim
 *                  the message) and a field for a comment.
 *   Listening .... the paused message is playing: Listen reads Pause, Resume is disabled and the
 *                  length reads "position / total".
 *
 * The dialog moves itself along these edges when the user asks -- so the interface answers at once
 * -- and tells the host with the signals; the host may still overrule it with setState().
 *
 * While the message is being recorded (Held, Pinned) the dialog is not closable, so Escape and the
 * outside click cannot throw a recording away; Cancel is the way out. See AbstractDialog::setClosable().
 *
 * Because a press that starts on the microphone button keeps the mouse grabbed there, the dialog
 * never sees the pointer during Held. The editor that owns the button forwards it with
 * pointerMoved() and pointerReleased().
 */
class UISE_DESKTOP_EXPORT AbstractVoiceRecorderDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        enum class State
        {
            Held,
            Pinned,
            Paused,
            Listening
        };
        Q_ENUM(State)

        //! What the pointer is over during Held.
        enum class Target
        {
            None,
            Continue,
            Cancel
        };
        Q_ENUM(Target)

        using AbstractDialog::AbstractDialog;

        virtual void setState(State state)=0;
        virtual State state() const=0;

        /**
         * @brief Length of the recording so far, in milliseconds. In Listening it is the total the
         *  playback position is shown against.
         */
        virtual void setElapsedMs(qint64 ms)=0;

        //! Position of the pre-listen playback in milliseconds. Moves the waveform's progress.
        virtual void setPlaybackMs(qint64 ms)=0;

        //! The waveform of what has been recorded, one byte 0..255 per bar.
        virtual void setWaveform(const QByteArray& waveform)=0;

        //! Set the trim handles, as fractions of the message. Not reported back through cropChanged().
        virtual void setCropRange(qreal start, qreal end)=0;
        virtual qreal cropStart() const=0;
        virtual qreal cropEnd() const=0;

        //! The comment that goes with the message. Plain text.
        virtual void setComment(const QString& comment)=0;
        virtual QString comment() const=0;

        // ---- forwarded by the editor during Held ------------------------------------------

        //! The pointer moved, to a global position. Highlights the area it is over.
        virtual void pointerMoved(const QPoint& globalPos)=0;

        /**
         * @brief The mouse button was let go, at a global position.
         *
         * Over "continue": the dialog becomes Pinned and pinned() is emitted. Over "cancel":
         * cancelRequested(). Anywhere else: sendRequested(). A no-op unless the state is Held.
         */
        virtual void pointerReleased(const QPoint& globalPos)=0;

        //! Which area a global position is over, for a host that wants to know.
        virtual Target targetAt(const QPoint& globalPos) const=0;

    signals:

        //! Held became Pinned: the user dropped onto "continue". Recording goes on.
        void pinned();

        //! Pinned became Paused. The host pauses the recording.
        void pauseRequested();

        //! Paused became Pinned. The host resumes the recording.
        void resumeRequested();

        //! The user wants the recording thrown away, from the Cancel button or by dropping onto "cancel".
        void cancelRequested();

        /**
         * @brief The user wants the message sent.
         * @param comment The comment, possibly empty.
         * @param cropStart Where the message is to start, 0..1 of what was recorded.
         * @param cropEnd Where it is to end. 0 and 1 mean untrimmed.
         */
        void sendRequested(const QString& comment, qreal cropStart, qreal cropEnd);

        //! Paused became Listening: play the recording from the waveform's position.
        void listenRequested();

        //! Listening became Paused: stop the playback.
        void listenPauseRequested();

        //! The user let go of the waveform at `fraction` (0..1): move the playback there.
        void seekRequested(qreal fraction);

        //! The user dragged a trim handle.
        void cropChanged(qreal start, qreal end);
};

}

#endif // UISE_DESKTOP_ABSTRACTVOICERECORDERDIALOG_HPP
