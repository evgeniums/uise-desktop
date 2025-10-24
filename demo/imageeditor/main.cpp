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
#include <QLabel>
#include <QPushButton>
#include <QGuiApplication>
#include <QScreen>

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

    auto contentFrame=new QFrame(mainFrame);
    auto contentL = Layout::horizontal(contentFrame);
    l->addWidget(contentFrame,1);

    auto editorFrame=new QFrame(contentFrame);
    auto editorL = Layout::vertical(editorFrame);
    contentL->addWidget(editorFrame);
    auto viewerFrame=new QFrame(contentFrame);
    auto viewerL = Layout::vertical(viewerFrame);
    contentL->addWidget(viewerFrame);

    auto editorCtrl=Style::instance().widgetFactory()->makeWidget<AbstractImageEditor>(editorFrame);
    Q_ASSERT(editorCtrl);
    editorCtrl->setEllipseCropPreview(true);
    editorCtrl->setMaximumImageSize(QSize(1000,1000));
    editorL->addWidget(editorCtrl->qWidget(),1);

    auto viewer=new QLabel(viewerFrame);
    viewerL->addWidget(viewer,1);

    QFrame* configFrame=new QFrame();
    l->addWidget(configFrame);
    auto cl=new QHBoxLayout(configFrame);

    auto square=new QCheckBox("Square crop",configFrame);
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

    auto ellipse=new QCheckBox("Ellipse crop preview",configFrame);
    ellipse->setChecked(true);
    cl->addWidget(ellipse);
    QObject::connect(
        ellipse,
        &QCheckBox::toggled,
        editorCtrl->qWidget(),
        [&](bool enable)
        {
            editorCtrl->setEllipseCropPreview(enable);
        }
    );

    auto aspectRatio=new QCheckBox("Keep aspect ratio crop",configFrame);
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

    auto takeImage=new QPushButton("Take image",configFrame);
    cl->addWidget(takeImage);
    QObject::connect(
        takeImage,
        &QPushButton::clicked,
        editorCtrl->qWidget(),
        [=]()
        {
            auto px=editorCtrl->editedImage();
            const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
            px.setDevicePixelRatio(pixelRatio);
            viewer->setPixmap(px);
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
