// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Catalogue id → component. The one place the mapping exists.
//
// ============================================================================
// WHY A SWITCH AND NOT A URL BUILT FROM THE ID
//
// `Qt.createComponent("qrc:/…/" + id + "Widget.qml")` would be four lines and
// would need no edit when a widget is added, which is exactly what is wrong
// with it. It moves the failure from configure time to run time: a typo in
// widgets/catalogue.json becomes an empty tile and a line in the journal
// instead of a QML file that will not compile, and a widget file that stops
// being listed in CMake is a module that still builds.
//
// The switch below is the same argument app/CMakeLists.txt makes at length
// about never using file(GLOB) for QML_FILES, one layer up.
//
// tests/tst_widgets.cpp asserts that every id in the catalogue reaches a
// component here, so the list going stale is caught by a test rather than by
// somebody's desktop.

import QtQuick

Loader {
    id: root

    required property string widgetId
    property string place: WidgetOptions.place

    asynchronous: false

    sourceComponent: {
        switch (root.widgetId) {
        case "current-conditions":     return currentConditions
        case "temperature-sparkline":  return temperatureSparkline
        case "hourly-strip":           return hourlyStrip
        case "precipitation-6h":       return precipitation
        case "wind-rose":              return windRose
        case "uv-dial":                return uvDial
        case "aqi-dial":               return aqiDial
        case "sun-arc":                return sunArc
        case "daily-strip":            return dailyStrip
        case "alerts":                 return alerts
        }
        console.warn("Clima.Widgets: no component for widget id \"" + root.widgetId + "\"")
        return null
    }

    Component { id: currentConditions;    CurrentConditionsWidget    { place: root.place } }
    Component { id: temperatureSparkline; TemperatureSparklineWidget { place: root.place } }
    Component { id: hourlyStrip;          HourlyStripWidget          { place: root.place } }
    Component { id: precipitation;        PrecipitationWidget        { place: root.place } }
    Component { id: windRose;             WindRoseWidget             { place: root.place } }
    Component { id: uvDial;               UvDialWidget               { place: root.place } }
    Component { id: aqiDial;              AqiDialWidget              { place: root.place } }
    Component { id: sunArc;               SunArcWidget               { place: root.place } }
    Component { id: dailyStrip;           DailyStripWidget           { place: root.place } }
    Component { id: alerts;               AlertsWidget               { place: root.place } }
}
