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

/** @file uise/desktop/floatingdialog.hpp
*
*  Declares FloatingDialogFrame and FloatingDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLOATINGDIALOG_HPP
#define UISE_DESKTOP_FLOATINGDIALOG_HPP

#include <memory>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/modaldialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class FloatingDialogFrame_p;

/**
 * @brief Non-modal floating host for a dialog widget.
 *
 * Unlike ModalPopup/FrameWithModalPopup, which embed a dialog as a child overlay that covers
 * and blocks its host frame, FloatingDialogFrame is a genuine top-level window: it can be
 * dragged by its content's title bar anywhere on the screen (including past the edges of the
 * host window), stays above the host window while the host is active, and never blocks
 * interaction with the host. Qt::Dialog (rather than the Qt::Tool used by DropdownFrame) keeps
 * it visible when the application loses activation on macOS, which is correct for a dialog
 * meant to stay open across window switches.
 *
 * The frame owns no chrome of its own -- it is a transparent shell around whatever content
 * widget is set via setWidget(), normally an AbstractDialog so the usual title bar, icon,
 * content and buttons chrome (and its QSS) is reused unchanged. Dragging is driven by an event
 * filter installed on the content's titleBar() (see AbstractDialog::titleBar()), or on an
 * explicit setDragHandle() override for non-dialog content.
 */
class UISE_DESKTOP_EXPORT FloatingDialogFrame : public QFrame
{
    Q_OBJECT

    public:

        //! Minimal number of pixels of the frame kept within the screen while dragging.
        constexpr static const int DefaultMinVisibleMargin=40;

