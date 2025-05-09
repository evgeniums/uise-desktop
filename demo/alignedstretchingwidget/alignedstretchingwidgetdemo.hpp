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

/** @file uise/desktop/demo/alignedstretchingwidgetdemo/alignedstretchingwidgetdemo.hpp
*
*  Demo widget of AlignedStretchingWidget.
*
*/

/****************************************************************************/

#include <QTabWidget>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>

#include <QDebug>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/alignedstretchingwidget.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

class AlignedStretchingWidgetDemo : public QTabWidget
{
    Q_OBJECT

    public:

        AlignedStretchingWidgetDemo(QWidget* parent=nullptr)
            : QTabWidget(parent),
              m_horizontalMin(nullptr),
              m_horizontalMax(nullptr),
              m_verticalMin(nullptr),
              m_verticalMax(nullptr),
              m_verticalFrame(nullptr),
              m_horizontalFrame(nullptr)
        {
            m_horizontalFrame=fillTab(Qt::Vertical);
            m_horizontalFrame->setObjectName("m_horizontalFrame");
            addTab(m_horizontalFrame,"Horizontal");

            m_verticalFrame=fillTab(Qt::Horizontal);
            addTab(m_verticalFrame,"Verical");
            m_horizontalFrame->setObjectName("m_verticalFrame");

            setStyleSheet("\n uise--AlignedStretchingWidget {padding: 8px 4px; background: #0000FF;} \n QTextBrowser {background: yellow}");
        }

        QFrame* fillTab(Qt::Orientation orientation)
        {
            bool horizontal=orientation==Qt::Horizontal;

            auto frame=new QFrame(this);
            QBoxLayout* layout=Layout::box(frame,orientation);

            auto left=addWidget(orientation,layout,Qt::AlignLeft);
            std::ignore=addWidget(orientation,layout,Qt::AlignHCenter);
            std::ignore=addWidget(orientation,layout,Qt::AlignRight);

            auto orthOrientation=horizontal?Qt::Vertical:Qt::Horizontal;

            auto minSlider=new QSlider(this);
            layout->addWidget(minSlider);
            minSlider->setOrientation(orthOrientation);

            auto maxSlider=new QSlider(this);
            layout->addWidget(maxSlider);
            maxSlider->setOrientation(orthOrientation);

            if (!horizontal)
            {
                m_horizontalMin=minSlider;
                m_horizontalMax=maxSlider;

                m_horizontalMin->setObjectName("m_horizontalMin");
                m_horizontalMax->setObjectName("m_horizontalMax");
            }
            else
            {
                m_verticalMin=minSlider;
                m_verticalMax=maxSlider;

                m_verticalMin->setObjectName("m_verticalMin");
                m_verticalMax->setObjectName("m_verticalMax");

                m_verticalMin->setInvertedAppearance(true);
                m_verticalMax->setInvertedAppearance(true);
            }

            auto connectSlider=[this](QSlider* slider)
            {
                connect(
                    slider,&QSlider::valueChanged,
                    [this,slider](int value)
                    {
                        sliderUpdated(slider,value);
                    }
                );
            };
            connectSlider(minSlider);
            connectSlider(maxSlider);

            auto initSlidersTimer=new SingleShotTimer(this);
            initSlidersTimer->shot(0,
                [this,minSlider,maxSlider,frame,left,horizontal]()
                {
                    maxSlider->setMinimum(0);
                    maxSlider->setMaximum(1024);
                    maxSlider->setValue(maxSlider->maximum()/2);

                    minSlider->setMinimum(0);
                    minSlider->setMaximum(1024);
                    minSlider->setValue(minSlider->minimum());
                }
            );

            return frame;
        }

