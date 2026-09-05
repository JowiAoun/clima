// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "devicelocator.h"

#ifdef CLIMA_HAVE_POSITIONING
#include "libclima/places/qtpositioninglocator.h"
#endif
#ifdef CLIMA_HAVE_PORTAL_LOCATION
#include "libclima/places/portallocator.h"
#endif

namespace clima {

DeviceLocator::DeviceLocator(QObject *parent)
    : QObject(parent)
{
}

DeviceLocator::~DeviceLocator() = default;

DeviceLocator *DeviceLocator::create(QObject *parent)
{
    // Three backends and one order, decided here and nowhere else:
    //
    //   1. inside a sandbox, the portal — the only route that works there,
    //      and the better arrangement anyway (portallocator.h)
    //   2. Qt Positioning, where it found a source: GeoClue2 on a desktop,
    //      Windows Location, CoreLocation
    //   3. the portal again, for a desktop where Qt found nothing — a packager
    //      who left Qt Positioning out, or a Qt without its GeoClue2 plugin —
    //      since any desktop with xdg-desktop-portal can still answer
    //
    // and the base class when none of those was compiled in.
#ifdef CLIMA_HAVE_PORTAL_LOCATION
    if (PortalLocator::inSandbox())
        return new PortalLocator(parent);
#endif

#ifdef CLIMA_HAVE_POSITIONING
    {
        auto *direct = new QtPositioningLocator(parent);
        if (direct->isAvailable())
            return direct;
        delete direct;
    }
#endif

#ifdef CLIMA_HAVE_PORTAL_LOCATION
    return new PortalLocator(parent);
#else
    // The base class *is* the null implementation: isAvailable() is false and
    // requestPosition() reports Unavailable. That is deliberate — a separate
    // NullDeviceLocator would be a class whose entire body is the base class's,
    // and the base class has to behave that way anyway for the case where Qt
    // Positioning is compiled in and finds no source at runtime.
    return new DeviceLocator(parent);
#endif
}

bool DeviceLocator::isAvailable() const
{
    return false;
}

void DeviceLocator::requestPosition()
{
    reportFailure(Failure::Unavailable,
                  QStringLiteral("this build has no positioning backend: Qt6::Positioning was not "
                                 "found when it was configured"));
}

void DeviceLocator::cancel()
{
    setRequestInFlight(false);
}

void DeviceLocator::setTimeout(int milliseconds)
{
    if (milliseconds > 0)
        m_timeoutMs = milliseconds;
}

void DeviceLocator::reportFailure(Failure failure, const QString &reason)
{
    m_inFlight = false;
    Q_EMIT failed(failure, reason);
}

void DeviceLocator::reportPosition(const Coordinate &coordinate, double accuracyMetres)
{
    m_inFlight = false;
    Q_EMIT located(coordinate, accuracyMetres);
}

} // namespace clima
