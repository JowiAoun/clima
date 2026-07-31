// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "devicelocator.h"

#ifdef CLIMA_HAVE_POSITIONING
#include "libclima/places/qtpositioninglocator.h"
#endif

namespace clima {

DeviceLocator::DeviceLocator(QObject *parent)
    : QObject(parent)
{
}

DeviceLocator::~DeviceLocator() = default;

DeviceLocator *DeviceLocator::create(QObject *parent)
{
#ifdef CLIMA_HAVE_POSITIONING
    return new QtPositioningLocator(parent);
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
