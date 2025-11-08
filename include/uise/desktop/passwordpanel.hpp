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

/** @file uise/desktop/passwordpanel.hpp
*
*  Declares PasswordPanel.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PASSWORD_PANEL_HPP
#define UISE_DESKTOP_PASSWORD_PANEL_HPP

#include <memory>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/widget.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class PasswordPanel_p;

class UISE_DESKTOP_EXPORT AbstractPasswordPanel : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void setDescription(const QString& message)=0;

        virtual void setDescriptionVisible(bool enable) {std::ignore=enable;};

        virtual QString password() const=0;

        virtual void setError(const QString& message)=0;

        virtual void setPasswordFocus()=0;

        virtual void reset()=0;

        virtual void setApplyButtonVisible(bool enable) {std::ignore=enable;}

        virtual bool isApplyButtonVisible() const {return false;}

        virtual void setApplyButtonContent(const QString& text, std::shared_ptr<SvgIcon> icon={})
        {
            std::ignore=text;
            std::ignore=icon;
        }

    signals:

        void passwordEntered();
};

class UISE_DESKTOP_EXPORT PasswordPanel : public AbstractPasswordPanel
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        PasswordPanel(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~PasswordPanel();

        PasswordPanel(const PasswordPanel&)=delete;
        PasswordPanel(PasswordPanel&&)=delete;
        PasswordPanel& operator=(const PasswordPanel&)=delete;
        PasswordPanel& operator=(PasswordPanel&&)=delete;

        virtual void setDescription(const QString& message) override;
        virtual QString password() const override;

        virtual void setError(const QString& message) override;

        virtual void setPasswordFocus() override;

        virtual void reset() override;

        virtual void construct() override;

        virtual void setApplyButtonVisible(bool enable) override;

        virtual bool isApplyButtonVisible() const override;

        virtual void setApplyButtonContent(const QString& text, std::shared_ptr<SvgIcon> icon={}) override;

        virtual void setDescriptionVisible(bool enable) override;

    private:

        void setDescriptionImpl(const QString& message);

        std::unique_ptr<PasswordPanel_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PASSWORD_PANEL_HPP
