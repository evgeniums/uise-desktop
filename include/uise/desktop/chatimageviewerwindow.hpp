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

/** @file uise/desktop/chatimageviewerwindow.hpp
*
*  Declares ChatImageViewerWindow.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATIMAGEVIEWERWINDOW_HPP
#define UISE_DESKTOP_CHATIMAGEVIEWERWINDOW_HPP

#include <memory>

#include <QFrame>
#include <QSize>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatImageViewer;
class ChatImageViewerWindow_p;

/**
 * @brief Top-level window host for ChatImageViewer.
 *
 * ChatImageViewer itself has no presentation of its own -- it is a WidgetController that only
 * asks to be closed (AbstractImageViewer::closeRequested() on Escape), leaving a host to place it
 * in a layout and act on the request. ChatImageViewerWindow is that host: it owns a
 * ChatImageViewer, opens fullscreen by default (see OpenMode), and closes itself on Escape or on
 * a plain click landing outside the viewer's controls (see AbstractImageViewer::viewerClicked()).
 *
 * The window creates its viewer in the constructor; use viewer() to load images and connect to
 * the viewer's own signals (saveAsRequested(), goToMessageRequested(), etc.) before popup().
 */
class UISE_DESKTOP_EXPORT ChatImageViewerWindow : public QFrame
{
    Q_OBJECT

    public:

        //! How the window is presented by popup().
        enum class OpenMode : uint8_t
        {
            FullScreen,   //!< Default.
            Window
        };
        Q_ENUM(OpenMode)

        constexpr static const int DefaultWindowWidth=1000;
        constexpr static const int DefaultWindowHeight=700;

        /**
         * @brief Constructor.
         * @param parent Owning widget. The window is always top-level (Qt::Window), but keeping
         *  a Qt parent is what ties its destruction to parent's, same as FloatingDialogFrame.
         */
        explicit ChatImageViewerWindow(QWidget* parent=nullptr);

        ~ChatImageViewerWindow();
        ChatImageViewerWindow(const ChatImageViewerWindow&)=delete;
        ChatImageViewerWindow(ChatImageViewerWindow&&)=delete;
        ChatImageViewerWindow& operator=(const ChatImageViewerWindow&)=delete;
        ChatImageViewerWindow& operator=(ChatImageViewerWindow&&)=delete;

        //! The owned viewer -- already initWidget()-ed, so loadChatImages()/selectImage()/its
        //! signals are usable right after construction.
        ChatImageViewer* viewer() const;

        //! Window title. Default empty, visible only in OpenMode::Window.
        void setCaption(const QString& caption);
        QString caption() const;

        //! Default OpenMode::FullScreen. Emits openModeChanged() when mode actually differs
        //! from the current value -- the single point every path that changes openMode() goes
        //! through (popup()'s own callers, changeEvent()'s external-transition catch below),
        //! so a host only ever needs the one connection to learn about every kind of change.
        void setOpenMode(OpenMode mode) noexcept;
        OpenMode openMode() const noexcept;

        //! Size used the first time popup() opens in OpenMode::Window. Default
        //  {DefaultWindowWidth,DefaultWindowHeight}.
        void setDefaultWindowSize(const QSize& size) noexcept;
        QSize defaultWindowSize() const noexcept;

        //! Destroy this window (deleteLater()) when it closes. Default true.
        void setDestroyOnClose(bool enable) noexcept;
        bool isDestroyOnClose() const noexcept;

        //! Close on a plain click landing outside the viewer's controls. Default true; honoured
        //! only in OpenMode::FullScreen -- an ordinary window has its own close button/chrome.
        void setCloseOnClick(bool enable) noexcept;
        bool isCloseOnClick() const noexcept;

    signals:

        //! Emitted once when the window closes, before any destroyOnClose teardown.
        void closed();

        //! F11 was pressed. This class does not toggle openMode() itself -- unlike Escape/
        //! viewerClicked(), which have one obvious meaning (close), FullScreen<->Window has no
        //! default this widget should impose (e.g. a host may not want the toggle persisted, or
        //! may want to persist it to app settings) -- so a host connects this and calls
        //! setOpenMode()+popup() itself.
        void openModeToggleRequested();

        //! openMode() actually changed -- see setOpenMode()'s own doc comment for every path
        //! that can trigger this, including ones with no call into openModeToggleRequested()
        //! at all (e.g. macOS's native fullscreen enter/exit via changeEvent() below). This is
        //! the one signal a host needs to keep e.g. a persisted "open fullscreen" setting in
        //! sync with reality, regardless of what changed it.
        void openModeChanged(OpenMode mode);

    public slots:

        //! Show, raise and activate the window according to openMode().
        void popup();

    protected:

        void closeEvent(QCloseEvent* event) override;

        //! Catches a FullScreen<->non-FullScreen window-state transition arriving from OUTSIDE
        //! popup() -- e.g. macOS's own native fullscreen enter/exit ("green traffic light")
        //! button, which changes windowState() directly with no call into this class at all.
        //! Both directions sync openMode() (via setOpenMode(), so openModeChanged() fires and
        //! e.g. a persisted setting stays correct regardless of how the mode actually changed)
        //! so a later toggle reflects what the window actually is; the exit direction also
        //! reapplies applyDefaultWindowSizing() -- popup()'s FullScreen branch already applies
        //! it before ever entering fullscreen, precisely so this restore lands on the right
        //! geometry with no visible tiny-then-resize flash, and this keeps that true for a
        //! window whose sizing was still pending for some other reason (e.g. windowPositioned
        //! reset by a future change).
        void changeEvent(QEvent* event) override;

    private:

        //! One-time (per windowPositioned) resize+center to defaultWindowSize(), applied before
        //! the window is ever shown in either mode (see popup()) and reapplied defensively by
        //! changeEvent()'s external-transition catch.
        void applyDefaultWindowSizing();

        std::unique_ptr<ChatImageViewerWindow_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATIMAGEVIEWERWINDOW_HPP
