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

/** @file uise/desktop/withpathandsize.hpp
*
*  Declares WithPath and WithPathAndSize.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_WITH_PATH_SIZE_HPP
#define UISE_DESKTOP_WITH_PATH_SIZE_HPP

#include <string>
#include <vector>

#include <QSize>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class WithPath
{
    public:

        WithPath()=default;

        WithPath(std::string plainPath)
            : m_path({std::move(plainPath)})
        {}

        WithPath(std::vector<std::string> nestedPath)
            : m_path(nestedPath)
        {}

        void setPath(std::string path)
        {
            m_path.clear();
            m_path.emplace_back(std::move(path));
        }

        void setPath(std::vector<std::string> nestedPath)
        {
            m_path=std::move(nestedPath);
        }

        const auto& path() const noexcept
        {
            return m_path;
        }

        auto& path() noexcept
        {
            return m_path;
        }

        void setPath(WithPath path)
        {
            m_path=path.path();
        }

        bool operator <(const WithPath& other) const noexcept
        {
            return m_path<other.m_path;
        }

        bool operator <(const std::vector<std::string>& other) const noexcept
        {
            return m_path<other;
        }

        bool empty() const
        {
            return m_path.empty();
        }

        operator std::vector<std::string>() const
        {
            return m_path;
        }

        void clear()
        {
            m_path.clear();
        }

        const auto& toWithPath() const
        {
            return *this;
        }

        auto& toWithPath()
        {
            return *this;
        }

        std::string toString() const
        {
            std::string p;
            for (const auto& el:m_path)
            {
                p.append("/");
                p.append(el);
            }
            return p;
        }

    private:

        std::vector<std::string> m_path;
};

class WithPathAndSize : public WithPath
{
    public:

        WithPathAndSize()=default;

        template <typename PathT>
        WithPathAndSize(PathT path, const QSize& size={})
            : WithPath(std::move(path)),
              m_size(size)
        {}

        void setSize(const QSize& size)
        {
            m_size=size;
        }

        QSize size() const noexcept
        {
            return m_size;
        }

        bool isValid() const
        {
            return m_size.isValid() && !empty();
        }

        template <typename OtherT>
        bool operator <(const OtherT& other) const noexcept
        {
            if (path()<other.path())
            {
                return true;
            }
            if (path()>other.path())
            {
                return false;
            }
            if (size().width()<other.size().width())
            {
                return true;
            }
            if (size().width()>other.size().width())
            {
                return false;
            }
            if (size().height()<other.size().height())
            {
                return true;
            }
            if (size().height()>other.size().height())
            {
                return false;
            }
            return false;
        }

    private:

        QSize m_size;
};

using PixmapKey=WithPathAndSize;

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_WITH_PATH_SIZE_HPP
