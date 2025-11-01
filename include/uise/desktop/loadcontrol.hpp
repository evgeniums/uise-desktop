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

/** @file uise/desktop/loadcontrol.hpp
*
*  Declares LoadControl.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LOAD_CONTROL_HPP
#define UISE_DESKTOP_LOAD_CONTROL_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/utils/enums.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT LoadControl : public QFrame
{
    Q_OBJECT

    public:

        enum class State
        {
            None,
            CanDownload,
            CanUpload,
            Paused,
            Waiting,
            Running
        };

        constexpr static const qreal CircleWidthRatio=0.8;
        constexpr static const qreal IconRadiusRatio=0.8;

        LoadControl(QWidget* parent=nullptr);

        void setState(State state);

        State state() const noexcept
        {
            return m_state;
        }

        void setProgress(qreal value);

        template <typename T1, typename T2>
        void setProgress(T1 currentvalue, T2 total)
        {
            if (qFuzzyIsNull(total))
            {
                setProgress(0);
                return;
            }
            if (currentvalue>total)
            {
                setProgress(100.0);
                return;
            }
            auto progress=100.0*static_cast<qreal>(currentvalue)/static_cast<qreal>(total);
            setProgress(progress);
        }

        qreal progress() const noexcept
        {
            return m_progress;
        }

    signals:

        void clicked();

    protected:

        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:

        void updateIcon(const QString name={});

        std::shared_ptr<SvgIcon> m_icon;

        QFrame* m_sample;
        State m_state;

        qreal m_progress;
        bool m_hovered;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LOAD_CONTROL_HPP
