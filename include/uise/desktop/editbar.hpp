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

/** @file uise/desktop/editbar.hpp
*
*  Declares EditBar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EDITBAR_HPP
#define UISE_DESKTOP_EDITBAR_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstracteditbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class IconTextButton;
class EditBar_p;

/**
 * @brief Default AbstractEditBar implementation.
 */
class UISE_DESKTOP_EXPORT EditBar : public AbstractEditBar
{
    Q_OBJECT

    public:

        explicit EditBar(QWidget* parent=nullptr);

        ~EditBar();
        EditBar(const EditBar&)=delete;
        EditBar(EditBar&&)=delete;
        EditBar& operator=(const EditBar&)=delete;
        EditBar& operator=(EditBar&&)=delete;

        void setEditData(ReplyPreviewData data) override;
        const ReplyPreviewData& editData() const override;
        void clear() override;

        AbstractReplyPreview* preview() const override;

        void setTitleFormat(const QString& format) override;
        QString titleFormat() const override;

        void setTextTrimLength(int length) override;
        int textTrimLength() const override;

        IconTextButton* editButton() const;
        IconTextButton* cancelButton() const;

    protected:

        void updateCloseOnEscape() override;

        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:

        std::unique_ptr<EditBar_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITBAR_HPP
