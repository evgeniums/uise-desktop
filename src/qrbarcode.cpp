/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/qrcodescanner.cpp
*
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/qrcodescanner.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** QrCodeScanner ****************************/

//--------------------------------------------------------------------------

class QrCodeScanner_p
{
    public:

        QString defaultCamera;
};

//--------------------------------------------------------------------------

QrCodeScanner::QrCodeScanner(QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<QrCodeScanner_p>())
{
}

//--------------------------------------------------------------------------

void QrCodeScanner::construct()
{
}

//--------------------------------------------------------------------------

QrCodeScanner::~QrCodeScanner()
{}

//--------------------------------------------------------------------------

void QrCodeScanner::setDefaultCamera(const QString& camera)
{
    pimpl->defaultCamera=camera;
}

//--------------------------------------------------------------------------

QString QrCodeScanner::defaultCamera() const
{
    return pimpl->defaultCamera;
}

//--------------------------------------------------------------------------

void QrCodeScanner::start()
{
}

//--------------------------------------------------------------------------

void QrCodeScanner::stop()
{
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
