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

/** @file uise/desktop/chatimageviewerwindow.cpp
*
*/

/****************************************************************************/

#include <QCloseEvent>
#include <QWindowStateChangeEvent>
#include <QShortcut>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QBoxLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/chatimageviewer.hpp>
#include <uise/desktop/chatimageviewerwindow.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class ChatImageViewerWindow_p
{
    public:

        ChatImageViewer* viewer=nullptr;

        ChatImageViewerWindow::OpenMode openMode=ChatImageViewerWindow::OpenMode::FullScreen;
        QSize defaultWindowSize{ChatImageViewerWindow::DefaultWindowWidth,ChatImageViewerWindow::DefaultWindowHeight};

        bool destroyOnClose=true;
        bool closeOnClick=true;

        //! Guards against Escape, viewerClicked() and a native close button all arriving for a
        //! single user action -- see closeEvent().
        bool closing=false;

        //! Window mode is positioned (centered) only the first time popup() opens it, same
        //! rationale as FloatingDialogFrame::hasPosition -- a later popup() should not undo a
        //! manual reposition/resize by the user.
        bool windowPositioned=false;
};

//--------------------------------------------------------------------------

//! Screen to open on: the host window's screen if there is a Qt parent, otherwise whatever
//! screen the cursor is currently on, falling back to the primary screen. Same recipe as
//! FloatingDialogFrame::popup()'s own host/cursor/primary fallback chain.
static QScreen* targetScreen(const ChatImageViewerWindow* self)
{
    auto* hostWindow=self->parentWidget()!=nullptr ? self->parentWidget()->window() : nullptr;
    if (hostWindow!=nullptr)
    {
        auto* screen=hostWindow->screen();
        if (screen!=nullptr)
        {
            return screen;
        }
    }

    auto* screen=QGuiApplication::screenAt(QCursor::pos());
    if (screen==nullptr)
    {
        screen=QGuiApplication::primaryScreen();
    }
    return screen;
}

//--------------------------------------------------------------------------

ChatImageViewerWindow::ChatImageViewerWindow(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ChatImageViewerWindow_p>())
{
    // A genuine top-level window: parent (if any) only ties this window's destruction to the
    // parent's, same rationale as FloatingDialogFrame's own constructor.
    setWindowFlags(Qt::Window);
    setObjectName("chatImageViewerWindow");
    setVisible(false);

    auto* l=Layout::vertical(this);

    pimpl->viewer=new ChatImageViewer(this);
    pimpl->viewer->initWidget(this);
    l->addWidget(pimpl->viewer->qWidget(),1);

    auto* escShortcut=new QShortcut(Qt::Key_Escape,this);
    escShortcut->setContext(Qt::WindowShortcut);
    connect(
        escShortcut,
        &QShortcut::activated,
        this,
        &ChatImageViewerWindow::close
    );

    auto* f11Shortcut=new QShortcut(Qt::Key_F11,this);
    f11Shortcut->setContext(Qt::WindowShortcut);
    connect(
        f11Shortcut,
        &QShortcut::activated,
        this,
        &ChatImageViewerWindow::openModeToggleRequested
    );

    // Covers a programmatic requestClose() (and Escape arriving through the viewer itself, see
    // ImageViewerWidget::keyPressEvent(), in case the shortcut above is ever disabled).
    connect(
        pimpl->viewer,
        &AbstractImageViewer::closeRequested,
        this,
        &ChatImageViewerWindow::close
    );

    connect(
        pimpl->viewer,
        &AbstractImageViewer::viewerClicked,
        this,
        [this]()
        {
            if (pimpl->closeOnClick && pimpl->openMode==OpenMode::FullScreen)
            {
                close();
            }
        }
    );
}

//--------------------------------------------------------------------------

ChatImageViewerWindow::~ChatImageViewerWindow()
{}

//--------------------------------------------------------------------------

