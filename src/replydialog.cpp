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

/** @file uise/desktop/src/replydialog.cpp
*
*  Defines ReplyDialog.
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
#include <uise/desktop/buttonslist.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/abstractchatmessage.hpp>
#include <uise/desktop/replypreviewdata.hpp>
#include <uise/desktop/replydialog.hpp>

// ReplyDialog is the only translation unit that instantiates Dialog<AbstractReplyDialog>, so
// the template definition must be visible here -- see the identical comment in
// src/calendardialog.cpp/src/statusdialog.cpp for why.
#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// Keeps a lone one-line reply from collapsing messageArea to a sliver, and caps a very tall
// bubble (e.g. an image album) so it scrolls internally instead of trying to grow the whole
// dialog without bound -- ModalReplyDialog's own maxHeightPercent() still applies on top of
// this as the final "don't exceed the host frame" clamp.
constexpr int MinMessageAreaHeight=80;
constexpr int MaxMessageAreaHeight=500;

}

//--------------------------------------------------------------------------

class ReplyDialog_p
{
    public:

        ScrollArea* messageArea=nullptr;
        QFrame* messageHolder=nullptr;
        QBoxLayout* messageHolderLayout=nullptr;
        QPointer<AbstractChatMessage> message;

        ButtonsList* actions=nullptr;
        std::vector<IconTextButton*> actionButtons;

        QLabel* comment=nullptr;
        QString commentText;
        bool commentVisible=false;
        //! Set once setCommentVisible() is called explicitly -- from then on
        //! updateCommentVisibility()'s auto-detection policy (visible for a text body) is
        //! skipped in favour of whatever the host last set.
        bool commentVisibleExplicit=false;

        //! Guards updateSaveButton() against rebuilding the button row (via setButtons(), which
        //! destroys and recreates every button -- see Dialog<>::doSetButtons()) on every single
        //! cursor movement; only an actual quote/no-quote transition rebuilds it.
        bool quoteMode=false;

        int quoteTrimLength=DefaultReplyQuoteTrimLength;
};

//--------------------------------------------------------------------------

ReplyDialog::ReplyDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<ReplyDialog_p>())
{
    pimpl->commentText=tr("You can select a part of the text to quote only that part.");

    setTitle(tr("Reply to message"));
    // ReplyDialogIcon is a DEDICATED context (not "ReplyDialog", which the #actions row
    // icons also use) with no "hovered" mode defined at all -- this title icon is purely
    // decorative (see abstractdialog.qss's #dialogIcon notes: nothing ever connects its
    // clicked()), but PushButton::enterEvent() unconditionally swaps to the icon's hoverIcon()
    // on mouse-enter regardless of whether the button does anything on click (src/pushbutton.cpp)
    // -- there is no per-instance way to opt out of that swap, only to make it invisible by
    // giving the icon no "hovered" colour to swap to, so SvgIcon::offContent()'s own fallback
    // (missing mode -> IconMode::Normal) renders the same colour either way.
    setSvgIcon(Style::instance().svgIconLocator().icon(QStringLiteral("ReplyDialogIcon::reply"),this));

    auto content=new QFrame(this);
    auto contentLayout=Layout::vertical(content);

    pimpl->messageArea=new ScrollArea(content);
    pimpl->messageArea->setObjectName("messageArea");
    pimpl->messageArea->setWidgetResizable(true);
    pimpl->messageArea->setFrameShape(QFrame::NoFrame);
    // ScrollArea::minimumSizeHint() is zero (see its own doc comment), so without an explicit
    // minimum here an empty (not-yet-setMessage()'d) dialog would measure to nothing -- same
    // rule as the ScrollArea construction recipe in Calendar::constructDatesArea().
    pimpl->messageArea->setMinimumSize(260,80);
    contentLayout->addWidget(pimpl->messageArea);

    pimpl->messageHolder=new QFrame(pimpl->messageArea);
    pimpl->messageHolder->setObjectName("messageHolder");
    pimpl->messageHolderLayout=Layout::vertical(pimpl->messageHolder);
    pimpl->messageHolderLayout->addStretch(1);
    pimpl->messageArea->setWidget(pimpl->messageHolder);
    // ScrollArea's viewport defaults to an opaque QPalette::Base fill, which would otherwise
    // paint over the dialog's own background -- same rule as Calendar's own scroll area.
    pimpl->messageArea->viewport()->setAutoFillBackground(false);

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
        AbstractReplyDialog::standardAction(ReplyDialogAction::ShowInChat,this),
        AbstractReplyDialog::standardAction(ReplyDialogAction::DoNotReply,this)
    });

    setButtons({
        AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this),
        AbstractDialog::ButtonConfig{static_cast<int>(AbstractDialog::StandardButton::Apply),tr("Save")}
    });

    // Cancel already auto-closes via Dialog<>'s own signal-mapper handler (see dialog.ipp) --
    // only Apply/"Quote selected" needs handling here, and deliberately does NOT close the
    // dialog itself: the host may need to keep it open (e.g. showing a busy state) until it has
    // finished applying the reply, same rationale as FileUploadDialog's own Send handler.
    connect(this,&AbstractDialog::buttonClicked,this,
        [this](int id)
        {
            if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Apply))
            {
                emit saveRequested(pimpl->quoteMode ? trimReplyText(selectedText(),pimpl->quoteTrimLength) : QString{});
            }
        }
    );
}

//--------------------------------------------------------------------------

ReplyDialog::~ReplyDialog()
{}

//--------------------------------------------------------------------------

void ReplyDialog::setMessage(AbstractChatMessage* message)
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
        // selection, not this flag.
        message->setSelectionMode(false);
        message->setSelectDetectionBlocked(true);

        if (message->content()!=nullptr && message->content()->body()!=nullptr)
        {
            auto* body=message->content()->body();
            connect(body,&AbstractChatMessageBody::selectionChanged,this,&ReplyDialog::updateSaveButton);

            // Static preview, not the live chat page -- enable the body's own text selection/
            // copy (Ctrl+C, right-click Copy), which are deliberately off by default there. See
            // AbstractChatMessageBody::setCopyable()'s own doc comment.
            body->setCopyable(true);
        }

        // Same wiring, for a forwarded message's own comment section (a sibling of body, see
        // AbstractChatMessageComment's own class doc comment) -- without this a selection made
        // ONLY in the comment never flips Save to "Quote selected", even though selectedText()
        // (AbstractChatMessage::selectedText()) already aggregates it.
        if (message->content()!=nullptr && message->content()->comment()!=nullptr)
        {
            auto* comment=message->content()->comment();
            connect(comment,&AbstractChatMessageComment::selectionChanged,this,&ReplyDialog::updateSaveButton);
            comment->setCopyable(true);
        }
    }

    updateCommentVisibility();
    updateSaveButton();
    updateMessageAreaHeight();
}

//--------------------------------------------------------------------------

AbstractChatMessage* ReplyDialog::message() const
{
    return pimpl->message;
}

//--------------------------------------------------------------------------

void ReplyDialog::setActions(std::vector<ReplyDialogActionConfig> actions)
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
        // replypreview.qss has to give a destructive action (red palette) a different style
        // from its siblings, see uise--ReplyDialog #actions #doNotReplyAction there.
        if (action.id==static_cast<int>(ReplyDialogAction::DoNotReply))
        {
            button->setObjectName("doNotReplyAction");
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

void ReplyDialog::setComment(const QString& text)
{
    pimpl->commentText=text;
    pimpl->comment->setText(text);
}

//--------------------------------------------------------------------------

QString ReplyDialog::comment() const
{
    return pimpl->commentText;
}

//--------------------------------------------------------------------------

void ReplyDialog::setCommentVisible(bool enable)
{
    pimpl->commentVisibleExplicit=true;
    pimpl->commentVisible=enable;
    pimpl->comment->setVisible(enable);
}

//--------------------------------------------------------------------------

bool ReplyDialog::isCommentVisible() const
{
    return pimpl->commentVisible;
}

//--------------------------------------------------------------------------

QString ReplyDialog::selectedText() const
{
    if (pimpl->message.isNull())
    {
        return QString{};
    }
    return pimpl->message->selectedText();
}

//--------------------------------------------------------------------------

void ReplyDialog::setQuoteTrimLength(int length)
{
    pimpl->quoteTrimLength=length;
}

//--------------------------------------------------------------------------

int ReplyDialog::quoteTrimLength() const
{
    return pimpl->quoteTrimLength;
}

//--------------------------------------------------------------------------

void ReplyDialog::updateSaveButton()
{
    const bool quote=!selectedText().isEmpty();
    if (quote==pimpl->quoteMode)
    {
        return;
    }
    pimpl->quoteMode=quote;

    // setButtonText() relabels the existing Apply button in place -- unlike setButtons(), it
    // does not destroy/recreate the whole row (see Dialog<>::doSetButtons()), which visibly
    // flickered every button (Cancel included) on each select/deselect.
    setButtonText(AbstractDialog::StandardButton::Apply,quote ? tr("Quote selected") : tr("Save"));
}

//--------------------------------------------------------------------------

void ReplyDialog::updateCommentVisibility()
{
    if (pimpl->commentVisibleExplicit)
    {
        return;
    }

    // Either section can be the one that actually carries selectable text -- a forwarded
    // image/file with no caption has none in its body at all, only in its own forward comment
    // (see AbstractChatMessageComment's own class doc comment).
    bool hasText=false;
    if (!pimpl->message.isNull() && pimpl->message->content()!=nullptr)
    {
        auto* content=pimpl->message->content();
        hasText=(content->body()!=nullptr && content->body()->hasSelectableText())
            || (content->comment()!=nullptr && content->comment()->hasSelectableText());
    }

    pimpl->commentVisible=hasText;
    pimpl->comment->setVisible(hasText);
}

//--------------------------------------------------------------------------

void ReplyDialog::prepareToShow()
{
    // ModalPopup::popup() invokes this synchronously immediately before it measures the dialog
    // for the FIRST time (see AbstractDialog::prepareToShow()'s own doc comment) -- the
    // authoritative point to settle messageArea's height regardless of whether setMessage()
    // happened to run before or after openDialog()/showDialog() was first called. The
    // setMessage()-triggered call handles a LATER setMessage() while already visible (updating
    // messageArea's own internal layout), but ModalPopup itself has no QLayout of its own (it
    // positions itself via its own updateWidgetGeometry(), not Qt's layout system), so nothing
    // after this first measurement can make the OUTER popup frame re-measure on its own --
    // this is the one point guaranteed to run before that first measurement happens.
    updateMessageAreaHeight();
}

//--------------------------------------------------------------------------

void ReplyDialog::updateMessageAreaHeight()
{
    // QScrollArea::sizeHint() is NOT a reliable measure of "how tall is my content" -- Qt's
    // own implementation bounds it at a generic, font-metric-derived ceiling regardless of the
    // actual widget() it holds, so it under-reports a real bubble's height and the ENCLOSING
    // dialog's own sizeHint()/heightForWidth() (what ModalReplyDialog's popup-auto-height
    // reads) inherits that under-report. Same problem, same fix, as FileUploadWidget's own
    // listArea: measure the CONTENT directly and pin messageArea's height to it (bounded),
    // rather than trusting the scroll area's own sizeHint -- see
    // FileUploadWidget::doUpdateListAreaHeight() (src/fileuploadwidget.cpp) for the original.
    if (pimpl->message.isNull())
    {
        pimpl->messageArea->setMinimumHeight(MinMessageAreaHeight);
        pimpl->messageArea->setMaximumHeight(MaxMessageAreaHeight);
        return;
    }

    Style::repolishRecursive(pimpl->messageHolder);
    pimpl->messageHolder->ensurePolished();

    // Two-pass measurement, same rationale as doUpdateListAreaHeight(): the message was just
    // (re)inserted, so its own layout's invalidation may still be an asynchronous, posted
    // QEvent::LayoutRequest rather than something already reflected in a synchronous
    // sizeHint() -- pass 1 primes geometry, pass 2 re-measures after a real layout cycle.
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

    // The two lines above only give messageArea the right constraints; this is what actually
    // gets that new size reflected on screen (and up through ModalPopup's own geometry), same
    // as doUpdateListAreaHeight()'s identical closing call.
    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractReplyDialog>;

UISE_DESKTOP_NAMESPACE_END
