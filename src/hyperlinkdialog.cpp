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

/** @file uise/desktop/src/hyperlinkdialog.cpp
*
*  Defines HyperlinkDialog.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/validatedinput.hpp>
#include <uise/desktop/hyperlinkdialog.hpp>

#include <uise/desktop/ipp/dialog.ipp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**************************** HyperlinkDialog ***********************************/

//--------------------------------------------------------------------------

class HyperlinkDialog_p
{
    public:

        ValidatedInput* urlInput=nullptr;
        ValidatedInput* titleInput=nullptr;
        QLabel* errorLabel=nullptr;
};

//--------------------------------------------------------------------------

HyperlinkDialog::HyperlinkDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<HyperlinkDialog_p>())
{
}

//--------------------------------------------------------------------------

HyperlinkDialog::~HyperlinkDialog()
{
}

//--------------------------------------------------------------------------

void HyperlinkDialog::construct()
{
    auto content=new QFrame(this);
    auto contentLayout=Layout::vertical(content);

    auto urlLabel=new QLabel(tr("URL"),content);
    contentLayout->addWidget(urlLabel);

    pimpl->urlInput=new ValidatedInput(content);
    pimpl->urlInput->setObjectName("urlInput");
    pimpl->urlInput->setApplyButtonVisible(false);
    pimpl->urlInput->setPlaceholderText(tr("https://example.com"));
    // ValidatedInput::text()/onTextChanged() dereference the validator unconditionally, so one
    // MUST be set even though a URL has no format this dialog wants to enforce beyond "no raw
    // whitespace" -- a URL containing a literal space breaks Qt's markdown writer unless
    // angle-bracketed (measured), and this dialog rejects rather than attempts that workaround.
    // Anchored automatically by QRegularExpressionValidator, so "" (Acceptable, empty) and any
    // non-whitespace run are both allowed; a space partway through leaves the match Intermediate,
    // which text() reports as std::nullopt.
    pimpl->urlInput->setValidator(
        std::make_shared<QRegularExpressionValidator>(QRegularExpression(QStringLiteral("\\S*")))
    );
    contentLayout->addWidget(pimpl->urlInput);

    auto titleLabel=new QLabel(tr("Title (optional)"),content);
    contentLayout->addWidget(titleLabel);

    pimpl->titleInput=new ValidatedInput(content);
    pimpl->titleInput->setObjectName("titleInput");
    pimpl->titleInput->setApplyButtonVisible(false);
    pimpl->titleInput->setPlaceholderText(tr("Link text"));
    // Unlike the URL field, a title is free-form text and may contain spaces -- accept anything
    // (still required, for the same not-null-validator reason as above).
    pimpl->titleInput->setValidator(
        std::make_shared<QRegularExpressionValidator>(QRegularExpression(QStringLiteral(".*")))
    );
    contentLayout->addWidget(pimpl->titleInput);

    pimpl->errorLabel=new QLabel(content);
    pimpl->errorLabel->setObjectName("error");
    pimpl->errorLabel->setWordWrap(true);
    pimpl->errorLabel->setVisible(false);
    contentLayout->addWidget(pimpl->errorLabel);

    setWidget(content);
    setTitle(tr("Insert link"));

    setButtons(
        {
            AbstractDialog::standardButton(AbstractDialog::StandardButton::OK,this),
            AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this)
        }
    );

    connect(
        this,
        &Base::closeRequested,
        this,
        [this]()
        {
            reset();
        }
    );

    // Same rule as ReplyDialog/PasswordDialog's own OK/Apply handler: accepting never closes the
    // dialog itself -- the host does that from its linkAccepted() handler, once it has applied
    // the link to the editor.
    connect(
        this,
        &Base::buttonClicked,
        this,
        [this](int id)
        {
            if (isButton(id,StandardButton::OK) || isButton(id,StandardButton::Accept) || isButton(id,StandardButton::Apply))
            {
                const auto rawUrl=url().trimmed();
                if (rawUrl.isEmpty())
                {
                    setError(tr("Enter a URL."));
                    return;
                }

                setError(QString{});

                const auto rawTitle=linkTitle().trimmed();
                emit linkAccepted(rawUrl,rawTitle.isEmpty() ? rawUrl : rawTitle);
            }
        }
    );
}

//--------------------------------------------------------------------------

void HyperlinkDialog::setUrl(const QString& url)
{
    pimpl->urlInput->setText(url);
}

//--------------------------------------------------------------------------

QString HyperlinkDialog::url() const
{
    return pimpl->urlInput->text().value_or(QString{});
}

//--------------------------------------------------------------------------

void HyperlinkDialog::setLinkTitle(const QString& title)
{
    pimpl->titleInput->setText(title);
}

//--------------------------------------------------------------------------

QString HyperlinkDialog::linkTitle() const
{
    return pimpl->titleInput->text().value_or(QString{});
}

//--------------------------------------------------------------------------

void HyperlinkDialog::setError(const QString& message)
{
    pimpl->errorLabel->setText(message);
    pimpl->errorLabel->setVisible(!message.isEmpty());
}

//--------------------------------------------------------------------------

void HyperlinkDialog::reset()
{
    pimpl->urlInput->clear();
    pimpl->titleInput->clear();
    setError(QString{});
}

//--------------------------------------------------------------------------

void HyperlinkDialog::setDialogFocus()
{
    pimpl->urlInput->editorWidget()->setFocus();
}

//--------------------------------------------------------------------------

}
