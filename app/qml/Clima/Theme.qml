// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The design tokens, as something a binding is allowed to watch.
//
// Every value, and every argument for it, is still in theme.js. This file does
// not restate either: it declares one QML property per token and binds it to
// that table. The indirection *is* the feature.
//
// A `.pragma library` is evaluated once per engine and reading a property out
// of one produces no change notification. So
//
//     color: Theme.ink.primary
//
// used to be evaluated exactly once — when the binding was created — and never
// again. Swapping the table at runtime repainted nothing, which is why
// dark/light was not merely unbuilt but unbuildable, and why every component
// written against the JS namespace in the meantime would have had to be
// migrated twice. A QML property has a NOTIFY signal, so the identical line
// above is now a live binding: when a second table and a mode to choose between
// them arrive, all ~60 components repaint and not one of them is edited.
//
// ---- why the groups are objects and not prefixes ---------------------------
//
// `surface`, `ink`, `line`, `metric`, `type` and the rest are sub-objects rather
// than flattened names (`inkPrimary`) for one reason beyond spelling: their
// identity never changes. A theme switch will assign new values to the
// properties *inside* these objects; it will never replace an object. Nothing
// that holds `Theme.ink` — a binding, a cached reference in a Canvas paint
// handler, a property alias — is ever left pointing at a corpse.
//
// Each group is an *inline component* instantiated once, rather than an
// anonymous `QtObject { … }`, and that is a tooling decision with real teeth.
// A property declared `QtObject` tells qmllint and qmlls that the thing on the
// other side has QObject's members and nothing else — so every one of the six
// hundred token reads in the tree came back as `Member "primary" not found on
// type "QObject"`. Six hundred warnings in the lint target and a squiggle under
// every colour in the editor is not a cost worth paying for two fewer lines
// here. Naming the type restores the check: a typo in a token name is a lint
// error again, which is exactly what it was when this was a JS object literal.
//
// ---- groups and tables -----------------------------------------------------
//
// Two shapes live here and the difference is what the keys are.
//
//   A *group* is keyed by token name: the eleven colour roles, plus `metric`,
//   `type`, `motion`, `scale`, `star`, `surfaceAlpha`. Someone wrote each of
//   those names down on purpose, and a component reads them one at a time, so
//   each is a property with its own notification.
//
//   A *table* is keyed by a data value: `sky` by a phase, `ramp` by a metric
//   id, `precip.wash` by a precipitation type. The keys are not tokens — they
//   are the domain — and the lookups in the tree are all dynamic
//   (`Theme.ramp[metric.ramp]`, `Theme.sky[phase]`). Declaring one property per
//   weather type would buy nothing that `Tokens.precip.wash[type]` does not
//   already give, so these stay whole objects behind a single `var`. They are
//   still reactive: re-binding the `var` notifies, which is the granularity a
//   whole-table swap needs anyway.
//
// ---- why theme.js is still on disk -----------------------------------------
//
// Its sibling viewports.js was absorbed into Viewports.qml and deleted, and the
// obvious question is why this one was not. gallery.js is why: it is a
// `.pragma library`, it reads `Theme.ramp` to give the chart specimens
// something to draw, and a `.pragma library` cannot import a QML singleton.
// Absorbing this file would mean either editing the catalogue to route three
// ramp lookups through QML, or keeping a second copy of a seventy-line colour
// table in step with the first by hand. A binding per token is the cheaper of
// the three, and it puts the values and the essays about them in one place —
// which is where they were.
//
// ---- what this file does not decide ----------------------------------------
//
// The role names, the deletions and the seven absorbed literals are all
// arguments made in theme.js, next to the values they are about. This file
// republishes whatever is there. The one thing it adds is `motion.easing`,
// which cannot live in a plain JS library because it is a QML enum, and the one
// thing it *asks* rather than states is `type.family` — see below.
pragma Singleton

import QtQuick
import "theme.js" as Tokens

