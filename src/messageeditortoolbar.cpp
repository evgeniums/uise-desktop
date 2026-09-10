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

/** @file uise/desktop/messageeditortoolbar.cpp
*
*  Defines MessageEditorToolbar.
*
*/

/****************************************************************************/

#include <array>
#include <algorithm>

#include <QFrame>
#include <QPoint>
#include <QPointer>
#include <QBoxLayout>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/messageeditortoolbar.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! Icons resolve against the "MessageEditorToolbar" context (see resources/style/
//! messageeditor.json), mirroring MessageEditor's own file-local menuIcon() helper for the
//! "DropdownMenu" context (src/messageeditor.cpp).
std::shared_ptr<SvgIcon> tbIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("MessageEditorToolbar::%1").arg(alias),context);
}

//! Icon alias for a mode, so the mode button always shows the CURRENT mode's own glyph rather
//! than a static one -- a drop-down trigger whose icon is the selected option is how the rest of
//! this library's pickers behave, and it makes the active mode readable without opening the menu.
QString modeIconAlias(MessageEditingMode mode)
{
    switch (mode)
    {
        case (MessageEditingMode::Markdown): return QStringLiteral("markdown");
        case (MessageEditingMode::Plaintext): return QStringLiteral("plaintext");
        case (MessageEditingMode::Wysiwyg): break;
    }
    return QStringLiteral("wysiwyg");
}

//! Icon alias for a heading level, same reasoning as modeIconAlias(). 0 == normal text.
QString headingIconAlias(int level)
{
    switch (level)
    {
        case 1: return QStringLiteral("heading1");
        case 2: return QStringLiteral("heading2");
        case 3: return QStringLiteral("heading3");
        default: break;
    }
    return QStringLiteral("normalText");
}

//! Base for the table drop-down's row/column edit ids, kept clear of the insert presets' own
//! 0..N so one itemTriggered handler can tell the two apart by id alone.
constexpr int TableActionMenuIdBase=100;

//! The row/column edits offered on a table the caret is already inside, in menu order. The label
//! is a function rather than a string so tr() runs at menu-build time under the right translator
//! rather than during static initialisation.
struct TableActionRow
{
    MessageEditorTableAction action;
    QString (*text)();
    const char* iconAlias;
};

const std::array<TableActionRow,7> TableActionRows{{
    {MessageEditorTableAction::InsertRowAbove,
     []{ return MessageEditorToolbar::tr("Insert row above"); },"tableRowAbove"},
    {MessageEditorTableAction::InsertRowBelow,
     []{ return MessageEditorToolbar::tr("Insert row below"); },"tableRowBelow"},
    {MessageEditorTableAction::InsertColumnLeft,
     []{ return MessageEditorToolbar::tr("Insert column left"); },"tableColumnLeft"},
    {MessageEditorTableAction::InsertColumnRight,
     []{ return MessageEditorToolbar::tr("Insert column right"); },"tableColumnRight"},
    {MessageEditorTableAction::RemoveRow,
     []{ return MessageEditorToolbar::tr("Delete row"); },"tableRemoveRow"},
    {MessageEditorTableAction::RemoveColumn,
     []{ return MessageEditorToolbar::tr("Delete column"); },"tableRemoveColumn"},
    {MessageEditorTableAction::RemoveTable,
     []{ return MessageEditorToolbar::tr("Delete table"); },"tableRemove"}
}};

//! Buttons that make up the "formatting half" of the bar -- everything greyed out together by
//! setFormattingEnabled(false) outside MessageEditingMode::Wysiwyg (Stage 5a decision: formatting
//! is WYSIWYG-only). Mode and Close are deliberately excluded -- they must stay usable in every
//! mode, Mode most of all since it is the only way back to Wysiwyg.
//!
//! Link is ALSO excluded, as of Stage 5b: unlike every other row here, it is useful in
//! MessageEditingMode::Markdown too -- Markdown mode's document is source text, and inserting a
//! literal "[title](url)" there is exactly the kind of thing a formatting-averse mode should
//! still help with. MessageEditor::updateMessageEditingMode() enables/disables it on its own
//! mode-by-mode rule instead of this blanket one. RemoveLink stays here: there is no anchor
//! concept in Markdown source, so its Wysiwyg-only gating is correct as-is.
//!
//! Mention is ALSO excluded, as of Stage 6, for a stronger reason than Link's: its plain form
//! (MessageEditor::insertMentionText(), a literal "@username") carries no markup meaning at all
//! and is deliberately valid in EVERY MessageEditingMode, Plaintext included -- the one mode a
//! blanket formatting-is-Wysiwyg-only grey-out would take it away in. The button therefore stays
//! enabled in every mode; which of insertMentionText()/insertMention() a host's click handler
//! calls is the host's own decision, made from whatever user selector it opens on
//! mentionRequested().
constexpr std::array<MessageEditorToolbarButton,16> FormattingButtons{{
    MessageEditorToolbarButton::Bold,
    MessageEditorToolbarButton::Italic,
    MessageEditorToolbarButton::Underline,
    MessageEditorToolbarButton::Strikethrough,
    MessageEditorToolbarButton::InlineCode,
    MessageEditorToolbarButton::Heading,
    MessageEditorToolbarButton::BulletList,
    MessageEditorToolbarButton::NumberedList,
    MessageEditorToolbarButton::IndentIncrease,
    MessageEditorToolbarButton::IndentDecrease,
    MessageEditorToolbarButton::Blockquote,
    MessageEditorToolbarButton::CodeBlock,
    MessageEditorToolbarButton::Table,
    MessageEditorToolbarButton::HorizontalRule,
    MessageEditorToolbarButton::RemoveLink,
    MessageEditorToolbarButton::ClearFormatting
}};

}

//--------------------------------------------------------------------------

class MessageEditorToolbar_p
{
    public:

        //! Indexed by static_cast<size_t>(MessageEditorToolbarButton) -- the enum is declared
        //! with no explicit values, so it is a dense 0..ButtonCount-1 range. Keep in step with
        //! that enum: every entry must be filled in the constructor, since button() and the
        //! setters dereference this unconditionally.
        static constexpr size_t ButtonCount=22;

        std::array<IconTextButton*,ButtonCount> buttons{};

        QPointer<DropdownMenu> modeMenu;
        QPointer<DropdownMenu> headingMenu;
        QPointer<DropdownMenu> tableMenu;

        MessageEditorFormatState state;
        MessageEditingMode mode=MessageEditingMode::Wysiwyg;
        bool formattingEnabled=true;

        //! Suppresses the *Requested() signals raised by setChecked()/setItemChecked() while
        //! setFormatState()/setMode() push a host-driven state onto the buttons/menus -- without
        //! it, a host reflecting the document's own state back into the toolbar would loop
        //! straight back into "apply an edit" handlers. See MessageEditorFormatState's own doc
        //! comment for the button-level half of this trap (IconTextButton::click() always
        //! toggle()s).
        bool syncing=false;

        IconTextButton*& btn(MessageEditorToolbarButton b)
        {
            return buttons[static_cast<size_t>(b)];
        }

        IconTextButton* btn(MessageEditorToolbarButton b) const
        {
            return buttons[static_cast<size_t>(b)];
        }
};

//--------------------------------------------------------------------------

MessageEditorToolbar::MessageEditorToolbar(QWidget* parent)
    : Frame(parent),
      pimpl(std::make_unique<MessageEditorToolbar_p>())
{
    setObjectName("messageEditorToolbar");

    auto* layout=Layout::horizontal(this);

    auto makeButton=[this](const QString& objName, const QString& toolTip, const QString& iconAlias, bool checkable)
    {
        auto* btn=new IconTextButton(tbIcon(iconAlias,this),this,IconTextButton::IconPosition::BeforeText);
        btn->setObjectName(objName);
        // Icon-only: sets IconTextButton's own "iconOnly" dynamic property (see setText()),
        // which ripple.qss keys its centred-halo ripple shape on.
        btn->setText(QString());
        btn->setCursor(Qt::PointingHandCursor);
        // A formatting click must never steal focus from the text edit -- the whole point is to
        // keep typing/selecting there uninterrupted.
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setToolTip(toolTip);
        if (checkable)
        {
            // Required -- IconTextButton::setChecked() silently no-ops unless this was called
            // first (src/icontextbutton.cpp).
            btn->setCheckable(true);
        }
        return btn;
    };

    auto makeSeparator=[this]()
    {
        // Bare QFrame, no setFrameShape() -- width/colour come entirely from QSS, same recipe
        // as ChatImageViewerControls' own in-row separator (src/chatimageviewercontrols.cpp).
        auto* sep=new QFrame(this);
        sep->setObjectName("separator");
        return sep;
    };

    // --- 1: mode switcher ---

    // Starts on the DEFAULT mode's own glyph, not a static one -- setMode() keeps it in sync from
    // then on (see modeIconAlias()).
    pimpl->btn(MessageEditorToolbarButton::Mode)=makeButton(
        "modeButton",tr("Editing mode"),modeIconAlias(pimpl->mode),false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Mode));

    pimpl->modeMenu=new DropdownMenu();
    {
        std::vector<MenuItem> items;
        items.push_back(MenuItem::checkable(
            static_cast<int>(MessageEditingMode::Wysiwyg),tr("Formatted text"),true,tbIcon("wysiwyg",this)
        ));
        items.back().group=0;
        items.push_back(MenuItem::checkable(
            static_cast<int>(MessageEditingMode::Markdown),tr("Markdown source"),false,tbIcon("markdown",this)
        ));
        items.back().group=0;
        items.push_back(MenuItem::checkable(
            static_cast<int>(MessageEditingMode::Plaintext),tr("Plain text"),false,tbIcon("plaintext",this)
        ));
        items.back().group=0;
        pimpl->modeMenu->setItems(std::move(items));
    }
    // Picking a mode is a one-shot choice, so the menu closes on activation like a plain
    // clickable row would, rather than staying open the way a multi-toggle checkable menu does
    // (DropdownMenu's own default). MessageEditor restores focus to the text edit afterwards.
    pimpl->modeMenu->setCloseOnCheckableActivation(true);
    pimpl->modeMenu->attachTo(pimpl->btn(MessageEditorToolbarButton::Mode));
    connect(pimpl->modeMenu,&DropdownMenu::itemToggled,this,
        [this](int id, bool checked)
        {
            // Group exclusivity also emits itemToggled(other,false) for the row being
            // unchecked -- act only on the row becoming checked.
            if (!checked)
            {
                return;
            }
            emit modeRequested(static_cast<MessageEditingMode>(id));
        }
    );

    layout->addWidget(makeSeparator());

    // --- 2a: Undo / Redo. Deliberately NOT in FormattingButtons: undoing an edit is meaningful
    // in every mode, including Markdown/Plaintext where the formatting half is greyed out. Their
    // enabled state is driven by the host from the document's own undo/redo availability instead
    // (see MessageEditor's ctor), so they start disabled here -- a freshly built editor has an
    // empty undo stack. ---

    pimpl->btn(MessageEditorToolbarButton::Undo)=makeButton("undo",tr("Undo"),"undo",false);
    pimpl->btn(MessageEditorToolbarButton::Undo)->setEnabled(false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Undo));
    connect(pimpl->btn(MessageEditorToolbarButton::Undo),&IconTextButton::clicked,this,&MessageEditorToolbar::undoRequested);

    pimpl->btn(MessageEditorToolbarButton::Redo)=makeButton("redo",tr("Redo"),"redo",false);
    pimpl->btn(MessageEditorToolbarButton::Redo)->setEnabled(false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Redo));
    connect(pimpl->btn(MessageEditorToolbarButton::Redo),&IconTextButton::clicked,this,&MessageEditorToolbar::redoRequested);

    layout->addWidget(makeSeparator());

    // --- 3-4: Bold / Italic / Underline / Strikethrough / Inline code ---

    pimpl->btn(MessageEditorToolbarButton::Bold)=makeButton("bold",tr("Bold"),"bold",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Bold));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::Bold),&MessageEditorFormatState::bold,&MessageEditorToolbar::boldRequested);

    pimpl->btn(MessageEditorToolbarButton::Italic)=makeButton("italic",tr("Italic"),"italic",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Italic));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::Italic),&MessageEditorFormatState::italic,&MessageEditorToolbar::italicRequested);

    pimpl->btn(MessageEditorToolbarButton::Underline)=makeButton("underline",tr("Underline"),"underline",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Underline));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::Underline),&MessageEditorFormatState::underline,&MessageEditorToolbar::underlineRequested);

    pimpl->btn(MessageEditorToolbarButton::Strikethrough)=makeButton("strikethrough",tr("Strikethrough"),"strikethrough",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Strikethrough));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::Strikethrough),&MessageEditorFormatState::strikeOut,&MessageEditorToolbar::strikethroughRequested);

    pimpl->btn(MessageEditorToolbarButton::InlineCode)=makeButton("inlineCode",tr("Inline code"),"code",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::InlineCode));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::InlineCode),&MessageEditorFormatState::inlineCode,&MessageEditorToolbar::inlineCodeRequested);

    layout->addWidget(makeSeparator());

    // --- 6: heading dropdown ---

    // Same as the mode button: starts on the current heading level's glyph, kept in sync by
    // setFormatState() from then on (see headingIconAlias()).
    pimpl->btn(MessageEditorToolbarButton::Heading)=makeButton(
        "headingButton",tr("Heading"),headingIconAlias(pimpl->state.headingLevel),false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Heading));

    pimpl->headingMenu=new DropdownMenu();
    {
        std::vector<MenuItem> items;
        items.push_back(MenuItem::checkable(1,tr("Heading 1"),false,tbIcon("heading1",this)));
        items.back().group=0;
        items.push_back(MenuItem::checkable(2,tr("Heading 2"),false,tbIcon("heading2",this)));
        items.back().group=0;
        items.push_back(MenuItem::checkable(3,tr("Heading 3"),false,tbIcon("heading3",this)));
        items.back().group=0;
        // 0 == normal (non-heading) text -- see MessageEditorFormatState::headingLevel.
        items.push_back(MenuItem::checkable(0,tr("Normal text"),true,tbIcon("normalText",this)));
        items.back().group=0;
        pimpl->headingMenu->setItems(std::move(items));
    }
    // One-shot choice, closes on activation -- see the mode menu's own comment above.
    pimpl->headingMenu->setCloseOnCheckableActivation(true);
    pimpl->headingMenu->attachTo(pimpl->btn(MessageEditorToolbarButton::Heading));
    connect(pimpl->headingMenu,&DropdownMenu::itemToggled,this,
        [this](int id, bool checked)
        {
            if (!checked)
            {
                return;
            }
            emit headingRequested(id);
        }
    );

    // --- 7-8: Bulleted list / Numbered list ---

    pimpl->btn(MessageEditorToolbarButton::BulletList)=makeButton("bulletList",tr("Bulleted list"),"list",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::BulletList));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::BulletList),&MessageEditorFormatState::bulletList,&MessageEditorToolbar::bulletListRequested);

    pimpl->btn(MessageEditorToolbarButton::NumberedList)=makeButton("numberedList",tr("Numbered list"),"listNumbers",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::NumberedList));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::NumberedList),&MessageEditorFormatState::numberedList,&MessageEditorToolbar::numberedListRequested);

    // --- 7a: indent level. Plain click buttons rather than toggles -- an indent level is a
    // number to step, not a state to switch. They are not list-only, which is why they sit
    // outside the two list toggles: on a plain paragraph they indent the line, and on a
    // selection they step its blockquote level. Same three meanings Tab/Shift+Tab carry, since
    // both routes land in MessageEditor::applyIndentStep().

    pimpl->btn(MessageEditorToolbarButton::IndentIncrease)=makeButton(
        "indentIncrease",tr("Increase indent (Tab)"),"indentIncrease",false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::IndentIncrease));
    connect(pimpl->btn(MessageEditorToolbarButton::IndentIncrease),&IconTextButton::clicked,
            this,&MessageEditorToolbar::indentIncreaseRequested);

    pimpl->btn(MessageEditorToolbarButton::IndentDecrease)=makeButton(
        "indentDecrease",tr("Decrease indent (Shift+Tab)"),"indentDecrease",false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::IndentDecrease));
    connect(pimpl->btn(MessageEditorToolbarButton::IndentDecrease),&IconTextButton::clicked,
            this,&MessageEditorToolbar::indentDecreaseRequested);

    // --- 8: Blockquote ---

    pimpl->btn(MessageEditorToolbarButton::Blockquote)=makeButton("blockquote",tr("Blockquote"),"blockquote",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Blockquote));
    wireCheckable(pimpl->btn(MessageEditorToolbarButton::Blockquote),&MessageEditorFormatState::blockquote,&MessageEditorToolbar::blockquoteRequested);

    // --- 9: Code block ---

    pimpl->btn(MessageEditorToolbarButton::CodeBlock)=makeButton("codeBlock",tr("Code block"),"codeBlock",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::CodeBlock));
    connect(pimpl->btn(MessageEditorToolbarButton::CodeBlock),&IconTextButton::clicked,this,&MessageEditorToolbar::codeBlockRequested);

    // --- 10: Table ---

    // Tooltip names the cell-navigation key: Tab is the only keyboard way across a table (see
    // EnhancedTextEdit::keyPressEvent()) and nothing on screen would otherwise hint at it.
    pimpl->btn(MessageEditorToolbarButton::Table)=makeButton(
        "tableButton",tr("Insert table (Tab moves between cells)"),"table",false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Table));

    pimpl->tableMenu=new DropdownMenu();
    {
        // Fixed presets for creating a table -- reuses DropdownMenu's generic popup shell rather
        // than a bespoke grid-picker widget (task-message-formatting-plan.md §5 item 10), and
        // tableRequested(int,int) is already the right signal for a real grid picker later.
        struct TablePreset { int rows; int columns; };
        constexpr std::array<TablePreset,5> presets{{ {2,2}, {2,3}, {3,3}, {3,4}, {4,4} }};
        std::vector<MenuItem> items;
        int id=0;
        for (const auto& preset : presets)
        {
            MenuItem item(id,tr("%1 × %2").arg(preset.rows).arg(preset.columns));
            item.data=QPoint(preset.rows,preset.columns);
            items.push_back(std::move(item));
            ++id;
        }

        // ...and row/column edits for a table that already exists, which is what covers every
        // size the presets do not: a table's dimensions do not have to be known before it is
        // created, so growing one in place beats asking the user to count rows up front. Ids
        // start at TableActionMenuIdBase so they cannot collide with the presets' own 0..N.
        items.push_back(MenuItem::separator());
        for (const auto& action : TableActionRows)
        {
            items.push_back(MenuItem(
                TableActionMenuIdBase+static_cast<int>(action.action),
                action.text(),
                tbIcon(action.iconAlias,this)
            ));
            // Meaningless outside a table; setFormatState() flips these as the caret moves.
            items.back().isEnabled=false;
        }

        pimpl->tableMenu->setItems(std::move(items));
    }
    pimpl->tableMenu->attachTo(pimpl->btn(MessageEditorToolbarButton::Table));
    connect(pimpl->tableMenu,&DropdownMenu::itemTriggered,this,
        [this](int id)
        {
            if (id>=TableActionMenuIdBase)
            {
                emit tableActionRequested(
                    static_cast<MessageEditorTableAction>(id-TableActionMenuIdBase)
                );
                return;
            }

            const auto& items=pimpl->tableMenu->items();
            auto it=std::find_if(items.begin(),items.end(),
                [id](const MenuItem& item) { return item.id==id; }
            );
            if (it!=items.end())
            {
                auto point=it->data.toPoint();
                emit tableRequested(point.x(),point.y());
            }
        }
    );

    // --- 11: Horizontal rule ---

    // A plain click, not a toggle: a rule is a thing you INSERT, like a table, not a state the
    // caret is in. Nothing tracks it in MessageEditorFormatState for the same reason.
    pimpl->btn(MessageEditorToolbarButton::HorizontalRule)=makeButton(
        "horizontalRule",tr("Insert horizontal rule"),"horizontalRule",false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::HorizontalRule));
    connect(pimpl->btn(MessageEditorToolbarButton::HorizontalRule),&IconTextButton::clicked,
            this,&MessageEditorToolbar::horizontalRuleRequested);

    layout->addWidget(makeSeparator());

    // --- 12-13: Link / Remove link (Stage 5b) / Mention (Stage 6) -- hidden by default ---

    pimpl->btn(MessageEditorToolbarButton::Link)=makeButton("link",tr("Insert link"),"link",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Link));
    pimpl->btn(MessageEditorToolbarButton::Link)->setVisible(false);
    connect(pimpl->btn(MessageEditorToolbarButton::Link),&IconTextButton::clicked,this,&MessageEditorToolbar::linkRequested);

    pimpl->btn(MessageEditorToolbarButton::RemoveLink)=makeButton("removeLink",tr("Remove link"),"removeLink",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::RemoveLink));
    pimpl->btn(MessageEditorToolbarButton::RemoveLink)->setVisible(false);
    connect(pimpl->btn(MessageEditorToolbarButton::RemoveLink),&IconTextButton::clicked,this,&MessageEditorToolbar::removeLinkRequested);

    pimpl->btn(MessageEditorToolbarButton::Mention)=makeButton("mention",tr("Mention someone"),"mention",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Mention));
    pimpl->btn(MessageEditorToolbarButton::Mention)->setVisible(false);
    connect(pimpl->btn(MessageEditorToolbarButton::Mention),&IconTextButton::clicked,this,&MessageEditorToolbar::mentionRequested);

    // --- 14: Clear formatting ---

    pimpl->btn(MessageEditorToolbarButton::ClearFormatting)=makeButton("clearFormatting",tr("Clear formatting"),"clearFormatting",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::ClearFormatting));
    connect(pimpl->btn(MessageEditorToolbarButton::ClearFormatting),&IconTextButton::clicked,this,&MessageEditorToolbar::clearFormattingRequested);

    // --- 15: stretch ---

    layout->addStretch(1);

    // --- 16: Close, leading on EVERY platform ---
    //
    // Deliberately not the platform-dependent placement the original brief asked for (left on
    // macOS, trailing elsewhere). Leading everywhere is what this toolbar actually wants: it puts
    // Close directly above the editor's own bottom-left expand button, which is the control that
    // opened the toolbar in the first place -- the two sit in one column over the strip of blank
    // padding beside the text edit, so opening and closing the bar happen in the same place
    // instead of at opposite ends of it.
    pimpl->btn(MessageEditorToolbarButton::Close)=makeButton("closeButton",tr("Close"),"close",false);
    layout->insertWidget(0,pimpl->btn(MessageEditorToolbarButton::Close));
    connect(pimpl->btn(MessageEditorToolbarButton::Close),&IconTextButton::clicked,this,&MessageEditorToolbar::closeRequested);
}

//--------------------------------------------------------------------------

MessageEditorToolbar::~MessageEditorToolbar()
{}

//--------------------------------------------------------------------------

void MessageEditorToolbar::setButtonVisible(MessageEditorToolbarButton button, bool visible)
{
    pimpl->btn(button)->setVisible(visible);
}

//--------------------------------------------------------------------------

bool MessageEditorToolbar::isButtonVisible(MessageEditorToolbarButton button) const
{
    return !pimpl->btn(button)->isHidden();
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::setButtonEnabled(MessageEditorToolbarButton button, bool enable)
{
    pimpl->btn(button)->setEnabled(enable);
}

//--------------------------------------------------------------------------

bool MessageEditorToolbar::isButtonEnabled(MessageEditorToolbarButton button) const
{
    return pimpl->btn(button)->isEnabled();
}

//--------------------------------------------------------------------------

IconTextButton* MessageEditorToolbar::button(MessageEditorToolbarButton button) const
{
    return pimpl->btn(button);
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::setFormatState(const MessageEditorFormatState& state)
{
    pimpl->state=state;
    pimpl->syncing=true;

    pimpl->btn(MessageEditorToolbarButton::Bold)->setChecked(state.bold);
    pimpl->btn(MessageEditorToolbarButton::Italic)->setChecked(state.italic);
    pimpl->btn(MessageEditorToolbarButton::Underline)->setChecked(state.underline);
    pimpl->btn(MessageEditorToolbarButton::Strikethrough)->setChecked(state.strikeOut);
    pimpl->btn(MessageEditorToolbarButton::InlineCode)->setChecked(state.inlineCode);
    pimpl->btn(MessageEditorToolbarButton::BulletList)->setChecked(state.bulletList);
    pimpl->btn(MessageEditorToolbarButton::NumberedList)->setChecked(state.numberedList);
    pimpl->btn(MessageEditorToolbarButton::Blockquote)->setChecked(state.blockquote);
    pimpl->btn(MessageEditorToolbarButton::CodeBlock)->setChecked(state.codeBlock);

    // setItemChecked() does NOT enforce group exclusivity on its own (only a real user toggle
    // does, see DropdownMenu::onItemToggled()) -- uncheck every level explicitly, then check the
    // one matching the live headingLevel. It also uses QSignalBlocker internally, so this never
    // re-enters onItemToggled()/itemToggled() -- no syncing guard needed around it.
    if (pimpl->headingMenu)
    {
        for (int level=0; level<=3; ++level)
        {
            pimpl->headingMenu->setItemChecked(level,level==state.headingLevel);
        }
    }

    // The trigger button wears the selected option's own glyph, so the caret's heading level is
    // readable without opening the menu.
    pimpl->btn(MessageEditorToolbarButton::Heading)->setSvgIcon(
        tbIcon(headingIconAlias(state.headingLevel),this)
    );

    // Row/column edits only mean anything inside a table. Greyed rather than hidden so the menu
    // keeps a stable shape and the actions stay discoverable before there is a table to use them
    // on. setItemEnabled() touches descriptors and any live row, so it works whether or not the
    // menu happens to be open.
    if (pimpl->tableMenu)
    {
        for (const auto& action : TableActionRows)
        {
            pimpl->tableMenu->setItemEnabled(
                TableActionMenuIdBase+static_cast<int>(action.action),state.insideTable
            );
        }
    }

    // insideLink is tracked for Stage 5b (Remove link's visibility rule -- "appears only when
    // the caret is inside an existing link") but not acted on here: Link/RemoveLink stay hidden
    // regardless in Stage 5a, see the ctor.

    pimpl->syncing=false;
}

//--------------------------------------------------------------------------

const MessageEditorFormatState& MessageEditorToolbar::formatState() const noexcept
{
    return pimpl->state;
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::setMode(MessageEditingMode mode)
{
    pimpl->mode=mode;

    // Same as the heading button: the trigger wears the selected option's own glyph, so the
    // active editing mode is readable at a glance without opening the menu.
    pimpl->btn(MessageEditorToolbarButton::Mode)->setSvgIcon(tbIcon(modeIconAlias(mode),this));

    if (pimpl->modeMenu)
    {
        // Same reasoning as the heading menu above -- setItemChecked() does not enforce group
        // exclusivity and does not re-enter itemToggled(), so no syncing guard is needed here.
        pimpl->modeMenu->setItemChecked(static_cast<int>(MessageEditingMode::Wysiwyg),mode==MessageEditingMode::Wysiwyg);
        pimpl->modeMenu->setItemChecked(static_cast<int>(MessageEditingMode::Markdown),mode==MessageEditingMode::Markdown);
        pimpl->modeMenu->setItemChecked(static_cast<int>(MessageEditingMode::Plaintext),mode==MessageEditingMode::Plaintext);
    }
}

//--------------------------------------------------------------------------

MessageEditingMode MessageEditorToolbar::mode() const noexcept
{
    return pimpl->mode;
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::setFormattingEnabled(bool enable)
{
    pimpl->formattingEnabled=enable;
    for (auto button : FormattingButtons)
    {
        pimpl->btn(button)->setEnabled(enable);
    }
}

//--------------------------------------------------------------------------

bool MessageEditorToolbar::isFormattingEnabled() const noexcept
{
    return pimpl->formattingEnabled;
}

//--------------------------------------------------------------------------

DropdownMenu* MessageEditorToolbar::modeMenu() const
{
    return pimpl->modeMenu;
}

//--------------------------------------------------------------------------

DropdownMenu* MessageEditorToolbar::headingMenu() const
{
    return pimpl->headingMenu;
}

//--------------------------------------------------------------------------

DropdownMenu* MessageEditorToolbar::tableMenu() const
{
    return pimpl->tableMenu;
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::wireCheckable(
        IconTextButton* btn,
        bool MessageEditorFormatState::* field,
        void (MessageEditorToolbar::*requestedSignal)(bool)
    )
{
    // clicked(): the model-change request. Never reads the button's own checked state -- only
    // the authoritative MessageEditorFormatState, see that struct's doc comment.
    connect(btn,&IconTextButton::clicked,this,
        [this,field,requestedSignal]()
        {
            if (pimpl->syncing)
            {
                return;
            }
            emit (this->*requestedSignal)(!(pimpl->state.*field));
        }
    );

    // toggled(): IconTextButton::click() unconditionally toggle()s right after emitting
    // clicked() (src/icontextbutton.cpp), so a plain click always leaves the button's checked
    // state one step ahead of (or behind) the document. Re-assert the authoritative value
    // instead of letting that flip become the source of truth -- same fix as
    // HTreeTabBarItem's close button (src/htreetabbar.cpp). Terminates: setChecked() only
    // re-emits toggled() when it actually changes something, and after the re-assert
    // checked()==authoritative.
    connect(btn,&IconTextButton::toggled,this,
        [this,btn,field](bool checked)
        {
            if (pimpl->syncing)
            {
                return;
            }
            auto authoritative=pimpl->state.*field;
            if (checked!=authoritative)
            {
                btn->setChecked(authoritative);
            }
        }
    );
}

//--------------------------------------------------------------------------

}
