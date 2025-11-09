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

/** @file uise/desktop/statusdialog.hpp
*
*  Declares StatusDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_STATUS_DIALOG_HPP
#define UISE_DESKTOP_STATUS_DIALOG_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class StatusDialog_p;

class UISE_DESKTOP_EXPORT StatusBase
{
    public:

        enum class Type : int
        {
            None,
            Error,
            Warning,
            Info,
            Question,
            Attention,

            User=0x100
        };

        static QString statusString(int status);

        static QString statusTitle(int status);

        template <typename T>
        static QString statusString(T status)
        {
            return statusString(static_cast<int>(status));
        }

        template <typename T>
        static QString statusTitle(T status)
        {
            return statusTitle(static_cast<int>(status));
        }
};

class UISE_DESKTOP_EXPORT Status : public StatusBase
{
    public:

        template <typename T>
        Status(T status) : m_status(static_cast<int>(status))
        {}

        Status(Type status=Type::None) : m_status(static_cast<int>(status))
        {}

        virtual ~Status()=default;
        Status(const Status&)=default;
        Status(Status&&)=default;
        Status& operator=(const Status&)=default;
        Status& operator=(Status&&)=default;

        QString statusString(int status) const;

        QString statusTitle(int status) const;

        template <typename T>
        QString statusString(T status) const
        {
            return statusString(static_cast<int>(status));
        }

        template <typename T>
        QString statusTitle(T status) const
        {
            return statusTitle(static_cast<int>(status));
        }

        QString statusString() const
        {
            return statusString(m_status);
        }

        QString statusTitle() const
        {
            return statusTitle(m_status);
        }

        template <typename T>
        void setStatusType(T status) noexcept
        {
            m_status=status;
        }

        int statusType() const noexcept
        {
            return m_status;
        }

        template <typename T>
        T statusType() const noexcept
        {
            return static_cast<T>(m_status);
        }

        template <typename T>
        bool isStatusType(T status) const noexcept
        {
            return m_status==static_cast<int>(status);
        }

    protected:

        virtual QString customStatusString(int /*status*/) const
        {
            return QString{};
        }

        virtual QString customStatusTitle(int /*status*/) const
        {
            return QString{};
        }

    private:

        int m_status;
};

class UISE_DESKTOP_EXPORT AbstractStatusDialog : public AbstractDialog,
                                                 public Status
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        virtual QLabel* textWidget() const=0;

        virtual void setStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={})=0;

        virtual void setStatus(const QString& message, Type type, const QString& title={})=0;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

class UISE_DESKTOP_EXPORT StatusDialog : public Dialog<AbstractStatusDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractStatusDialog>;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        StatusDialog(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~StatusDialog();

        StatusDialog(const StatusDialog&)=delete;
        StatusDialog(StatusDialog&&)=delete;
        StatusDialog& operator=(const StatusDialog&)=delete;
        StatusDialog& operator=(StatusDialog&&)=delete;

        QLabel* textWidget() const override;

        void setStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={}) override;

        void setStatus(const QString& message, Type type, const QString& title={}) override;

    private:

        std::unique_ptr<StatusDialog_p> pimpl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STATUS_DIALOG_HPP
