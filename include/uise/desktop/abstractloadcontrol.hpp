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
            Running
        };

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

    protected:

        virtual void updateState() =0;
        virtual void updateProgress() =0;

    private:

        State m_state=State::CanDownload;
        qreal m_progress=0.0;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTLOADCONTROL_HPP
