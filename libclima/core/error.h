// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// What went wrong, in a form the caller can branch on.
//
// docs/04-architecture.md §4.4 states the contract this type exists to keep:
// "Never returns a partial success silently: either a Forecast or a typed
// Error." The word doing the work there is *typed*. A QString message is a
// thing to log; it is not a thing to decide with, and a UI that has to decide
// whether to show cached data, retry, hide a feature or tell the user their
// clock is wrong cannot do it by matching on English.
//
// So the kind is an enum and the message is decoration. The rule for adding a
// kind is that somebody has to be able to name the different thing they would
// *do* about it — if two kinds always lead to the same branch, they are one
// kind with two messages.
//
// ---- why there is no error code space, and no errno -------------------------
//
// Providers speak HTTP, SQLite speaks its own result codes, and Qt speaks
// QNetworkReply::NetworkError. Flattening those into one integer space means
// inventing a registry that every provider has to be taught, and the first
// provider that returns a code nobody registered becomes "unknown error 27".
// The kinds below are deliberately few and deliberately about consequence
// rather than cause; the cause travels alongside in `httpStatus` and `message`
// for the log.

#pragma once

#include <QDateTime>
#include <QString>

namespace clima {

enum class ErrorKind {
    // The request never got an answer. DNS, TLS, connection refused, the
    // machine is on a train. Retryable, and the UI shows cached data.
    Network,

    // An answer that took too long. Split from Network because a timeout is
    // the one transport failure where retrying immediately is usually wrong.
    Timeout,

    // 403 on a request we sent with our identifying User-Agent.
    //
    // This is a policy refusal, not a transport failure, and it is the single
    // most important value in this enum. MET Norway and api.weather.gov both
    // return it for a User-Agent they will not serve — verified: api.weather.gov
    // answers 403 to an empty UA today. Retrying is how a project gets its IP
    // banned rather than merely refused, so HttpClient treats this as a hard
    // stop for the provider's whole process lifetime and never sends again.
    //
    // Anything that sees this kind should surface it to a human. It means our
    // code is wrong, not that the network is.
    UserAgentRejected,

    // 429. Backoff applies, and Retry-After is honoured if the server sent one.
    RateLimited,

    // 5xx. Backoff applies.
    ServerError,

    // 404, 410, and the rest of the 4xx range that is about *this* request
    // rather than about us. Not retryable — the same request will fail again.
    NotFound,

    // A status we have no specific handling for. Carries `httpStatus`.
    HttpStatus,

    // We refused to send. Either the provider is disabled by a prior 403, or a
    // backoff window has not elapsed. Distinct from the failure that caused it,
    // because the caller's right move is "show cache and say nothing" rather
    // than "report a network problem".
    ProviderDisabled,

    // The caller cancelled, or the client is shutting down.
    Cancelled,

    // The bytes arrived and were not what the provider's contract promised.
    // Golden-file tests exist to keep this one rare.
    Parse,

    // SQLite could not open, migrate, read or write.
    Storage,

    // The provider does not cover this coordinate, or does not offer this
    // product here. Not a failure so much as an absence — docs §4.4: "A
    // provider that returns nothing must make the UI *hide* the feature, not
    // show a broken one."
    Unsupported,
};

QString errorKindName(ErrorKind kind);

class Error
{
public:
    Error() = default;
    Error(ErrorKind kind, QString message);

    [[nodiscard]] ErrorKind kind() const { return m_kind; }
    [[nodiscard]] QString   message() const { return m_message; }

    // 0 when the failure did not come from an HTTP response.
    [[nodiscard]] int  httpStatus() const { return m_httpStatus; }
    void               setHttpStatus(int status) { m_httpStatus = status; }

    // Which provider, so a diagnostic can name it without the caller having to
    // remember what it asked for.
    [[nodiscard]] QString providerId() const { return m_providerId; }
    void                  setProviderId(QString id) { m_providerId = std::move(id); }

    // When it is worth trying again. Invalid means "no advice" — either because
    // the failure is permanent or because nobody told us. Set from Retry-After
    // when the server sent one, and from the backoff schedule otherwise, so a
    // caller that wants to show "retrying at 14:05" has a number to show.
    [[nodiscard]] QDateTime retryAfter() const { return m_retryAfter; }
    void                    setRetryAfter(QDateTime when) { m_retryAfter = std::move(when); }

    // Whether trying the identical request again could plausibly succeed.
    // Derived from the kind, and deliberately not settable: a caller that
    // disagrees with the table below is a caller reading the kind wrong.
    [[nodiscard]] bool isRetryable() const;

    // For logs and for QCOMPARE failure output. Not for users — user-facing
    // strings are the app's job and go through Qt Linguist.
    [[nodiscard]] QString toString() const;

private:
    ErrorKind m_kind = ErrorKind::Network;
    QString   m_message;
    QString   m_providerId;
    QDateTime m_retryAfter;
    int       m_httpStatus = 0;
};

} // namespace clima
