// SPDX-License-Identifier: GPL-3.0-or-later
// The component catalogue behind `./run.sh --gallery`.
//
// One entry per component, grouped. An entry names the file to instantiate and
// the properties to instantiate it with, so adding a component to the gallery
// is a few lines here rather than a new QML file — and a component that is in
// the tree but not in this list shows up as a gap you can see.
//
// `variants` renders the same component several times side by side. That is
// what the gallery is for: a toggle you can only see switched off is a toggle
// you have not checked, and the states no screen currently uses are exactly
// the ones that rot.
//
// Fields:
//   name     what to call it in the sidebar
//   file     the .qml to instantiate; omit for a `kind` page
//   kind     "palette" | "type" for the generated foundation pages
//   blurb    one line, shown above the stage
//   stage    { w, h } when the component has no implicit size of its own
//   fills    true for a screen or a shell: in a device frame it takes the
//            whole device rather than a content column inside it
//   variants [{ label, props }]; omit for a single default instance
//
// `stage.w` and the viewport frames interact, and the rule is worth stating:
// a stage width is a stand-in for a host that is not there, so when a frame
// *is* there the frame wins and the component gets the width that viewport's
// shell would actually give it. `stage.h` is left alone — a height in this
// catalogue is usually the component saying it has no opinion, and the frame
// has none either.
.pragma library

.import "theme.js" as Theme
.import "precip.js" as Precip

// A gentle curve for the series specimens, so the two chart primitives have
// something to draw. Sample input for a catalogue, not weather.
function _curve(n, w, h) {
    var pts = [];
    for (var i = 0; i < n; ++i) {
        var t = i / (n - 1);
        var y = 0.5 + 0.34 * Math.sin(t * Math.PI * 1.15 - 0.4);
        pts.push({ x: t * w, y: h - y * h });
    }
    return pts;
}

// Precipitation levels, as specimens. The page can only ever show the weather
// its mock data has, and at 18–28 °C that is rain — so every frozen level below
// exists solely here, which is the argument for having a gallery at all.
//
// `hours` wide of one level, with a dry hour at each end so the wash has an
// edge to draw and you can see where it decided the spell begins.
function _spell(type, intensity, hours) {
    var cells = [null];
    var wet = Precip.uniform(hours, type, intensity);
    for (var i = 0; i < wet.length; ++i)
        cells.push(wet[i]);
    cells.push(null);
    return cells;
}

function _levels(hours) {
    var out = [];
    // The six levels anyone would name first, then the four that only exist
    // because the model is a type crossed with an intensity rather than ten
    // hand-drawn pictures.
    var levels = [
        ["drizzle", "light"],
        ["rain", "light"], ["rain", "moderate"], ["rain", "heavy"],
        ["snow", "light"], ["snow", "moderate"], ["snow", "heavy"],
        ["sleet", "moderate"], ["hail", "moderate"], ["thunder", "heavy"]
    ];
    for (var i = 0; i < levels.length; ++i)
        out.push({
            label: Precip.label(levels[i][0], levels[i][1]),
            props: { cells: _spell(levels[i][0], levels[i][1], hours),
                     hourWidth: 48, contentWidth: 48 * (hours + 2) }
        });
    return out;
}

