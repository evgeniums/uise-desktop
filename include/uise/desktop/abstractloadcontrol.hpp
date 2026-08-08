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
 *  state icon (download/upload/pause/wait/stop), used wherever a chat message item's content is
 *  not available locally yet.
 *
 * There is deliberately only a single clicked() signal, no separate cancel()/retry()/start() --
 * the owner inspects state() in its clicked() handler and decides what a click means; this keeps
 * the control itself free of any transfer-direction or queue knowledge.
 */
class UISE_DESKTOP_EXPORT AbstractLoadControl : public QFrame
{
    Q_OBJECT

    public:

        enum class State
        {
            None,
            CanDownload,
            CanUpload,
            Paused,
            Waiting,
            Running,
            Complete,    //!< This item's own transfer is done while sibling items in the same
                         //!< message are not -- once every item is done, the caller hides the
                         //!< load control entirely rather than leaving it in this state.
            Failed       //!< Transfer failed; distinct from Paused (a user-initiated pause),
                         //!< shown with a distinct error icon/color -- see LoadControl.
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

        State m_state=State::CanDownload;
        qreal m_progress=0.0;
        ProgressMode m_progressMode=ProgressMode::Static;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTLOADCONTROL_HPP
