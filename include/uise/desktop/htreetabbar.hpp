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

/** @file uise/desktop/htreetabbar.hpp
*
*  Declares custom tab bar for HTree, with a composite per-tab widget instead of the plain
*  QTabBar label/icon.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_TAB_BAR_HPP
#define UISE_DESKTOP_HTREE_TAB_BAR_HPP

#include <memory>

#include <QFrame>
#include <QTabBar>
#include <QPointer>
#include <QMargins>

#include <uise/desktop/uisedesktop.hpp>

class QBoxLayout;
class QStyleOptionTab;

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeTab;
class IconTextButton;

class HTreeTabBarItem_p;

//! Base class for a composite widget that HTreeTabBar installs as a tab's whole visible
//! content (QTabBar::setTabButton(), LeftSide) in place of the plain text/icon label. An
//! application subclasses this to give every tab a close button plus whatever else the tab
//! needs to show (see whitemdesktop's ChatTabBarItem for an avatar + a chat-pin toggle).
//!
//! HTreeTab's own nameUpdated()/iconUpdated()/tooltipUpdated() signals keep driving the
//! underlying QTabWidget's text/icon exactly as they do without a custom bar (see
//! HTree_p::addTab()); this class additionally receives the same values via
//! setTabText()/setTabIcon()/setTabTooltip() so a subclass never needs to reach back into
//! HTreeTab's signals itself.
class UISE_DESKTOP_EXPORT HTreeTabBarItem : public QFrame
{
    Q_OBJECT

    public:

        //! Character-count cap applied to setTabText() before it reaches the base label --
        //! deliberately a character cap rather than pixel-accurate elision: eliding to the
        //! label's own current width would derive the tab's size from a label whose size in
        //! turn depends on the (already-elided) text, feeding back into
        //! HTreeTabBar::tabSizeHint() and oscillating. A fixed character cap keeps the tab's
        //! width a pure function of the model instead. See ChatTabBarItem::DefaultTitleMaxLength
        //! for the same idiom applied to a subclass's own additional content.
        constexpr static const int DefaultMaxTitleLength=32;

        HTreeTabBarItem(HTreeTab* tab, QWidget* parent=nullptr);

        ~HTreeTabBarItem();

        HTreeTabBarItem(const HTreeTabBarItem&)=delete;
        HTreeTabBarItem(HTreeTabBarItem&&)=delete;
        HTreeTabBarItem& operator=(const HTreeTabBarItem&)=delete;
        HTreeTabBarItem& operator=(HTreeTabBarItem&&)=delete;

        //! The tab this item belongs to, or nullptr once the tab has been destroyed -- the
        //! item and its tab live in disjoint widget trees (bar vs QTabWidget page stack), so
        //! neither one's destruction implies the other's.
        HTreeTab* treeTab() const noexcept;

        bool isCurrent() const noexcept;

        //! Whether the close button leads the item's other content (macOS convention) or
        //! trails it (every other platform) -- see task-custom-tabbar.md item 1.
        static bool closeButtonLeading() noexcept;

    public slots:

        virtual void setTabText(const QString& text);
        virtual void setTabIcon(const QIcon& icon);
        virtual void setTabTooltip(const QString& tooltip);

        //! Called by HTreeTabBar whenever this item's tab becomes (or stops being) the
        //! current one -- e.g. to drive a "selected" QSS property.
        virtual void setCurrent(bool enable);

        //! Enables/disables the close button, e.g. to keep the last remaining tab open.
        virtual void setCloseEnabled(bool enable);

        //! Recompute this item's own content from its tab's current state. The base
        //! implementation does nothing; a subclass overrides it to pull whatever
        //! application-level state it displays (see ChatTabBarItem::refresh()).
        virtual void refresh();

        //! See DefaultMaxTitleLength. Re-elides whatever setTabText() last received.
        void setMaxTitleLength(int maxLength);
        int maxTitleLength() const noexcept;

    signals:

        //! The close button was clicked. HTreeTabBar routes this to HTree::closeTab(HTreeTab*).
        void closeRequested();

        //! Anywhere on the item other than the close/pin/etc. sub-buttons was clicked, i.e.
        //! "make my tab current". HTreeTabBar routes this to HTree::setCurrentTab(HTreeTab*).
        void selectRequested();

        //! This item's sizeHint()/minimumSizeHint() may have changed -- HTreeTabBar listens
        //! and coalesces a relayout (see HTreeTabBar::scheduleRelayout()).
        void sizeHintChanged();

    protected:

        //! Adds \p w to the item's content, in the position appropriate for the OS: appended
        //! after previously added content on macOS (close button leads), prepended before it
        //! everywhere else (close button trails) -- so a subclass declares content in one
        //! logical order (e.g. "pin, then avatar") and gets the correct mirrored layout on
        //! both conventions for free.
        void addContentWidget(QWidget* w, int stretch =0);

        //! Shows/hides the base icon+text pair (set via setTabIcon()/setTabText()) as a
        //! whole -- on by default. A subclass that replaces them with its own content in some
        //! modes but not others (e.g. ChatTabBarItem's AvatarButton, shown only once a
        //! character is known) hides the pair rather than leaving both visible at once; the
        //! icon label stays hidden regardless while no icon is set, exactly as before this
        //! call. Re-enabling restores whatever setTabText()/setTabIcon() last set.
        void setBaseLabelsVisible(bool enable);

        IconTextButton* closeButton() const;
        QBoxLayout* contentLayout() const;

        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:

        std::unique_ptr<HTreeTabBarItem_p> pimpl;
};

//! Builder for HTreeTabBarItem, one instance registered per HTree via
//! HTree::setTabBarBuilder(). Same shape as HTreeNodeBuilder.
class UISE_DESKTOP_EXPORT HTreeTabBarBuilder
{
    public:

        HTreeTabBarBuilder()=default;
        virtual ~HTreeTabBarBuilder();

        HTreeTabBarBuilder(const HTreeTabBarBuilder&)=default;
        HTreeTabBarBuilder(HTreeTabBarBuilder&&)=default;
        HTreeTabBarBuilder& operator=(const HTreeTabBarBuilder&)=default;
        HTreeTabBarBuilder& operator=(HTreeTabBarBuilder&&)=default;

        virtual HTreeTabBarItem* makeItem(HTreeTab* tab, QWidget* parent=nullptr) const=0;
};

class HTreeTabBar_p;

//! QTabBar subclass that hosts one HTreeTabBarItem per tab as the tab's whole visible
//! content, installed via QTabBar::setTabButton()/LeftSide. Never constructed directly by
//! application code -- HTree installs one internally, from construction (QTabWidget::
//! setTabBar() is undefined once tabs already exist, so it cannot wait for
//! HTree::setTabBarBuilder()). Every tab's item() stays null, and this class defers to plain
//! QTabBar behaviour, until a builder is actually registered.
//!
//! QTabBar has two gaps a composite-item bar must work around, both handled here:
//!  - it never resizes tab buttons on its own (only moves them) -- see tabLayoutChange();
//!  - it exposes no public "my content changed size" relayout trigger -- see
//!    scheduleRelayout().
class UISE_DESKTOP_EXPORT HTreeTabBar : public QTabBar
{
    Q_OBJECT

    public:

        HTreeTabBar(QWidget* parent=nullptr);

        ~HTreeTabBar();

        HTreeTabBar(const HTreeTabBar&)=delete;
        HTreeTabBar(HTreeTabBar&&)=delete;
        HTreeTabBar& operator=(const HTreeTabBar&)=delete;
        HTreeTabBar& operator=(HTreeTabBar&&)=delete;

        //! The item installed at \p index, or nullptr.
        HTreeTabBarItem* item(int index) const;

        //! Installs \p item as the tab content at \p index, destroying (via destroyWidget())
        //! whatever item was there before -- QTabBar::setTabButton() replacing a widget hides
        //! the old one but does not delete it.
        void setItem(int index, HTreeTabBarItem* item);

        //! Margins applied inside each tabRect() when positioning its item -- default is
        //! zero, i.e. the item fills the whole tab.
        void setItemMargins(const QMargins& margins);
        QMargins itemMargins() const noexcept;

        //! Schedules a coalesced relayout of every tab's geometry (see tabLayoutChange()).
        //! Safe to call repeatedly in a burst -- only the last request in a given event-loop
        //! turn actually runs.
        void scheduleRelayout();

    protected:

        QSize tabSizeHint(int index) const override;
        QSize minimumTabSizeHint(int index) const override;
        void tabLayoutChange() override;

        //! Clears the text/icon that QTabBar::paintEvent() would otherwise still draw
        //! *underneath* the item (the item only covers the tab's SE_TabBarTabLeftButton sub
        //! -rect, not the whole tab). Geometry/shape/selection painting are untouched --
        //! only QStyleOptionTab::text/icon are blanked, via the base implementation's own
        //! QStyleOptionTab fields.
        void initStyleOption(QStyleOptionTab* option, int tabIndex) const override;

    private:

        std::unique_ptr<HTreeTabBar_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_TAB_BAR_HPP
