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

/** @file uise/desktop/textviewer.cpp
*
*  Defines TextViewer.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QPointer>
#include <QShortcut>
#include <QClipboard>
#include <QGuiApplication>
#include <QTextEdit>

#include <uise/desktop/style.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/floatingdialog.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/markdownrenderer.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/textviewer.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! Floor a viewer may be shrunk to once it is open -- small enough to get out of the way, large
//! enough to still be a viewer. See TextViewer::popupSized().
constexpr int ViewerMinWidth=280;
constexpr int ViewerMinHeight=200;

//! Ids of the "..." menu rows.
constexpr int MenuOpenExternal=1;
constexpr int MenuSaveAs=2;
constexpr int MenuAlwaysExternal=3;

//! Icon for the header buttons, in the themed "TextViewer" context.
std::shared_ptr<SvgIcon> headerIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("TextViewer::%1").arg(alias),context);
}

//! Icon for a row of the "..." menu. Separate context from the header's: the menu is a normal
//! themed DropdownFrame popup, whose icons follow light/dark like ChatImageViewerMenu's do.
std::shared_ptr<SvgIcon> menuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("TextViewerMenu::%1").arg(alias),context);
}

/**
 * @brief Wrap `code` in a markdown fence long enough to survive whatever it contains.
 *
 * CommonMark closes a fence only on a run at least as long as the opening one, so a shorter fence
 * around code containing ``` would terminate early and the tail would render as prose.
 */
QString fenceCode(const QString& code, const QString& language)
{
    int longestRun=0;
    int run=0;
    for (const auto ch : code)
    {
        run=(ch==QLatin1Char('`')) ? run+1 : 0;
        longestRun=qMax(longestRun,run);
    }
    const QString fence(qMax(3,longestRun+1),QLatin1Char('`'));

    return fence+language+QStringLiteral("\n")+code+QStringLiteral("\n")+fence;
}

} // anonymous namespace

//--------------------------------------------------------------------------

class TextViewer_p
{
    public:

        enum class Mode : int
        {
            Plain,
            Code,
            Markdown
        };

        TextViewer_p(TextViewer* viewer) : q(viewer)
        {}

        //! Load the current text into the browser according to mode/showSource, and reset the
        //! wrap toggle to what that view wants.
        void render()
        {
            const bool rendered=(mode==Mode::Markdown && !showSource);

            // Everything but rendered markdown is one fenced block. The language only matters for
            // code: markdown source and plain text get an unhighlighted slab (an empty info string
            // attaches no highlighter, see ChatMessageTextBrowser::ensureSyntaxHighlighter()).
            QString html;
            if (rendered)
            {
                html=markdownToHtml(normalized());
            }
            else
            {
                html=markdownToHtml(fenceCode(normalized(),mode==Mode::Code ? language : QString()));
            }

            // A code block inside rendered markdown keeps its Copy strip; a viewer that IS one
            // code block has no use for it -- Copy is in the header.
            browser->setCodeBlockOverlayEnabled(rendered);
            // setHtmlContent(), not setHtml(): it is the entry point that tracks the block,
            // reserves the padding and attaches the highlighter.
            browser->setHtmlContent(html);

            // Rendered prose has to wrap, source does not.
            wrap->setChecked(rendered);
            applyWrap(rendered);

            source->setVisible(mode==Mode::Markdown);
            titleIcon->setSvgIcon(headerIcon(iconAlias(),q));
        }

        QString iconAlias() const
        {
            switch (mode)
            {
                case (Mode::Markdown): return QStringLiteral("fileMarkdown");
                case (Mode::Code): return QStringLiteral("fileCode");
                default: break;
            }
            return QStringLiteral("fileText");
        }

        //! CRLF text would leave a stray '\r' at the end of every line of the slab.
        QString normalized() const
        {
            QString result=text;
            result.replace(QStringLiteral("\r\n"),QStringLiteral("\n"));
            return result;
        }

