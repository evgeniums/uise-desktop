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

/** @file uise/desktop/loadcontrolmenu.hpp
*
*  Declares LoadControlMenu.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LOADCONTROLMENU_HPP
#define UISE_DESKTOP_LOADCONTROLMENU_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractloadcontrol.hpp>
#include <uise/desktop/dropdownframe.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class LoadControl;
class LoadControlMenu_p;

/**
 * @brief Wraps a plain LoadControl, adding a pause-or-cancel decision menu for its in-progress
 *  states without teaching LoadControl itself anything about that flow.
 *
 * For every other state, a click passes straight through as clicked() -- identical to a bare
 * LoadControl, so a host that never cares about the pause-or-cancel flow can use this as a
 * drop-in replacement for LoadControl and never notice the difference.
 *
 * While state() is Running or Waiting -- a queued transfer is as pausable and cancellable as
 * one already transferring, only without progress to show yet -- a click instead:
 *  1. Emits pauseRequested() immediately -- the transfer is presumed genuinely paused, not just
 *     visually frozen, while the user decides what to do next.
 *  2. Opens a two-item menu anchored to the control: a direction-aware Pause entry ("Pause
 *     downloading"/"Pause sending") and a direction-/filename-aware Cancel entry (see
 *     setFileDescription()) -- neither says just "Pause"/"Cancel" alone, so paired with their
 *     icons they can't later be misread as media-playback controls once the app grows an inline
 *     player for audio/video messages.
 *  3. Picking Cancel emits cancelRequested(). Picking Pause -- or dismissing the menu with
 *     Escape or an outside click -- leaves it paused: there is deliberately no silent-resume
 *     path there, so a dismissal the user didn't direct at the control itself never resumes a
 *     transfer they haven't explicitly asked to continue.
 *  4. Clicking the control again while its own menu is still open is different: that click is a
 *     deliberate interaction with the control, not an incidental dismissal, so it's treated as
 *     an ordinary click on the (by now Paused) control -- emits clicked(), same as a click in
 *     any pass-through state -- letting the host's own click handling decide what it means
 *     (typically resume), exactly like LoadControl's own docs describe for every other state.
 */
class UISE_DESKTOP_EXPORT LoadControlMenu : public QFrame
{
    Q_OBJECT

    public:

        LoadControlMenu(QWidget* parent=nullptr);

        ~LoadControlMenu();

        LoadControlMenu(const LoadControlMenu&)=delete;
        LoadControlMenu(LoadControlMenu&&)=delete;
        LoadControlMenu& operator=(const LoadControlMenu&)=delete;
        LoadControlMenu& operator=(LoadControlMenu&&)=delete;

        /**
         * @brief The wrapped control, for direct access to anything not covered by this class's
         *  own setState()/state()/setProgress() pass-throughs (e.g. progressMode()).
         */
        LoadControl* loadControl() const;

        void setState(AbstractLoadControl::State state);

        AbstractLoadControl::State state() const;

        void setProgress(qreal value);

        template <typename T1, typename T2>
        void setProgress(T1 currentValue, T2 total)
        {
            setProgressImpl(static_cast<qreal>(currentValue),static_cast<qreal>(total));
        }

        /**
         * @brief Set the file name/kind/direction shown in the Pause/Cancel menu entries' text.
         * @param fileName Shown middle-elided if too wide for the menu; empty falls back to a
         *  generic "this image"/"this file" phrase, selected by isImage. Only used by the
         *  Cancel entry -- Pause stays short, it costs nothing to get wrong.
         * @param isImage Selects "this image" vs "this file" for the empty-fileName fallback.
         * @param incoming Direction of the owning message -- selects "Pause/Cancel downloading"
         *  vs "Pause/Cancel sending".
         */
        void setFileDescription(const QString& fileName, bool isImage, bool incoming);

    signals:

        /**
         * @brief Emitted for a click in any state other than Running/Waiting -- identical to
         *  LoadControl's own clicked() signal.
         */
        void clicked();

        void pauseRequested();
        void cancelRequested();

    private:

        void setProgressImpl(qreal currentValue, qreal total);
        void rebuildMenuItems();

    private slots:

        void onLoadControlClicked();
        void onMenuItemTriggered(int id);
        void onMenuCloseRequested(DropdownFrame::CloseReason reason);

    private:

        std::unique_ptr<LoadControlMenu_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LOADCONTROLMENU_HPP
