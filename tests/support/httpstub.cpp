// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "httpstub.h"

#include <QHostAddress>
#include <QTcpSocket>

namespace {

const char *reasonFor(int status)
{
    switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default:  return "Unspecified";
    }
}

} // namespace

QByteArray StubRequest::header(const QByteArray &name) const
{
    return headers.value(name.toLower());
}

StubResponse StubResponse::ok(const QByteArray &body, const QByteArray &contentType)
{
    StubResponse response;
    response.status = 200;
    response.body = body;
    response.headers.insert(QByteArrayLiteral("Content-Type"), contentType);
    return response;
}

StubResponse StubResponse::withStatus(int code)
{
    StubResponse response;
    response.status = code;
    return response;
}

StubResponse &StubResponse::with(const QByteArray &name, const QByteArray &value)
{
    headers.insert(name, value);
    return *this;
}

HttpStub::HttpStub(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &HttpStub::onConnection);
}

HttpStub::~HttpStub() = default;

bool HttpStub::listen()
{
    // Closed first, so that a stub reused across a test class's init() gets a
    // fresh port each time rather than QTcpServer's "called when already
    // listening" warning and a false return. A port per test also means a
    // socket left open by a failing test cannot be picked up by the next one.
    if (m_server.isListening())
        m_server.close();

    // LocalHost, and not Any. Binding to every interface would make this stub
    // reachable from the network during a test run, which is the opposite of
    // what a suite that forbids external sockets is trying to be.
    return m_server.listen(QHostAddress::LocalHost, 0);
}

QString HttpStub::baseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
}

void HttpStub::enqueue(const StubResponse &response)
{
    m_responses.append(response);
}

void HttpStub::reset()
{
    m_responses.clear();
    m_requests.clear();
    m_served = 0;
}

void HttpStub::onConnection()
{
    QTcpSocket *socket = m_server.nextPendingConnection();
    if (socket == nullptr)
        return;

    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
        // Read until the blank line that ends the headers. Every request these
        // tests make is a GET with no body, so there is nothing after it and no
        // Content-Length to honour.
        static const QByteArray terminator = QByteArrayLiteral("\r\n\r\n");
        QByteArray              buffer = socket->property("buffer").toByteArray();
        buffer += socket->readAll();
        socket->setProperty("buffer", buffer);

        const int end = buffer.indexOf(terminator);
        if (end < 0)
            return;

        StubRequest request;
        const QList<QByteArray> lines = buffer.left(end).split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            const QByteArray line = lines[i].trimmed();
            if (line.isEmpty())
                continue;
            if (i == 0) {
                const QList<QByteArray> parts = line.split(' ');
                if (parts.size() >= 2) {
                    request.method = parts[0];
                    request.target = parts[1];
                }
                continue;
            }
            const int colon = line.indexOf(':');
            if (colon <= 0)
                continue;
            // Lowercased keys. HTTP header names are case-insensitive and Qt
            // does not promise a casing, so a test asserting on `User-Agent`
            // would otherwise be asserting on a Qt implementation detail.
            request.headers.insert(line.left(colon).trimmed().toLower(),
                                   line.mid(colon + 1).trimmed());
        }
        m_requests.append(request);

        // The queue is consumed in order and the last entry repeats. A test
        // that wants "500 forever" queues one 500; a test that wants "500 then
        // 200" queues both and gets exactly that.
        StubResponse response;
        if (!m_responses.isEmpty())
            response = m_responses.at(qMin(m_served, int(m_responses.size()) - 1));
        ++m_served;

        QByteArray out;
        out += QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(response.status)
            + QByteArrayLiteral(" ") + reasonFor(response.status) + QByteArrayLiteral("\r\n");
        for (auto it = response.headers.cbegin(); it != response.headers.cend(); ++it)
            out += it.key() + QByteArrayLiteral(": ") + it.value() + QByteArrayLiteral("\r\n");

        // 304 must not carry a body — RFC 7232 — and Qt's HTTP stack is right
        // to be confused by one. Content-Length is still sent, as zero, so the
        // client does not wait for bytes that are not coming.
        const bool bodyAllowed = response.status != 304 && response.status != 204;
        const QByteArray body = bodyAllowed ? response.body : QByteArray();
        out += QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size())
            + QByteArrayLiteral("\r\n");
        out += QByteArrayLiteral("Connection: close\r\n\r\n");
        out += body;

        socket->write(out);
        socket->flush();
        socket->disconnectFromHost();
    });
}
