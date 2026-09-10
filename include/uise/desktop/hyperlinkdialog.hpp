/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/hyperlinkdialog.hpp
*
*  Declares HyperlinkDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HYPERLINKDIALOG_HPP
#define UISE_DESKTOP_HYPERLINKDIALOG_HPP

#include <memory>

#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/modaldialog.hpp>
#include <uise/desktop/abstracthyperlinkdialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class HyperlinkDialog_p;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief Default AbstractHyperlinkDialog implementation.
 *
 * Content, top to bottom: a URL ValidatedInput, a title ValidatedInput, an error label -- built
 * in construct() rather than the constructor (the NewPasswordDialog/PasswordDialog pattern, not
 * ReplyDialog's) so a factory-substituted implementation participates the same way.
 */
class UISE_DESKTOP_EXPORT HyperlinkDialog : public Dialog<AbstractHyperlinkDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractHyperlinkDialog>;

        explicit HyperlinkDialog(QWidget* parent=nullptr);

        ~HyperlinkDialog();
        HyperlinkDialog(const HyperlinkDialog&)=delete;
        HyperlinkDialog(HyperlinkDialog&&)=delete;
        HyperlinkDialog& operator=(const HyperlinkDialog&)=delete;
        HyperlinkDialog& operator=(HyperlinkDialog&&)=delete;

        void setUrl(const QString& url) override;
        QString url() const override;

        void setLinkTitle(const QString& title) override;
        QString linkTitle() const override;

        void setError(const QString& message) override;

        void reset() override;

        void setDialogFocus() override;

        void construct() override;

    private:

        std::unique_ptr<HyperlinkDialog_p> pimpl;
};

// Template parameter order is (popupMaxWidth, maxWidthPercent, popupMaxHeight, maxHeightPercent).
// The two PERCENTAGES are ceilings measured against the HOST FRAME's own rect, not the screen --
// and with popup auto-height on, the height percentage is the only thing that can cut the form
// short. 80 rather than a "this is only a small dialog" number for exactly that reason: auto-height
// already fits the popup to the form's real sizeHint(), so a low percentage does not make the box
// tidier, it just clips it (measured: 40% of a 220px-tall host frame is an 88px popup, which
// squeezed the two fields into nothing). The WIDTH is bounded absolutely instead, at 520px, which
// is what actually keeps a two-field form from stretching across a wide window.
using ModalHyperlinkDialogType=ModalDialog<AbstractHyperlinkDialog,HyperlinkDialog,520,80,-1,80>;

class UISE_DESKTOP_EXPORT ModalHyperlinkDialog : public ModalHyperlinkDialogType
{
    Q_OBJECT

    public:

        explicit ModalHyperlinkDialog(
                QWidget* parent=nullptr,
                int defaultMaxWidthPercent=80,
                int defaultPopupMaxWidth=520,
                int defaultMaxHeightPercent=80,
                int defaultPopupMaxHeight=-1
            ) : ModalHyperlinkDialogType(parent,defaultMaxWidthPercent,defaultPopupMaxWidth,defaultMaxHeightPercent,defaultPopupMaxHeight)
        {
            // Same pairing as ModalReplyDialog/ModalForwardDialog: auto-height fits the popup to
            // the two-field form's real sizeHint() instead of a fixed percentage, and the
            // shortcut re-enable lets Escape close it the same way Cancel does.
            setPopupAutoHeight(true);
            setShortcutEnabled(true);
        }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_HYPERLINKDIALOG_HPP
