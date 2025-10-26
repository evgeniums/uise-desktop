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

#include <QWidget>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

#include <uise/desktop/uisedesktop.hpp>

class QGraphicsOpacityEffect;

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT Toast : public QWidget
{
    Q_OBJECT

    public:

        constexpr static const int DefaultDuration=1300;
        constexpr static const int DefaultWidth=300;
        constexpr static const int DefaultHeight=50;
        constexpr static const int DefaultMargin=30;

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

        explicit Toast(const QString &message, int duration, QWidget *parent = nullptr);

        explicit Toast(const QString &message, QWidget *parent = nullptr);

        explicit Toast(QWidget *parent = nullptr);

        void setPosition(Position position);

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

    protected:

        void paintEvent(QPaintEvent *event) override;

    private slots:

        void fadeOut();
        void finished();

    private:

        QLabel* m_label;
        QTimer* m_timer;
        QPropertyAnimation* m_animation;
        int m_duration;
        Position m_position;
        bool m_deleteOnClose;

        bool m_drawInParent;
        QGraphicsOpacityEffect* m_opacityEffect;
        qreal m_currentOpacity;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_TOAST_HPP
