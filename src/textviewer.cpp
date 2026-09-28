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

#include <vector>

#include <QFrame>
#include <QPointer>
#include <QShortcut>
#include <QClipboard>
#include <QGuiApplication>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QKeyEvent>
#include <QSignalBlocker>

#include <uise/desktop/style.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/lineedit.hpp>
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

//! Most matches painted at once. A one-letter query in a file near the size limit would otherwise
//! build tens of thousands of selections on every keystroke. Navigation does not use the painted
//! list -- it asks the document for the next match -- so it reaches matches past this too.
constexpr int MaxFindHighlights=5000;

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

        enum class Mode
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

        //! Show or hide the find bar. Opening focuses the input, hands Escape over from the frame to
        //! the bar (the frame's own Escape shortcut closes the whole viewer, and a second enabled
        //! Escape shortcut in the same window would make Qt treat every press as ambiguous), and
        //! searches for whatever the input still holds; closing undoes all of that and removes
        //! every highlight and the selection.
        void setFindBarVisible(bool visible)
        {
            if (visible==findBarShown)
            {
                if (visible)
                {
                    // Cmd/Ctrl+F on an open bar: the usual "take me back to the input".
                    findEdit->setFocus();
                    findEdit->selectAll();
                }
                return;
            }
            findBarShown=visible;

            // The button is only a view of this state -- its own toggled() must not come back here.
            {
                QSignalBlocker blocker(findButton);
                findButton->setChecked(visible);
            }
            findBar->setVisible(visible);

            if (visible)
            {
                if (!frame.isNull())
                {
                    // Remembered rather than assumed true: restore what was there, not what is usual.
                    frameShortcutWasEnabled=frame->isShortcutEnabled();
                    frame->setShortcutEnabled(false);
                }
                findEscape->setEnabled(true);
                findEdit->setFocus();
                findEdit->selectAll();
                runFind();
            }
            else
            {
                findEscape->setEnabled(false);
                if (!frame.isNull())
                {
                    frame->setShortcutEnabled(frameShortcutWasEnabled);
                }
                clearFind();
            }
        }

        //! Remove every highlight and the current match's selection.
        void clearFind()
        {
            browser->setExtraSelections({});

            auto cursor=browser->textCursor();
            cursor.clearSelection();
            browser->setTextCursor(cursor);

            setFindNoMatch(false);
            setFindNavigationEnabled(false);
        }

        void setFindNoMatch(bool noMatch)
        {
            if (findEdit->property("noMatch").toBool()==noMatch)
            {
                return;
            }
            findEdit->setProperty("noMatch",noMatch);
            // A dynamic property change alone does not invalidate Qt's cached style evaluation.
            Style::updateWidgetStyle(findEdit);
        }

        void setFindNavigationEnabled(bool enable)
        {
            findNextButton->setEnabled(enable);
            findPreviousButton->setEnabled(enable);
        }

        //! Make `match` (a cursor with the match selected, as QTextDocument::find() returns it) the
        //! current one: the real selection, in the full selection colour, scrolled into view.
        void selectFindMatch(const QTextCursor& match)
        {
            browser->setTextCursor(match);
            browser->ensureCursorVisible();
            findAnchor=match.selectionStart();
        }

        //! Search for the input's text: paint every match (a QTextEdit has one real selection, so
        //! "all of them" can only be extra selections in a tint of the selection colour) and make
        //! the first one at or after the previous current match the current one -- so extending the
        //! query keeps its place instead of jumping back to the top, wrapping when nothing follows.
        void runFind()
        {
            const auto query=findEdit->text();
            auto* doc=browser->document();
            if (query.isEmpty() || doc==nullptr)
            {
                clearFind();
                return;
            }

            QTextEdit::ExtraSelection highlight;
            auto tint=browser->palette().color(QPalette::Highlight);
            tint.setAlpha(110);
            highlight.format.setBackground(tint);

            QList<QTextEdit::ExtraSelection> selections;
            int from=0;
            while (selections.size()<MaxFindHighlights)
            {
                auto found=doc->find(query,from);
                if (found.isNull())
                {
                    break;
                }
                highlight.cursor=found;
                selections.append(highlight);
                // Past the match, so matches never overlap and the loop always advances.
                from=found.selectionEnd();
            }
            browser->setExtraSelections(selections);

            const int last=qMax(0,doc->characterCount()-1);
            auto current=doc->find(query,qBound(0,findAnchor,last));
            if (current.isNull())
            {
                current=doc->find(query,0);
            }

            const bool found=!current.isNull();
            setFindNoMatch(!found);
            setFindNavigationEnabled(found);
            if (found)
            {
                selectFindMatch(current);
            }
            else
            {
                auto cursor=browser->textCursor();
                cursor.clearSelection();
                browser->setTextCursor(cursor);
            }
        }

        //! Move to the next (or previous) match, wrapping at the ends. Asks the document rather than
        //! walking the painted list, so it works past MaxFindHighlights and from wherever the
        //! selection is now -- including a selection the user made themselves.
        void findStep(bool forward)
        {
            const auto query=findEdit->text();
            auto* doc=browser->document();
            if (query.isEmpty() || doc==nullptr)
            {
                return;
            }

            const auto cursor=browser->textCursor();
            const int last=qMax(0,doc->characterCount()-1);

            QTextCursor next;
            if (forward)
            {
                next=doc->find(query,cursor.hasSelection() ? cursor.selectionEnd() : cursor.position());
                if (next.isNull())
                {
                    next=doc->find(query,0);
                }
            }
            else
            {
                next=doc->find(query,cursor.hasSelection() ? cursor.selectionStart() : cursor.position(),
                               QTextDocument::FindBackward);
                if (next.isNull())
                {
                    next=doc->find(query,last,QTextDocument::FindBackward);
                }
            }

            if (!next.isNull())
            {
                selectFindMatch(next);
            }
        }

        //! (Re)fill the "..." menu from the state above. The "Always ..." row, and the separator
        //! that sets it apart, exist only while it has text: a file with no extension has nothing
        //! for it to refer to, and a per-row hide could not take the separator with it.
        void rebuildMenu()
        {
            std::vector<MenuItem> items{
                MenuItem(MenuOpenExternal,TextViewer::tr("Open in system app"),menuIcon("openExternal",q)),
                MenuItem(MenuSaveAs,TextViewer::tr("Save as"),menuIcon("saveAs",q))
            };
            if (!alwaysText.isEmpty())
            {
                items.push_back(MenuItem::separator());
                items.push_back(MenuItem::checkable(MenuAlwaysExternal,alwaysText,alwaysChecked));
            }
            menu->setItems(std::move(items));
        }

        TextViewer* q;

        ChatMessageTextBrowser* browser=nullptr;
        QFrame* header=nullptr;
        IconTextButton* titleIcon=nullptr;
        ElidedLabel* title=nullptr;
        Label* subtitle=nullptr;

        IconTextButton* findButton=nullptr;
        IconTextButton* copyButton=nullptr;
        IconTextButton* wrap=nullptr;
        IconTextButton* source=nullptr;
        IconTextButton* fullScreenButton=nullptr;
        IconTextButton* menuButton=nullptr;
        IconTextButton* closeButton=nullptr;
        QFrame* findBar=nullptr;
        LineEdit* findEdit=nullptr;
        IconTextButton* findNextButton=nullptr;
        IconTextButton* findPreviousButton=nullptr;
        IconTextButton* findCloseButton=nullptr;
        QShortcut* findEscape=nullptr;
        bool findBarShown=false;
        bool frameShortcutWasEnabled=true;
        int findAnchor=0;

        QPointer<DropdownMenu> menu;
        QString alwaysText;
        bool alwaysChecked=false;

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

    // Checked while the bar is open; the bar is the truth, see setFindBarVisible().
    pimpl->findButton=pimpl->makeButton(QStringLiteral("search"),QStringLiteral("findButton"),tr("Find"),pimpl->header);
    pimpl->findButton->setCheckable(true);
    pimpl->findButton->setRectRipple(true);
    hl->addWidget(pimpl->findButton);
    connect(pimpl->findButton,&IconTextButton::toggled,this,[this](bool checked){pimpl->setFindBarVisible(checked);});

    pimpl->copyButton=pimpl->makeButton(QStringLiteral("copy"),QStringLiteral("copyButton"),tr("Copy"),pimpl->header);
    hl->addWidget(pimpl->copyButton);
    connect(pimpl->copyButton,&IconTextButton::clicked,this,[this](){pimpl->copy();});

    pimpl->wrap=pimpl->makeButton(QStringLiteral("wrap"),QStringLiteral("wrapButton"),tr("Wrap lines"),pimpl->header);
    pimpl->wrap->setCheckable(true);
    pimpl->wrap->setRectRipple(true);
    hl->addWidget(pimpl->wrap);
    connect(pimpl->wrap,&IconTextButton::toggled,this,[this](bool checked){pimpl->applyWrap(checked);});

    // Markdown only, see TextViewer_p::render().
    pimpl->source=pimpl->makeButton(QStringLiteral("source"),QStringLiteral("sourceButton"),tr("Show source"),pimpl->header);
    pimpl->source->setCheckable(true);
    pimpl->source->setRectRipple(true);
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
    pimpl->alwaysText=tr("Always open in system app");
    pimpl->rebuildMenu();
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
                // Kept in step so a later rebuildMenu() does not undo the click.
                pimpl->alwaysChecked=checked;
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

    // --- find bar ---

    // Hidden until Find or Cmd/Ctrl+F. Same shape as the header row: input, then previous (up),
    // next (down), close -- the order browsers and editors put them in, up before down.
    pimpl->findBar=new QFrame(this);
    pimpl->findBar->setObjectName(QStringLiteral("textViewerFindBar"));
    pimpl->findBar->setVisible(false);
    l->addWidget(pimpl->findBar);
    auto fl=Layout::horizontal(pimpl->findBar);

    pimpl->findEdit=new LineEdit(pimpl->findBar);
    pimpl->findEdit->setObjectName(QStringLiteral("findEdit"));
    pimpl->findEdit->setPlaceholderText(tr("Find in text"));
    fl->addWidget(pimpl->findEdit,1);
    // Every keystroke searches -- the matches are painted as the user types.
    connect(pimpl->findEdit,&QLineEdit::textChanged,this,[this](const QString&){pimpl->runFind();});
    // Enter / Shift+Enter, see eventFilter().
    pimpl->findEdit->installEventFilter(this);

    pimpl->findPreviousButton=pimpl->makeButton(QStringLiteral("chevronUp"),QStringLiteral("findPreviousButton"),tr("Previous match"),pimpl->findBar);
    pimpl->findPreviousButton->setEnabled(false);
    fl->addWidget(pimpl->findPreviousButton);
    connect(pimpl->findPreviousButton,&IconTextButton::clicked,this,[this](){pimpl->findStep(false);});

    pimpl->findNextButton=pimpl->makeButton(QStringLiteral("chevronDown"),QStringLiteral("findNextButton"),tr("Next match"),pimpl->findBar);
    pimpl->findNextButton->setEnabled(false);
    fl->addWidget(pimpl->findNextButton);
    connect(pimpl->findNextButton,&IconTextButton::clicked,this,[this](){pimpl->findStep(true);});

    pimpl->findCloseButton=pimpl->makeButton(QStringLiteral("close"),QStringLiteral("findCloseButton"),tr("Close"),pimpl->findBar);
    fl->addWidget(pimpl->findCloseButton);
    connect(pimpl->findCloseButton,&IconTextButton::clicked,this,[this](){pimpl->setFindBarVisible(false);});

    // Cmd+F / Ctrl+F. Window-scoped: the viewer is its own top-level window, and this must work
    // wherever focus is inside it (the browser itself takes none).
    auto* findShortcut=new QShortcut(QKeySequence::Find,this);
    findShortcut->setContext(Qt::WindowShortcut);
    connect(findShortcut,&QShortcut::activated,this,[this](){pimpl->setFindBarVisible(true);});

    // Escape closes the BAR, before it closes the viewer. Created disabled and only enabled while
    // the bar is open, because the frame's own Escape shortcut is switched off for exactly that
    // long (see setFindBarVisible()): two enabled Escape shortcuts in one window fire as
    // "ambiguous" and never as "activated". Both signals go to the same handler, as the frame's
    // own pair does, so a press is not lost if some other enabled Escape shortcut ever joins in.
    pimpl->findEscape=new QShortcut(Qt::Key_Escape,this);
    pimpl->findEscape->setContext(Qt::WindowShortcut);
    pimpl->findEscape->setEnabled(false);
    connect(pimpl->findEscape,&QShortcut::activated,this,[this](){pimpl->setFindBarVisible(false);});
    connect(pimpl->findEscape,&QShortcut::activatedAmbiguously,this,[this](){pimpl->setFindBarVisible(false);});

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

    // New content and the theme-change replay both rebuild the document, which strands every
    // position the search holds. Searching again is what keeps the bar honest.
    connect(pimpl->browser,&ChatMessageTextBrowser::documentRebuilt,this,
        [this]()
        {
            if (pimpl->findBarShown)
            {
                pimpl->runFind();
            }
        }
    );
}

