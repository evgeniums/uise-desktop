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

/** @file uise/desktop/chatobjectgallerybrowser.hpp
*
*  Declares ChatObjectGalleryBrowser, the thumbnail grid of the image objects of a chat.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATOBJECTGALLERYBROWSER_HPP
#define UISE_DESKTOP_CHATOBJECTGALLERYBROWSER_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatobjectbrowser.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

//! The widget behind the grid; defined in the source file because it instantiates
//! FlyweightListView.
class ChatObjectGalleryView;

/**
 * @brief Browser of the objects of one kind shown as a grid of square thumbnails, newest first.
 *
 * Built after ChatReactionGalleryGrid: a FlyweightListView of ROWS of cells, so only the rows
 * near the viewport exist as widgets. Each cell shows the object's thumbnail, which comes from a
 * RoundedImageSource and may resolve later; until then the cell shows its placeholder, the
 * background of #galleryCell in chatobjectbrowser.qss.
 *
 * The number of columns follows the width: as many cells of at least minCellSize as fit, then the
 * cells are stretched to fill the width. Rows are anchored at an origin object, so that rows
 * prepended later do not regroup the loaded ones; when the newest end is reached the grid regroups
 * once from the newest object so that only the oldest row can be short.
 *
 * Activating a cell emits itemActivated(); the owner opens its viewer.
 */
class UISE_DESKTOP_EXPORT ChatObjectGalleryBrowser : public AbstractChatObjectBrowser
{
    Q_OBJECT

    //! Smallest side of a cell in pixels; the columns are as many as fit. Settable from QSS:
    //! `qproperty-minCellSize: 96;`
    Q_PROPERTY(int minCellSize READ minCellSize WRITE setMinCellSize)

    //! Gap between cells in pixels. Settable from QSS: `qproperty-cellSpacing: 2;`
    Q_PROPERTY(int cellSpacing READ cellSpacing WRITE setCellSpacing)

    public:

        constexpr static const int DefaultMinCellSize=96;
        constexpr static const int DefaultCellSpacing=2;

        explicit ChatObjectGalleryBrowser(QString kind, QWidget* parent=nullptr);

        int minCellSize() const noexcept;
        void setMinCellSize(int value);

        int cellSpacing() const noexcept;
        void setCellSpacing(int value);

        //! Columns at the current width, 0 before the first layout.
        int columnCount() const noexcept;

    protected:

        void doReset() override;
        void doLoad(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus) override;
        void doRemove(const std::vector<ChatObjectKey>& keys) override;
        void doUpdate(const ChatObjectItem& item) override;
        void doEdgeArrived(bool older) override;
        void doGroupingChanged() override;

    private:

        void jumpToNewest();

        ChatObjectGalleryView* m_gallery;
};

//! Browser of the "image" kind.
UISE_DESKTOP_EXPORT ChatObjectGalleryBrowser* makeChatObjectImageBrowser(QWidget* parent=nullptr);

} // namespace uise

#endif // UISE_DESKTOP_CHATOBJECTGALLERYBROWSER_HPP
