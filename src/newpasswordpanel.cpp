/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/newpasswordpanel.cpp
*
*  Defines NewPasswordPanel.
*
*/

/****************************************************************************/

#include <string>
#include <random>
#include <algorithm>

#include <QLabel>
#include <QPushButton>
#include <QClipboard>
#include <QGuiApplication>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/passwordinput.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/newpasswordpanel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/****** defaultPasswordGenerator() code was assisted by a Google large language model **/

// Function to generate a random password with length between min and max
std::string defaultPasswordGenerator(int min_length, int max_length)
{
    // Define the character sets
    const std::string uppercase_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string lowercase_chars = "abcdefghijklmnopqrstuvwxyz";
    const std::string digit_chars = "0123456789";
    const std::string special_chars = "!@#$%^&*()-_=+[{]};:<>/?";

    // Combine all character sets into one pool
    const std::string all_chars = uppercase_chars + lowercase_chars + digit_chars + special_chars;

    // Use a cryptographically secure random number generator
    std::random_device rd;
    std::mt19937 generator(rd());

    // Create a distribution for the password length
    std::uniform_int_distribution<int> length_distribution(min_length, max_length);
    int length = length_distribution(generator);

    // Create a distribution for character selection
    std::uniform_int_distribution<> char_distribution(0, all_chars.size() - 1);

    std::string password;
    password.reserve(length);

    // Ensure the password contains at least one character of each type, if possible
    if (length >= 4) {
        password += uppercase_chars[char_distribution(generator) % uppercase_chars.size()];
        password += lowercase_chars[char_distribution(generator) % lowercase_chars.size()];
        password += digit_chars[char_distribution(generator) % digit_chars.size()];
        password += special_chars[char_distribution(generator) % special_chars.size()];
    }

    // Fill the rest of the password length with random characters from the combined pool
    for (int i = password.length(); i < length; ++i) {
        password += all_chars[char_distribution(generator)];
    }

    // Shuffle the generated password to randomize the order
    std::shuffle(password.begin(), password.end(), generator);

    return password;
}

/****************************** End defaultPasswordGenerator *********************/

/**************************** NewPasswordPanel ***********************************/

//--------------------------------------------------------------------------

class NewPasswordPanel_p
{
    public:

        QFrame* passwordFrame;
        PasswordInput* password;
        PasswordInput* repeatPassword;

        QFrame* buttonsFrame;

        QPushButton* genButton;
        QPushButton* copyButton;
        QPushButton* clearButton;

        QLabel* error;

        Toast* copyToast;

        bool maskMode=true;
};

//--------------------------------------------------------------------------

NewPasswordPanel::NewPasswordPanel(QWidget* parent)
    : AbstractNewPasswordPanel(parent),
      pimpl(std::make_unique<NewPasswordPanel_p>())
{
}

//--------------------------------------------------------------------------

void NewPasswordPanel::construct()
{
    auto l=Layout::vertical(this);

    pimpl->passwordFrame=new QFrame(this);
    pimpl->passwordFrame->setObjectName("passwordFrame");
    auto pl=Layout::horizontal(pimpl->passwordFrame);
    l->addWidget(pimpl->passwordFrame);

    pimpl->password=new PasswordInput(this);
    pimpl->password->setObjectName("password");
    pl->addWidget(pimpl->password);

    pimpl->repeatPassword=new PasswordInput(this);
    pimpl->repeatPassword->setObjectName("repeatPassword");
    pl->addWidget(pimpl->repeatPassword);

    pimpl->buttonsFrame=new QFrame(this);
    pimpl->buttonsFrame->setObjectName("buttonsFrame");
    auto bl=Layout::horizontal(pimpl->buttonsFrame);
    l->addWidget(pimpl->buttonsFrame);

    pimpl->genButton=new QPushButton(tr("Generate"),pimpl->buttonsFrame);
    pimpl->genButton->setObjectName("generate");
    pimpl->genButton->setCursor(Qt::PointingHandCursor);
    bl->addWidget(pimpl->genButton);
    pimpl->copyButton=new QPushButton(tr("Copy"),pimpl->buttonsFrame);
    pimpl->copyButton->setObjectName("copy");
    pimpl->copyButton->setCursor(Qt::PointingHandCursor);
    bl->addWidget(pimpl->copyButton);
    pimpl->clearButton=new QPushButton(tr("Clear"),pimpl->buttonsFrame);
    pimpl->clearButton->setObjectName("clear");
    pimpl->clearButton->setCursor(Qt::PointingHandCursor);
    bl->addWidget(pimpl->clearButton);
    bl->addStretch(1);

    pimpl->error=new QLabel(this);
    l->addWidget(pimpl->error);
    pimpl->error->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->error->setWordWrap(true);
    pimpl->error->setObjectName("error");

    connect(
        pimpl->password,
        &PasswordInput::returnPressed,
        this,
        [this]()
        {
            setError(QString{});
            checkPasswordThenEmit();
        }
    );

    connect(
        pimpl->repeatPassword,
        &PasswordInput::returnPressed,
        this,
        [this]()
        {
            setError(QString{});
            checkPasswordThenEmit();
        }
    );

    connect(
        pimpl->password->editor(),
        &QLineEdit::textChanged,
        this,
        [this](const QString&)
        {
            setError(QString{});
        }
    );

    connect(
        pimpl->repeatPassword->editor(),
        &QLineEdit::textChanged,
        this,
        [this](const QString&)
        {
            setError(QString{});
        }
    );

    connect(
        pimpl->copyButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            QClipboard *clipboard = QGuiApplication::clipboard();
            if (clipboard!=nullptr)
            {
                auto text=pimpl->password->password();
                if (!text.isEmpty())
                {
                    clipboard->setText(pimpl->password->password());
                    auto toast=globalToast();
                    if (toast==nullptr)
                    {
                        toast=pimpl->copyToast;
                    }
                    toast->show(tr("Copied to clipboard"));
                }
            }
        }
    );

    connect(
        pimpl->clearButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            reset();
        }
    );

    connect(
        pimpl->genButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            auto gen=passwordGenerator();
            if (gen)
            {
                auto passwd=gen();

                pimpl->password->editor()->setText(passwd);
                pimpl->repeatPassword->editor()->setText(passwd);
                pimpl->error->clear();

                setMaskMode(false);
            }
        }
    );

    connect(
        pimpl->password,
        &PasswordInput::unmaskButtonToggled,
        this,
        [this](bool enable)
        {
            setMaskMode(!enable);
        }
    );

    connect(
        pimpl->repeatPassword,
        &PasswordInput::unmaskButtonToggled,
        this,
        [this](bool enable)
        {
            setMaskMode(!enable);
        }
    );

    setPasswordTitleImpl(tr("Enter password"));
    setRepeatPasswordTitleImpl(tr("Repeat password"));

    pimpl->password->setTitleVisible(true);
    pimpl->repeatPassword->setTitleVisible(true);
    pimpl->password->setClearButtonEnabled(false);
    pimpl->repeatPassword->setClearButtonEnabled(false);

    setMaskMode(true);

    setPasswordGenerator(
        [this]()
        {
            auto str=UISE_DESKTOP_NAMESPACE::defaultPasswordGenerator(defaultGenMinLength(),defaultGenMaxLength());
            return QString::fromStdString(str);
        }
    );

    pimpl->copyToast=new Toast(this);
}

