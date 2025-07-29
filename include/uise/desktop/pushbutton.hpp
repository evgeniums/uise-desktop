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

/** @file uise/desktop/pushm_button.hpp
*
*  Declares PushButton.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PUSHBUTTON_HPP
#define UISE_DESKTOP_PUSHBUTTON_HPP

#include <QFrame>
#include <QPushButton>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/svgicon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT PushButton : public QFrame
{
    Q_OBJECT

    public:

        PushButton(std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr)
            : QFrame(parent),
              m_icon(std::move(icon)),
              m_parentHovered(false)
        {
            auto l=Layout::vertical(this);
            m_button=new QPushButton(this);
            l->addWidget(m_button);

            if (m_icon)
            {
                setIcon(m_icon->icon());
            }

            connect(m_button,&QPushButton::clicked,this,&PushButton::clicked);
            connect(m_button,&QPushButton::toggled,this,&PushButton::toggled);
        }

        PushButton(QWidget* parent=nullptr) : PushButton(std::shared_ptr<SvgIcon>{},parent)
        {}

        PushButton(const QString text, QWidget* parent=nullptr) : PushButton(std::shared_ptr<SvgIcon>{},parent)
        {
            setText(text);
        }

        PushButton(const QString text, std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr) : PushButton(std::move(icon),parent)
        {
            setText(text);
        }

        void setSvgIcon(std::shared_ptr<SvgIcon> icon)
        {
            m_icon=std::move(icon);
            if (m_icon)
            {
                setIcon(m_icon->icon());
            }
            else
            {
                setIcon(QIcon{});
            }
        }

        std::shared_ptr<SvgIcon> svgIcon() const
        {
            return m_icon;
        }

        void setParentHovered(bool enable);

        bool isParentHovered() const
        {
            return m_parentHovered;
        }

        void setChecked(bool enable);

        bool isChecked() const
        {
            return m_button->isChecked();
        }

        void setCheckable(bool enable)
        {
            m_button->setCheckable(enable);
        }

        bool isCheckable() const
        {
            return m_button->isCheckable();
        }

        void setIcon(const QIcon& icon)
        {
            m_button->setIcon(icon);
        }

        QIcon icon() const
        {
            return m_button->icon();
        }

        void setText(const QString& text)
        {
            m_button->setText(text);
        }

        QString text() const
        {
            return m_button->text();
        }

    signals:

        void clicked();
        void toggled(bool enable);

    public slots:

        void click()
        {
            m_button->click();
        }

        void toggle()
        {
            m_button->toggle();
        }

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        QPushButton* m_button;
        std::shared_ptr<SvgIcon> m_icon;
        bool m_parentHovered;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PUSHBUTTON_HPP
