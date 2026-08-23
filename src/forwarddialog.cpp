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

/** @file uise/desktop/src/forwarddialog.cpp
*
*  Defines ForwardDialog.
*
*/

/****************************************************************************/

#include <vector>

#include <QFrame>
#include <QLabel>
#include <QBoxLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/checkbox.hpp>
#include <uise/desktop/buttonslist.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/abstractchatmessage.hpp>
#include <uise/desktop/replypreviewdata.hpp>
#include <uise/desktop/forwarddialog.hpp>

// ForwardDialog is the only translation unit that instantiates Dialog<AbstractForwardDialog>, so
// the template definition must be visible here -- see the identical comment in
// src/replydialog.cpp for why.
#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// Same rationale as ReplyDialog's identical constants -- see replydialog.cpp.
constexpr int MinMessageAreaHeight=80;
constexpr int MaxMessageAreaHeight=500;

}

//--------------------------------------------------------------------------

class ForwardDialog_p
{
    public:

        ScrollArea* messageArea=nullptr;
        QFrame* messageHolder=nullptr;
        QBoxLayout* messageHolderLayout=nullptr;
        QPointer<AbstractChatMessage> message;

        //! Shown instead of messageArea whenever messageCount>1 -- see ForwardDialog::updateMode().
        QLabel* countLabel=nullptr;
        int messageCount=0;

        CheckBox* hideSenderName=nullptr;
        //! Guards against setHideSenderName()'s own setChecked() call re-triggering
        //! hideSenderNameChanged() a second time via the checkbox's toggled() signal -- a plain
        //! flag, not QSignalBlocker, which would also suppress CheckBox's own repaint (see
        //! uise-checkbox-qsignalblocker-gotcha).
        bool settingHideSenderName=false;

        ButtonsList* actions=nullptr;
        std::vector<IconTextButton*> actionButtons;

        QLabel* comment=nullptr;
        QString commentText;
        bool commentVisible=false;
        //! Set once setCommentVisible() is called explicitly -- from then on
        //! updateCommentVisibility()'s auto-detection policy (visible for a text body) is
        //! skipped in favour of whatever the host last set. Multi-message mode still forces the
        //! comment off regardless -- see updateCommentVisibility().
        bool commentVisibleExplicit=false;

        //! Guards updateSaveButton() against rebuilding the button row (via setButtons(), which
        //! destroys and recreates every button -- see Dialog<>::doSetButtons()) on every single
        //! cursor movement; only an actual quote/no-quote transition rebuilds it.
        bool quoteMode=false;

        int quoteTrimLength=DefaultReplyQuoteTrimLength;
};

//--------------------------------------------------------------------------

ForwardDialog::ForwardDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<ForwardDialog_p>())
{
    pimpl->commentText=tr("You can select a part of the text to quote only that part.");

    setTitle(tr("Forward message"));
    // ForwardDialogIcon is a DEDICATED context (not "ForwardDialog", which the #actions row
    // icons also use) with no "hovered" mode defined at all -- this title icon is purely
    // decorative (see abstractdialog.qss's #dialogIcon notes: nothing ever connects its
    // clicked()), but PushButton::enterEvent() unconditionally swaps to the icon's hoverIcon()
    // on mouse-enter regardless of whether the button does anything on click (src/pushbutton.cpp)
    // -- there is no per-instance way to opt out of that swap, only to make it invisible by
    // giving the icon no "hovered" colour to swap to, so SvgIcon::offContent()'s own fallback
    // (missing mode -> IconMode::Normal) renders the same colour either way.
    setSvgIcon(Style::instance().svgIconLocator().icon(QStringLiteral("ForwardDialogIcon::forward"),this));

    auto content=new QFrame(this);
    auto contentLayout=Layout::vertical(content);

    pimpl->messageArea=new ScrollArea(content);
    pimpl->messageArea->setObjectName("messageArea");
    pimpl->messageArea->setWidgetResizable(true);
    pimpl->messageArea->setFrameShape(QFrame::NoFrame);
    // ScrollArea::minimumSizeHint() is zero (see its own doc comment), so without an explicit
    // minimum here an empty (not-yet-setMessage()'d) dialog would measure to nothing -- same
    // rule as ReplyDialog's identical messageArea.
    pimpl->messageArea->setMinimumSize(260,80);
    contentLayout->addWidget(pimpl->messageArea);

    pimpl->messageHolder=new QFrame(pimpl->messageArea);
    pimpl->messageHolder->setObjectName("messageHolder");
    pimpl->messageHolderLayout=Layout::vertical(pimpl->messageHolder);
    pimpl->messageHolderLayout->addStretch(1);
    pimpl->messageArea->setWidget(pimpl->messageHolder);
    // ScrollArea's viewport defaults to an opaque QPalette::Base fill, which would otherwise
    // paint over the dialog's own background -- same rule as ReplyDialog's own scroll area.
    pimpl->messageArea->viewport()->setAutoFillBackground(false);

    pimpl->countLabel=new QLabel(content);
    pimpl->countLabel->setObjectName("countLabel");
    pimpl->countLabel->setAlignment(Qt::AlignCenter);
    pimpl->countLabel->setVisible(false);
    contentLayout->addWidget(pimpl->countLabel);

    pimpl->hideSenderName=new CheckBox(tr("Hide sender name"),content);
    pimpl->hideSenderName->setObjectName("hideSenderName");
    contentLayout->addWidget(pimpl->hideSenderName);
    connect(pimpl->hideSenderName,&QAbstractButton::toggled,this,
        [this](bool checked)
        {
            if (pimpl->settingHideSenderName)
            {
                return;
            }
            emit hideSenderNameChanged(checked);
        }
    );

    pimpl->comment=new QLabel(content);
    pimpl->comment->setObjectName("comment");
    pimpl->comment->setWordWrap(true);
    pimpl->comment->setText(pimpl->commentText);
    pimpl->comment->setVisible(false);
    contentLayout->addWidget(pimpl->comment);

    pimpl->actions=new ButtonsList(content);
    pimpl->actions->setObjectName("actions");
    contentLayout->addWidget(pimpl->actions);

    setWidget(content);

    setActions({
        AbstractForwardDialog::standardAction(ForwardDialogAction::ChangeRecipient,this),
        AbstractForwardDialog::standardAction(ForwardDialogAction::ShowInChat,this)
    });

    setButtons({
        AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this),
        AbstractDialog::ButtonConfig{static_cast<int>(AbstractDialog::StandardButton::Apply),tr("Send")}
    });

    // Cancel already auto-closes via Dialog<>'s own signal-mapper handler (see dialog.ipp) --
    // only Apply/"Quote selected" needs handling here, and deliberately does NOT close the
    // dialog itself -- same rationale as ReplyDialog's identical handler.
    connect(this,&AbstractDialog::buttonClicked,this,
        [this](int id)
        {
            if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Apply))
            {
                emit saveRequested(
                    pimpl->quoteMode ? trimReplyText(selectedText(),pimpl->quoteTrimLength) : QString{},
                    isHideSenderName()
                );
            }
        }
    );
}

//--------------------------------------------------------------------------

ForwardDialog::~ForwardDialog()
{}

//--------------------------------------------------------------------------

