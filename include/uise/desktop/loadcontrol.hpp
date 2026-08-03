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
#include <uise/desktop/abstractloadcontrol.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT LoadControl : public AbstractLoadControl
{
    Q_OBJECT

    public:

        //! Diameter of the drawn circle, as a ratio of the control's own (shorter) side.
        constexpr static const qreal CircleWidthRatio=0.6;
        //! Diameter of the icon, as a ratio of the circle (CircleWidthRatio), not of the
        //! control itself -- the icon shrinks/grows along with the circle.
        constexpr static const qreal IconRadiusRatio=0.65;

        LoadControl(QWidget* parent=nullptr);

    protected:

        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

        void updateState() override;
        void updateProgress() override;

    private:

        void updateIcon(const QString name={});

        std::shared_ptr<SvgIcon> m_icon;

        QFrame* m_sample;

        bool m_hovered;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LOAD_CONTROL_HPP
