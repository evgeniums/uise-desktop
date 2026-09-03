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

/** @file uise/desktop/forwardbar.hpp
*
*  Declares ForwardBar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FORWARDBAR_HPP
#define UISE_DESKTOP_FORWARDBAR_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractforwardbar.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class IconTextButton;
class ForwardBar_p;

/**
 * @brief Default AbstractForwardBar implementation.
 */
class UISE_DESKTOP_EXPORT ForwardBar : public AbstractForwardBar
{
    Q_OBJECT

    public:

        explicit ForwardBar(QWidget* parent=nullptr);

        ~ForwardBar();
        ForwardBar(const ForwardBar&)=delete;
        ForwardBar(ForwardBar&&)=delete;
        ForwardBar& operator=(const ForwardBar&)=delete;
        ForwardBar& operator=(ForwardBar&&)=delete;

        void setForwardData(ReplyPreviewData data) override;
        const ReplyPreviewData& forwardData() const override;
        void clear() override;

        AbstractReplyPreview* preview() const override;

        void setTitleFormat(const QString& format) override;
        QString titleFormat() const override;

        void setTextTrimLength(int length) override;
        int textTrimLength() const override;

        void setMessageCount(int count) override;
        int messageCount() const override;

        void setCountFormat(const QString& format) override;
        QString countFormat() const override;

        IconTextButton* configureButton() const;
        IconTextButton* cancelButton() const;

    private:

        void refresh();

        std::unique_ptr<ForwardBar_p> pimpl;
};

}

#endif // UISE_DESKTOP_FORWARDBAR_HPP