//--------------------------------------------------------------------------

bool TextViewer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->findEdit && event->type()==QEvent::KeyPress)
    {
        const auto* keyEvent=static_cast<QKeyEvent*>(event);
        if (keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter)
        {
            // Swallowed, so the line edit's own returnPressed() never fires for it.
            pimpl->findStep(!(keyEvent->modifiers() & Qt::ShiftModifier));
            return true;
        }
    }

    return QFrame::eventFilter(watched,event);
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
    pimpl->findAnchor=0;
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
    pimpl->findAnchor=0;
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
    pimpl->alwaysText=text;
    if (!pimpl->menu.isNull())
    {
        pimpl->rebuildMenu();
    }
}

//--------------------------------------------------------------------------

void TextViewer::setAlwaysExternalChecked(bool checked)
{
    pimpl->alwaysChecked=checked;
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

QSize TextViewer::expandedSize(const QWidget* anchor, int contentWidth, qreal scale)
{
    const auto* win=(anchor!=nullptr) ? anchor->window() : nullptr;
    // A widget with no window yet (constructed off-screen) has nothing to take a fraction OF --
    // fall back to the fixed size the viewers used to open at rather than to zero.
    const QSize windowSize=(win!=nullptr && win->width()>0 && win->height()>0)
                           ? win->size() : QSize{900,400};

    const int w=qMin(qMax(contentWidth,qRound(scale*windowSize.width()/2)),windowSize.width());
    const int h=qMin(qMax(qRound(scale*400),qRound(scale*windowSize.height()/2)),windowSize.height());
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
    popupSized(viewer,frame,expandedSize(anchor,contentWidth,DefaultScale));
    return frame;
}

//--------------------------------------------------------------------------

}