        void applyWrap(bool on)
        {
            // Viewer mode is NoWrap, and nothing in viewer mode negotiates a wrap width (that is
            // setWrapWidth(), which only a bubble's host calls), so the browser is an ordinary
            // QTextBrowser here and WidgetWidth wraps its prose. Code is a second switch: its
            // lines are unwrappable by default, so without it a code line, or a spaceless run
            // such as a row of asterisks, would still run out of the viewport.
            browser->setLineWrapMode(on ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
            browser->setCodeWrapEnabled(on);
        }

        void copy()
        {
            QGuiApplication::clipboard()->setText(text);
            if (!toast.isNull())
            {
                toast->show(TextViewer::tr("Copied"));
            }
        }

        void updateFullScreenToolTip()
        {
            const bool fullScreen=!frame.isNull() && frame->isFullScreen();
            fullScreenButton->setToolTip(fullScreen ? TextViewer::tr("Exit full screen") : TextViewer::tr("Full screen"));
        }

        IconTextButton* makeButton(const QString& alias, const QString& objectName, const QString& toolTip, QWidget* parent)
        {
            auto* button=new IconTextButton(headerIcon(alias,q),parent,IconTextButton::IconPosition::BeforeText);
            button->setObjectName(objectName);
            button->setText(QString());
            button->setToolTip(toolTip);
            button->setCursor(Qt::PointingHandCursor);
            button->setFocusPolicy(Qt::NoFocus);
            return button;
        }

        TextViewer* q;

        ChatMessageTextBrowser* browser=nullptr;
        QFrame* header=nullptr;
        IconTextButton* titleIcon=nullptr;
        ElidedLabel* title=nullptr;
        Label* subtitle=nullptr;

        IconTextButton* copyButton=nullptr;
        IconTextButton* wrap=nullptr;
        IconTextButton* source=nullptr;
        IconTextButton* fullScreenButton=nullptr;
        IconTextButton* menuButton=nullptr;
        IconTextButton* closeButton=nullptr;
        QPointer<DropdownMenu> menu;

        QPointer<FloatingDialogFrame> frame;
        QPointer<Toast> toast;

        QString text;
        QString language;
        QString titleText;
        Mode mode=Mode::Plain;
        bool showSource=false;
        bool fileActionsVisible=false;
};

//--------------------------------------------------------------------------

TextViewer::TextViewer(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<TextViewer_p>(this))
{
    // The name the light/dark chat.qss backgrounds hang off: FloatingDialogFrame is a translucent
    // top-level window, so content that paints no background of its own shows straight through it.
    setObjectName(QStringLiteral("textViewerFrame"));

    auto l=Layout::vertical(this);

    // --- header ---

    pimpl->header=new QFrame(this);
    pimpl->header->setObjectName(QStringLiteral("textViewerHeader"));
    l->addWidget(pimpl->header);
    auto hl=Layout::horizontal(pimpl->header);

    // Not interactive: a button only for the icon sizing and theming the buttons already have.
    pimpl->titleIcon=new IconTextButton(headerIcon(pimpl->iconAlias(),this),pimpl->header,IconTextButton::IconPosition::BeforeText);
    pimpl->titleIcon->setObjectName(QStringLiteral("titleIcon"));
    pimpl->titleIcon->setText(QString());
    pimpl->titleIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    pimpl->titleIcon->setFocusPolicy(Qt::NoFocus);
    hl->addWidget(pimpl->titleIcon);

    // Title over subtitle rather than side by side: the title elides to whatever width is left,
    // and beside it a size would drift to the far end of that width instead of staying next to a
    // short name.
    auto* textBlock=new QFrame(pimpl->header);
    textBlock->setObjectName(QStringLiteral("textBlock"));
    hl->addWidget(textBlock,1);
    auto tl=Layout::vertical(textBlock);

    pimpl->title=new ElidedLabel(textBlock);
    pimpl->title->setObjectName(QStringLiteral("title"));
    // The middle, so a file name keeps the extension it is recognised by.
    pimpl->title->setElideMode(Qt::ElideMiddle);
    tl->addWidget(pimpl->title);

    pimpl->subtitle=new Label(textBlock);
    pimpl->subtitle->setObjectName(QStringLiteral("subtitle"));
    pimpl->subtitle->setVisible(false);
    tl->addWidget(pimpl->subtitle);

    pimpl->copyButton=pimpl->makeButton(QStringLiteral("copy"),QStringLiteral("copyButton"),tr("Copy"),pimpl->header);
    hl->addWidget(pimpl->copyButton);
    connect(pimpl->copyButton,&IconTextButton::clicked,this,[this](){pimpl->copy();});

    pimpl->wrap=pimpl->makeButton(QStringLiteral("wrap"),QStringLiteral("wrapButton"),tr("Wrap lines"),pimpl->header);
    pimpl->wrap->setCheckable(true);
    hl->addWidget(pimpl->wrap);
    connect(pimpl->wrap,&IconTextButton::toggled,this,[this](bool checked){pimpl->applyWrap(checked);});

    // Markdown only, see TextViewer_p::render().
    pimpl->source=pimpl->makeButton(QStringLiteral("source"),QStringLiteral("sourceButton"),tr("Show source"),pimpl->header);
    pimpl->source->setCheckable(true);
    pimpl->source->setVisible(false);
    hl->addWidget(pimpl->source);
    connect(pimpl->source,&IconTextButton::toggled,this,
        [this](bool checked)
        {
            pimpl->showSource=checked;
            pimpl->source->setToolTip(checked ? tr("Show rendered") : tr("Show source"));
            pimpl->render();
        }
    );

    // Not checkable: the frame's own state is the truth, and a checked look kept in step with it
    // would only be a second copy of it to get out of sync -- Escape or the window manager can end
    // full screen without going through this button. The tooltip carries the state instead.
    pimpl->fullScreenButton=pimpl->makeButton(QStringLiteral("fullScreen"),QStringLiteral("fullScreenButton"),tr("Full screen"),pimpl->header);
    hl->addWidget(pimpl->fullScreenButton);
    connect(pimpl->fullScreenButton,&IconTextButton::clicked,this,[this](){toggleFullScreen();});

    auto* fullScreenShortcut=new QShortcut(Qt::Key_F11,this);
    fullScreenShortcut->setContext(Qt::WindowShortcut);
    connect(fullScreenShortcut,&QShortcut::activated,this,[this](){toggleFullScreen();});

    // --- "..." menu ---

    pimpl->menuButton=pimpl->makeButton(QStringLiteral("menu"),QStringLiteral("menuButton"),tr("More"),pimpl->header);
    pimpl->menuButton->setVisible(false);
    hl->addWidget(pimpl->menuButton);

    // Constructed parentless -- DropdownFrame reparents itself lazily to the trigger's window() on
    // first opening, same recipe as ChatImageViewerControls' own menu button. See ~TextViewer()
    // for the one case where that never happens.
    pimpl->menu=new DropdownMenu();
    pimpl->menu->setCloseOnCheckableActivation(true);
    pimpl->menu->setItems(
        {
            MenuItem(MenuOpenExternal,tr("Open in system app"),menuIcon("openExternal",this)),
            MenuItem(MenuSaveAs,tr("Save as"),menuIcon("saveAs",this)),
            MenuItem::separator(),
            MenuItem::checkable(MenuAlwaysExternal,tr("Always open in system app"))
        }
    );
    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,
        [this](int id)
        {
            if (id==MenuOpenExternal)
            {
                emit openExternallyRequested();
            }
            else if (id==MenuSaveAs)
            {
                emit saveAsRequested();
            }
        }
    );
    connect(pimpl->menu,&DropdownMenu::itemToggled,this,
        [this](int id, bool checked)
        {
            if (id==MenuAlwaysExternal)
            {
                emit alwaysExternalToggled(checked);
            }
        }
    );

