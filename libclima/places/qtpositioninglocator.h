// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// DeviceLocator over Qt Positioning — GeoClue2 on Linux, Windows Location on
// Windows, CoreLocation on macOS.
//
// Compiled only when Qt6::Positioning was found at configure time; see
// libclima/CMakeLists.txt and DeviceLocator::create(), which is the only thing
// that constructs this.
//
// ---- Qt6::Positioning links Qt6::Core and nothing else ----------------------
//
// Checked, and it has to be: cmake/ClimaEngineGuard.cmake refuses to configure
// a libclima whose link closure reaches Qt6::Gui, and a positioning module that
// dragged in a windowing toolkit would take the engine's whole GUI-free promise
// with it. `libQt6Positioning.so.6` needs `libQt6Core.so.6` and no other Qt
// library, and its CMake interface names `Qt6::Core` alone. The QML bindings
// that would have pulled in QtQml live in a separate Qt6::PositioningQuick
// target, which this does not link and must not.
//
// ---- requestUpdate, not startUpdates ----------------------------------------
//
// One fix, on demand, and then nothing. `startUpdates()` would keep a GeoClue2
// client alive for the life of the process — a location indicator burning in
// the user's status bar, a GPS radio kept warm, and a stream of positions
// nobody asked for. A weather app needs to know where you are when you press
// the button. docs/04-architecture.md §4.5 says the same thing about polling:
// no background work the user did not ask for.

#pragma once

#include "libclima/places/devicelocator.h"

class QGeoPositionInfo;
class QGeoPositionInfoSource;

namespace clima {

class QtPositioningLocator final : public DeviceLocator
{
    Q_OBJECT

public:
    explicit QtPositioningLocator(QObject *parent = nullptr);
    ~QtPositioningLocator() override;

    [[nodiscard]] bool isAvailable() const override;

    void requestPosition() override;
    void cancel() override;

    // Which Qt Positioning plugin answered — "geoclue2", "winrt", "corelocation".
    // For a diagnostics panel and for a bug report: "location does not work" is
    // a different conversation depending on the answer.
    [[nodiscard]] QString sourceName() const;

private:
    void onPositionUpdated(const QGeoPositionInfo &info);
    void onErrorOccurred(int error);

    QGeoPositionInfoSource *m_source = nullptr;
};

} // namespace clima