        /**
         * @brief Constructor.
         * @param parent Owning widget. The frame is always a top-level window, but keeping a
         *  Qt parent is what makes the window stay above parent->window() and be destroyed
         *  together with it.
         */
        FloatingDialogFrame(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~FloatingDialogFrame();

        FloatingDialogFrame(const FloatingDialogFrame&)=delete;
        FloatingDialogFrame(FloatingDialogFrame&&)=delete;
        FloatingDialogFrame& operator=(const FloatingDialogFrame&)=delete;
        FloatingDialogFrame& operator=(FloatingDialogFrame&&)=delete;

        /**
         * @brief Set content widget.
         * @param widget Widget to show in the frame, reparented to this frame.
         * @param autoDestroy Destroy the previous content widget (if any) and, on close(),
         *  this one too.
         *
         * If widget is an AbstractDialog and no explicit drag handle was set with
         * setDragHandle(), its titleBar() becomes the drag handle.
         */
        void setWidget(QWidget* widget, bool autoDestroy=true);

        /**
         * @brief Get content widget.
         * @return Operation result, may be nullptr.
         */
        QWidget* widget() const;

        /**
         * @brief Detach content widget without destroying it.
         * @return Detached widget, may be nullptr.
         */
        QWidget* takeWidget();

        /**
         * @brief Set an extra, non-owning lifetime anchor for this frame.
         * @param obj Object to watch, or nullptr to clear.
         *
         * In addition to the ordinary Qt parent (whose destruction already destroys this
         * frame), a pseudo parent lets the frame be tied to some other object it is logically
         * attached to -- e.g. a tree node or controller that is not, and cannot be, its Qt
         * parent. When obj is destroyed the frame hides and schedules its own deletion.
         */
        void setPseudoParent(QObject* obj);

        QObject* pseudoParent() const;

        /**
         * @brief Set the widget whose mouse-drag moves the frame.
         * @param handle Drag handle widget, or nullptr to fall back to the content's
         *  AbstractDialog::titleBar() (resolved again on the next setWidget()).
         */
        void setDragHandle(QWidget* handle);

        QWidget* dragHandle() const;

        void setMinVisibleMargin(int val) noexcept;
        int minVisibleMargin() const noexcept;

        /**
         * @brief Enable/disable closing the frame with the Escape key.
         * @param enable Default true.
         */
        void setShortcutEnabled(bool enable);
        bool isShortcutEnabled() const noexcept;

        /**
         * @brief Enable/disable hiding/restoring the frame together with its host window.
         * @param enable Default true.
         */
        void setFollowHostVisibility(bool enable) noexcept;
        bool isFollowHostVisibility() const noexcept;

    signals:

        void moved(const QPoint& globalPos);
        void closed();

    public slots:

        /**
         * @brief Show, raise and activate the frame.
         *
         * The first opening is centered over the parent widget's window (or, if there is no
         * parent, over the screen currently under the cursor). A subsequent popup() after the
         * user has dragged the frame reuses the last position.
         */
        void popup();

        /**
         * @brief Show the frame anchored at a global position.
         * @param globalPos Global position of the frame's top-left corner.
         */
        void popupAt(const QPoint& globalPos);

        /**
         * @brief Close the frame.
         * @param autoDestroy Destroy the content widget set with autoDestroy=true.
         */
        void close(bool autoDestroy=true);

    protected:

        void paintEvent(QPaintEvent* event) override;
        bool eventFilter(QObject* obj, QEvent* event) override;
        void closeEvent(QCloseEvent* event) override;

    private:

        void clampToScreen(QPoint& pos) const;

        std::unique_ptr<FloatingDialogFrame_p> pimpl;
};

constexpr const int FloatingDialogDefaultMaxWidth=500;
constexpr const int FloatingDialogDefaultMaxHeight=500;

/**
 * @brief Floating counterpart of ModalDialog: builds a dialog through the widget factory and
 *  hosts it in a FloatingDialogFrame instead of a ModalPopup.
 *
 * See ModalDialog for the general shape of this API -- the two are meant to be used the same
 * way, the only difference being modal-embedded vs. floating-top-level presentation.
 */
template <typename AbstractDialogT, typename DefaultImplT,
         int DefaultMaxWidthT=FloatingDialogDefaultMaxWidth,
         int DefaultMaxHeightT=FloatingDialogDefaultMaxHeight,
         typename DialogWidgetExtractor=DefaultDialogWidgetExtractor>
class FloatingDialog : public FloatingDialogFrame,
                       public Widget
{
    public:

        FloatingDialog(QWidget* parent=nullptr,
                       int defaultMaxWidth=DefaultMaxWidthT,
                       int defaultMaxHeight=DefaultMaxHeightT
                       )
            : FloatingDialogFrame(parent),
              m_defaultMaxWidth(defaultMaxWidth),
              m_defaultMaxHeight(defaultMaxHeight)
        {}

        QWidget* qWidget() override
        {
            return this;
        }

        void setDialogMaxWidth(int width)
        {
            m_defaultMaxWidth=width;
        }

        void setDialogMaxHeight(int height)
        {
            m_defaultMaxHeight=height;
        }

        /**
         * @brief Open floating dialog.
         * @param destroyOnClose Destroy dialog when it is closed.
         * @param show Show the frame right away.
         * @return Returns true if a new dialog is created, false if it already existed.
         */
        bool openDialog(bool destroyOnClose=true, bool show=true)
        {
            if (m_dialog)
            {
                popup();
                return false;
            }

            m_dialog=makeWidget<AbstractDialogT,DefaultImplT>();

            auto dialogWidget=DialogWidgetExtractor::dialogWidget(m_dialog);

            if (m_defaultMaxWidth>0)
            {
                dialogWidget->setMaximumWidth(m_defaultMaxWidth);
            }
            if (m_defaultMaxHeight>0)
            {
                dialogWidget->setMaximumHeight(m_defaultMaxHeight);
            }

            setWidget(dialogWidget,destroyOnClose);
            connect(
                dialogWidget,
                &AbstractDialog::closeRequested,
                this,
                [this,destroyOnClose]()
                {
                    blockSignals(true);
                    close(destroyOnClose);
                    blockSignals(false);
                }
            );
            connect(
                this,
                &FloatingDialogFrame::closed,
                dialogWidget,
                &AbstractDialog::closeDialog
            );

            if (show)
            {
                popup();
                dialogWidget->setDialogFocus();
            }
            return true;
        }

        QPointer<AbstractDialogT> dialog() const
        {
            return m_dialog;
        }

    private:

        QPointer<AbstractDialogT> m_dialog;

        int m_defaultMaxWidth;
        int m_defaultMaxHeight;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLOATINGDIALOG_HPP