    pimpl->closeButton=pimpl->makeButton(QStringLiteral("close"),QStringLiteral("closeButton"),tr("Close"),pimpl->header);
    hl->addWidget(pimpl->closeButton);
    connect(pimpl->closeButton,&IconTextButton::clicked,this,
        [this]()
        {
            if (!pimpl->frame.isNull())
            {
                pimpl->frame->close();
            }
        }
    );

    // --- content ---

    // The SAME widget the bubble renders through, in viewer mode -- not a second browser. That is
    // what makes the expanded text genuinely identical to the bubble's: the syntax highlighting,
    // the painted slab with its padding and radius, messagetext.css and the theme-change replay
    // are all this class's own behaviour, and a plain QTextBrowser has none of them.
    pimpl->browser=new ChatMessageTextBrowser(this);
    pimpl->browser->setObjectName(QStringLiteral("textViewerContent"));
    pimpl->browser->setViewerMode(true);
    // Selectable, with the same right-click Copy the bubble offers.
    pimpl->browser->setCopyable(true);
    l->addWidget(pimpl->browser,1);
}

//--------------------------------------------------------------------------

TextViewer::~TextViewer()
{
    // A menu that was never opened was never reparented, so nothing else owns it. Once it has
    // been opened it is a child of the window and is destroyed with it.
    if (!pimpl->menu.isNull() && pimpl->menu->parent()==nullptr)
    {
        delete pimpl->menu.data();
    }
}

//--------------------------------------------------------------------------

void TextViewer::setCode(const QString& text, const QString& language)
{
    pimpl->text=text;
    pimpl->language=language;
    pimpl->mode=language.isEmpty() ? TextViewer_p::Mode::Plain : TextViewer_p::Mode::Code;
    pimpl->showSource=false;
    // Programmatic reset of a markdown-only control: block its toggled() so it does not re-render.
    pimpl->source->blockSignals(true);
    pimpl->source->setChecked(false);
    pimpl->source->blockSignals(false);
    pimpl->render();
}

//--------------------------------------------------------------------------

