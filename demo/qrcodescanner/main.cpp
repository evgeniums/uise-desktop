/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-comboed licenses.

*/

/****************************************************************************/

/** @file demo/umageeditor/main.cpp
*
*  Demo application of SImpleImageEDitor.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QFrame>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/qrcodescanner.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l = Layout::vertical(mainFrame);

    // auto contentFrame=new QFrame(mainFrame);
    // auto contentL = Layout::horizontal(contentFrame);
    // l->addWidget(contentFrame,1);

    // auto scannerFrame=new QFrame(contentFrame);
    // auto scannerL = Layout::vertical(scannerFrame);
    // contentL->addWidget(scannerFrame);

    auto scannerFrame=mainFrame;
    auto scannerL=l;
    auto scanner=new QrCodeScanner(scannerFrame);
    scanner->construct();
    scannerL->addWidget(scanner,1);

    // auto viewerFrame=new QFrame(contentFrame);
    // auto viewerL = Layout::vertical(viewerFrame);
    // contentL->addWidget(viewerFrame);

    w.setCentralWidget(mainFrame);
    w.resize(1200,800);
    w.setWindowTitle("QR Code Scanner Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
