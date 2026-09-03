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

/** @file uise/desktop/src/replybar.cpp
*
*  Defines ReplyBar.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/abstractreplypreview.hpp>
#include <uise/desktop/replypreview.hpp>
#include <uise/desktop/replybar.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ReplyBar_p
{
    public:

        QBoxLayout* layout;

        IconTextButton* configureButton;
        AbstractReplyPreview* preview;
        IconTextButton* cancelButton;
};

//--------------------------------------------------------------------------

ReplyBar::ReplyBar(QWidget* parent)
    : AbstractReplyBar(parent),
      pimpl(std::make_unique<ReplyBar_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->configureButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("ReplyBar::configure"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->configureButton->setObjectName("configureButton");
    pimpl->configureButton->setText(QString());
    pimpl->configureButton->setCursor(Qt::PointingHandCursor);
    pimpl->configureButton->setToolTip(tr("Configure reply"));
    pimpl->layout->addWidget(pimpl->configureButton);
    connect(pimpl->configureButton,&IconTextButton::clicked,this,&AbstractReplyBar::configureRequested);

    pimpl->preview=makeWidget<AbstractReplyPreview,ReplyPreview>(this);
    pimpl->layout->addWidget(pimpl->preview,1);
    connect(pimpl->preview,&AbstractReplyPreview::clicked,this,&AbstractReplyBar::clicked);

    pimpl->cancelButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("ReplyBar::cancel"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->cancelButton->setObjectName("cancelButton");
    pimpl->cancelButton->setText(QString());
    pimpl->cancelButton->setCursor(Qt::PointingHandCursor);
    pimpl->cancelButton->setToolTip(tr("Cancel reply"));
    pimpl->layout->addWidget(pimpl->cancelButton);
    connect(pimpl->cancelButton,&IconTextButton::clicked,this,&AbstractReplyBar::cancelRequested);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ReplyBar::~ReplyBar()
{}

//--------------------------------------------------------------------------

void ReplyBar::setReplyData(ReplyPreviewData data)
{
    pimpl->preview->setData(std::move(data));
}

//--------------------------------------------------------------------------

const ReplyPreviewData& ReplyBar::replyData() const
{
    return pimpl->preview->data();
}

//--------------------------------------------------------------------------

void ReplyBar::clear()
{
    pimpl->preview->clear();
}

//--------------------------------------------------------------------------

AbstractReplyPreview* ReplyBar::preview() const
{
    return pimpl->preview;
}

//--------------------------------------------------------------------------

void ReplyBar::setTextTrimLength(int length)
{
    pimpl->preview->setTextTrimLength(length);
}

//--------------------------------------------------------------------------

int ReplyBar::textTrimLength() const
{
    return pimpl->preview->textTrimLength();
}

//--------------------------------------------------------------------------

IconTextButton* ReplyBar::configureButton() const
{
    return pimpl->configureButton;
}

//--------------------------------------------------------------------------

IconTextButton* ReplyBar::cancelButton() const
{
    return pimpl->cancelButton;
}

//--------------------------------------------------------------------------

}
