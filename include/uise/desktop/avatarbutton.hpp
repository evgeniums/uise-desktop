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

/** @file uise/desktop/avatarbutton.hpp
*
*  Declares AvatarButton.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AVATARBUTTON_HPP
#define UISE_DESKTOP_AVATARBUTTON_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class RoundedImage;
class AvatarWidget;
class AvatarSource;

class UISE_DESKTOP_EXPORT AvatarButton : public QFrame
{
    Q_OBJECT

    public:
    
        AvatarButton(QWidget* parent=nullptr);

        void setParentHovered(bool enable);

        bool isParentHovered() const noexcept
        {
            return m_parentHovered;
        }

        bool isChecked() const noexcept
        {
            return m_checked;
        }

        void setCheckable(bool enable) noexcept
        {
            m_checkable=enable;
        }

        bool isCheckable() const noexcept
        {
            return m_checkable;
        }

        void setText(const QString& text);
        QString text() const;

        void setTailSvgIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> tailSvgIcon() const;

        void setAvatarPath(WithPath path);
        const WithPath& avatarPath() const noexcept;

        void setAvatarSource(std::shared_ptr<AvatarSource> avatarSource);
        std::shared_ptr<AvatarSource> avatarSource() const;

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

        QBoxLayout* m_layout;
        AvatarWidget* m_avatar;
        RoundedImage* m_tailIcon;
        QLabel* m_text;

        bool m_parentHovered;
        bool m_checked;
        bool m_checkable;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_AVATARBUTTON_HPP
