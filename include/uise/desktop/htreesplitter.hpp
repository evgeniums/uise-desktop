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

/** @file uise/desktop/htreesplitter.hpp
*
*  Declares splitter for horizontal tree.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_SPLITTER_HPP
#define UISE_DESKTOP_HTREE_SPLITTER_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeSplitter_p;

class UISE_DESKTOP_EXPORT HTreeSplitter : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeSplitter(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeSplitter();

        HTreeSplitter(const HTreeSplitter&)=delete;
        HTreeSplitter(HTreeSplitter&&)=delete;
        HTreeSplitter& operator=(const HTreeSplitter&)=delete;
        HTreeSplitter& operator=(HTreeSplitter&&)=delete;

        void addWidget(QWidget* widget);
        QWidget* widget(int index) const;

        int count() const;

    public slots:

        void scrollToEnd();
        void scrollToHome();
        void scrollToWidget(QWidget* widget, int xmargin = 50);

    private:

        std::unique_ptr<HTreeSplitter_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_SPLITTER_HPP
