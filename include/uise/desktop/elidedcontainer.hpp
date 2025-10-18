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

/** @file uise/desktop/elidedcontainer.hpp
*
*  Declares ElidedContainer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ELIDED_CONTAINER_HPP
#define UISE_DESKTOP_ELIDED_CONTAINER_HPP

#include <vector>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/elidedlabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SingleShotTimer;
class ElidedLabel;

class UISE_DESKTOP_EXPORT ElidedContainer : public QFrame
{
    Q_OBJECT

    public:

        ElidedContainer(QWidget* parent=nullptr);

        void addWidget(
            QWidget* widget
        );

        void addElidedLabel(
            ElidedLabel* label
        );

        QSize sizeHint() const override;

        void refresh();

    protected:

        void resizeEvent(QResizeEvent* event) override;

        bool eventFilter(QObject *watched, QEvent *event) override;

    private slots:

        void onElidedTextUpdate();

    private:

        void updateMinimumHeight();
        void updateSize(const QSize& size);

        struct Widget
        {
            QPointer<ElidedLabel> label;
            QPointer<QWidget> widget;
            int width=0;
        };

        void onWidgetAdded(Widget w);

        std::vector<Widget> m_widgets;
        SingleShotTimer* m_updateTimer;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ELIDED_CONTAINER_HPP