        AlignedStretchingWidget* addWidget(Qt::Orientation orientation, QBoxLayout* layout, Qt::Alignment alignment)
        {
            auto widget=new QTextBrowser();
            auto testWidget=new AlignedStretchingWidget();
            auto orthOrientation=Qt::Vertical;

            if (orientation==Qt::Horizontal)
            {
                orthOrientation=Qt::Vertical;

                switch (alignment)
                {
                    case Qt::AlignLeft:
                        alignment=Qt::AlignTop;
                    break;

                    case Qt::AlignRight:
                        alignment=Qt::AlignBottom;
                    break;

                    case Qt::AlignHCenter:
                        alignment=Qt::AlignVCenter;
                    break;

                    default:
                    break;
                }

                m_verticalWidgets.push_back(testWidget);
            }
            else
            {
                orthOrientation=Qt::Horizontal;
                m_horizontalWidgets.push_back(testWidget);
            }

            switch (alignment)
            {
                case Qt::AlignLeft:
                    testWidget->setObjectName("Left");
                break;

                case Qt::AlignRight:
                    testWidget->setObjectName("Right");
                break;

                case Qt::AlignHCenter:
                    testWidget->setObjectName("HCenter");
                break;

                case Qt::AlignTop:
                    testWidget->setObjectName("Top");
                break;

                case Qt::AlignBottom:
                    testWidget->setObjectName("Bottom");
                break;

                case Qt::AlignVCenter:
                    testWidget->setObjectName("VCenter");
                break;

                default:
                break;
            }

            testWidget->setWidget(widget,orthOrientation,alignment);

            layout->addWidget(testWidget);
            return testWidget;
        }

        void updateHorizontalWidgets()
        {
            for (auto&& widget:m_horizontalWidgets)
            {
                widget->updateMinMaxSize();
            }
        }

        void updateVerticalWidgets()
        {
            for (auto&& widget:m_verticalWidgets)
            {
                widget->updateMinMaxSize();
            }
        }

        void sliderUpdated(QSlider* slider, int value)
        {
            if (slider==m_horizontalMin)
            {
                if (m_horizontalMax!=nullptr)
                {
                    if (m_horizontalMin->value()>m_horizontalMax->value())
                    {
                        m_horizontalMin->setValue(m_horizontalMax->value());
                        return;
                    }
                }

                for (auto&& widget:m_horizontalWidgets)
                {
                    widget->widget()->setMinimumWidth(value);
                }
                updateHorizontalWidgets();
            }
            else if (slider==m_horizontalMax)
            {
                if (m_horizontalMin!=nullptr)
                {
                    if (m_horizontalMin->value()>m_horizontalMax->value())
                    {
                        m_horizontalMax->setValue(m_horizontalMin->value());
                        return;
                    }
                }

                for (auto&& widget:m_horizontalWidgets)
                {
                    widget->widget()->setMaximumWidth(value);
                }
                updateHorizontalWidgets();
            }
            else if (slider==m_verticalMin)
            {
                if (m_verticalMax!=nullptr)
                {
                    if (m_verticalMin->value()>m_verticalMax->value())
                    {
                        m_verticalMin->setValue(m_verticalMax->value());
                        return;
                    }
                }

                for (auto&& widget:m_verticalWidgets)
                {
                    widget->widget()->setMinimumHeight(value);
                }
                updateVerticalWidgets();
            }
            else if (slider==m_verticalMax)
            {
                if (m_verticalMin!=nullptr)
                {
                    if (m_verticalMin->value()>m_verticalMax->value())
                    {
                        m_verticalMax->setValue(m_verticalMin->value());
                        return;
                    }
                }

                for (auto&& widget:m_verticalWidgets)
                {
                    widget->widget()->setMaximumHeight(value);
                }
                updateVerticalWidgets();
            }
        }

    private:

        QFrame* m_horizontalFrame;
        QFrame* m_verticalFrame;

        QSlider* m_horizontalMin;
        QSlider* m_horizontalMax;
        QSlider* m_verticalMin;
        QSlider* m_verticalMax;

        std::vector<AlignedStretchingWidget*> m_horizontalWidgets;
        std::vector<AlignedStretchingWidget*> m_verticalWidgets;
};

//--------------------------------------------------------------------------