//--------------------------------------------------------------------------

NewPasswordPanel::~NewPasswordPanel()
{
}

//--------------------------------------------------------------------------

QString NewPasswordPanel::password() const
{
    return pimpl->password->editor()->text();
}

//--------------------------------------------------------------------------

void NewPasswordPanel::setPasswordTitle(const QString& message)
{
    setPasswordTitleImpl(message);
}

//--------------------------------------------------------------------------

void NewPasswordPanel::setRepeatPasswordTitle(const QString& message)
{
    setRepeatPasswordTitleImpl(message);
}

//--------------------------------------------------------------------------

void NewPasswordPanel::setPasswordTitleImpl(const QString& message)
{
    pimpl->password->setTitle(message);
}

//--------------------------------------------------------------------------

void NewPasswordPanel::setRepeatPasswordTitleImpl(const QString& message)
{
    pimpl->repeatPassword->setTitle(message);
}

//--------------------------------------------------------------------------

void NewPasswordPanel::setPasswordFocus()
{
    pimpl->password->editor()->setFocus(Qt::MouseFocusReason);
}

//--------------------------------------------------------------------------

void NewPasswordPanel::reset()
{
    setMaskMode(true);
    pimpl->password->editor()->clear();
    pimpl->repeatPassword->editor()->clear();
    pimpl->error->setText(" ");
    pimpl->password->editor()->setFocus();
}

//--------------------------------------------------------------------------

void NewPasswordPanel::setError(const QString& message)
{
    pimpl->error->setText(message);
    if (message.isEmpty())
    {
        pimpl->error->setText(" ");
    }
}

//--------------------------------------------------------------------------

void NewPasswordPanel::setMaskMode(bool enable)
{
    pimpl->maskMode=enable;

    pimpl->password->blockSignals(true);
    pimpl->repeatPassword->blockSignals(true);

    pimpl->password->setUnmaskButtonChecked(!enable);
    pimpl->repeatPassword->setUnmaskButtonChecked(!enable);

    pimpl->repeatPassword->setVisible(enable);
    pimpl->password->setUnmaskButtonVisible(!enable);    

    pimpl->password->blockSignals(false);
    pimpl->repeatPassword->blockSignals(false);

    // pimpl->password->setClearButtonEnabled(!enable);
}

//--------------------------------------------------------------------------

void NewPasswordPanel::checkPasswordThenEmit()
{
    if (!pimpl->maskMode)
    {
        emit passwordEntered();
        return;
    }

    if (pimpl->password->password()==pimpl->repeatPassword->password())
    {
        emit passwordEntered();
    }
    else
    {
        setError(tr("Values do not match"));
    }
}

//--------------------------------------------------------------------------

bool NewPasswordPanel::checkPassword()
{
    if (!pimpl->maskMode)
    {
        return true;
    }

    if (pimpl->password->password()==pimpl->repeatPassword->password())
    {
        return true;
    }
    setError(tr("Values do not match"));
    return false;
}

//--------------------------------------------------------------------------

AbstractNewPasswordPanel::GeneratorFn NewPasswordPanel::defaultPasswordGenerator() const
{
    AbstractNewPasswordPanel::GeneratorFn fn=[this]() -> QString
    {
        auto str=UISE_DESKTOP_NAMESPACE::defaultPasswordGenerator(defaultGenMinLength(),defaultGenMaxLength());
        return QString::fromStdString(str);
    };
    return fn;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