void ForwardDialog::setMessage(AbstractChatMessage* message)
{
    if (!pimpl->message.isNull())
    {
        destroyWidget(pimpl->message);
    }
    pimpl->message=message;

    if (message!=nullptr)
    {
        message->setParent(pimpl->messageHolder);
        pimpl->messageHolderLayout->insertWidget(0,message);

        // This dialog is a static preview, not the live chat page -- a click inside the bubble
        // means nothing here, so its own selection-mode gesture (AbstractChatMessage::
        // detectMouseSelection(), normally toggling multi-select) is disabled. Text selection
        // for "Quote selected" is unaffected: it goes through the body's own QTextEdit
        // selection, not this flag. Same rationale as ReplyDialog::setMessage().
        message->setSelectionMode(false);
        message->setSelectDetectionBlocked(true);

        if (message->content()!=nullptr && message->content()->body()!=nullptr)
        {
            auto* body=message->content()->body();
            connect(body,&AbstractChatMessageBody::selectionChanged,this,&ForwardDialog::updateSaveButton);

            // Static preview, not the live chat page -- enable the body's own text selection/
            // copy (Ctrl+C, right-click Copy), which are deliberately off by default there. See
            // AbstractChatMessageBody::setCopyable()'s own doc comment.
            body->setCopyable(true);
        }
    }

    updateMode();
}

//--------------------------------------------------------------------------

AbstractChatMessage* ForwardDialog::message() const
{
    return pimpl->message;
}

//--------------------------------------------------------------------------

void ForwardDialog::setMessageCount(int count)
{
    pimpl->messageCount=count;
    updateMode();
}

//--------------------------------------------------------------------------

int ForwardDialog::messageCount() const
{
    return pimpl->messageCount;
}

//--------------------------------------------------------------------------

void ForwardDialog::setActions(std::vector<ForwardDialogActionConfig> actions)
{
    for (auto* button : pimpl->actionButtons)
    {
        destroyWidget(button);
    }
    pimpl->actionButtons.clear();

    for (const auto& action : actions)
    {
        auto button=pimpl->actions->addButton(action.text,action.icon);
        // ButtonsList::addButton() assigns no objectName of its own -- this is the only hook
        // forwardpreview.qss has to give a destructive action (red palette) a different style
        // from its siblings, see uise--ForwardDialog #actions #doNotForwardAction there.
        if (action.id==static_cast<int>(ForwardDialogAction::DoNotForward))
        {
            button->setObjectName("doNotForwardAction");
        }
        auto id=action.id;
        connect(button,&IconTextButton::clicked,this,
            [this,id]()
            {
                emit actionTriggered(id);
            }
        );
        pimpl->actionButtons.push_back(button);
    }
}

//--------------------------------------------------------------------------

void ForwardDialog::setHideSenderName(bool enable)
{
    if (pimpl->hideSenderName->isChecked()==enable)
    {
        return;
    }
    pimpl->settingHideSenderName=true;
    pimpl->hideSenderName->setChecked(enable);
    pimpl->settingHideSenderName=false;
    emit hideSenderNameChanged(enable);
}

//--------------------------------------------------------------------------

bool ForwardDialog::isHideSenderName() const
{
    return pimpl->hideSenderName->isChecked();
}

//--------------------------------------------------------------------------

void ForwardDialog::setComment(const QString& text)
{
    pimpl->commentText=text;
    pimpl->comment->setText(text);
}

//--------------------------------------------------------------------------

QString ForwardDialog::comment() const
{
    return pimpl->commentText;
}

//--------------------------------------------------------------------------

void ForwardDialog::setCommentVisible(bool enable)
{
    pimpl->commentVisibleExplicit=true;
    pimpl->commentVisible=enable;
    pimpl->comment->setVisible(enable && pimpl->messageCount<=1);
}

//--------------------------------------------------------------------------

bool ForwardDialog::isCommentVisible() const
{
    return pimpl->commentVisible;
}

//--------------------------------------------------------------------------

QString ForwardDialog::selectedText() const
{
    // Multi-message mode has no single message to quote from -- see class doc comment.
    if (pimpl->messageCount>1 || pimpl->message.isNull())
    {
        return QString{};
    }
    return pimpl->message->selectedText();
}

//--------------------------------------------------------------------------

void ForwardDialog::setQuoteTrimLength(int length)
{
    pimpl->quoteTrimLength=length;
}

//--------------------------------------------------------------------------

int ForwardDialog::quoteTrimLength() const
{
    return pimpl->quoteTrimLength;
}

//--------------------------------------------------------------------------

