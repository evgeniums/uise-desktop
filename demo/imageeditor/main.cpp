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
#include <QCheckBox>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/simpleimageeditor.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l = Layout::vertical(mainFrame);

    auto editorCtrl=Style::instance().widgetFactory()->makeWidget<AbstractImageEditor>();
    Q_ASSERT(editorCtrl);
    editorCtrl->setEllipseCrop(true);
    l->addWidget(editorCtrl->qWidget(),1);

    QFrame* configFrame=new QFrame();
    l->addWidget(configFrame);
    auto cl=new QHBoxLayout(configFrame);

    auto square=new QCheckBox("Square",configFrame);
    square->setChecked(true);
    cl->addWidget(square);
    QObject::connect(
        square,
        &QCheckBox::toggled,
        editorCtrl->qWidget(),
        [&](bool enable)
        {
            editorCtrl->setSquareCrop(enable);
        }
    );

    auto ellipse=new QCheckBox("Ellipse",configFrame);
    ellipse->setChecked(true);
    cl->addWidget(ellipse);
    QObject::connect(
        ellipse,
        &QCheckBox::toggled,
        editorCtrl->qWidget(),
        [&](bool enable)
        {
            editorCtrl->setEllipseCrop(enable);
        }
    );

    auto aspectRatio=new QCheckBox("Keep aspect ratio",configFrame);
    aspectRatio->setChecked(false);
    cl->addWidget(aspectRatio);
    QObject::connect(
        aspectRatio,
        &QCheckBox::toggled,
        editorCtrl->qWidget(),
        [&](bool enable)
        {
            editorCtrl->setKeepAspectRatio(enable);
        }
    );

    auto filenameEditable=new QCheckBox("Filename editable",configFrame);
    filenameEditable->setChecked(true);
    cl->addWidget(filenameEditable);
    QObject::connect(
        filenameEditable,
        &QCheckBox::toggled,
        editorCtrl->qWidget(),
        [&](bool enable)
        {
            editorCtrl->setFilenameEditable(enable);
        }
    );

    auto filenameVisible=new QCheckBox("Filename visible",configFrame);
    filenameVisible->setChecked(true);
    cl->addWidget(filenameVisible);
    QObject::connect(
        filenameVisible,
        &QCheckBox::toggled,
        editorCtrl->qWidget(),
        [&](bool enable)
        {
            editorCtrl->setFilenameVisible(enable);
        }
    );

    cl->addStrut(1);

    w.setCentralWidget(mainFrame);
    w.resize(1200,800);
    w.setWindowTitle("Image Editor Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
