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

/** @file uise/desktop/src/forwardbar.cpp
*
*  Defines ForwardBar.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/abstractreplypreview.hpp>
#include <uise/desktop/replypreview.hpp>
#include <uise/desktop/forwardbar.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ForwardBar_p
{
    public:

        QBoxLayout* layout;

        IconTextButton* configureButton;
        AbstractReplyPreview* preview;
        //! Shown instead of preview() whenever messageCount>1 -- see ForwardBar::refresh().
        ElidedLabel* countLabel;
        IconTextButton* cancelButton;

        int messageCount=0;
        QString countFormat;
};

//--------------------------------------------------------------------------

ForwardBar::ForwardBar(QWidget* parent)
    : AbstractForwardBar(parent),
      pimpl(std::make_unique<ForwardBar_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->configureButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("ForwardBar::configure"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->configureButton->setObjectName("configureButton");
    pimpl->configureButton->setText(QString());
    pimpl->configureButton->setCursor(Qt::PointingHandCursor);
    pimpl->configureButton->setToolTip(tr("Configure forward"));
    pimpl->layout->addWidget(pimpl->configureButton);
    connect(pimpl->configureButton,&IconTextButton::clicked,this,&AbstractForwardBar::configureRequested);

    pimpl->preview=makeWidget<AbstractReplyPreview,ReplyPreview>(this);
    pimpl->preview->setTitleFormat(tr("Forwarded from %1"));
    pimpl->layout->addWidget(pimpl->preview,1);
    connect(pimpl->preview,&AbstractReplyPreview::clicked,this,&AbstractForwardBar::clicked);

    pimpl->countLabel=new ElidedLabel(this);
    pimpl->countLabel->setObjectName("countLabel");
    pimpl->countLabel->setVisible(false);
    pimpl->layout->addWidget(pimpl->countLabel,1);

    pimpl->cancelButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("ForwardBar::cancel"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->cancelButton->setObjectName("cancelButton");
    pimpl->cancelButton->setText(QString());
    pimpl->cancelButton->setCursor(Qt::PointingHandCursor);
    pimpl->cancelButton->setToolTip(tr("Cancel forward"));
    pimpl->layout->addWidget(pimpl->cancelButton);
    connect(pimpl->cancelButton,&IconTextButton::clicked,this,&AbstractForwardBar::cancelRequested);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    refresh();
}

//--------------------------------------------------------------------------

ForwardBar::~ForwardBar()
{}

//--------------------------------------------------------------------------

void ForwardBar::setForwardData(ReplyPreviewData data)
{
    pimpl->preview->setData(std::move(data));
}

//--------------------------------------------------------------------------

const ReplyPreviewData& ForwardBar::forwardData() const
{
    return pimpl->preview->data();
}

//--------------------------------------------------------------------------

void ForwardBar::clear()
{
    pimpl->preview->clear();
}

//--------------------------------------------------------------------------

AbstractReplyPreview* ForwardBar::preview() const
{
    return pimpl->preview;
}

//--------------------------------------------------------------------------

void ForwardBar::setTitleFormat(const QString& format)
{
    pimpl->preview->setTitleFormat(format);
}

//--------------------------------------------------------------------------

QString ForwardBar::titleFormat() const
{
    return pimpl->preview->titleFormat();
}

//--------------------------------------------------------------------------

void ForwardBar::setTextTrimLength(int length)
{
    pimpl->preview->setTextTrimLength(length);
}

//--------------------------------------------------------------------------

int ForwardBar::textTrimLength() const
{
    return pimpl->preview->textTrimLength();
}

//--------------------------------------------------------------------------

void ForwardBar::setMessageCount(int count)
{
    pimpl->messageCount=count;
    refresh();
}

//--------------------------------------------------------------------------

int ForwardBar::messageCount() const
{
    return pimpl->messageCount;
}

//--------------------------------------------------------------------------

void ForwardBar::setCountFormat(const QString& format)
{
    pimpl->countFormat=format;
    refresh();
}

//--------------------------------------------------------------------------

QString ForwardBar::countFormat() const
{
    return pimpl->countFormat;
}

//--------------------------------------------------------------------------

IconTextButton* ForwardBar::configureButton() const
{
    return pimpl->configureButton;
}

//--------------------------------------------------------------------------

IconTextButton* ForwardBar::cancelButton() const
{
    return pimpl->cancelButton;
}

//--------------------------------------------------------------------------

void ForwardBar::refresh()
{
    // task-message-forwarding.md: "If there are multiple messages selected for forwarding then
    // message preview differs. It shows only number of messages to forward."
    const bool multi=pimpl->messageCount>1;

    if (multi)
    {
        QString text;
        if (!pimpl->countFormat.isEmpty())
        {
            text=pimpl->countFormat.arg(pimpl->messageCount);
        }
        else
        {
            // Re-evaluated against the CURRENT count every time -- see setCountFormat()'s own
            // doc comment for why a custom format cannot get the same plural treatment.
            text=tr("%n messages to forward","",pimpl->messageCount);
        }
        pimpl->countLabel->setText(text);
    }

    pimpl->countLabel->setVisible(multi);
    pimpl->preview->setVisible(!multi);
}

//--------------------------------------------------------------------------

}
