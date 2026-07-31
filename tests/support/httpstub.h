// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A single-purpose HTTP/1.1 server on loopback, for testing HttpClient.
//
// ---- why a real socket and not a mocked QNetworkAccessManager ---------------
//
// Because the things being tested are all below the QNetworkAccessManager API.
// "Does the request carry our User-Agent" is a question about bytes on a
// socket. "Does a 304 come back as a success" is a question about how Qt's HTTP
// stack reports a status with no body. "Is exactly one request issued when
// three callers ask" is a question about how many times a server was contacted
// — which is a count only the server can take, and which a mock of our own
// client would answer by construction rather than by observation.
//
// A QTcpServer bound to 127.0.0.1 on an ephemeral port answers all three
// honestly, costs a few milliseconds, and reaches no network. It is not a
// compromise for want of a mocking framework; it is the more truthful test.
//
// ---- what it deliberately does not do ---------------------------------------
//
// No keep-alive: every response carries `Connection: close` and the socket is
// closed after it. Persistent connections would make "how many requests did the
// client make" a question about connection reuse, which is not what any of
// these tests are about.
//
// No chunked encoding, no compression, no HTTPS. A test that needs any of those
// is testing Qt, not us.

#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTcpServer>

// One request as the stub received it.
struct StubRequest {
    QByteArray                       method;
    QByteArray                       target;   // path and query, as sent
    QMap<QByteArray, QByteArray>     headers;  // keys lowercased

    [[nodiscard]] QByteArray header(const QByteArray &name) const;
};

// One canned response. The queue is consumed in order; when it runs out the
// last entry repeats, so a test that wants "always 500" queues one.
struct StubResponse {
    int                          status = 200;
    QByteArray                   body;
    QMap<QByteArray, QByteArray> headers;

    static StubResponse ok(const QByteArray &body,
                           const QByteArray &contentType = QByteArrayLiteral("application/json"));
    static StubResponse withStatus(int code);
    StubResponse       &with(const QByteArray &name, const QByteArray &value);
};

class HttpStub : public QObject
{
    Q_OBJECT

public:
    explicit HttpStub(QObject *parent = nullptr);
    ~HttpStub() override;

    // 127.0.0.1 on a port the kernel picks. Returns false only if loopback is
    // unavailable, which would mean the machine is broken rather than the test.
    bool listen();

    [[nodiscard]] QString baseUrl() const;

    void enqueue(const StubResponse &response);

    // Every request received, in order. The count is the number that matters
    // for coalescing and for "a 403 was not retried".
    [[nodiscard]] const QList<StubRequest> &requests() const { return m_requests; }
    [[nodiscard]] int                       requestCount() const { return int(m_requests.size()); }

    void reset();

private:
    void onConnection();

    QTcpServer         m_server;
    QList<StubResponse> m_responses;
    QList<StubRequest>  m_requests;
    int                 m_served = 0;
};
