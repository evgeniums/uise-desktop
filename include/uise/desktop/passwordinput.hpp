/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/passwordinput.hpp
*
*  Declares PasswordInput.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PASSWORD_INPUT_HPP
#define UISE_DESKTOP_PASSWORD_INPUT_HPP

#include <memory>

#include <QLineEdit>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class SvgIcon;

class UISE_DESKTOP_EXPORT AbstractPasswordInput : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual QString password() const =0;
        virtual void reset() =0;

        virtual void setApplyButtonVisible(bool enable) {std::ignore=enable;}

        virtual bool isApplyButtonVisible() const {return false;}

        virtual void setApplyButtonContent(const QString& text, std::shared_ptr<SvgIcon> icon={})
        {
            std::ignore=text;
            std::ignore=icon;
        }

        virtual QString title() const =0;
        virtual bool isTitleVisible() const =0;

    public slots:

        virtual void setTitle(const QString& title) =0;
        virtual void setTitleVisible(bool enable) =0;

    signals:

        void passwordEntered();
};

class PasswordInput_p;

class UISE_DESKTOP_EXPORT PasswordInput : public AbstractPasswordInput
{
    Q_OBJECT

    public:

        PasswordInput(QWidget* parent=nullptr);
        ~PasswordInput();

        PasswordInput(const PasswordInput&)=delete;
        PasswordInput(PasswordInput&&)=delete;
        PasswordInput& operator=(const PasswordInput&)=delete;
        PasswordInput& operator=(PasswordInput&&)=delete;

        QLineEdit* editor() const;

        bool isUnmaskButtonChecked() const;
        bool isUnmaskButtonVisible() const;
        bool isClearButtonEnabled() const;

        QString password() const override;

        void reset() override;

        virtual void setApplyButtonVisible(bool enable) override;

        virtual bool isApplyButtonVisible() const override;

        virtual void setApplyButtonContent(const QString& text, std::shared_ptr<SvgIcon> icon={}) override;

        QString title() const override;
        bool isTitleVisible() const override;

    signals:

        void returnPressed();

        void unmaskButtonToggled(bool enable);

    public slots:

        void setUnmaskButtonChecked(bool enable);
        void setUnmaskButtonVisible(bool enable);
        void setClearButtonEnabled(bool enable);

        void setTitle(const QString& title) override;
        void setTitleVisible(bool enable) override;

    private:

        std::unique_ptr<PasswordInput_p> pimpl;
};

}

#endif // UISE_DESKTOP_PASSWORD_INPUT_HPP
