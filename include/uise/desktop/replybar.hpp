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

/** @file uise/desktop/replybar.hpp
*
*  Declares ReplyBar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_REPLYBAR_HPP
#define UISE_DESKTOP_REPLYBAR_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractreplybar.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class IconTextButton;
class ReplyBar_p;

/**
 * @brief Default AbstractReplyBar implementation.
 */
class UISE_DESKTOP_EXPORT ReplyBar : public AbstractReplyBar
{
    Q_OBJECT

    public:

        explicit ReplyBar(QWidget* parent=nullptr);

        ~ReplyBar();
        ReplyBar(const ReplyBar&)=delete;
        ReplyBar(ReplyBar&&)=delete;
        ReplyBar& operator=(const ReplyBar&)=delete;
        ReplyBar& operator=(ReplyBar&&)=delete;

        void setReplyData(ReplyPreviewData data) override;
        const ReplyPreviewData& replyData() const override;
        void clear() override;

        AbstractReplyPreview* preview() const override;

        void setTextTrimLength(int length) override;
        int textTrimLength() const override;

        IconTextButton* configureButton() const;
        IconTextButton* cancelButton() const;

    private:

        std::unique_ptr<ReplyBar_p> pimpl;
};

}

#endif // UISE_DESKTOP_REPLYBAR_HPP
