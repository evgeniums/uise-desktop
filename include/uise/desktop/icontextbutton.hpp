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

/** @file uise/desktop/icontextbutton.hpp
*
*  Declares IconTextButton.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ICONTEXTBUTTON_HPP
#define UISE_DESKTOP_ICONTEXTBUTTON_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/svgicon.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class RoundedImage;

class UISE_DESKTOP_EXPORT IconTextButton : public QFrame
{
    Q_OBJECT

    public:

        enum class IconPosition
        {
            Invisible,
            BeforeText,
            AfterText,
            AboveText,
            BelowText
        };

        IconTextButton(std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText);

        IconTextButton(QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText) : IconTextButton(std::shared_ptr<SvgIcon>{},parent,iconPosition)
        {}

        IconTextButton(const QString text, QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText) : IconTextButton(std::shared_ptr<SvgIcon>{},parent,iconPosition)
        {
            setText(text);
        }

        IconTextButton(const QString text, std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText) : IconTextButton(std::move(icon),parent,iconPosition)
        {
            setText(text);
        }

        void setSvgIcon(std::shared_ptr<SvgIcon> icon);

        std::shared_ptr<SvgIcon> svgIcon() const;

        void setParentHovered(bool enable);

        bool isParentHovered() const
        {
            return m_parentHovered;
        }

        bool isChecked() const
        {
            return m_checked;
        }

        void setCheckable(bool enable)
        {
            m_checkable=enable;
        }

        bool isCheckable() const
        {
            return m_checkable;
        }

        void setText(const QString& text);

        QString text() const;

        void setIconPosition(IconPosition iconPosition);

        IconPosition iconPosition() const noexcept
        {
            return m_iconPosition;
        }

        void setTextInteractionFlags(Qt::TextInteractionFlags flags);
        Qt::TextInteractionFlags textInteractionFlags() const;

    signals:

        void clicked();
        void toggled(bool enable);
        void hovered(bool enable);

    public slots:

        void click();

        void toggle();

        void setChecked(bool enable);

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

    private:

        void setHovered(bool enable);

        IconPosition m_iconPosition;

        QBoxLayout* m_layout;
        RoundedImage* m_icon;
        QLabel* m_text;

        bool m_parentHovered;
        bool m_checked;
        bool m_checkable;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ICONTEXTBUTTON_HPP
