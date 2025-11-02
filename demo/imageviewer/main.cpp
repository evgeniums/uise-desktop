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

/** @file demo/imageviewer/main.cpp
*
*  Demo application of ImageViewer.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QFrame>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGuiApplication>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/directoryimagesviewer.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l = Layout::vertical(mainFrame);

    auto viewer=new DirectoryImagesViewer(mainFrame);
    l->addWidget(viewer,1);

    w.setCentralWidget(mainFrame);
    w.resize(1000,700);
    w.setWindowTitle("Image Viewer Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