QtObject {
    id: theme

    // The token names in a group, in declaration order.
    //
    // The gallery's palette and type pages enumerate a group rather than
    // transcribing it, so that a token added to theme.js shows up there without
    // anyone remembering to add it. `Object.keys()` on a plain JS object
    // returned exactly the tokens; on a QObject it also returns `objectName`
    // and a `<name>Changed` entry per property. That is a wart of the port and
    // not something every caller should have to know, so it is filtered here
    // and only here.
    function names(group) {
        return Object.keys(group).filter(function (key) {
            return key !== "objectName" && !/Changed$/.test(key)
        })
    }

    // The colour roles, in the order the design system introduces them.
    //
    // This is the one list in the project that has to be maintained by hand, and
    // it is here rather than in the gallery because it is a fact about the
    // palette rather than about the tool that draws it. The palette page walks
    // it and then walks `names()` inside each role, so a *token* added to a role
    // still appears without anyone doing anything; only a whole new role costs a
    // line. That trade is deliberate — a new role is a design decision worth
    // one line of bookkeeping, and a new token is not.
    readonly property var colorRoles: [
        "page", "surface", "ink", "line", "accent",
        "control", "overlay", "state", "glyph", "badge", "scaffold"
    ]

    // ---- surfaces ----------------------------------------------------------
    // The alpha ladder, as the leading pair of an #AARRGGBB literal. Nothing
    // reads these — the composed `surface.*` values below are what the tree
    // uses — and they stay exported because they are the ladder the design
    // system quotes, not a leftover.
    component SurfaceAlphaTokens: QtObject {
        readonly property string recede: Tokens.surfaceAlpha.recede
        readonly property string base:   Tokens.surfaceAlpha.base
        readonly property string raised: Tokens.surfaceAlpha.raised
    }
    readonly property SurfaceAlphaTokens surfaceAlpha: SurfaceAlphaTokens { }

    // ---- colour ------------------------------------------------------------
    // Eleven roles. Every one of them is declared `string` rather than `color`,
    // which is not laziness. A `color` property round-trips through QColor, so
    // `"transparent"` reads back as `"#00000000"` and `"#ffffff"` as
    // `"#ffffffff"` — and the gallery's palette page prints these values as text
    // beside each swatch. Assigning a string to a `color` property at the call
    // site is a conversion QML already does everywhere, so nothing downstream
    // can tell the difference; the palette page can.

    // The ground everything is composited on. The gradient itself is `sky`.
    component PageTokens: QtObject {
        readonly property string bg: Tokens.page.bg
    }
    readonly property PageTokens page: PageTokens { }

    // §10.1's three-rung ladder, plus §10.12's opaque exceptions.
    component SurfaceTokens: QtObject {
        readonly property string recede: Tokens.surface.recede
        readonly property string base:   Tokens.surface.base
        readonly property string raised: Tokens.surface.raised
        readonly property string panel:  Tokens.surface.panel
        readonly property string rowAlt: Tokens.surface.rowAlt
        readonly property string rowNow: Tokens.surface.rowNow
        readonly property string nav:    Tokens.surface.nav
        readonly property string menu:   Tokens.surface.menu
    }
    readonly property SurfaceTokens surface: SurfaceTokens { }

    // Text. Ink for anything on the accent is `accent.ink`.
    component InkTokens: QtObject {
        readonly property string primary: Tokens.ink.primary
        readonly property string muted:   Tokens.ink.muted
        readonly property string dim:     Tokens.ink.dim
    }
    readonly property InkTokens ink: InkTokens { }

    // Anything a pixel wide: it measures, it separates, or it is the unfilled
    // remainder of a gauge.
    component LineTokens: QtObject {
        readonly property string grid:     Tokens.line.grid
        readonly property string gridWeak: Tokens.line.gridWeak
        readonly property string track:    Tokens.line.track
        readonly property string now:      Tokens.line.now
        readonly property string forecast: Tokens.line.forecast
        readonly property string series:   Tokens.line.series
        readonly property string card:     Tokens.line.card
        readonly property string divider:  Tokens.line.divider
        readonly property string nav:      Tokens.line.nav
        readonly property string menu:     Tokens.line.menu
        readonly property string control:  Tokens.line.control
    }
    readonly property LineTokens line: LineTokens { }

    // The one saturated colour, and the only ink legible on it.
    //
    // This pair used to be `color.accent` and `color.onAccent` on one object,
    // and the second of those could not be written like its neighbours: any
    // binding whose name is `on` + a capital letter is parsed as a signal
    // handler first, so `onAccent:` resolved against a member called `accent` —
    // which existed — and the value went to the signal rather than to the
    // property.
    //
    // How that failed is why it is worth recording. With a literal it is at
    // least an error: `Cannot assign a value to a signal`. With an *expression*
    // — which every token here is, since they all read out of theme.js — a
    // script assigned to a signal is perfectly legal QML, so it compiled clean,
    // ran clean, and left the token as the empty string. An empty string is a
    // valid colour to QML: it paints black. The whole symptom was a 9x9 patch of
    // the trend badge's arrow going from #141d33 to #000000 on four
    // screenshots, with nothing on stderr. The workaround was a `Binding` that
    // named the property as a *string*, taking the parser out of it, at the cost
    // of the property's `readonly`.
    //
    // Naming the pair by role deleted the problem instead of working around it.
    // `fill` and `ink` say what they are for, they sit together because they are
    // one decision, and neither of them starts with "on".
    component AccentTokens: QtObject {
        readonly property string fill: Tokens.accent.fill
        readonly property string ink:  Tokens.accent.ink
    }
    readonly property AccentTokens accent: AccentTokens { }

    // Interactive chrome that carries its own colour rather than a surface's.
    component ControlTokens: QtObject {
        readonly property string toggleTrack:    Tokens.control.toggleTrack
        readonly property string toggleKnob:     Tokens.control.toggleKnob
        readonly property string pagerFill:      Tokens.control.pagerFill
        readonly property string pagerFillHover: Tokens.control.pagerFillHover
        readonly property string pagerGlyph:     Tokens.control.pagerGlyph
        readonly property string navGlyph:       Tokens.control.navGlyph
    }
    readonly property ControlTokens control: ControlTokens { }

    // Drawn over content it must not let through.
    component OverlayTokens: QtObject {
        readonly property string past:      Tokens.overlay.past
        readonly property string pastHatch: Tokens.overlay.pastHatch
        readonly property string hatch:     Tokens.overlay.hatch
        readonly property string caption:   Tokens.overlay.caption
        readonly property string readout:   Tokens.overlay.readout
        readonly property string scrim:     Tokens.overlay.scrim
    }
    readonly property OverlayTokens overlay: OverlayTokens { }

    // Colour as a verdict rather than as a value (§10.5).
    component StateTokens: QtObject {
        readonly property string trendUp:     Tokens.state.trendUp
        readonly property string trendDown:   Tokens.state.trendDown
        readonly property string trendSteady: Tokens.state.trendSteady
        readonly property string good:        Tokens.state.good
        readonly property string caution:     Tokens.state.caution
        readonly property string poor:        Tokens.state.poor
    }
    readonly property StateTokens state: StateTokens { }

    // The paints a weather glyph is drawn in, shared by six screens.
    component GlyphTokens: QtObject {
        readonly property string sunWarm:            Tokens.glyph.sunWarm
        readonly property string sunCool:            Tokens.glyph.sunCool
        readonly property string moon:               Tokens.glyph.moon
        readonly property string moonShade:          Tokens.glyph.moonShade
        readonly property string cloudTop:           Tokens.glyph.cloudTop
        readonly property string cloudBottom:        Tokens.glyph.cloudBottom
        readonly property string cloudTopOnLight:    Tokens.glyph.cloudTopOnLight
        readonly property string cloudBottomOnLight: Tokens.glyph.cloudBottomOnLight
        readonly property string rain:               Tokens.glyph.rain
        readonly property string droplet:            Tokens.glyph.droplet
    }
    readonly property GlyphTokens glyph: GlyphTokens { }

    // The disc a day's glyph sits on in the ten-day strip.
    component BadgeTokens: QtObject {
        readonly property string dayTop:      Tokens.badge.dayTop
        readonly property string dayBottom:   Tokens.badge.dayBottom
        readonly property string nightTop:    Tokens.badge.nightTop
        readonly property string nightBottom: Tokens.badge.nightBottom
    }
    readonly property BadgeTokens badge: BadgeTokens { }

    // Deliberately off-palette: something not built yet.
    component ScaffoldTokens: QtObject {
        readonly property string ink:    Tokens.scaffold.ink
        readonly property string stroke: Tokens.scaffold.stroke
    }
    readonly property ScaffoldTokens scaffold: ScaffoldTokens { }

    // ---- precipitation -----------------------------------------------------
    // Three tokens and three tables in one group. `edge`, `splash` and `flash`
    // are single decisions; `wash`, `washAlpha` and `drop` are keyed by
    // precipitation type and intensity, which is data, and every read of them
    // in the tree is already a dynamic lookup.
    component PrecipTokens: QtObject {
        readonly property var    wash:      Tokens.precip.wash
        readonly property var    washAlpha: Tokens.precip.washAlpha
        readonly property var    drop:      Tokens.precip.drop
        readonly property string edge:      Tokens.precip.edge
        readonly property string splash:    Tokens.precip.splash
        readonly property string flash:     Tokens.precip.flash
    }
    readonly property PrecipTokens precip: PrecipTokens { }

    // ---- the sky -----------------------------------------------------------
    // A table keyed by phase — `night`, `dawn`, `day`, `dusk` — because that is
    // how it is read: `Theme.sky[phase]`, with the phase computed from the
    // clock. Main.qml also leans on a miss returning `undefined`, which is how
    // `--sky nonsense` is rejected rather than obeyed.
    readonly property var sky: Tokens.sky

    // `glow` is four gradient stops rather than one colour, so it is a `var`
    // for the same reason a ramp is: PageBackdrop indexes it.
    component StarTokens: QtObject {
        readonly property string ink:  Tokens.star.ink
        readonly property string line: Tokens.star.line
        readonly property var    glow: Tokens.star.glow
    }
    readonly property StarTokens star: StarTokens { }

    // ---- measurements ------------------------------------------------------
    // Theme-invariant: a 14 px radius is 14 px in any palette. These are not
    // roles and they did not move when the colours became roles.
    component MetricTokens: QtObject {
        readonly property int hourWidth:        Tokens.metric.hourWidth
        readonly property int plotHeight:       Tokens.metric.plotHeight
        readonly property int axisTopPad:       Tokens.metric.axisTopPad
        readonly property int headerBandHeight: Tokens.metric.headerBandHeight
        readonly property int gutterWidth:      Tokens.metric.gutterWidth
        readonly property int stripHeight:      Tokens.metric.stripHeight
        readonly property int stripGap:         Tokens.metric.stripGap
        readonly property int panelPadding:     Tokens.metric.panelPadding
        readonly property int cardPadding:      Tokens.metric.cardPadding
        readonly property int cardRadius:       Tokens.metric.cardRadius
        readonly property int panelRadius:      Tokens.metric.panelRadius
        readonly property int controlRadius:    Tokens.metric.controlRadius
        readonly property int filletRadius:     Tokens.metric.filletRadius

        readonly property int detailCardWidth:  Tokens.metric.detailCardWidth
        readonly property int detailCardHeight: Tokens.metric.detailCardHeight
        readonly property int detailRadius:     Tokens.metric.detailRadius
        readonly property int detailPadH:       Tokens.metric.detailPadH
        readonly property int detailPadV:       Tokens.metric.detailPadV
        readonly property int detailGap:        Tokens.metric.detailGap

        readonly property int sectionGap:       Tokens.metric.sectionGap
        readonly property int pageMargin:       Tokens.metric.pageMargin

        readonly property int mobileMargin:     Tokens.metric.mobileMargin
        readonly property int mobileGap:        Tokens.metric.mobileGap
        readonly property int mobileCardPadH:   Tokens.metric.mobileCardPadH
        readonly property int mobileCardPadV:   Tokens.metric.mobileCardPadV
        readonly property int navHeight:        Tokens.metric.navHeight
        readonly property int navSafeArea:      Tokens.metric.navSafeArea
        readonly property int mobileContentMax: Tokens.metric.mobileContentMax
    }
    readonly property MetricTokens metric: MetricTokens { }

    // ---- type --------------------------------------------------------------
    // `font.pixelSize` is an int in Qt and a fractional value fails object
    // creation, reported only as `Type X unavailable` from the *parent* file.
    // Declaring these `int` here turns that into a value that was rounded on
    // the way in rather than a component that silently does not exist.
    component TypeTokens: QtObject {
        // The face, and the second token in this file that does not come from
        // theme.js. `easing` is there because a QML enum cannot live in a plain
        // JS library; this one is there because the *right* answer is not a
        // string at all — it is a question, asked of the running application.
        //
        // app/appfont.cpp registers the bundled Inter faces and makes the family
        // they declare the application font. That is what every one of the 158
        // Text items in the tree already renders in, without one of them naming
        // a family, and it is what a settings surface would change when D9's
        // "use the system UI font" arrives: one call to QGuiApplication::setFont
        // and the whole tree follows, because that is where a Text resolves its
        // family from.
        //
        // Reading it back here rather than writing "Inter" into theme.js means
        // there is one spelling of the family in the project and it is the one
        // inside the font file. The failure the duplicate would have caused is
        // quiet and slow: swap the bundled face, C++ picks up the new family
        // automatically, theme.js still says the old one, and every component
        // that referenced the token falls back to the host's font — half the
        // screen in the right face and half in the wrong one.
        //
        // Nothing in the tree needs to set `font.family` today; the application
        // font covers it. The token is for the cases that are coming: text drawn
        // into a Canvas or a QPainter, which take a font by name and inherit
        // nothing, and any surface that has to opt back out to the platform.
        //
        // Not live, one caveat. This is read once when the singleton is created,
        // and Qt.application.font has no change notification we can rely on, so
        // a runtime font switch will need this to become a property on a C++
        // singleton with a NOTIFY of its own. That is a later problem, and the
        // call sites do not change when it is solved.
        //
        // The suppression is a gap in qmllint's type data rather than a gap in
        // Qt: `Qt.application.font` is documented and works — the gallery's Type
        // page prints "family · Inter" off this very property — but the QML type
        // description for QQmlApplication does not list `font`, so the linter
        // reports a member that is there. Scoped to the one line, because
        // `missing-property` is the category that catches real typos in a token
        // name and it should keep doing that everywhere else in this file.
        readonly property string family: Qt.application.font.family // qmllint disable missing-property

        readonly property int sectionTitle: Tokens.type.sectionTitle
        readonly property int cardTitle:    Tokens.type.cardTitle
        readonly property int detailTitle:  Tokens.type.detailTitle
        readonly property int reading:      Tokens.type.reading
        readonly property int readingPair:  Tokens.type.readingPair
        readonly property int status:       Tokens.type.status
        readonly property int body:         Tokens.type.body
        readonly property int label:        Tokens.type.label
        readonly property int axis:         Tokens.type.axis
        readonly property int heroReading:  Tokens.type.heroReading
        readonly property int heroUnit:     Tokens.type.heroUnit
        readonly property int heroCaption:  Tokens.type.heroCaption
        readonly property int heroDetail:   Tokens.type.heroDetail
        readonly property int heroLabel:    Tokens.type.heroLabel
    }
    readonly property TypeTokens type: TypeTokens { }

    // ---- motion ------------------------------------------------------------
    // `easing` is the one thing this file adds rather than republishes. theme.js
    // says outright that it cannot hold `Easing.OutCubic` because a QML enum
    // cannot live in a plain JS library, so the house rule — **OutCubic unless
    // there is a stated reason** — is written literally at some sixty call sites
    // and enforced by nothing. A singleton can hold it.
    component MotionTokens: QtObject {
        readonly property int tint:    Tokens.motion.tint
        readonly property int move:    Tokens.motion.move
        readonly property int view:    Tokens.motion.view
        readonly property int reveal:  Tokens.motion.reveal
        readonly property int stagger: Tokens.motion.stagger

        readonly property int easing:  Easing.OutCubic
    }
    readonly property MotionTokens motion: MotionTokens { }

    component ScaleTokens: QtObject {
        readonly property int tempMin:  Tokens.scale.tempMin
        readonly property int tempMax:  Tokens.scale.tempMax
        readonly property int tickStep: Tokens.scale.tickStep
    }
    readonly property ScaleTokens scale: ScaleTokens { }

    // ---- ramps -------------------------------------------------------------
    // A table keyed by ramp name, which is a metric's choice and not a token:
    // the chart reads `Theme.ramp[root.metric.ramp].fill` and the registry is
    // what decides the key. gallery.js reads this table too, and it reads it
    // straight out of theme.js — a `.pragma library` cannot import a QML
    // singleton, which is the other half of why theme.js is still the file the
    // values live in.
    readonly property var ramp: Tokens.ramp
}