void ForwardDialog::updateSaveButton()
{
    // selectedText() is already forced empty in multi-message mode, so quoteMode naturally
    // never turns on there -- no separate mode check needed here.
    const bool quote=!selectedText().isEmpty();
    if (quote==pimpl->quoteMode)
    {
        return;
    }
    pimpl->quoteMode=quote;

    // setButtonText() relabels the existing Apply button in place -- unlike setButtons(), it
    // does not destroy/recreate the whole row (see Dialog<>::doSetButtons()), which visibly
    // flickered every button (Cancel included) on each select/deselect. Same rationale as
    // ReplyDialog::updateSaveButton().
    setButtonText(AbstractDialog::StandardButton::Apply,quote ? tr("Quote selected") : tr("Send"));
}

//--------------------------------------------------------------------------

void ForwardDialog::updateCommentVisibility()
{
    // No single message to show the quote hint about -- see class doc comment. Deliberately
    // does NOT touch commentVisibleExplicit, so a host's earlier explicit choice (or the
    // auto-detection policy below) resumes correctly once messageCount() drops back to <=1.
    if (pimpl->messageCount>1)
    {
        pimpl->commentVisible=false;
        pimpl->comment->setVisible(false);
        return;
    }

    if (pimpl->commentVisibleExplicit)
    {
        pimpl->comment->setVisible(pimpl->commentVisible);
        return;
    }

    bool hasText=!pimpl->message.isNull()
        && pimpl->message->content()!=nullptr
        && pimpl->message->content()->body()!=nullptr
        && pimpl->message->content()->body()->hasSelectableText();

    pimpl->commentVisible=hasText;
    pimpl->comment->setVisible(hasText);
}

//--------------------------------------------------------------------------

void ForwardDialog::prepareToShow()
{
    // See ReplyDialog::prepareToShow()'s identical rationale.
    updateMessageAreaHeight();
}

//--------------------------------------------------------------------------

void ForwardDialog::updateMessageAreaHeight()
{
    // Multi-message mode: messageArea is hidden entirely (see updateMode()), nothing to
    // measure. Its minimum/maximum height are left at whatever the single-message branch last
    // set -- harmless since the widget contributes no layout space while hidden.
    if (pimpl->messageCount>1)
    {
        return;
    }

    // QScrollArea::sizeHint() is NOT a reliable measure of "how tall is my content" -- see
    // ReplyDialog::updateMessageAreaHeight()'s identical rationale, copied verbatim below.
    if (pimpl->message.isNull())
    {
        pimpl->messageArea->setMinimumHeight(MinMessageAreaHeight);
        pimpl->messageArea->setMaximumHeight(MaxMessageAreaHeight);
        return;
    }

    Style::repolishRecursive(pimpl->messageHolder);
    pimpl->messageHolder->ensurePolished();

    int contentHeight=0;
    for (int pass=0;pass<2;++pass)
    {
        pimpl->messageHolderLayout->invalidate();
        pimpl->messageHolderLayout->activate();

        auto hint=pimpl->messageHolder->sizeHint().height();
        if (pass>0 && hint==contentHeight)
        {
            break;
        }
        contentHeight=hint;
    }

    auto h=qBound(MinMessageAreaHeight,contentHeight,MaxMessageAreaHeight);

    pimpl->messageArea->setMinimumHeight(h);
    pimpl->messageArea->setMaximumHeight(h);

    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

void ForwardDialog::updateMode()
{
    // task-message-forwarding.md: "If there are multiple messages selected for forwarding then
    // message preview differs. It shows only number of messages to forward."
    const bool multi=pimpl->messageCount>1;

    pimpl->messageArea->setVisible(!multi);
    pimpl->countLabel->setVisible(multi);
    if (multi)
    {
        pimpl->countLabel->setText(tr("%n messages to forward","",pimpl->messageCount));
    }

    updateCommentVisibility();
    updateSaveButton();
    updateMessageAreaHeight();
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractForwardDialog>;

UISE_DESKTOP_NAMESPACE_END