void TextViewer::setMarkdown(const QString& text)
{
    pimpl->text=text;
    pimpl->language.clear();
    pimpl->mode=TextViewer_p::Mode::Markdown;
    pimpl->showSource=false;
    pimpl->source->blockSignals(true);
    pimpl->source->setChecked(false);
    pimpl->source->blockSignals(false);
    pimpl->source->setToolTip(tr("Show source"));
    pimpl->render();
}

//--------------------------------------------------------------------------

void TextViewer::setPlainText(const QString& text)
{
    setCode(text,QString());
}

//--------------------------------------------------------------------------

const QString& TextViewer::text() const noexcept
{
    return pimpl->text;
}

//--------------------------------------------------------------------------

ChatMessageTextBrowser* TextViewer::browser() const noexcept
{
    return pimpl->browser;
}

//--------------------------------------------------------------------------

void TextViewer::setTitle(const QString& title)
{
    pimpl->titleText=title;
    pimpl->title->setText(title);
    pimpl->title->setToolTip(title);
    if (!pimpl->frame.isNull())
    {
        pimpl->frame->setWindowTitle(title);
    }
}

//--------------------------------------------------------------------------

void TextViewer::setSubtitle(const QString& subtitle)
{
    pimpl->subtitle->setText(subtitle);
    pimpl->subtitle->setVisible(!subtitle.isEmpty());
}

//--------------------------------------------------------------------------

void TextViewer::setFileActionsVisible(bool visible)
{
    pimpl->fileActionsVisible=visible;
    pimpl->menuButton->setVisible(visible);
}

//--------------------------------------------------------------------------

bool TextViewer::isFileActionsVisible() const noexcept
{
    return pimpl->fileActionsVisible;
}

//--------------------------------------------------------------------------

void TextViewer::setAlwaysExternalText(const QString& text)
{
    if (!pimpl->menu.isNull())
    {
        pimpl->menu->setItemText(MenuAlwaysExternal,text);
    }
}

//--------------------------------------------------------------------------

void TextViewer::setAlwaysExternalChecked(bool checked)
{
    if (!pimpl->menu.isNull())
    {
        // Programmatic: setItemChecked() blocks the row's own signal, so no alwaysExternalToggled()
        // -- the host is the one who knows this state.
        pimpl->menu->setItemChecked(MenuAlwaysExternal,checked);
    }
}

//--------------------------------------------------------------------------

void TextViewer::setToast(Toast* toast)
{
    pimpl->toast=toast;
    pimpl->browser->setToast(toast);
}

//--------------------------------------------------------------------------

void TextViewer::toggleFullScreen()
{
    if (pimpl->frame.isNull())
    {
        return;
    }

    if (pimpl->frame->isFullScreen())
    {
        pimpl->frame->showNormal();
    }
    else
    {
        pimpl->frame->showFullScreen();
    }
    pimpl->updateFullScreenToolTip();
}

//--------------------------------------------------------------------------

QSize TextViewer::expandedSize(const QWidget* anchor, int contentWidth)
{
    const auto* win=(anchor!=nullptr) ? anchor->window() : nullptr;
    // A widget with no window yet (constructed off-screen) has nothing to take a fraction OF --
    // fall back to the fixed size the viewers used to open at rather than to zero.
    const QSize windowSize=(win!=nullptr && win->width()>0 && win->height()>0)
                           ? win->size() : QSize{900,400};

    const int w=qMin(qMax(contentWidth,windowSize.width()/2),windowSize.width());
    const int h=qMin(qMax(400,windowSize.height()/2),windowSize.height());
    return QSize{w,h};
}

//--------------------------------------------------------------------------

void TextViewer::popupSized(QWidget* container, FloatingDialogFrame* frame, const QSize& size)
{
    container->setMinimumSize(size);
    frame->popup();
    container->setMinimumSize(qMin(ViewerMinWidth,size.width()),
                              qMin(ViewerMinHeight,size.height()));
}

//--------------------------------------------------------------------------

FloatingDialogFrame* TextViewer::open(TextViewer* viewer, QWidget* anchor, int contentWidth)
{
    auto* frame=new FloatingDialogFrame(anchor);
    frame->setWidget(viewer,true);
    frame->setAutoCloseOnOutsideClick(true);
    // After setWidget(): it resets the handle to the content's titleBar(), which this has none of.
    frame->setDragHandle(viewer->pimpl->header);
    frame->setWindowTitle(viewer->pimpl->titleText);
    viewer->pimpl->frame=frame;
    viewer->pimpl->updateFullScreenToolTip();

    connect(frame,&FloatingDialogFrame::closed,frame,&QObject::deleteLater);

    // Not frame->resize(): popup() adjustSize()s over it -- see popupSized().
    popupSized(viewer,frame,expandedSize(anchor,contentWidth));
    return frame;
}

//--------------------------------------------------------------------------

}
