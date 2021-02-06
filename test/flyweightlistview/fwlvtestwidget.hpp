/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/fwlvtestwidget.cpp
*
*  Test widgety of FlyweightListView.
*
*/

/****************************************************************************/

#include <QMainWindow>
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
#include <uise/test/uise-test.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

UISE_TEST_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class FwlvTestWidget_p;

class FwlvTestWidget : public QFra
{
    Q_OBJECT

    public:

      FwlvTestWidget(QWidget* parent=nullptr);

    private:

      std::uniqu
};

//--------------------------------------------------------------------------
UISE_TEST_NAMESPACE_END
