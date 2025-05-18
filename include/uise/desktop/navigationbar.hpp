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

/** @file uise/desktop/navigationbar.hpp
*
*  Declares navigation bar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_NAVBAR_HPP
#define UISE_DESKTOP_NAVBAR_HPP

#include <memory>

#include <QFrame>
#include <QToolButton>
#include <QLabel>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Item widget of navigation bar.
 */
class UISE_DESKTOP_EXPORT NavigationBarItem : public QToolButton
{
    Q_OBJECT

    public:

        NavigationBarItem(QWidget* parent=nullptr, bool chackable=true);

        void setHoveringCursor(const Qt::CursorShape& cursor) noexcept
        {
            m_hoveringCursor=cursor;
        }

        Qt::CursorShape hoveringCursor() const noexcept
        {
            return m_hoveringCursor;
        }

    protected:

        void enterEvent(QEnterEvent * event) override;

    private:

        Qt::CursorShape m_hoveringCursor;
};

/**
 * @brief Separator widget of navigatioon bar.
 */
class UISE_DESKTOP_EXPORT NavigationBarSeparator : public QLabel
{
    Q_OBJECT

    public:

        constexpr static const char* DefaultCharacter=">";
        constexpr static const char* DefaultHoverCharacter="<";

        NavigationBarSeparator(QWidget* parent=nullptr);

        NavigationBarSeparator* clone() const;

        void setHoverCharacterEnabled(bool enable)
        {
            m_hoverCharacterEnabled=enable;
        }

        bool isHoverCharacterEnabled() const
        {
            return m_hoverCharacterEnabled;
        }

        void setHoverCharacter(QString val)
        {
            m_hoverCharacter=std::move(val);
        }

        QString hoverCharacter() const
        {
            return m_hoverCharacter;
        }

        void setText(const QString& text)
        {
            m_fallbackCharacter=text;
            QLabel::setText(text);
        }

    signals:

        void clicked();
        void hovered(bool enable);

    protected:

        void mousePressEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        bool m_hoverCharacterEnabled;
        QString m_hoverCharacter;
        QString m_fallbackCharacter;
};

/**
 * @brief Panel widget of navigatioon bar.
 */
class UISE_DESKTOP_EXPORT NavigationBarPanel : public QFrame
{
    Q_OBJECT

    public:

        using QFrame::QFrame;
};

class NavigationBar_p;

/**
 * @brief Horizontal navigation bar can be used to present a linear path to some object.
 *  Each item in the navigation bar represents corresponding element in the path.
 *  Items can be added only sequentially.
 *  Removing an item from the bar truncates all subsequent items from the bar starting from the removed one.
 *  In exclusive mode only one item of the bar can be selected at once, otherwise any number of items can be selected at the same time.
 */
class UISE_DESKTOP_EXPORT NavigationBar : public QFrame
{
    Q_OBJECT

    public:

        constexpr static const Qt::CursorShape DefaultHoveringCursor=Qt::PointingHandCursor;

        /**
         * @brief Constructor.
         * @param parent Parent frame.
         */
        NavigationBar(QWidget* parent=nullptr);

        /**
         * Destructor.
         */
        ~NavigationBar();

        NavigationBar(const NavigationBar&)=delete;
        NavigationBar(NavigationBar&&)=delete;
        NavigationBar& operator=(const NavigationBar&)=delete;
        NavigationBar& operator=(NavigationBar&&)=delete;

        /**
         * @brief Add item to the bar.
         * @param name Item name.
         * @param tooltip Item tooltip.
         * @param id Item id.
         */
        void addItem(const QString& name, const QString& tooltip=QString{}, const QString& id={});

        QString itemName(int index) const;
        QString itemId(int index) const;
        QString itemTooltip(int index) const;
        QVariant itemData(int index) const;

        /**
         * @brief Find item by ID
         * @param id ID to look for.
         * @return Id of found item or -1 if such id not found.
         */
        int indexForId(const QString& id) const;

        /**
         * @brief Select item by index in exclusive mode.
         * @param index Index of item to select.
         */
        void setCurrentIndex(int index);

        void setCheckable(bool enable);
        bool isCheckable() const;

        /**
         * @brief Check the item by index.
         * @param index Index of the item to check.
         * @parem checked If true then item is checked,
         */
        void setItemChecked(int index, bool checked=true);

        /**
         * @brief Check if the item is checked.
         * @param index Index of the item.
         * @return true if item is checked, false otherwise.
         */
        bool isItemChecked(int index) const;

        /**
         * @brief Clear navigation bar.
         */
        void clear();

        /**
         * @brief Trunctae items starting from index.
         * @param index Inclusive index to truncate items starting from.
         */
        void truncate(int index);

        /**
         * @brief Set widget to use as a sample for separators between items in the navigation bar.
         * @param sep Sample separator.
         */
        void setSeparatorSample(NavigationBarSeparator* sep);
        NavigationBarSeparator* separatorSample() const noexcept;

        /**
         * @brief Set mode of separators visibility.
         * @param enable If true then separators are visible in the bar.
         */
        void setSeparatorsVisible(bool enable) noexcept;
        bool isSeparatorsVisible() const noexcept;

        void setSeparatorTooltip(int index, const QString& val);
        QString separatorTooltip(int index) const;

        /**
         * @brief Set/unset exclusive mode.
         * @param enable If true (default) then items can be exclusivly selected.
         */
        void setExclusive(bool enable);

        /**
         * @brief Check if navigation bar is of exclusive mode.
         * @return If true (default) then exclusive mode is enabled.
         */
        bool isExclusive() const;

        /**
         * @brief Set cursor shape used when cursor is hovering over the item.
         * @param cursor Shape of hovering cursor.
         */
        void setItemHoveringCursor(const Qt::CursorShape& cursor) noexcept;
        Qt::CursorShape itemHoveringCursor() const noexcept;

    public slots:

        /**
         * @brief Set name of the item.
         * @param index Item's index.
         * @param name New name.
         */
        void setItemName(int index, const QString& name);

        /**
         * @brief Set ID of the item.
         * @param index Item's index.
         * @param name New ID.
         */
        void setItemId(int index, const QString& id);

        /**
         * @brief Set data for the item.
         * @param index Item's index.
         * @param data New data.
         */
        void setItemData(int index, QVariant data);

        /**
         * @brief Set ID of the item.
         * @param index Item's index.
         * @param name New tooltip.
         */
        void setItemTooltip(int index, const QString& tooltip);

    signals:

        void indexClicked(int index);
        void indexSelected(int index);
        void indexToggled(int index, bool checked);        
        void indexSeparatorClicked(int index);
        void indexSeparatorHovered(int index, bool enable);

        void idClicked(const QString& id);
        void idSelected(const QString& id);
        void idToggled(const QString& id, bool checked);
        void idSeparatorClicked(const QString& id);
        void idSeparatorHovered(const QString& id, bool enable);

    protected:

        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:

        std::unique_ptr<NavigationBar_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_NAVBAR_HPP
