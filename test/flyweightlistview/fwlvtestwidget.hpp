/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/fwlvtestwidget.hpp
*
*  Test widgety of FlyweightListView.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QTextBrowser>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>
#include <uise/desktop/flyweightlistitem.hpp>

#include <helloworlditem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

using FlwListType=FlyweightListView<HelloWorldItemWrapper>;

//--------------------------------------------------------------------------
class FwlvTestWidget_p
{
    public:

        std::map<size_t,size_t> items;

        FlwListType* view=nullptr;

        QSpinBox* insertFrom=nullptr;
        QSpinBox* insertNum=nullptr;
        QPushButton* insertButton=nullptr;

        QSpinBox* delFrom=nullptr;
        QSpinBox* delNum=nullptr;
        QPushButton* delButton=nullptr;

        QLineEdit* insertRandom=nullptr;
        QPushButton* insertRandomButton=nullptr;

        QLineEdit* delWidgets=nullptr;
        QPushButton* delWidgetsButton=nullptr;

        QSpinBox* resizeWidget=nullptr;
        QSpinBox* resizeWidgetVal=nullptr;
        QPushButton* resizeWidgetButton=nullptr;

        QSpinBox* jumpItem=nullptr;
        QSpinBox* jumpOffset=nullptr;
        QComboBox* jumpMode=nullptr;
        QPushButton* jumpToButton=nullptr;

        QPushButton* clearButton=nullptr;
        QPushButton* reloadButton=nullptr;
        QPushButton* orientationButton=nullptr;
        QPushButton* flyweightButton=nullptr;
        QComboBox* stickMode=nullptr;
};

//--------------------------------------------------------------------------
class FwlvTestWidget : public QFrame
{
    Q_OBJECT

    public:

        FwlvTestWidget(QWidget* parent=nullptr);

    public slots:

        void loadItems();

    public:

        std::unique_ptr<FwlvTestWidget_p> pimpl;

    private:

        void setup();
};

//--------------------------------------------------------------------------
UISE_DESKTOP_NAMESPACE_EMD
