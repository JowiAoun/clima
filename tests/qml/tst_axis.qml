// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The hourly chart's value axis, and the one thing it must never do.
//
// Nine of the eleven metrics carry a fixed min/max, and that is deliberate: an
// axis that means the same thing every time you look at it is what lets a
// reader compare Tuesday with Friday, where an auto axis makes a flat day look
// dramatic. What those numbers are not is a *clip*. Each was written down as
// the range the weather usually sits in, and weather leaves it — and when it
// did, the curve was drawn past the last gridline and out of the card.
//
// That shipped. Toronto's visibility runs to 64 km against an axis that stopped
// at 25, so two thirds of the afternoon was drawn above the plot box. It was
// not the only one exposed, and it could not have been caught by the fifty
// golden images because none of them photographs the visibility chart.
//
// So the guarantee is asserted here instead, on the arithmetic, for every
// metric in the registry and for data well outside each one's comfortable
// range: **the axis contains the data.** Every metric, both ends, always.
import QtQuick
import QtTest
import Clima

TestCase {
    id: testCase
    name: "Axis"

    // A property, not a call: `list` is Q_PROPERTY(QVariantList list READ list).
    readonly property var metrics: Metrics.list

    function test_theRegistryIsLoaded() {
        verify(testCase.metrics.length >= 10,
               "only " + testCase.metrics.length + " metrics — is Metrics reachable?")
    }

    // Values that leave the metric's declared range by a long way, which is what
    // a heatwave, a deep low or a very clear afternoon look like.
    //
    // Never below the floor for an auto-scaled metric. Precipitation is the only
    // one, its floor is zero, and there is no such thing as negative rainfall —
    // so an axis that will not go under zero for it is right rather than
    // clipping, and asking it to would be testing a reading no provider can
    // send. Its ceiling is tested like every other.
    function outsideValues(m) {
        var span = Math.max(1, m.max - m.min)
        var below = m.autoScale === true ? m.min : m.min - span * 0.7
        return [below, m.min + span * 0.3, m.max + span * 1.4,
                m.max + span * 0.2, m.min + span * 0.5]
    }

    function minOf(vals) {
        var v = vals[0]
        for (var i = 1; i < vals.length; ++i) if (vals[i] < v) v = vals[i]
        return v
    }

    function maxOf(vals) {
        var v = vals[0]
        for (var i = 1; i < vals.length; ++i) if (vals[i] > v) v = vals[i]
        return v
    }

    function rows() {
        var out = []
        for (var i = 0; i < testCase.metrics.length; ++i)
            out.push({ tag: testCase.metrics[i].id, m: testCase.metrics[i] })
        return out
    }

    // ---- the guarantee -----------------------------------------------------

    function test_noMetricCanClipItsData_data() { return rows() }

    function test_noMetricCanClipItsData(row) {
        var m = row.m
        var vals = outsideValues(m)
        var lo = Metrics.axisMin(m, vals)
        var hi = Metrics.axisMax(m, vals)

        verify(lo <= minOf(vals) + 1e-9,
               row.tag + ": the data reaches " + minOf(vals).toFixed(1)
               + " and the axis floor is " + lo.toFixed(1)
               + ". Everything below the floor is drawn under the plot box.")

        verify(hi >= maxOf(vals) - 1e-9,
               row.tag + ": the data reaches " + maxOf(vals).toFixed(1)
               + " and the axis ceiling is " + hi.toFixed(1)
               + ". Everything above the ceiling is drawn over the plot box — "
               + "which is the visibility chart's 64 km against a 25 km axis, "
               + "the defect this file exists for.")
    }

    // Giving way is for data that needs it. A chart whose hours all sit inside
    // the declared range must draw the declared range, or the fixed axis has
    // quietly become an auto one and Tuesday stops being comparable with Friday.
    function test_anAxisThatIsNotPushedDoesNotMove_data() { return rows() }

    function test_anAxisThatIsNotPushedDoesNotMove(row) {
        var m = row.m
        if (m.autoScale === true)
            return   // precipitation asked for the opposite; see the registry.

        var span = m.max - m.min
        var inside = [m.min + span * 0.1, m.min + span * 0.5, m.max - span * 0.1]

        compare(Metrics.axisMin(m, inside), m.min,
                row.tag + "'s floor moved for data that fits inside it")
        compare(Metrics.axisMax(m, inside), m.max,
                row.tag + "'s ceiling moved for data that fits inside it")
    }

    // An axis with no reading on it at all — every hour absent, which is what a
    // provider gap looks like — must not collapse or run away.
    function test_anEmptySeriesLeavesTheDeclaredAxis_data() { return rows() }

    function test_anEmptySeriesLeavesTheDeclaredAxis(row) {
        var m = row.m
        if (m.autoScale === true)
            return

        compare(Metrics.axisMin(m, []), m.min, row.tag + " moved its floor for no data")
        compare(Metrics.axisMax(m, []), m.max, row.tag + " moved its ceiling for no data")

        var allNaN = [NaN, NaN, NaN]
        compare(Metrics.axisMin(m, allNaN), m.min, row.tag + " moved its floor for NaN")
        compare(Metrics.axisMax(m, allNaN), m.max, row.tag + " moved its ceiling for NaN")
    }

    // ---- the gridlines -----------------------------------------------------

    // An axis that gave way must be labelled over the whole of what it now
    // shows. Growing the range and leaving the ticks where they were is the same
    // defect one step out: a stretch of plot with no scale against it.
    function test_ticksSpanWhateverTheAxisEndedUpBeing_data() { return rows() }

    function test_ticksSpanWhateverTheAxisEndedUpBeing(row) {
        var m = row.m
        var vals = outsideValues(m)
        var lo = Metrics.axisMin(m, vals)
        var hi = Metrics.axisMax(m, vals)
        var ticks = Metrics.axisTicks(m, vals)

        verify(ticks.length >= 2, row.tag + " drew " + ticks.length + " gridline(s)")
        compare(ticks[0], lo, row.tag + "'s first gridline is not the axis floor")

        // The last tick sits at or below the ceiling — a range that is not a
        // whole number of steps ends between two of them, which is correct and
        // is why this is not an equality.
        var last = ticks[ticks.length - 1]
        verify(last <= hi + 1e-9, row.tag + "'s last gridline is above the axis")
        verify(hi - last < (hi - lo) / 2,
               row.tag + " labels only up to " + last + " on an axis reaching "
               + hi + " — the top half of the plot has no scale against it.")

        // Eleven gridlines on a 170 px plot is a hatch, not a scale.
        verify(ticks.length <= 10,
               row.tag + " drew " + ticks.length + " gridlines")
    }

    // Evenly spaced, and on numbers a person would have chosen. A range that has
    // given way at both ends carries more labels than the gutter has room for,
    // and the step is doubled to fit — doubling is what keeps 5 going to 10 and
    // 25 to 50 rather than to something nobody writes down.
    function test_gridlinesAreEvenlySpaced_data() { return rows() }

    function test_gridlinesAreEvenlySpaced(row) {
        var m = row.m
        if (m.autoScale === true)
            return

        var ticks = Metrics.axisTicks(m, outsideValues(m))
        if (ticks.length < 3)
            return

        var step = ticks[1] - ticks[0]
        verify(step > 0, row.tag + "'s gridlines do not ascend")
        for (var i = 2; i < ticks.length; ++i)
            verify(Math.abs((ticks[i] - ticks[i - 1]) - step) < 0.01,
                   row.tag + "'s gridlines are unevenly spaced at " + ticks[i])

        var registry = m.step
        var ratio = step / registry
        verify(Math.abs(ratio - Math.round(ratio)) < 0.001
               && Math.round(ratio) === Math.pow(2, Math.round(Math.log(ratio) / Math.log(2))),
               row.tag + "'s gridline step is " + step + " against a declared "
               + registry + " — a widened axis doubles its step and does nothing else.")
    }

    // ---- the overlay -------------------------------------------------------

    // Three metrics draw a second line over the series. Whatever the axis is
    // asked to hold has to include it, or the line goes off the box — which is
    // the same defect as clipping, reached from the other side. The chart passes
    // both series in; this is the half of the contract the registry owns.
    function test_everyOverlayNamesASeriesTheChartCanPlot_data() { return rows() }

    function test_everyOverlayNamesASeriesTheChartCanPlot(row) {
        var m = row.m
        if (m.overlay === undefined)
            return

        verify(m.overlay.length > 0, row.tag + " declares an empty overlay")
        verify(m.overlayLegend !== undefined && m.overlayLegend.length > 0,
               row.tag + " draws a second line and does not name it in the legend")
        verify(m.overlay !== m.series,
               row.tag + " overlays its own series on itself")
    }

}
