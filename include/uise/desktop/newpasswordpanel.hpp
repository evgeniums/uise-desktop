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

/** @file uise/desktop/newpasswordpanel.hpp
*
*  Declares NewPasswordPanel.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_NEW_PASSWORD_PANEL_HPP
#define UISE_DESKTOP_NEW_PASSWORD_PANEL_HPP

#include <memory>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/widget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class Toast;
class NewPasswordPanel_p;

class UISE_DESKTOP_EXPORT AbstractNewPasswordPanel : public WidgetQFrame
{
    Q_OBJECT

    public:

        using GeneratorFn=std::function<QString()>;

        using WidgetQFrame::WidgetQFrame;

        virtual void setPasswordTitle(const QString& message)=0;

        virtual void setRepeatPasswordTitle(const QString& message)=0;

        virtual QString password() const=0;

        virtual void setError(const QString& message)=0;

        virtual void setPasswordFocus()=0;

        virtual void reset()=0;

        void setPasswordGenerator(GeneratorFn gen) noexcept
        {
            m_generator=gen;
        }

        GeneratorFn passwordGenerator() const noexcept
        {
            return m_generator;
        }

        void setGlobalToast(Toast* toast)
        {
            m_globalToast=toast;
        }

        Toast* globalToast() const noexcept
        {
            return m_globalToast;
        }

        virtual GeneratorFn defaultPasswordGenerator() const =0;

        virtual bool checkPassword() =0;

    signals:

        void passwordEntered();

    private:

        GeneratorFn m_generator;
        Toast* m_globalToast=nullptr;
};

class UISE_DESKTOP_EXPORT NewPasswordPanel : public AbstractNewPasswordPanel
{
    Q_OBJECT

    public:

        constexpr static const uint32_t DefaultMinPasswordLength=6;
        constexpr static const uint32_t DefaultMaxPasswordLength=12;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        NewPasswordPanel(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~NewPasswordPanel();

        NewPasswordPanel(const NewPasswordPanel&)=delete;
        NewPasswordPanel(NewPasswordPanel&&)=delete;
        NewPasswordPanel& operator=(const NewPasswordPanel&)=delete;
        NewPasswordPanel& operator=(NewPasswordPanel&&)=delete;

        virtual void setPasswordTitle(const QString& message) override;

        virtual void setRepeatPasswordTitle(const QString& message) override;

        virtual QString password() const override;

        virtual void setError(const QString& message) override;

        virtual void setPasswordFocus() override;

        virtual void reset() override;

        virtual void construct() override;

        void setDefaultGenMinLength(size_t min)
        {
            m_defaultGenMinLength=min;
        }

        void setDefaultGenMaxLength(size_t max)
        {
            m_defaultGenMaxLength=DefaultMaxPasswordLength;
        }

        size_t defaultGenMinLength() const
        {
            return m_defaultGenMinLength;
        }

        size_t defaultGenMaxLength() const
        {
            return m_defaultGenMaxLength;
        }

        virtual GeneratorFn defaultPasswordGenerator() const override;

        bool checkPassword() override;

    public slots:

        void setMaskMode(bool enable);

    private:

        void checkPasswordThenEmit();


        void setPasswordTitleImpl(const QString& message);
        void setRepeatPasswordTitleImpl(const QString& message);

        std::unique_ptr<NewPasswordPanel_p> pimpl;

        size_t m_defaultGenMinLength=DefaultMinPasswordLength;
        size_t m_defaultGenMaxLength=DefaultMaxPasswordLength;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_NEW_PASSWORD_PANEL_HPP