ChatImageViewer* ChatImageViewerWindow::viewer() const
{
    return pimpl->viewer;
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::setCaption(const QString& caption)
{
    setWindowTitle(caption);
}

//--------------------------------------------------------------------------

QString ChatImageViewerWindow::caption() const
{
    return windowTitle();
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::setOpenMode(OpenMode mode) noexcept
{
    if (pimpl->openMode==mode)
    {
        return;
    }
    pimpl->openMode=mode;
    emit openModeChanged(mode);
}

//--------------------------------------------------------------------------

ChatImageViewerWindow::OpenMode ChatImageViewerWindow::openMode() const noexcept
{
    return pimpl->openMode;
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::setDefaultWindowSize(const QSize& size) noexcept
{
    pimpl->defaultWindowSize=size;
}

//--------------------------------------------------------------------------

QSize ChatImageViewerWindow::defaultWindowSize() const noexcept
{
    return pimpl->defaultWindowSize;
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::setDestroyOnClose(bool enable) noexcept
{
    pimpl->destroyOnClose=enable;
}

//--------------------------------------------------------------------------

bool ChatImageViewerWindow::isDestroyOnClose() const noexcept
{
    return pimpl->destroyOnClose;
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::setCloseOnClick(bool enable) noexcept
{
    pimpl->closeOnClick=enable;
}

//--------------------------------------------------------------------------

bool ChatImageViewerWindow::isCloseOnClick() const noexcept
{
    return pimpl->closeOnClick;
}

//--------------------------------------------------------------------------

//! Shared by popup()'s own Window branch and changeEvent()'s external-transition catch below --
//! see windowPositioned's own doc comment: applied once, ever, so a manual resize the user
//! already made is never undone regardless of which of the two call sites got there first.
void ChatImageViewerWindow::applyDefaultWindowSizing()
{
    if (pimpl->windowPositioned)
    {
        return;
    }

    resize(pimpl->defaultWindowSize);
    if (auto* screen=targetScreen(this); screen!=nullptr)
    {
        auto avail=screen->availableGeometry();
        move(avail.center()-QPoint(width()/2,height()/2));
    }
    pimpl->windowPositioned=true;
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::popup()
{
    if (pimpl->openMode==OpenMode::FullScreen)
    {
        // Sized/centered before ever entering fullscreen -- not just when opening in Window
        // mode -- so the "normal" geometry Qt remembers to restore to is already correct. That
        // restore is what runs both for a live openModeToggleRequested() switch and for macOS's
        // own native fullscreen-exit ("green traffic light"), which restores directly with no
        // call into this class at all (see changeEvent() below). Without this, the very first
        // restore visibly flashes the window at whatever tiny default geometry it had before
        // ever being shown, then jumps to defaultWindowSize() a frame later -- changeEvent()
        // still fires afterward, but as a no-op once windowPositioned is already set.
        applyDefaultWindowSizing();
        showFullScreen();
    }
    else
    {
        // A live FullScreen -> Window switch (setOpenMode() called on an already-visible
        // window, e.g. from a menu toggle) needs showNormal() first -- show() alone does not
        // clear Qt::WindowFullScreen, so without this the window would stay fullscreen despite
        // openMode() now reporting Window.
        if (isFullScreen())
        {
            showNormal();
        }

        applyDefaultWindowSizing();
        show();
    }

    raise();
    activateWindow();

    auto* viewerWidget=pimpl->viewer->qWidget();
    if (viewerWidget!=nullptr)
    {
        viewerWidget->setFocus();
    }
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::changeEvent(QEvent* event)
{
    QFrame::changeEvent(event);

    if (event->type()!=QEvent::WindowStateChange)
    {
        return;
    }

    // Only a genuine FullScreen<->non-FullScreen transition, not e.g. minimize/restore or the
    // very first show.
    auto* stateEvent=static_cast<QWindowStateChangeEvent*>(event);
    bool wasFullScreen=(stateEvent->oldState() & Qt::WindowFullScreen)!=0;
    bool isFullScreenNow=(windowState() & Qt::WindowFullScreen)!=0;
    if (wasFullScreen==isFullScreenNow)
    {
        return;
    }

    if (isFullScreenNow)
    {
        // Native fullscreen entry (e.g. the same macOS button, going the other way, while
        // already in Window mode) -- fullscreen fills the screen regardless of prior geometry,
        // so no sizing catch-up is needed, only syncing openMode() (via setOpenMode(), which
        // emits openModeChanged()) so a later toggle and any persisted setting both reflect
        // what the window actually is now.
        setOpenMode(OpenMode::FullScreen);
        return;
    }

    // A macOS native fullscreen-exit ("green traffic light") arrives here with no call into
    // popup() at all -- without this, the window is left at whatever "normal" geometry it had
    // before it was ever shown (never actually set, since FullScreen mode's own popup() branch
    // never resizes), which is why it otherwise renders tiny.
    setOpenMode(OpenMode::Window);
    applyDefaultWindowSizing();
}

//--------------------------------------------------------------------------

void ChatImageViewerWindow::closeEvent(QCloseEvent* event)
{
    if (!pimpl->closing)
    {
        pimpl->closing=true;
        emit closed();
        if (pimpl->destroyOnClose)
        {
            // deleteLater(), not destroyWidget() (utils/destroywidget.hpp) -- that helper's
            // setParent(nullptr) is for detaching *content* from a host frame, and would
            // re-create this window's own native handle mid-close, which is not what is wanted
            // for a window destroying itself.
            deleteLater();
        }
    }

    event->accept();
    QFrame::closeEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
