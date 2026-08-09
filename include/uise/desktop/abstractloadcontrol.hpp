/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/abstractloadcontrol.hpp
*
*  Declares AbstractLoadControl.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTLOADCONTROL_HPP
#define UISE_DESKTOP_ABSTRACTLOADCONTROL_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Interface of a click-to-transfer control showing a circular progress arc around a
 *  state icon (download/upload/running/wait/stop), used wherever a chat message item's content
 *  is not available locally yet.
 *
 * There is deliberately only a single clicked() signal, no separate cancel()/retry()/start() --
 * the owner inspects state() in its clicked() handler and decides what a click means; this keeps
 * the control itself free of any transfer-direction or queue knowledge. A host that wants a
 * click on Running to open a pause-or-cancel decision menu rather than act immediately should
 * wrap this control in LoadControlMenu instead of teaching it that flow directly.
 */
class UISE_DESKTOP_EXPORT AbstractLoadControl : public QFrame
{
    Q_OBJECT

    public:

        enum class State
        {
            None,
            Download,    //!< Click starts (or, if progress()>0, continues) a download -- a
                         //!< deliberately paused-by-the-user transfer maps here too, not to a
                         //!< separate "Resume" state: the click means the same thing either way,
                         //!< and progress() alone (0 vs partial) already tells the two apart
                         //!< visually, without a play-triangle icon that could later be confused
                         //!< with actual media playback.
            Upload,      //!< Same as Download, mirrored for the opposite direction.
            Running,     //!< An active transfer -- deliberately shown with a plain, non-media-
                         //!< transport icon (X) rather than pause bars, since what a click here
                         //!< does is entirely up to the host: LoadControlMenu turns it into a
                         //!< pause-or-cancel decision menu; a simpler host could just pause
                         //!< outright, or map it to Stop/Cancel instead if not pausable at all.
            Waiting,
            Complete,    //!< This item's own transfer is done while sibling items in the same
                         //!< message are not -- once every item is done, the caller hides the
                         //!< load control entirely rather than leaving it in this state.
            Failed,      //!< Transfer failed; distinct from a deliberately paused transfer --
                         //!< see Download/Upload -- shown with a distinct error icon/color, see
                         //!< LoadControl.
            Cancelled,   //!< Transfer cancelled by the user; deliberately distinct from Failed --
                         //!< nothing broke, so this gets a neutral icon, not the error styling,
                         //!< and no retry affordance.
            Cancel,      //!< An active operation whose only available action is to cancel it
                         //!< outright (X icon) -- the Running alternative for a host whose
                         //!< transfer can't be paused at all. Not wired to any
                         //!< ChatFileTransferState yet. Distinct from Cancelled's own "already
                         //!< cancelled" terminal state.
            Stop         //!< An active operation whose only available action is to stop it
                         //!< (player-stop icon) -- another Running alternative, for a host that
                         //!< distinguishes "stop" from "cancel". Also not wired yet.
        };
        Q_ENUM(State)

        //! Mode of drawing the progress arc.
        enum class ProgressMode
        {
            Static,             //!< Arc starts at 12 o'clock, span grows with progress() (default).
            Indeterminate,      //!< Fixed-span arc circulating around the circle; progress() ignored.
            AnimatedProgress    //!< Span follows progress(), but the start angle circulates.
        };
        Q_ENUM(ProgressMode)

        using QFrame::QFrame;

        void setState(State state)
        {
            auto changed=(m_state!=state);
            m_state=state;
            updateState();
            if (changed)
            {
                emit stateChanged(m_state);
            }
        }

        State state() const noexcept
        {
            return m_state;
        }

        void setProgressMode(ProgressMode mode)
        {
            auto changed=(m_progressMode!=mode);
            m_progressMode=mode;
            updateProgressMode();
            if (changed)
            {
                emit progressModeChanged(m_progressMode);
            }
        }

        ProgressMode progressMode() const noexcept
        {
            return m_progressMode;
        }

        void setProgress(qreal value)
        {
            m_progress=value;
            updateProgress();
        }

        template <typename T1, typename T2>
        void setProgress(T1 currentvalue, T2 total)
        {
            if (qFuzzyIsNull(static_cast<qreal>(total)))
            {
                setProgress(0);
                return;
            }
            if (currentvalue>total)
            {
                setProgress(100.0);
                return;
            }
            auto progress=100.0*static_cast<qreal>(currentvalue)/static_cast<qreal>(total);
            setProgress(progress);
        }

        qreal progress() const noexcept
        {
            return m_progress;
        }

    signals:

        void clicked();

        void stateChanged(State state);

        void progressModeChanged(ProgressMode mode);

    protected:

        virtual void updateState() =0;
        virtual void updateProgress() =0;
        virtual void updateProgressMode() =0;

    private:

        State m_state=State::Download;
        qreal m_progress=0.0;
        ProgressMode m_progressMode=ProgressMode::Static;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTLOADCONTROL_HPP
