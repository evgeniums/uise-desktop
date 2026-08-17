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
#include <uise/desktop/abstractimageviewer.hpp>
#include <uise/desktop/imageviewer.hpp>
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

    // Escape closes the viewer; here (no wrapping dialog) that just quits the demo.
    QObject::connect(
        viewer->viewer(),
        &AbstractImageViewer::closeRequested,
        &app,
        &QApplication::quit
    );

    // Toggle between AbstractImageViewer::ControlsMode::Static (the default: toolbar below the
    // image) and ::Overlay (toolbar + prev/next float over the image and fade out when idle).
    auto modeButton=new QPushButton(mainFrame);
    modeButton->setText("Switch to Overlay Mode");
    l->addWidget(modeButton);
    QObject::connect(
        modeButton,
        &QPushButton::clicked,
        &w,
        [viewer,modeButton]()
        {
            auto* imageViewer=viewer->viewer();
            auto overlay=imageViewer->controlsMode()==AbstractImageViewer::ControlsMode::Overlay;
            if (overlay)
            {
                imageViewer->setControlsMode(AbstractImageViewer::ControlsMode::Static);
                modeButton->setText("Switch to Overlay Mode");
            }
            else
            {
                imageViewer->setControlsMode(AbstractImageViewer::ControlsMode::Overlay);
                modeButton->setText("Switch to Static Mode");
            }
        }
    );

    // Manual test rig for the trackpad/mouse zoom feature: pinch or Ctrl/Cmd+wheel over the
    // image, or drag once zoomed in to pan -- this label makes the live GraphicsViewZoom state
    // observable without a debugger. DirectoryImagesViewer always builds a concrete ImageViewer
    // under the hood (see directoryimagesviewer.cpp), so the cast here is safe.
    auto* imageViewer=qobject_cast<ImageViewer*>(viewer->viewer());
    auto zoomRow=new QFrame(mainFrame);
    auto zl=Layout::horizontal(zoomRow);
    auto zoomLabel=new QLabel(zoomRow);
    zoomLabel->setText("Zoom: 100%");
    zl->addWidget(zoomLabel);
    zl->addStretch(1);
    auto resetZoomButton=new QPushButton(zoomRow);
    resetZoomButton->setText("Reset zoom");
    zl->addWidget(resetZoomButton);
    l->addWidget(zoomRow);
    if (imageViewer!=nullptr)
    {
        QObject::connect(
            imageViewer,
            &ImageViewer::zoomChanged,
            zoomLabel,
            [zoomLabel](qreal zoomFactor)
            {
                zoomLabel->setText(QString("Zoom: %1%").arg(qRound(zoomFactor*100)));
            }
        );
        QObject::connect(
            resetZoomButton,
            &QPushButton::clicked,
            imageViewer,
            &ImageViewer::resetZoom
        );
    }

    w.setCentralWidget(mainFrame);
    w.resize(1000,700);
    w.setWindowTitle("Image Viewer Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