var groups = [
    {
        name: "Foundations",
        items: [
            { name: "Colour", kind: "palette",
              blurb: "Every token in theme.js, over the page gradient it is composited on." },
            { name: "Type", kind: "type",
              blurb: "The type scale. Sizes are tokens: a range is not a rule." }
        ]
    },
    {
        name: "Weather details",
        items: [
            { name: "Card shell", file: "DetailCard.qml",
              blurb: "The anatomy the twelve share: title, visualisation slot, status line, context. Shown with the slot empty.",
              variants: [
                  { label: "with trend", props: { title: "Card title", status: "Status", body: "Two lines of context sit here, and the shell reserves both whether the sentence fills them or not.", trend: "up" } },
                  { label: "no trend", props: { title: "Card title", status: "Status", body: "One line.", trend: "none" } }
              ] },
            { name: "Temperature",   file: "DetailTemperatureCard.qml",   blurb: "Twelve-hour sparkline; observed solid, forecast dimmed." },
            { name: "Feels like",    file: "DetailFeelsLikeCard.qml",     blurb: "Two curves and the band between them — the card's subject is the distance." },
            { name: "Cloud cover",   file: "DetailCloudCoverCard.qml",    blurb: "Dial, ramped off the cloud palette: clear reads sky-blue, overcast near-white." },
            { name: "Precipitation", file: "DetailPrecipitationCard.qml", blurb: "The number says how much; the columns say when." },
            { name: "Wind",          file: "DetailWindCard.qml",          blurb: "Compass rose: blunt end into the wind, reach scales with speed." },
            { name: "Humidity",      file: "DetailHumidityCard.qml",      blurb: "Eight readings against full-height tracks." },
            { name: "UV",            file: "DetailUvCard.qml",            blurb: "WHO bands are the palette, so the ring itself carries the reading." },
            { name: "Air quality",   file: "DetailAirQualityCard.qml",    blurb: "The same dial as UV, on the European AQI bands." },
            { name: "Visibility",    file: "DetailVisibilityCard.qml",    blurb: "A sight line down the long axis of the box." },
            { name: "Pressure",      file: "DetailPressureCard.qml",      blurb: "Sparkline against a fixed 1005–1020 mb scale." },
            { name: "Sun",           file: "DetailSunCard.qml",           blurb: "Altitude as a sinusoid; the horizon is zero, so daylight's width is the day length." },
            { name: "Moon",          file: "DetailMoonCard.qml",          blurb: "The sun card's twin, on the night ramp." }
        ]
    },
    {
        name: "Screens",
        items: [
            { name: "Weather page", file: "WeatherPage.qml", stage: { w: 1300, h: 740 }, fills: true,
              blurb: "The whole thing: location, current conditions, hourly, details. Scrolls — its Flickable is layered, which is what keeps every chart on it inside the viewport." },
            // 980, not the page's own 1244: the gallery's rail takes 232 px, so a
            // full-width stage runs off the right of a default window and clips
            // the dew-point slug and the high/low — the two things furthest right.
            { name: "Current conditions", file: "CurrentConditions.qml", stage: { w: 980, h: 0 },
              blurb: "The page headline. Every number on it is read from the same place the detail card for that measurable reads from, so the two cannot drift." },
            { name: "Weather details grid", file: "WeatherDetails.qml", stage: { w: 1244, h: 0 },
              blurb: "All twelve, responsive columns. Lays out in full and reports its height; the page it sits on owns the scrolling." },
            { name: "Hourly overview", file: "HourlyOverview.qml", stage: { w: 1000, h: 430 },
              blurb: "The chart panel: crossed gradients, no stroked line.",
              variants: [
                  { label: "overview", props: { metricId: "overview" } },
                  { label: "wind",     props: { metricId: "wind" } },
                  { label: "uv",       props: { metricId: "uv" } }
              ] },
            { name: "Hourly list", file: "HourlyList.qml", stage: { w: 1000, h: 330 },
              blurb: "The same hours as rows, for scanning rather than shape-reading." },
            { name: "Day strip", file: "DayStrip.qml", stage: { w: 1100, h: 130 },
              blurb: "Seven days; the selected card abuts the panel below rather than overlapping it." },
            { name: "Metric tab bar", file: "MetricTabBar.qml", stage: { w: 900, h: 38 },
              blurb: "Metric pills and the chart/list switch." }
        ]
    },
    {
        // Everything below runs at 390 px in the app. Reviewing it at the
        // gallery's own stage width is reviewing a layout the phone never
        // draws — pick the Mobile viewport in the rail, or pass
        // `--viewport mobile` alongside `--gallery`.
        name: "Mobile screens",
        items: [
            { name: "Mobile shell", file: "MobileShell.qml", fills: true,
              blurb: "Five destinations under a bottom nav. The nav pill slides; the page behind it does not transition.",
              variants: [
                  { label: "today",   props: { tab: "today" } },
                  { label: "hourly",  props: { tab: "hourly" } },
                  { label: "monthly", props: { tab: "monthly" } }
              ] },
            { name: "Today screen", file: "MobileTodayPage.qml", fills: true,
              blurb: "The screen the app opens on: headline, hourly strip, ten days, sun & moon, pollen, activities." },
            { name: "Hourly screen", file: "MobileHourlyPage.qml", fills: true,
              blurb: "The desktop's chart card at 40 px columns and a shorter plot, with the ten metric pills replaced by one button." },
            { name: "Monthly screen", file: "MobileMonthlyPage.qml", fills: true,
              blurb: "A month of forecasts, four days to a row rather than seven." },
            { name: "Maps screen", file: "MobileMapsPage.qml", fills: true,
              blurb: "The placeholder. There is no map component; this says so in a way a screenshot cannot hide." },
            { name: "Me screen", file: "MobileMePage.qml", fills: true,
              blurb: "Units, places and attribution. The one screen here that is a proposal rather than a rebuild." }
        ]
    },
    {
        name: "Mobile parts",
        items: [
            { name: "Bottom nav", file: "BottomNav.qml", stage: { w: 390, h: 0 },
              blurb: "The only persistent chrome the phone has, and the only thing on it allowed to be more opaque than a wash.",
              variants: [
                  { label: "today", props: { currentId: "today" } },
                  { label: "maps",  props: { currentId: "maps" } },
                  { label: "me",    props: { currentId: "me" } }
              ] },
            { name: "Mobile card", file: "MobileCard.qml", stage: { w: 362, h: 0 },
              blurb: "The shell the phone's cards are built in. Unlike DetailCard it grows to its body, because on a phone a card is as tall as what is in it.",
              variants: [
                  { label: "with link", props: { title: "Today", link: "Hourly" } },
                  { label: "plain",     props: { title: "Sun & Moon" } }
              ] },
            { name: "Mobile hero", file: "MobileCurrentWeather.qml", stage: { w: 362, h: 0 },
              blurb: "The desktop headline with no card, no high/low, and six slugs wrapped three by two." },
            { name: "Hour strip", file: "MobileHourStrip.qml", stage: { w: 362, h: 0 },
              blurb: "Twenty-four hours, two at a time. The band's top edge is the temperature — the reference draws it flat." },
            { name: "Ten-day strip", file: "MobileDailyStrip.qml", stage: { w: 362, h: 0 },
              blurb: "A readout, not a control. Highs bold, lows not: the pair is the reading and one of them has to lead." },
            { name: "Sun & moon", file: "MobileSunMoonCard.qml", stage: { w: 330, h: 0 },
              blurb: "Two arcs on one reveal. They have to leave together or it reads as a race." },
            { name: "Sky arc", file: "SkyArc.qml", stage: { w: 170, h: 0 },
              blurb: "Progress from rise to set. Not DetailSunCard's altitude sinusoid — at this size that is a bump with nothing to read.",
              variants: [
                  { label: "mid-morning", props: { riseMin: 364, setMin: 1243, nowMin: 620,
                                                   riseLabel: "6:04", riseSuffix: "AM", riseName: "Sunrise",
                                                   setLabel: "8:43", setSuffix: "PM", setName: "Sunset",
                                                   span: "14 hrs 39 mins" } },
                  { label: "just risen",  props: { riseMin: 364, setMin: 1243, nowMin: 400,
                                                   riseLabel: "6:04", riseSuffix: "AM", riseName: "Sunrise",
                                                   setLabel: "8:43", setSuffix: "PM", setName: "Sunset",
                                                   span: "14 hrs 39 mins" } },
                  { label: "already set", props: { riseMin: 364, setMin: 1243, nowMin: 1400,
                                                   riseLabel: "6:04", riseSuffix: "AM", riseName: "Sunrise",
                                                   setLabel: "8:43", setSuffix: "PM", setName: "Sunset",
                                                   span: "14 hrs 39 mins" } }
              ] },
            { name: "Pollen", file: "MobilePollenCard.qml", stage: { w: 330, h: 0 },
              blurb: "The headline is a word, because the published answer is a band and nobody reads grains per cubic metre." },
            { name: "Activities", file: "MobileActivitiesCard.qml", stage: { w: 330, h: 0 },
              blurb: "Five verdicts. The dot is last on the row, or a column of colours outreads the labels saying what they are about." },
            { name: "Week strip", file: "MobileWeekStrip.qml", stage: { w: 362, h: 0 },
              blurb: "Seven days across a phone, so a column is 48 px. That is why it is not DayStrip." },
            { name: "Metric picker", file: "MobileMetricPicker.qml",
              blurb: "Ten pills become one button and a list. Shown open, because a menu you have only seen shut is a menu you have not checked.",
              stage: { w: 0, h: 0 },
              variants: [
                  { label: "closed", props: { currentId: "overview", open: false } },
                  { label: "open",   props: { currentId: "wind",     open: true } }
              ] },
            { name: "Calendar", file: "MobileCalendar.qml", stage: { w: 330, h: 0 },
              blurb: "Four columns, not seven. A forecast is scanned for warm stretches, not looked up by weekday." },
            { name: "Map placeholder", file: "MapPlaceholder.qml", stage: { w: 362, h: 520 },
              blurb: "Deliberately off-palette and deliberately ugly. A tasteful empty state is what a finished screen with no data looks like." },
            { name: "Nav glyph", file: "NavGlyph.qml",
              blurb: "Outlines, not fills: at 22 px in a row of five, the silhouette is all there is to tell them apart.",
              variants: [
                  { label: "today",   props: { kind: "today",   glyphSize: 30 } },
                  { label: "hourly",  props: { kind: "hourly",  glyphSize: 30 } },
                  { label: "monthly", props: { kind: "monthly", glyphSize: 30 } },
                  { label: "maps",    props: { kind: "maps",    glyphSize: 30 } },
                  { label: "me",      props: { kind: "me",      glyphSize: 30 } }
              ] }
        ]
    },
    {
        name: "Controls",
        items: [
            { name: "Chevron", file: "ChevronGlyph.qml",
              blurb: "One path, rotated. Four hand-written ones is four chances for one to drift off its siblings.",
              variants: [
                  { label: "right", props: { direction: "right", glyphSize: 22 } },
                  { label: "down",  props: { direction: "down",  glyphSize: 22 } },
                  { label: "left",  props: { direction: "left",  glyphSize: 22 } },
                  { label: "up",    props: { direction: "up",    glyphSize: 22 } }
              ] },
            { name: "Location bar", file: "LocationBar.qml",
              blurb: "Sits on the page gradient rather than on a surface — there is nothing here to lift off the background.",
              variants: [
                  { label: "home",     props: { label: "Toronto, Ontario", isHome: true } },
                  { label: "not home", props: { label: "Reykjavík, Iceland", isHome: false } }
              ] },
            { name: "Section header", file: "SectionHeader.qml", stage: { w: 420, h: 0 },
              blurb: "One token for every section title on the page. The hourly and details sections had picked 18 and 15 independently.",
              variants: [
                  { label: "with stamp", props: { title: "Weather details", stamp: "12:28 PM" } },
                  { label: "plain",      props: { title: "Hourly" } }
              ] },
            { name: "Trend badge", file: "TrendBadge.qml",
              blurb: "The arrow tracks the number, not whether the news is good.",
              variants: [
                  { label: "up",     props: { direction: "up",     badgeSize: 22 } },
                  { label: "down",   props: { direction: "down",   badgeSize: 22 } },
                  { label: "steady", props: { direction: "steady", badgeSize: 22 } },
                  { label: "none",   props: { direction: "none",   badgeSize: 22 } }
              ] },
            { name: "Feels-like toggle", file: "FeelsLikeToggle.qml",
              blurb: "Both states, because a switch you have only seen off is a switch you have not checked.",
              variants: [
                  { label: "off", props: { checked: false } },
                  { label: "on",  props: { checked: true } }
              ] },
            { name: "Pager button", file: "PagerButton.qml",
              blurb: "Floats over the chart, so it stays more opaque than a surface.",
              variants: [
                  { label: "left",     props: { pointsLeft: true,  enabledState: true } },
                  { label: "right",    props: { pointsLeft: false, enabledState: true } },
                  { label: "disabled", props: { pointsLeft: true,  enabledState: false } }
              ] },
            { name: "Tab fillet", file: "TabFillet.qml", stage: { w: 40, h: 40 },
              blurb: "The concave corner where a selected tab meets its panel.",
              variants: [
                  { label: "left",  props: { filletRadius: 18, fillColor: "#33ffffff", mirrored: false } },
                  { label: "right", props: { filletRadius: 18, fillColor: "#33ffffff", mirrored: true } }
              ] }
        ]
    },
    {
        name: "Glyphs",
        items: [
            { name: "Weather glyph", file: "WeatherGlyph.qml",
              blurb: "Condition icons, drawn rather than shipped as raster.",
              variants: [
                  { label: "clear-day",    props: { kind: "clear-day",    glyphSize: 44 } },
                  { label: "clear-night",  props: { kind: "clear-night",  glyphSize: 44 } },
                  { label: "partly-day",   props: { kind: "partly-day",   glyphSize: 44 } },
                  { label: "partly-night", props: { kind: "partly-night", glyphSize: 44 } },
                  { label: "cloudy",       props: { kind: "cloudy",       glyphSize: 44 } },
                  { label: "rain",         props: { kind: "rain",         glyphSize: 44 } },
                  { label: "rain-night",   props: { kind: "rain-night",   glyphSize: 44 } }
              ] },
            { name: "Day icon badge", file: "DayIconBadge.qml",
              blurb: "The glyph on its pale day plate — clouds get a darker variant there or they vanish.",
              variants: [
                  { label: "day",   props: { kind: "partly-day",   night: false, badgeSize: 56 } },
                  { label: "night", props: { kind: "partly-night", night: true,  badgeSize: 56 } }
              ] },
            { name: "Moon glyph", file: "MoonGlyph.qml",
              blurb: "Illumination is a real parameter, not three fixed pictures.",
              variants: [
                  { label: "0.08", props: { illuminated: 0.08, glyphSize: 40 } },
                  { label: "0.50", props: { illuminated: 0.50, glyphSize: 40 } },
                  { label: "0.95", props: { illuminated: 0.95, glyphSize: 40 } }
              ] },
            { name: "Sun event glyph", file: "SunEventGlyph.qml",
              blurb: "Sunrise and sunset: the half-disc sits above or below the horizon line, so the two differ in shape and not only in colour.",
              variants: [
                  { label: "sunrise", props: { kind: "sunrise", glyphSize: 40 } },
                  { label: "sunset",  props: { kind: "sunset",  glyphSize: 40 } }
              ] },
            { name: "Droplet", file: "DropletGlyph.qml",
              blurb: "Precipitation marker.",
              variants: [ { label: "", props: { glyphSize: 34 } } ] }
        ]
    },
    {
        name: "Chart parts",
        items: [
            { name: "Series area", file: "SeriesArea.qml", stage: { w: 480, h: 150 },
              blurb: "Crossed gradients: colour across, alpha down. Sample curve, not weather.",
              variants: [
                  { label: "temperature", props: { points: _curve(13, 480, 150), baselineY: 150,
                                                   gradientTop: 0, gradientBottom: 150,
                                                   fillRamp: Theme.ramp.temp.fill, lineRamp: Theme.ramp.temp.line } },
                  { label: "humidity",    props: { points: _curve(13, 480, 150), baselineY: 150,
                                                   gradientTop: 0, gradientBottom: 150,
                                                   fillRamp: Theme.ramp.humidity.fill, lineRamp: Theme.ramp.humidity.line } }
              ] },
            { name: "Series bars", file: "SeriesBars.qml", stage: { w: 480, h: 150 },
              blurb: "For sums and bands. Sample values, not weather.",
              variants: [
                  { label: "cloud", props: { values: [12, 20, 34, 46, 58, 71, 66, 52, 44, 30], hourWidth: 48,
                                             axisTop: 8, axisBottom: 142, minValue: 0, maxValue: 100,
                                             ramp: Theme.ramp.cloud.fill } }
              ] },
            { name: "Precipitation strip", file: "PrecipitationStrip.qml", stage: { w: 480, h: 28 },
              blurb: "One cell per label interval; past hours are hatched, not blank." },
            { name: "Precipitation wash", file: "PrecipBands.qml", stage: { w: 480, h: 120 },
              blurb: "When it falls. Hue is the type, alpha the intensity — a narrow ladder on purpose, because \"is it raining here\" has to read the same at every level.",
              variants: _levels(8) },
            { name: "Precipitation field", file: "PrecipField.qml", stage: { w: 480, h: 200 },
              blurb: "What is falling, and how hard. Ten levels off one particle model: type picks the shape, intensity scales the count, size and speed. Shown without the wash it normally sits over.",
              variants: _levels(8) },
            { name: "Hatch", file: "HatchPattern.qml", stage: { w: 240, h: 90 },
              blurb: "\"The past — there is no forecast here\", so absent data reads as deliberate." }
        ]
    }
];
