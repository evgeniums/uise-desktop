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

/** @file uise/desktop/qrbarcode.hpp
**
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_QRBARCODE_HPP
#define UISE_DESKTOP_QRBARCODE_HPP

#include <QPoint>
#include <QString>
#include <QList>

#include <ReadBarcode.h>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace qrcode {

using ZXing::BarcodeFormat;
using ZXing::ContentType;
using ZXing::TextMode;

using ZXing::ReaderOptions;
using ZXing::Binarizer;
using ZXing::BarcodeFormats;

class Position : public ZXing::Quadrilateral<QPoint>
{
    using Base = ZXing::Quadrilateral<QPoint>;

    public:

        using Base::Base;
};

class Barcode : private ZXing::Barcode
{
    public:

        Barcode() = default;

        explicit Barcode(ZXing::Barcode&& r)
            : ZXing::Barcode(std::move(r))
        {
            m_text = QString::fromStdString(ZXing::Barcode::text());
            m_bytes = QByteArray(reinterpret_cast<const char*>(ZXing::Barcode::bytes().data()), Size(ZXing::Barcode::bytes()));
            auto& pos = ZXing::Barcode::position();
            auto qp = [&pos](int i) { return QPoint(pos[i].x, pos[i].y); };
            m_position = {qp(0), qp(1), qp(2), qp(3)};
        }

        using ZXing::Barcode::isValid;

        BarcodeFormat format() const { return static_cast<BarcodeFormat>(ZXing::Barcode::format()); }
        ContentType contentType() const { return static_cast<ContentType>(ZXing::Barcode::contentType()); }
        QString formatName() const { return QString::fromStdString(ZXing::ToString(ZXing::Barcode::format())); }
        QString contentTypeName() const { return QString::fromStdString(ZXing::ToString(ZXing::Barcode::contentType())); }
        const QString& text() const { return m_text; }
        const QByteArray& bytes() const { return m_bytes; }
        const Position& position() const { return m_position; }

    private:

        QString m_text;
        QByteArray m_bytes;
        Position m_position;
};

inline QList<Barcode> ZXBarcodesToQBarcodes(ZXing::Barcodes&& zxres)
{
    QList<Barcode> res;
    for (auto&& r : zxres)
    {
        res.emplace_back(std::move(r));
    }
    return res;
}

} // namespace qrcode

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_QRBARCODE_HPP
