// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "daemonadaptor.h"

#include "libclima/wire/snapshot.h"
#include "snapshotservice.h"

DaemonAdaptor::DaemonAdaptor(SnapshotService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
    // The service speaks QByteArray and the bus speaks QString; the conversion
    // is here rather than in the service so that nothing below this file knows
    // there is a bus at all.
    connect(service, &SnapshotService::snapshotChanged, this, &DaemonAdaptor::SnapshotChanged);
}

int DaemonAdaptor::SchemaVersion() const
{
    return clima::wire::kSchemaVersion;
}

QString DaemonAdaptor::GetSnapshot(const QString    &placeId,
                                   const QStringList &fields,
                                   int                hours,
                                   int                days)
{
    return QString::fromUtf8(m_service->snapshot(placeId, fields, hours, days));
}

QString DaemonAdaptor::Subscribe(const QString    &placeId,
                                 const QStringList &fields,
                                 int                hours,
                                 int                days)
{
    return m_service->subscribe(placeId, fields, hours, days);
}

bool DaemonAdaptor::Unsubscribe(const QString &token)
{
    return m_service->unsubscribe(token);
}

void DaemonAdaptor::RequestRefresh(const QString &placeId)
{
    m_service->requestRefresh(placeId);
}

QString DaemonAdaptor::ListWidgets()
{
    return QString::fromUtf8(m_service->catalogue());
}

QStringList DaemonAdaptor::ListPlaces()
{
    return m_service->placeIds();
}
