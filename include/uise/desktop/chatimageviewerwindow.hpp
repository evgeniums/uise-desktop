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

UISE_DESKTOP_NAMESPACE_BEGIN

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

        //! Default OpenMode::FullScreen.
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

    public slots:

        //! Show, raise and activate the window according to openMode().
        void popup();

    protected:

        void closeEvent(QCloseEvent* event) override;

        //! Catches a FullScreen -> non-FullScreen window-state transition arriving from
        //! OUTSIDE popup() -- e.g. macOS's own native fullscreen-exit ("green traffic light")
        //! button, which changes windowState() directly with no call into this class at all.
        //! Without this, such a transition leaves the window at whatever "normal" geometry it
        //! had before it was ever shown (never actually set, since FullScreen mode's own
        //! popup() branch never resizes) -- visually a tiny/default-sized window, unlike the
        //! same transition driven by openModeToggleRequested(), which goes through popup()'s
        //! own sizing. Applies the identical one-time sizing popup()'s Window branch does
        //! (still gated on windowPositioned, so a manual resize the user already made is never
        //! undone) and syncs openMode() so a later toggle reflects what the window actually is.
        void changeEvent(QEvent* event) override;

    private:

        //! One-time (per windowPositioned) resize+center to defaultWindowSize(), shared by
        //! popup()'s own Window branch and changeEvent()'s external-transition catch.
        void applyDefaultWindowSizing();

        std::unique_ptr<ChatImageViewerWindow_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATIMAGEVIEWERWINDOW_HPP
