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
#include <optional>

#include <QFrame>
#include <QPoint>
#include <QPointer>
#include <QBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>

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
//!
//! SpellCheck is excluded too, task-spellcheck.md, for exactly Mention's reason: a misspelling is
//! a misspelling in Markdown source and in Plaintext exactly as much as in Wysiwyg, so the
//! blanket formatting-is-Wysiwyg-only grey-out would take the feature away in the two modes whose
//! content is pure prose -- precisely where it matters most.
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

//! Order in which buttons leave the bar for the overflow menu when the toolbar is too narrow to
//! hold all of them -- FIRST entry is demoted FIRST, i.e. the list runs from "cheapest to lose" to
//! "last thing standing". Close and Mode are absent on purpose and can NEVER be demoted: Close is
//! the only way to dismiss the bar, and Mode is the only way back to MessageEditingMode::Wysiwyg
//! (the same two exclusions FormattingButtons makes above, for the same reason) -- so the bar's
//! floor is always just those two plus the overflow trigger itself (see relayout()).
//!
//! Deliberately a FIXED order rather than a mode-aware one (e.g. "demote whatever
//! FormattingButtons would grey out first"): reshuffling the bar under the user's cursor on a mode
//! switch would be worse than a stable-but-imperfect order. Within that constraint:
//!  - SpellCheck/Mention go first -- both are host-OPTIONAL (hidden unless the host opts in) and
//!    each duplicates a path that exists without the button (an in-text "@" trigger for Mention;
//!    SpellCheck is a once-per-session toggle, not a per-caret action -- see
//!    MessageEditorFormatState::spellCheckEnabled's own doc comment).
//!  - ClearFormatting/HorizontalRule/Table/CodeBlock/Blockquote are rare, one-shot block/insert
//!    actions -- Table and Heading additionally own a DropdownMenu each, so demoting them into a
//!    SUBMENU is a better home for their extra rows than an 18px trigger anyway.
//!  - IndentIncrease/IndentDecrease advertise their own keyboard equivalent right in their tooltip
//!    ("Increase indent (Tab)" / "Decrease indent (Shift+Tab)", see the ctor).
//!  - RemoveLink is host-visible only while the caret already sits inside a link (see
//!    MessageEditor::syncToolbarState()), so it is absent from the bar most of the time regardless.
//!  - Undo/Redo OUTLIVE the character-format toggles even though they precede them on the bar: per
//!    the ctor's own comment, they stay meaningful in EVERY MessageEditingMode, so keeping a
//!    greyed-out Underline on the bar while hiding a live Undo would be backwards.
//!  - Bold is the last to go: the single most-used formatting control.
constexpr std::array<MessageEditorToolbarButton,21> DemotionOrder{{
    MessageEditorToolbarButton::SpellCheck,
    MessageEditorToolbarButton::Mention,
    MessageEditorToolbarButton::ClearFormatting,
    MessageEditorToolbarButton::HorizontalRule,
    MessageEditorToolbarButton::Table,
    MessageEditorToolbarButton::CodeBlock,
    MessageEditorToolbarButton::Blockquote,
    MessageEditorToolbarButton::IndentDecrease,
    MessageEditorToolbarButton::IndentIncrease,
    MessageEditorToolbarButton::RemoveLink,
    MessageEditorToolbarButton::Link,
    MessageEditorToolbarButton::NumberedList,
    MessageEditorToolbarButton::BulletList,
    MessageEditorToolbarButton::Heading,
    MessageEditorToolbarButton::InlineCode,
    MessageEditorToolbarButton::Strikethrough,
    MessageEditorToolbarButton::Underline,
    MessageEditorToolbarButton::Redo,
    MessageEditorToolbarButton::Undo,
    MessageEditorToolbarButton::Italic,
    MessageEditorToolbarButton::Bold
}};

//! Id bases for the overflow menu's own tree. DropdownMenu requires ids to be unique across the
//! WHOLE tree, submenus included (see MenuItem::submenu()'s own doc comment, dropdownmenu.hpp) --
//! and the standalone Mode/Heading/Table menus built below all reuse small ids among themselves
//! (mode 0..2, heading 0..3, table presets 0..4, table actions TableActionMenuIdBase+0..6), which
//! would collide the moment they become siblings under one overflow root. Each group below gets
//! its own decade instead, sized generously past what it will ever hold, and every dispatch below
//! reads the button/mode/level/preset/action back out of the id by subtracting the matching base
//! -- see rebuildOverflowMenu()/onOverflowTriggered()/onOverflowToggled(). Separators need no base
//! of their own: MenuItem::separator() always carries id=-1 (see its own factory) and is never
//! looked up by id.
constexpr int OverflowActionIdBase=1000;        // + static_cast<int>(MessageEditorToolbarButton)
constexpr int OverflowSubmenuIdBase=1100;       // + static_cast<int>(MessageEditorToolbarButton) -- the Mode/Heading/Table parent rows
constexpr int OverflowModeIdBase=1200;          // + static_cast<int>(MessageEditingMode)
constexpr int OverflowHeadingIdBase=1300;       // + heading level 0..3
constexpr int OverflowTableIdBase=1400;         // + preset index, see TablePresets below
constexpr int OverflowTableActionIdBase=1500;   // + static_cast<int>(MessageEditorTableAction)

//! (button, format-state field, request signal) triples for every CHECKABLE formatting action --
//! shared by wireCheckable()'s call sites in the ctor (via wireCheckableAction()) and by
//! onOverflowToggled(), so the pairing is written exactly once. CodeBlock is deliberately absent:
//! it is built NON-checkable (see the ctor), even though setFormatState() still calls setChecked()
//! on it for symmetry with the rest of MessageEditorFormatState -- a pre-existing silent no-op,
//! since IconTextButton::setChecked() requires setCheckable(true) first -- so CodeBlock's overflow
//! row mirrors the button as a plain clickable, not a member of this table.
struct CheckableAction
{
    MessageEditorToolbarButton button;
    bool MessageEditorFormatState::* field;
    void (MessageEditorToolbar::*requestedSignal)(bool);
};

constexpr std::array<CheckableAction,9> CheckableActions{{
    {MessageEditorToolbarButton::Bold,&MessageEditorFormatState::bold,&MessageEditorToolbar::boldRequested},
    {MessageEditorToolbarButton::Italic,&MessageEditorFormatState::italic,&MessageEditorToolbar::italicRequested},
    {MessageEditorToolbarButton::Underline,&MessageEditorFormatState::underline,&MessageEditorToolbar::underlineRequested},
    {MessageEditorToolbarButton::Strikethrough,&MessageEditorFormatState::strikeOut,&MessageEditorToolbar::strikethroughRequested},
    {MessageEditorToolbarButton::InlineCode,&MessageEditorFormatState::inlineCode,&MessageEditorToolbar::inlineCodeRequested},
    {MessageEditorToolbarButton::BulletList,&MessageEditorFormatState::bulletList,&MessageEditorToolbar::bulletListRequested},
    {MessageEditorToolbarButton::NumberedList,&MessageEditorFormatState::numberedList,&MessageEditorToolbar::numberedListRequested},
    {MessageEditorToolbarButton::Blockquote,&MessageEditorFormatState::blockquote,&MessageEditorToolbar::blockquoteRequested},
    {MessageEditorToolbarButton::SpellCheck,&MessageEditorFormatState::spellCheckEnabled,&MessageEditorToolbar::spellCheckRequested}
}};

//! Looks up a button's CheckableAction row, or nullptr if it is not a checkable action (every
//! non-checkable button, plus CodeBlock -- see CheckableActions' own doc comment).
const CheckableAction* checkableActionFor(MessageEditorToolbarButton button)
{
    for (const auto& action : CheckableActions)
    {
        if (action.button==button)
        {
            return &action;
        }
    }
    return nullptr;
}

//! Builds the three checkable mode rows -- used both for MessageEditorToolbar's own standalone
//! mode DropdownMenu (the ctor) and, with idBase shifted into OverflowModeIdBase's range, for the
//! overflow menu's Mode submenu (rebuildOverflowMenu()) -- so the two can never read differently.
//! Checked state seeds from currentMode at build time; setMode() re-asserts it afterwards via
//! setItemChecked() on whichever menu(s) are actually live.
std::vector<MenuItem> buildModeMenuItems(QWidget* context, int idBase, MessageEditingMode currentMode)
{
    std::vector<MenuItem> items;
    auto add=[&](MessageEditingMode mode, const QString& text, const QString& iconAlias)
    {
        items.push_back(MenuItem::checkable(
            idBase+static_cast<int>(mode),text,mode==currentMode,tbIcon(iconAlias,context)
        ));
        items.back().group=0;
    };
    add(MessageEditingMode::Wysiwyg,MessageEditorToolbar::tr("Formatted text"),QStringLiteral("wysiwyg"));
    add(MessageEditingMode::Markdown,MessageEditorToolbar::tr("Markdown source"),QStringLiteral("markdown"));
    add(MessageEditingMode::Plaintext,MessageEditorToolbar::tr("Plain text"),QStringLiteral("plaintext"));
    return items;
}

//! Same reasoning as buildModeMenuItems() -- shared by the standalone heading DropdownMenu (ctor)
//! and the overflow menu's Heading submenu (rebuildOverflowMenu()). 0 == normal (non-heading) text.
std::vector<MenuItem> buildHeadingMenuItems(QWidget* context, int idBase, int currentLevel)
{
    std::vector<MenuItem> items;
    auto add=[&](int level, const QString& text, const QString& iconAlias)
    {
        items.push_back(MenuItem::checkable(
            idBase+level,text,level==currentLevel,tbIcon(iconAlias,context)
        ));
        items.back().group=0;
    };
    add(1,MessageEditorToolbar::tr("Heading 1"),QStringLiteral("heading1"));
    add(2,MessageEditorToolbar::tr("Heading 2"),QStringLiteral("heading2"));
    add(3,MessageEditorToolbar::tr("Heading 3"),QStringLiteral("heading3"));
    add(0,MessageEditorToolbar::tr("Normal text"),QStringLiteral("normalText"));
    return items;
}

//! Fixed size presets for creating a table -- shared by the standalone table DropdownMenu (ctor)
//! and the overflow menu's Table submenu (rebuildOverflowMenu()). Presets are assigned ids
//! [presetIdBase, presetIdBase+TablePresets.size()) in table order, so a handler recovers the
//! (rows,columns) pair from the id alone -- see onOverflowTriggered() -- without walking
//! item.data the way the ctor's own standalone-menu handler still does.
struct TablePreset { int rows; int columns; };
constexpr std::array<TablePreset,5> TablePresets{{ {2,2}, {2,3}, {3,3}, {3,4}, {4,4} }};

//! Row/column edit ids run [actionIdBase, actionIdBase+TableActionRows.size()), in TableActionRows
//! order, mirroring TablePresets' own scheme above.
std::vector<MenuItem> buildTableMenuItems(QWidget* context, int presetIdBase, int actionIdBase, bool insideTable)
{
    std::vector<MenuItem> items;
    int id=presetIdBase;
    for (const auto& preset : TablePresets)
    {
        MenuItem item(id,MessageEditorToolbar::tr("%1 × %2").arg(preset.rows).arg(preset.columns));
        item.data=QPoint(preset.rows,preset.columns);
        items.push_back(std::move(item));
        ++id;
    }
    items.push_back(MenuItem::separator());
    for (const auto& action : TableActionRows)
    {
        items.push_back(MenuItem(
            actionIdBase+static_cast<int>(action.action),
            action.text(),
            tbIcon(action.iconAlias,context)
        ));
        // Meaningless outside a table; setFormatState()/syncOverflowState() flip these as the
        // caret moves.
        items.back().isEnabled=insideTable;
    }
    return items;
}

}

//--------------------------------------------------------------------------

class MessageEditorToolbar_p
{
    public:

        //! Indexed by static_cast<size_t>(MessageEditorToolbarButton) -- the enum is declared
        //! with no explicit values, so it is a dense 0..ButtonCount-1 range. Keep in step with
        //! that enum: every entry must be filled in the constructor, since button() and the
        //! setters dereference this unconditionally.
        static constexpr size_t ButtonCount=23;

        std::array<IconTextButton*,ButtonCount> buttons{};

        QPointer<DropdownMenu> modeMenu;
        QPointer<DropdownMenu> headingMenu;
        QPointer<DropdownMenu> tableMenu;

        //! Overflow collapse (see MessageEditorToolbar's own class doc comment). Chrome, not a
        //! host-addressable action -- deliberately not a MessageEditorToolbarButton entry, so it
        //! never has to be special-cased by setFormattingEnabled()/setFormatState() the way every
        //! real button in `buttons` above does.
        IconTextButton* overflowButton=nullptr;
        QPointer<DropdownMenu> overflowMenu;

        //! The HOST's intent for each button, written only by setButtonVisible() and read back by
        //! isButtonVisible() -- deliberately NOT the widget's own isHidden(): a button can also be
        //! hidden because it did not FIT (see demoted below), and the two reasons must not
        //! clobber each other. A Link demoted into the overflow menu must still come back to the
        //! bar on widening; a RemoveLink the host hid because the caret left a link must never
        //! appear in the overflow menu regardless of width. Effective on-screen visibility is
        //! always hostVisible[b] && !demoted[b] -- see applyButtonVisibility().
        std::array<bool,ButtonCount> hostVisible{};

        //! Buttons currently living in the overflow menu instead of on the bar. Recomputed from
        //! scratch by relayout() as a pure function of the bar's width -- never patched
        //! incrementally -- which is what makes the result stable rather than oscillating; see
        //! relayout()'s own doc comment.
        std::array<bool,ButtonCount> demoted{};

        //! Re-entrancy guard for relayout(): every setVisible() call below invalidates the parent
        //! layout, which POSTS a QEvent::LayoutRequest that could otherwise land back in
        //! resizeEvent()/showEvent() while a pass is still committing its own result (the same
        //! deferred-LayoutRequest trap documented at src/messageeditor.cpp and
        //! src/fileuploadwidget.cpp). relayout() early-outs on an unchanged result regardless, so
        //! this flag exists only to keep a nested pass off the call stack entirely.
        bool relayouting=false;

        MessageEditorFormatState state;
        MessageEditingMode mode=MessageEditingMode::Wysiwyg;
        bool formattingEnabled=true;

        //! Suppresses the *Requested() signals raised by setChecked()/setItemChecked() while
        //! setFormatState()/setMode() push a host-driven state onto the buttons/menus -- without
        //! it, a host reflecting the document's own state back into the toolbar would loop
        //! straight back into "apply an edit" handlers. See MessageEditorFormatState's own doc
        //! comment for the button-level half of this trap (IconTextButton::click() always
        //! toggle()s). Now guards THREE surfaces rather than two: the buttons themselves, the
        //! pre-existing standalone menus, and the overflow menu's own mirrored rows (see
        //! onOverflowToggled()).
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

    // Every button starts host-visible; the four that ship hidden (Link/RemoveLink/Mention/
    // SpellCheck, below) flip this individually as they are built. Overflow demotion (the
    // `demoted` array) starts all-false via MessageEditorToolbar_p's own {}-initialisation --
    // nothing is demoted until the first relayout() actually measures the bar.
    pimpl->hostVisible.fill(true);

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

    // Looks a button up in CheckableActions and forwards to wireCheckable() -- so that table is
    // the ONE place the (field,requestedSignal) pairing is written; onOverflowToggled() looks up
    // the exact same table for the exact same buttons. Every call site below passes a button that
    // IS in CheckableActions; silently doing nothing for one that is not (there is currently none
    // -- see that table's own doc comment on CodeBlock, its one near-miss) is safer for a widget
    // constructor than throwing.
    auto wireCheckableAction=[this](MessageEditorToolbarButton button)
    {
        if (const auto* action=checkableActionFor(button))
        {
            wireCheckable(pimpl->btn(button),action->field,action->requestedSignal);
        }
    };

    // --- 1: mode switcher ---

    // Starts on the DEFAULT mode's own glyph, not a static one -- setMode() keeps it in sync from
    // then on (see modeIconAlias()).
    pimpl->btn(MessageEditorToolbarButton::Mode)=makeButton(
        "modeButton",tr("Editing mode"),modeIconAlias(pimpl->mode),false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Mode));

    pimpl->modeMenu=new DropdownMenu();
    // buildModeMenuItems() is shared with the overflow menu's own Mode submenu
    // (rebuildOverflowMenu()), with idBase 0 here so ids match MessageEditingMode's own values --
    // exactly what this menu's itemToggled handler below already assumes.
    pimpl->modeMenu->setItems(buildModeMenuItems(this,0,pimpl->mode));
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
    wireCheckableAction(MessageEditorToolbarButton::Bold);

    pimpl->btn(MessageEditorToolbarButton::Italic)=makeButton("italic",tr("Italic"),"italic",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Italic));
    wireCheckableAction(MessageEditorToolbarButton::Italic);

    pimpl->btn(MessageEditorToolbarButton::Underline)=makeButton("underline",tr("Underline"),"underline",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Underline));
    wireCheckableAction(MessageEditorToolbarButton::Underline);

    pimpl->btn(MessageEditorToolbarButton::Strikethrough)=makeButton("strikethrough",tr("Strikethrough"),"strikethrough",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Strikethrough));
    wireCheckableAction(MessageEditorToolbarButton::Strikethrough);

    pimpl->btn(MessageEditorToolbarButton::InlineCode)=makeButton("inlineCode",tr("Inline code"),"code",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::InlineCode));
    wireCheckableAction(MessageEditorToolbarButton::InlineCode);

    layout->addWidget(makeSeparator());

    // --- 6: heading dropdown ---

    // Same as the mode button: starts on the current heading level's glyph, kept in sync by
    // setFormatState() from then on (see headingIconAlias()).
    pimpl->btn(MessageEditorToolbarButton::Heading)=makeButton(
        "headingButton",tr("Heading"),headingIconAlias(pimpl->state.headingLevel),false
    );
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Heading));

    pimpl->headingMenu=new DropdownMenu();
    // buildHeadingMenuItems() is shared with the overflow menu's own Heading submenu
    // (rebuildOverflowMenu()), with idBase 0 here so ids match the raw heading level -- exactly
    // what this menu's itemToggled handler below already assumes.
    pimpl->headingMenu->setItems(buildHeadingMenuItems(this,0,pimpl->state.headingLevel));
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
    wireCheckableAction(MessageEditorToolbarButton::BulletList);

    pimpl->btn(MessageEditorToolbarButton::NumberedList)=makeButton("numberedList",tr("Numbered list"),"listNumbers",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::NumberedList));
    wireCheckableAction(MessageEditorToolbarButton::NumberedList);

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
    wireCheckableAction(MessageEditorToolbarButton::Blockquote);

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
    // buildTableMenuItems() is shared with the overflow menu's own Table submenu
    // (rebuildOverflowMenu()) -- presets reuse DropdownMenu's generic popup shell rather than a
    // bespoke grid-picker widget (task-message-formatting-plan.md §5 item 10), and
    // tableRequested(int,int) is already the right signal for a real grid picker later. presetIdBase
    // 0 / actionIdBase TableActionMenuIdBase here so ids match what this menu's itemTriggered
    // handler below already assumes; insideTable starts false, same as the caret starting outside
    // any table.
    pimpl->tableMenu->setItems(buildTableMenuItems(this,0,TableActionMenuIdBase,false));
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
    //
    // Hidden here by writing hostVisible[] directly and calling applyButtonVisibility() --
    // NOT setButtonVisible() -- because setButtonVisible() also calls relayout(), which reads
    // pimpl->overflowButton/overflowMenu, and neither exists yet at this point in the ctor (they
    // are built after the stretch, further down). relayout() is called exactly once, at the very
    // end of the ctor, once the whole bar (overflow button and menu included) actually exists.

    pimpl->btn(MessageEditorToolbarButton::Link)=makeButton("link",tr("Insert link"),"link",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Link));
    pimpl->hostVisible[static_cast<size_t>(MessageEditorToolbarButton::Link)]=false;
    applyButtonVisibility(MessageEditorToolbarButton::Link);
    connect(pimpl->btn(MessageEditorToolbarButton::Link),&IconTextButton::clicked,this,&MessageEditorToolbar::linkRequested);

    pimpl->btn(MessageEditorToolbarButton::RemoveLink)=makeButton("removeLink",tr("Remove link"),"removeLink",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::RemoveLink));
    pimpl->hostVisible[static_cast<size_t>(MessageEditorToolbarButton::RemoveLink)]=false;
    applyButtonVisibility(MessageEditorToolbarButton::RemoveLink);
    connect(pimpl->btn(MessageEditorToolbarButton::RemoveLink),&IconTextButton::clicked,this,&MessageEditorToolbar::removeLinkRequested);

    pimpl->btn(MessageEditorToolbarButton::Mention)=makeButton("mention",tr("Mention someone"),"mention",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::Mention));
    pimpl->hostVisible[static_cast<size_t>(MessageEditorToolbarButton::Mention)]=false;
    applyButtonVisibility(MessageEditorToolbarButton::Mention);
    connect(pimpl->btn(MessageEditorToolbarButton::Mention),&IconTextButton::clicked,this,&MessageEditorToolbar::mentionRequested);

    // --- 13a: Check spelling (task-spellcheck.md) -- hidden by default, same arrangement as
    // Link/RemoveLink/Mention above: a checker with no dictionary behind it does nothing (the
    // editor ships none, see AbstractSpellChecker's own doc comment), so a host opts in via
    // AbstractMessageEditor::setSpellCheckButtonVisible(true) once it has one. Checkable, and
    // driven by MessageEditorFormatState::spellCheckEnabled like the formatting toggles above --
    // see that field's own doc comment for why this one is editor-wide rather than caret state.

    pimpl->btn(MessageEditorToolbarButton::SpellCheck)=makeButton("spellCheck",tr("Check spelling"),"spellCheck",true);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::SpellCheck));
    pimpl->hostVisible[static_cast<size_t>(MessageEditorToolbarButton::SpellCheck)]=false;
    applyButtonVisibility(MessageEditorToolbarButton::SpellCheck);
    wireCheckableAction(MessageEditorToolbarButton::SpellCheck);

    // --- 14: Clear formatting ---

    pimpl->btn(MessageEditorToolbarButton::ClearFormatting)=makeButton("clearFormatting",tr("Clear formatting"),"clearFormatting",false);
    layout->addWidget(pimpl->btn(MessageEditorToolbarButton::ClearFormatting));
    connect(pimpl->btn(MessageEditorToolbarButton::ClearFormatting),&IconTextButton::clicked,this,&MessageEditorToolbar::clearFormattingRequested);

    // --- 15: stretch ---

    layout->addStretch(1);

    // --- 15a: overflow -- see the class's own doc comment on overflow collapse. Added AFTER the
    // stretch, so it is the LAST item in the row and the stretch pins it hard against the right
    // edge no matter how many (or how few) of the buttons above it are currently on the bar.
    // Starts hidden: relayout(), called once at the very end of this ctor and again from every
    // resizeEvent()/showEvent() after, is what decides whether anything is actually demoted.
    pimpl->overflowButton=makeButton("overflowButton",tr("More formatting options"),"more",false);
    layout->addWidget(pimpl->overflowButton);
    pimpl->overflowButton->setVisible(false);

    // attachTo() makes the trigger checkable itself and keeps it checked while the menu is open
    // (DropdownMenu::attachTo()'s own doc comment) -- same mechanism the mode/heading/table
    // triggers above already rely on for their own "checked" QSS state, so checkable=false above
    // is correct: makeButton() must not also make it checkable.
    pimpl->overflowMenu=new DropdownMenu();
    pimpl->overflowMenu->attachTo(pimpl->overflowButton);
    connect(pimpl->overflowMenu,&DropdownMenu::itemTriggered,this,&MessageEditorToolbar::onOverflowTriggered);
    connect(pimpl->overflowMenu,&DropdownMenu::itemToggled,this,&MessageEditorToolbar::onOverflowToggled);

    // --- 16: Close, leading on EVERY platform ---
    //
    // Deliberately not the platform-dependent placement the original brief asked for (left on
    // macOS, trailing elsewhere). Leading everywhere is what this toolbar actually wants: it puts
    // Close directly above the editor's own bottom-left expand button, which is the control that
    // opened the toolbar in the first place -- the two sit in one column over the strip of blank
    // padding beside the text edit, so opening and closing the bar happen in the same place
    // instead of at opposite ends of it. insertWidget(0,...) is unaffected by the overflow button
    // added above: index 0 is still ahead of everything, overflow button included.
    pimpl->btn(MessageEditorToolbarButton::Close)=makeButton("closeButton",tr("Close"),"close",false);
    layout->insertWidget(0,pimpl->btn(MessageEditorToolbarButton::Close));
    connect(pimpl->btn(MessageEditorToolbarButton::Close),&IconTextButton::clicked,this,&MessageEditorToolbar::closeRequested);

    // The bar's whole button set now exists (overflow button/menu included) -- safe to let
    // relayout() run for the first time. It bails out immediately if the widget has no real width
    // yet (not shown, not yet laid out by a parent), which is the common case here; showEvent()/
    // resizeEvent() take over once the bar actually gets a geometry.
    relayout();
}

//--------------------------------------------------------------------------

MessageEditorToolbar::~MessageEditorToolbar()
{}

//--------------------------------------------------------------------------

void MessageEditorToolbar::setButtonVisible(MessageEditorToolbarButton button, bool visible)
{
    pimpl->hostVisible[static_cast<size_t>(button)]=visible;
    applyButtonVisibility(button);
    // The set of buttons that FIT can change when one of them stops competing for space (a
    // host-hidden button costs zero width -- see relayout()), so re-evaluate the overflow
    // collapse. Cheap when nothing actually changes: relayout() early-outs before touching any
    // widget if the recomputed demoted set is the same as what is already committed -- which is
    // what keeps MessageEditor::syncToolbarState()'s own setButtonVisible(RemoveLink,...) call on
    // every caret move effectively free.
    relayout();
}

//--------------------------------------------------------------------------

bool MessageEditorToolbar::isButtonVisible(MessageEditorToolbarButton button) const
{
    // The HOST's own intent, set by setButtonVisible() above -- see this accessor's doc comment
    // in the header for why it deliberately does NOT read the widget's current on-screen state.
    return pimpl->hostVisible[static_cast<size_t>(button)];
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::setButtonEnabled(MessageEditorToolbarButton button, bool enable)
{
    pimpl->btn(button)->setEnabled(enable);

    // Mirror onto the overflow menu so a DEMOTED button reads greyed there too, whether or not it
    // is demoted right now -- setItemEnabled() is a harmless no-op for an id the menu does not
    // currently hold (DropdownMenu::setItemEnabled()/findItem()), and rebuildOverflowMenu() seeds
    // isEnabled from the live widget on every rebuild regardless, so this can never go stale.
    if (pimpl->overflowMenu)
    {
        auto idx=static_cast<int>(button);
        pimpl->overflowMenu->setItemEnabled(OverflowActionIdBase+idx,enable);
        pimpl->overflowMenu->setItemEnabled(OverflowSubmenuIdBase+idx,enable);
    }
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
    pimpl->btn(MessageEditorToolbarButton::SpellCheck)->setChecked(state.spellCheckEnabled);

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

    // Mirrors everything above onto the overflow menu's own rows -- inside the same syncing guard
    // as the buttons/standalone menus, even though syncOverflowState()'s own setItemChecked()/
    // setItemEnabled() calls do not themselves re-enter onOverflowToggled() (DropdownMenu blocks
    // a row's own signals while it is set programmatically, same as the standalone menus above).
    // The guard is what matters for onOverflowToggled() ITSELF, which early-returns on it -- see
    // that method's own doc comment.
    syncOverflowState();

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

    // Mirrors the mode (and, redundantly but harmlessly, the last-pushed MessageEditorFormatState)
    // onto the overflow menu's own rows -- same function setFormatState() calls, see its own
    // comment on why no syncing guard is needed around it.
    syncOverflowState();
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
        // Routed through setButtonEnabled() rather than the widget directly, so the overflow-menu
        // mirroring lives in exactly one place -- Heading and Table are both members of
        // FormattingButtons, so their SUBMENU parent rows grey out here too, and a disabled
        // submenu row cannot be opened (DropdownMenu's own click-to-open connection checks
        // isEnabled()).
        setButtonEnabled(button,enable);
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

void MessageEditorToolbar::applyButtonVisibility(MessageEditorToolbarButton button)
{
    auto idx=static_cast<size_t>(button);
    pimpl->btn(button)->setVisible(pimpl->hostVisible[idx] && !pimpl->demoted[idx]);
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::resizeEvent(QResizeEvent* event)
{
    Frame::resizeEvent(event);
    relayout();
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::showEvent(QShowEvent* event)
{
    Frame::showEvent(event);
    // The bar is built hidden (MessageEditor::MessageEditor(), pimpl->toolbar->setVisible(false))
    // and only ever shown later by setExpanded(true), so THIS is where its first real width
    // arrives, not the ctor -- relayout() there bailed out on width()<=0.
    relayout();
}

//--------------------------------------------------------------------------

QSize MessageEditorToolbar::minimumSizeHint() const
{
    // Deliberately NOT the sum of every button's width -- see the class's own doc comment on
    // overflow collapse. Reports only what never demotes: Close, Mode, and the overflow trigger
    // itself, counted whether or not anything is CURRENTLY demoted -- reserving its width even
    // while the bar is at full strength keeps this value stable across a demotion/promotion, which
    // matters because a value that changed the instant something got demoted would itself trigger
    // another resize/relayout right as the bar was settling.
    //
    // Ignores the toolbar's own QSS "padding: 2px 4px" (resources/style/messageeditor.qss) -- this
    // is a FLOOR meant to stop the ~790px full-bar demand from propagating into the host layout,
    // not an exact measurement; relayout()'s own contentsRect()-based fit check is the actual
    // authority for what gets demoted, and being a few px more generous here than strictly
    // necessary is harmless.
    if (layout()==nullptr || pimpl->overflowButton==nullptr)
    {
        return Frame::minimumSizeHint();
    }
    auto* closeBtn=pimpl->btn(MessageEditorToolbarButton::Close);
    auto* modeBtn=pimpl->btn(MessageEditorToolbarButton::Mode);
    if (closeBtn==nullptr || modeBtn==nullptr)
    {
        return Frame::minimumSizeHint();
    }

    auto width=closeBtn->sizeHint().width()+modeBtn->sizeHint().width()
        +pimpl->overflowButton->sizeHint().width();
    return QSize(width,Frame::minimumSizeHint().height());
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::updateSeparators()
{
    // Walks the layout LEFT TO RIGHT, stopping at the stretch -- itemAt(i)->spacerItem()!=nullptr
    // is exactly addStretch(1)'s own item (see the ctor's "--- 15: stretch ---" section), which
    // also keeps the overflow button (added AFTER the stretch) out of this walk: it is never a
    // #separator and has nothing to do with separator orphaning.
    //
    // A #separator is revealed only once a further visible widget is found after it AND at least
    // one visible widget was already seen before it -- so there is never a leading orphan (nothing
    // visible before the first one), never a trailing orphan (nothing visible after the last one,
    // including a run that reaches the stretch with nothing left to reveal `pending`), and a run
    // of separators bracketing an emptied group collapses to exactly the last one seen, because
    // each later separator simply overwrites `pending` while the earlier one stays hidden.
    QFrame* pending=nullptr;
    bool anyVisibleBefore=false;
    auto* lay=layout();
    for (int i=0; i<lay->count(); ++i)
    {
        auto* item=lay->itemAt(i);
        if (item->spacerItem()!=nullptr)
        {
            break;
        }
        auto* w=item->widget();
        if (w==nullptr)
        {
            continue;
        }
        if (w->objectName()==QStringLiteral("separator"))
        {
            w->setVisible(false);
            if (anyVisibleBefore)
            {
                pending=static_cast<QFrame*>(w);
            }
            continue;
        }
        if (w->isVisible())
        {
            if (pending!=nullptr)
            {
                pending->setVisible(true);
                pending=nullptr;
            }
            anyVisibleBefore=true;
        }
    }
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::relayout()
{
    if (pimpl->relayouting || !isVisible() || width()<=0)
    {
        return;
    }
    pimpl->relayouting=true;

    // Pure function of the bar's CURRENT width, computed from a clean slate (every hostVisible
    // button promoted) on every call -- never patched incrementally from the live pimpl->demoted.
    // That is what makes this oscillation-free: the same width always recomputes the same target
    // set, so the "did the committed set actually change" check below is a genuine fixed point
    // rather than a moving target that could flip back and forth across a resize.
    std::array<bool,MessageEditorToolbar_p::ButtonCount> target{};

    // Applies `target`, remeasures, and reports whether it fits -- re-applying and remeasuring on
    // every candidate (rather than subtracting each button's width by hand from a running total)
    // costs at most DemotionOrder.size() extra layout passes, only on an actual threshold
    // crossing, and cannot be thrown off by the QSS padding/margins a hand-summed total would have
    // to reproduce exactly (padding: 2px 4px on the bar itself, margin: 0 1px per button -- see
    // resources/style/messageeditor.qss) to agree with what Qt's own layout engine will do.
    auto applyAndFits=[this,&target]()
    {
        for (size_t i=0; i<MessageEditorToolbar_p::ButtonCount; ++i)
        {
            auto button=static_cast<MessageEditorToolbarButton>(i);
            pimpl->btn(button)->setVisible(pimpl->hostVisible[i] && !target[i]);
        }
        updateSeparators();
        bool anyDemoted=std::any_of(target.begin(),target.end(),[](bool d){ return d; });
        pimpl->overflowButton->setVisible(anyDemoted);

        auto* lay=layout();
        lay->invalidate();
        lay->activate();
        return lay->minimumSize().width()<=contentsRect().width();
    };

    if (!applyAndFits())
    {
        for (auto button : DemotionOrder)
        {
            auto idx=static_cast<size_t>(button);
            if (!pimpl->hostVisible[idx])
            {
                // Already costs zero width on the bar -- demoting it would not free anything.
                continue;
            }
            target[idx]=true;
            if (applyAndFits())
            {
                break;
            }
        }
    }

    if (target==pimpl->demoted)
    {
        // Nothing actually changed -- applyAndFits() above already left every widget in the
        // state applyButtonVisibility() would compute for the (unchanged) committed set, so
        // there is nothing left to commit. This is the common case on every caret-move-driven
        // setButtonVisible(RemoveLink,...) call (MessageEditor::syncToolbarState()), which is
        // what keeps that call effectively free.
        pimpl->relayouting=false;
        return;
    }

    pimpl->demoted=target;
    rebuildOverflowMenu();

    pimpl->relayouting=false;
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::rebuildOverflowMenu()
{
    if (!pimpl->overflowMenu)
    {
        return;
    }

    // Maps a layout child back to the MessageEditorToolbarButton it belongs to, by pointer --
    // the walk below needs this to read each row's live state (checked/enabled/icon/tooltip) off
    // the SAME button object the bar itself uses, so the menu can never show something the bar
    // itself disagrees with.
    auto buttonFor=[this](QWidget* w) -> std::optional<MessageEditorToolbarButton>
    {
        for (size_t i=0; i<MessageEditorToolbar_p::ButtonCount; ++i)
        {
            if (pimpl->buttons[i]==w)
            {
                return static_cast<MessageEditorToolbarButton>(i);
            }
        }
        return std::nullopt;
    };

    // Builds one row (or submenu) for a demoted button, reading its live state off the SAME
    // button widget the bar itself uses.
    //
    // Mode/Heading/Table become SUBMENUS rather than flat rows -- built from the exact same
    // buildModeMenuItems()/buildHeadingMenuItems()/buildTableMenuItems() helpers the standalone
    // menus in the ctor use, just with ids shifted into this menu's own id space (see those
    // Overflow*IdBase constants' own doc comment on why the shift is required). Mode is
    // implemented here for completeness even though DemotionOrder never actually demotes it.
    // The parent row's icon is the trigger button's OWN current glyph (already kept in sync by
    // setMode()/setFormatState()), so it needs no separate lookup here.
    auto appendRow=[this](std::vector<MenuItem>& items, MessageEditorToolbarButton button)
    {
        auto idx=static_cast<int>(button);
        auto* btn=pimpl->btn(button);
        auto text=btn->toolTip();
        auto icon=btn->svgIcon();

        if (button==MessageEditorToolbarButton::Mode)
        {
            items.push_back(MenuItem::submenu(
                OverflowSubmenuIdBase+idx,text,buildModeMenuItems(this,OverflowModeIdBase,pimpl->mode),icon
            ));
        }
        else if (button==MessageEditorToolbarButton::Heading)
        {
            items.push_back(MenuItem::submenu(
                OverflowSubmenuIdBase+idx,text,
                buildHeadingMenuItems(this,OverflowHeadingIdBase,pimpl->state.headingLevel),icon
            ));
        }
        else if (button==MessageEditorToolbarButton::Table)
        {
            items.push_back(MenuItem::submenu(
                OverflowSubmenuIdBase+idx,text,
                buildTableMenuItems(this,OverflowTableIdBase,OverflowTableActionIdBase,pimpl->state.insideTable),icon
            ));
        }
        else if (const auto* action=checkableActionFor(button))
        {
            items.push_back(MenuItem::checkable(OverflowActionIdBase+idx,text,pimpl->state.*(action->field),icon));
        }
        else
        {
            // Plain clickable -- every non-checkable, non-submenu button (Undo, Redo,
            // IndentIncrease, IndentDecrease, CodeBlock, HorizontalRule, Link, RemoveLink,
            // Mention, ClearFormatting).
            items.push_back(MenuItem(OverflowActionIdBase+idx,text,icon));
        }

        items.back().isEnabled=btn->isEnabled();
    };

    std::vector<MenuItem> items;
    bool pendingSeparator=false;

    // BAR order (not DemotionOrder, which is a priority list, not a reading order), so the menu
    // reads like a continuation of the bar's own tail -- walked straight off the live layout
    // rather than a hand-maintained order constant, so it can never drift from the real bar.
    auto* lay=layout();
    for (int i=0; i<lay->count(); ++i)
    {
        auto* item=lay->itemAt(i);
        if (item->spacerItem()!=nullptr)
        {
            // The stretch -- the overflow button itself sits after it and is never a candidate.
            break;
        }
        auto* w=item->widget();
        if (w==nullptr)
        {
            continue;
        }
        if (w->objectName()==QStringLiteral("separator"))
        {
            // Only matters if a further row actually follows it -- see the flush below, which is
            // the same orphan-free rule updateSeparators() applies to the bar itself.
            pendingSeparator=true;
            continue;
        }

        auto button=buttonFor(w);
        if (!button)
        {
            continue;
        }
        auto idx=static_cast<size_t>(*button);
        if (!pimpl->demoted[idx] || !pimpl->hostVisible[idx])
        {
            continue;
        }

        if (pendingSeparator && !items.empty())
        {
            items.push_back(MenuItem::separator());
        }
        pendingSeparator=false;

        appendRow(items,*button);
    }

    pimpl->overflowMenu->setItems(std::move(items));
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::syncOverflowState()
{
    if (!pimpl->overflowMenu)
    {
        return;
    }

    // Checkable formatting actions -- mirrors setFormatState()'s own setChecked() calls on the
    // buttons above. A no-op for any id the menu does not currently hold (the button is not
    // demoted right now), same as setButtonEnabled()'s own mirroring.
    for (const auto& action : CheckableActions)
    {
        pimpl->overflowMenu->setItemChecked(
            OverflowActionIdBase+static_cast<int>(action.button),pimpl->state.*(action.field)
        );
    }

    // Heading level -- uncheck-all-then-check-one, same reason as the standalone heading menu
    // above: setItemChecked() does not enforce group exclusivity on its own.
    for (int level=0; level<=3; ++level)
    {
        pimpl->overflowMenu->setItemChecked(OverflowHeadingIdBase+level,level==pimpl->state.headingLevel);
    }
    pimpl->overflowMenu->setItemIcon(
        OverflowSubmenuIdBase+static_cast<int>(MessageEditorToolbarButton::Heading),
        tbIcon(headingIconAlias(pimpl->state.headingLevel),this)
    );

    // Table row/column actions -- meaningless outside a table, same as the standalone table menu.
    for (const auto& action : TableActionRows)
    {
        pimpl->overflowMenu->setItemEnabled(
            OverflowTableActionIdBase+static_cast<int>(action.action),pimpl->state.insideTable
        );
    }

    // Mode -- mirrors setMode()'s own three setItemChecked() calls on the standalone mode menu.
    pimpl->overflowMenu->setItemChecked(OverflowModeIdBase+static_cast<int>(MessageEditingMode::Wysiwyg),pimpl->mode==MessageEditingMode::Wysiwyg);
    pimpl->overflowMenu->setItemChecked(OverflowModeIdBase+static_cast<int>(MessageEditingMode::Markdown),pimpl->mode==MessageEditingMode::Markdown);
    pimpl->overflowMenu->setItemChecked(OverflowModeIdBase+static_cast<int>(MessageEditingMode::Plaintext),pimpl->mode==MessageEditingMode::Plaintext);
    pimpl->overflowMenu->setItemIcon(
        OverflowSubmenuIdBase+static_cast<int>(MessageEditorToolbarButton::Mode),
        tbIcon(modeIconAlias(pimpl->mode),this)
    );
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::onOverflowTriggered(int id)
{
    if (id>=OverflowTableActionIdBase && id<OverflowTableActionIdBase+100)
    {
        emit tableActionRequested(static_cast<MessageEditorTableAction>(id-OverflowTableActionIdBase));
        return;
    }
    if (id>=OverflowTableIdBase && id<OverflowTableIdBase+100)
    {
        auto idx=static_cast<size_t>(id-OverflowTableIdBase);
        if (idx<TablePresets.size())
        {
            emit tableRequested(TablePresets[idx].rows,TablePresets[idx].columns);
        }
        return;
    }
    if (id>=OverflowActionIdBase && id<OverflowActionIdBase+100)
    {
        // Routes through the SAME button every wireCheckable()/plain connect() in the ctor is
        // already wired to -- see the class's own doc comment on overflow collapse. A hidden
        // IconTextButton is still clickable programmatically (Qt does not gate click() on
        // isVisible()), so this reuses every existing emit path verbatim instead of duplicating
        // it here, and can never drift from what the bar itself does for the same button.
        pimpl->btn(static_cast<MessageEditorToolbarButton>(id-OverflowActionIdBase))->click();
        return;
    }
    // Submenu parent rows (OverflowSubmenuIdBase) never reach here -- DropdownMenu opens the
    // submenu instead of emitting itemTriggered() for one (MenuItem::submenu()'s own doc comment)
    // -- and neither do checkable Mode/Heading rows, whose activation is itemToggled(), handled
    // below in onOverflowToggled().
}

//--------------------------------------------------------------------------

void MessageEditorToolbar::onOverflowToggled(int id, bool checked)
{
    if (pimpl->syncing)
    {
        return;
    }

    if (id>=OverflowModeIdBase && id<OverflowModeIdBase+100)
    {
        // Group exclusivity also emits itemToggled(other,false) for the row being unchecked --
        // act only on the row becoming checked, same as the standalone mode menu's own handler.
        if (checked)
        {
            emit modeRequested(static_cast<MessageEditingMode>(id-OverflowModeIdBase));
        }
        return;
    }

    if (id>=OverflowHeadingIdBase && id<OverflowHeadingIdBase+100)
    {
        if (checked)
        {
            emit headingRequested(id-OverflowHeadingIdBase);
        }
        return;
    }

    if (id>=OverflowActionIdBase && id<OverflowActionIdBase+100)
    {
        const auto* action=checkableActionFor(static_cast<MessageEditorToolbarButton>(id-OverflowActionIdBase));
        if (action==nullptr)
        {
            return;
        }

        // Same two-part contract as wireCheckable()'s clicked()/toggled() pair, collapsed into a
        // single call because DropdownMenu's own row already conflates click-and-toggle the same
        // way IconTextButton::click() does: DropdownMenu::onItemToggled() writes item.isChecked
        // straight from the row's own post-click state before this signal is even emitted, so
        // `checked` here is NOT authoritative, exactly like a plain IconTextButton's toggled(bool)
        // is not (see wireCheckable()'s own doc comment). Compute the request from the negation of
        // the authoritative MessageEditorFormatState field, emit it, then re-assert the
        // still-authoritative value via setItemChecked() -- which blocks the row's own signals
        // internally (DropdownMenu::setItemChecked()), so this cannot loop back in here.
        auto authoritative=!(pimpl->state.*(action->field));
        emit (this->*(action->requestedSignal))(authoritative);
        pimpl->overflowMenu->setItemChecked(id,authoritative);
    }
}

//--------------------------------------------------------------------------

}
