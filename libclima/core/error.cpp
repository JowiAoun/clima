// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "error.h"

namespace clima {

QString errorKindName(ErrorKind kind)
{
    switch (kind) {
    case ErrorKind::Network:           return QStringLiteral("Network");
    case ErrorKind::Timeout:           return QStringLiteral("Timeout");
    case ErrorKind::UserAgentRejected: return QStringLiteral("UserAgentRejected");
    case ErrorKind::RateLimited:       return QStringLiteral("RateLimited");
    case ErrorKind::ServerError:       return QStringLiteral("ServerError");
    case ErrorKind::NotFound:          return QStringLiteral("NotFound");
    case ErrorKind::HttpStatus:        return QStringLiteral("HttpStatus");
    case ErrorKind::ProviderDisabled:  return QStringLiteral("ProviderDisabled");
    case ErrorKind::Cancelled:         return QStringLiteral("Cancelled");
    case ErrorKind::Parse:             return QStringLiteral("Parse");
    case ErrorKind::Storage:           return QStringLiteral("Storage");
    case ErrorKind::Unsupported:       return QStringLiteral("Unsupported");
    }
    return QStringLiteral("Unknown");
}

Error::Error(ErrorKind kind, QString message)
    : m_kind(kind)
    , m_message(std::move(message))
{
}

bool Error::isRetryable() const
{
    switch (m_kind) {
    case ErrorKind::Network:
    case ErrorKind::Timeout:
    case ErrorKind::RateLimited:
    case ErrorKind::ServerError:
        return true;

    // UserAgentRejected is first in this list and it is the reason the list is
    // a switch rather than a default. A 403 that answers our identifying
    // User-Agent is a statement about our client, and the identical request
    // will earn the identical refusal — plus one more entry in whatever log
    // eventually gets our address blocked.
    case ErrorKind::UserAgentRejected:
    case ErrorKind::NotFound:
    case ErrorKind::HttpStatus:
    case ErrorKind::ProviderDisabled:
    case ErrorKind::Cancelled:
    case ErrorKind::Parse:
    case ErrorKind::Storage:
    case ErrorKind::Unsupported:
        return false;
    }
    return false;
}

QString Error::toString() const
{
    QString text = errorKindName(m_kind);
    if (!m_providerId.isEmpty())
        text += QLatin1Char('[') + m_providerId + QLatin1Char(']');
    if (m_httpStatus != 0)
        text += QLatin1String(" HTTP ") + QString::number(m_httpStatus);
    if (!m_message.isEmpty())
        text += QLatin1String(": ") + m_message;
    if (m_retryAfter.isValid())
        text += QLatin1String(" (retry after ") + m_retryAfter.toString(Qt::ISODate) + QLatin1Char(')');
    return text;
}

} // namespace clima
