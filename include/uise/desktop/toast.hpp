/**
@copyright Evgeny Sidorov 2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/toast.hpp
*
*  Declares Toast.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TOAST_HPP
#define UISE_DESKTOP_TOAST_HPP

#include <memory>

#include <QWidget>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QBoxLayout>

#include <uise/desktop/uisedesktop.hpp>

class QGraphicsOpacityEffect;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class SvgIcon;
class RoundedImage;
class WithRoundedImage;

class UISE_DESKTOP_EXPORT Toast : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QString verticalPosition READ verticalPositionName WRITE setVerticalPositionName)
    Q_PROPERTY(QString horizontalPosition READ horizontalPositionName WRITE setHorizontalPositionName)
    Q_PROPERTY(int verticalOffset READ verticalOffset WRITE setVerticalOffset)
    Q_PROPERTY(int horizontalOffset READ horizontalOffset WRITE setHorizontalOffset)

    public:

        constexpr static const int DefaultDuration=1300;
        constexpr static const int DefaultWidth=300;
        constexpr static const int DefaultHeight=50;
        constexpr static const int DefaultMargin=30;
        constexpr static const int DefaultIconSize=24;
        constexpr static const int DefaultMaxWidth=600;
        constexpr static const int DefaultMinAutoWidth=120;

        //! @deprecated Use setVerticalPosition()/setHorizontalPosition() instead.
        //! Kept only so out-of-tree callers of setPosition(Position) keep compiling.
        enum Position
        {
            TopLeft,
            TopCenter,
            TopRight,
            BottomLeft,
            BottomCenter,
            BottomRight,
            Center
        };

        //! Vertical placement of the toast within its bounding rect (parent rect if
        //! setDrawInParent(true), otherwise the screen's available geometry).
        enum class VerticalPosition
        {
            Center,
            Top,
            Bottom
        };

        //! Horizontal placement of the toast within its bounding rect.
        enum class HorizontalPosition
        {
            Center,
            Left,
            Right
        };

        explicit Toast(const QString &message, int duration, QWidget *parent = nullptr);

        explicit Toast(const QString &message, QWidget *parent = nullptr);

        explicit Toast(QWidget *parent = nullptr);

        //! @deprecated See Position. Maps onto setVerticalPosition()/setHorizontalPosition()
        //! with the default DefaultMargin offset on any non-centered axis.
        void setPosition(Position position);

        void setVerticalPosition(VerticalPosition position) noexcept
        {
            m_verticalPosition=position;
        }

        void setVerticalPosition(VerticalPosition position, int offset) noexcept
        {
            m_verticalPosition=position;
            m_verticalOffset=offset;
        }

        VerticalPosition verticalPosition() const noexcept
        {
            return m_verticalPosition;
        }

        void setVerticalOffset(int offset) noexcept
        {
            m_verticalOffset=offset;
        }

        int verticalOffset() const noexcept
        {
            return m_verticalOffset;
        }

        void setHorizontalPosition(HorizontalPosition position) noexcept
        {
            m_horizontalPosition=position;
        }

        void setHorizontalPosition(HorizontalPosition position, int offset) noexcept
        {
            m_horizontalPosition=position;
            m_horizontalOffset=offset;
        }

        HorizontalPosition horizontalPosition() const noexcept
        {
            return m_horizontalPosition;
        }

        void setHorizontalOffset(int offset) noexcept
        {
            m_horizontalOffset=offset;
        }

        int horizontalOffset() const noexcept
        {
            return m_horizontalOffset;
        }

        //! QSS-friendly string form of VerticalPosition: "vcenter" | "top" | "bottom".
        void setVerticalPositionName(const QString& name);
        QString verticalPositionName() const;

        //! QSS-friendly string form of HorizontalPosition: "hcenter" | "left" | "right".
        void setHorizontalPositionName(const QString& name);
        QString horizontalPositionName() const;

        void show();

        void show(const QString& message)
        {
            setMessage(message);
            show();
        }

        void setDuration(int duration) noexcept
        {
            m_duration=duration;
        }

        int duration() const noexcept
        {
            return m_duration;
        }

        void setMessage(const QString &message);
        QString message() const;

        void setDeleteOnclose(bool enable) noexcept
        {
            m_deleteOnClose=enable;
        }

        bool isDeleteOnClose() const noexcept
        {
            return m_deleteOnClose;
        }

        void setDrawInParent(bool enable);

        bool isDrawInParent() const
        {
            return m_drawInParent;
        }

        //! Sets the optional icon shown to the left of the message. Pass a null pointer
        //! to hide the icon and restore the message's centered alignment.
        void setSvgIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> svgIcon() const;

        //! Logical (not device) pixel size of the icon. Overridden by any QSS min/max-width
        //! rule on "uise--Toast #icon uise--RoundedImage".
        void setIconSize(const QSize& size);

        QSize iconSize() const noexcept
        {
            return m_iconSize;
        }

        //! When enabled, show() sizes the toast to fit its content (icon + wrapped text),
        //! clamped to maxWidth() and to 90% of the bounding rect. Default is disabled, which
        //! preserves the historical fixed DefaultWidth x DefaultHeight geometry.
        void setAutoSize(bool enable) noexcept
        {
            m_autoSize=enable;
        }

        bool isAutoSize() const noexcept
        {
            return m_autoSize;
        }

        void setMaxWidth(int width) noexcept
        {
            m_maxWidth=width;
        }

        int maxWidth() const noexcept
        {
            return m_maxWidth;
        }

    protected:

        void paintEvent(QPaintEvent *event) override;

    private slots:

        void fadeOut();
        void finished();

    private:

        QSize autoSizeHint(const QRect& boundingRect);

        QHBoxLayout* m_layout;
        WithRoundedImage* m_iconFrame;
        RoundedImage* m_icon;
        QLabel* m_label;
        QTimer* m_timer;
        QPropertyAnimation* m_animation;
        int m_duration;
        VerticalPosition m_verticalPosition;
        HorizontalPosition m_horizontalPosition;
        int m_verticalOffset;
        int m_horizontalOffset;
        bool m_deleteOnClose;
        bool m_autoSize;
        int m_maxWidth;
        QSize m_iconSize;

        bool m_drawInParent;
        QGraphicsOpacityEffect* m_opacityEffect;
        qreal m_currentOpacity;
};

}

#endif // UISE_DESKTOP_TOAST_HPP
