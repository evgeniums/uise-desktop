/**
@copyright Evgeny Sidorov 2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/buttonslist.hpp
*
*  Declares ButtonsList.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_BUTTONSLIST_HPP
#define UISE_DESKTOP_BUTTONSLIST_HPP

#include <QFrame>
#include <QBoxLayout>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/icontextbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT ButtonsList : public QFrame
{
    Q_OBJECT

    public:

        ButtonsList(QWidget* parent=nullptr);

        void addWidget(QWidget* widget);
        IconTextButton* addButton(const QString& text, std::shared_ptr<SvgIcon> icon={});

    private:

        QBoxLayout* m_layout;
};

class UISE_DESKTOP_EXPORT PopupButtonsList : public QFrame
{
    Q_OBJECT

    public:

        PopupButtonsList(QWidget* parent=nullptr);

        ButtonsList* buttonsList() const
        {
            return m_buttonsList;
        }

    public slots:

        void show();

    protected:

        void closeEvent(QCloseEvent* event) override;

    private:

        ButtonsList* m_buttonsList;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_BUTTONSLIST_HPP
