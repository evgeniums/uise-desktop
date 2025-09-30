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

/** @file uise/desktop/roundpixmaplabel.hpp
*
*  Declares round label widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ROUND_PIXMAP_LABEL_HPP
#define UISE_DESKTOP_ROUND_PIXMAP_LABEL_HPP

#include <QLabel>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class RoundPixmapLabel : public QLabel
{
    Q_OBJECT

    public:

        explicit RoundPixmapLabel(QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());

        void setPixmapSource(
            std::shared_ptr<PixmapSource> source,
            QString name,
            QSize size={}
        );

        void setText(const QString&)=delete;
        QString text() const=delete;

        //! @todo Implement configurable circle border

    protected:

        void paintEvent(QPaintEvent *event) override;

    private:

        PixmapConsumer* m_pixmapConsumer=nullptr;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ROUND_PIXMAP_LABEL_HPP
