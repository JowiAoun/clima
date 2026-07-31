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
//     color: Theme.color.textPrimary
//
// used to be evaluated exactly once — when the binding was created — and never
// again. Swapping the table at runtime repainted nothing, which is why
// dark/light was not merely unbuilt but unbuildable, and why every component
// written against the JS namespace in the meantime would have had to be
// migrated twice. A QML property has a NOTIFY signal, so the identical line
// above is now a live binding: when W3 gives this file a second table and a
// mode to choose between them, all ~60 components repaint and not one of them
// is edited.
//
// ---- why the groups are objects and not prefixes ---------------------------
//
// `color`, `metric`, `type` and the rest are sub-objects rather than flattened
// names (`colorTextPrimary`) for one reason beyond spelling: their identity
// never changes. A theme switch will assign new values to the properties
// *inside* these objects; it will never replace an object. Nothing that holds
// `Theme.color` — a binding, a cached reference in a Canvas paint handler, a
// property alias — is ever left pointing at a corpse.
//
// Each group is an *inline component* instantiated once, rather than an
// anonymous `QtObject { … }`, and that is a tooling decision with real teeth.
// A property declared `QtObject` tells qmllint and qmlls that the thing on the
// other side has QObject's members and nothing else — so every one of the six
// hundred token reads in the tree came back as `Member "textPrimary" not found
// on type "QObject"`. Six hundred warnings in the lint target and a squiggle
// under every colour in the editor is not a cost worth paying for two fewer
// lines here. Naming the type restores the check: a typo in a token name is a
// lint error again, which is exactly what it was when this was a JS object
// literal.
//
// ---- groups and tables -----------------------------------------------------
//
// Two shapes live here and the difference is what the keys are.
//
//   A *group* is keyed by token name: `color`, `metric`, `type`, `motion`,
//   `scale`, `star`, `surfaceAlpha`. Someone wrote each of those names down on
//   purpose, and a component reads them one at a time, so each is a property
//   with its own notification.
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
// ---- what has deliberately NOT changed -------------------------------------
//
// Not one token was renamed, restructured or deleted. `Theme.color.textPrimary`
// is still `Theme.color.textPrimary`. The semantic pass — `Theme.ink.primary`,
// dropping the handful of tokens that are duplicates of each other, adding the
// light values — is a separate commit with its own review, because holding the
// names still is the only thing that makes this one provable: the screenshots
// come out byte-identical or the refactor is wrong.
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

    // ---- surfaces ----------------------------------------------------------
    // The alpha ladder, as the leading pair of an #AARRGGBB literal. Nothing
    // reads these yet — the composed `color.surface*` values below are what the
    // tree uses — and they stay exported because they are the ladder the
    // design system quotes, not a leftover.
    component SurfaceAlphaTokens: QtObject {
        readonly property string recede: Tokens.surfaceAlpha.recede
        readonly property string base:   Tokens.surfaceAlpha.base
        readonly property string raised: Tokens.surfaceAlpha.raised
    }
    readonly property SurfaceAlphaTokens surfaceAlpha: SurfaceAlphaTokens { }

    // ---- colour ------------------------------------------------------------
    // Declared `string` rather than `color`, which is not laziness. A `color`
    // property round-trips through QColor, so `"transparent"` reads back as
    // `"#00000000"` and `"#ffffff"` as `"#ffffffff"` — and the gallery's palette
    // page prints these values as text beside each swatch. Assigning a string to
    // a `color` property at the call site is a conversion QML already does
    // everywhere, so nothing downstream can tell the difference; the palette
    // page can.
    component ColorTokens: QtObject {
        readonly property string pageStop0:          Tokens.color.pageStop0
        readonly property string pageStop1:          Tokens.color.pageStop1
        readonly property string pageStop2:          Tokens.color.pageStop2
        readonly property string pageStop3:          Tokens.color.pageStop3
        readonly property string pageStop4:          Tokens.color.pageStop4
        readonly property string pageBg:             Tokens.color.pageBg

        readonly property string surfaceRecede:      Tokens.color.surfaceRecede
        readonly property string surfaceBase:        Tokens.color.surfaceBase
        readonly property string surfaceRaised:      Tokens.color.surfaceRaised

        readonly property string cardBg:             Tokens.color.cardBg
        readonly property string cardBorder:         Tokens.color.cardBorder
        readonly property string panelBg:            Tokens.color.panelBg
        readonly property string dayCardBg:          Tokens.color.dayCardBg

        // Declared bare and bound further down, in its position in the table so
        // the gallery's palette page still lists it where theme.js puts it. See
        // `onAccentBinding` for why it cannot be written like its neighbours.
        property string onAccent

        readonly property string textPrimary:        Tokens.color.textPrimary
        readonly property string textMuted:          Tokens.color.textMuted
        readonly property string textDim:            Tokens.color.textDim

        readonly property string gridLine:           Tokens.color.gridLine
        readonly property string gridLineWeak:       Tokens.color.gridLineWeak

        readonly property string pastVeil:           Tokens.color.pastVeil
        readonly property string pastHatch:          Tokens.color.pastHatch
        readonly property string nowLine:            Tokens.color.nowLine
        readonly property string forecastDim:        Tokens.color.forecastDim
        readonly property string trackLine:          Tokens.color.trackLine

        readonly property string listRowAlt:         Tokens.color.listRowAlt
        readonly property string nowRowBg:           Tokens.color.nowRowBg

        readonly property string stripBg:            Tokens.color.stripBg
        readonly property string stripPast:          Tokens.color.stripPast
        readonly property string stripDivider:       Tokens.color.stripDivider
        readonly property string droplet:            Tokens.color.droplet

        readonly property string accent:             Tokens.color.accent
        readonly property string toggleTrack:        Tokens.color.toggleTrack
        readonly property string toggleKnob:         Tokens.color.toggleKnob

        readonly property string pillHover:          Tokens.color.pillHover
        readonly property string switchActive:       Tokens.color.switchActive
        readonly property string switchBorder:       Tokens.color.switchBorder
        readonly property string daySelectedBorder:  Tokens.color.daySelectedBorder

        readonly property string pagerBg:            Tokens.color.pagerBg
        readonly property string pagerBgHover:       Tokens.color.pagerBgHover
        readonly property string pagerGlyph:         Tokens.color.pagerGlyph

        readonly property string trendUp:            Tokens.color.trendUp
        readonly property string trendDown:          Tokens.color.trendDown
        readonly property string trendSteady:        Tokens.color.trendSteady

        readonly property string sunGlyphWarm:       Tokens.color.sunGlyphWarm
        readonly property string sunGlyphCool:       Tokens.color.sunGlyphCool
        readonly property string moonGlyph:          Tokens.color.moonGlyph
        readonly property string cloudTop:           Tokens.color.cloudTop
        readonly property string cloudBottom:        Tokens.color.cloudBottom
        readonly property string rainDrop:           Tokens.color.rainDrop
        readonly property string cloudTopOnLight:    Tokens.color.cloudTopOnLight
        readonly property string cloudBottomOnLight: Tokens.color.cloudBottomOnLight

        readonly property string badgeDayTop:        Tokens.color.badgeDayTop
        readonly property string badgeDayBottom:     Tokens.color.badgeDayBottom
        readonly property string badgeNightTop:      Tokens.color.badgeNightTop
        readonly property string badgeNightBottom:   Tokens.color.badgeNightBottom

        readonly property string navBg:              Tokens.color.navBg
        readonly property string navHairline:        Tokens.color.navHairline
        readonly property string navGlyph:           Tokens.color.navGlyph
        readonly property string navGlyphOn:         Tokens.color.navGlyphOn
        readonly property string navPill:            Tokens.color.navPill

        readonly property string menuBg:             Tokens.color.menuBg
        readonly property string menuBorder:         Tokens.color.menuBorder

        readonly property string statusGood:         Tokens.color.statusGood
        readonly property string statusCaution:      Tokens.color.statusCaution
        readonly property string statusPoor:         Tokens.color.statusPoor

        readonly property string placeholderInk:     Tokens.color.placeholderInk
        readonly property string placeholderStroke:  Tokens.color.placeholderStroke
    }
    readonly property ColorTokens color: ColorTokens { id: colorTokens }

    // ---- the one token that cannot be written like the others --------------
    //
    // `onAccent` and `accent` are both in the table above, and QML will not host
    // both on one object. Any binding whose name is `on` + a capital letter is
    // parsed as a signal handler first and as a property second, so `onAccent:`
    // is resolved against a member called `accent` — which exists — and the
    // value goes to the signal rather than to the property.
    //
    // How that fails is the reason this comment is long. With a literal it is at
    // least an error: `Cannot assign a value to a signal`. With an *expression*
    // — which is what every token here is, since they all read out of theme.js —
    // a script assigned to a signal is perfectly legal QML, so it compiles
    // clean, runs clean, and leaves `Theme.color.onAccent` as the empty string.
    // An empty string is a valid colour to QML: it paints black. The whole
    // symptom was a 9x9 patch of the trend badge's arrow going from #141d33 to
    // #000000 on four screenshots, with nothing on stderr.
    //
    // Binding the property by *name* takes the parser out of it: `"onAccent"` is
    // a string here, not an identifier the grammar gets an opinion about. The
    // property has to give up `readonly` to be a Binding's target, which is the
    // whole price. It is still a binding — when the theme gains a mode to
    // switch on, this re-evaluates with the other seventy.
    //
    // Renaming the token would also fix it, and that is W3's call to make with
    // the rest of the semantic pass. It is not a thing to do quietly inside a
    // refactor that claims the pixels did not move.
    readonly property Binding onAccentBinding: Binding {
        target: colorTokens
        property: "onAccent"
        value: Tokens.color.onAccent
    }

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

    component StarTokens: QtObject {
        readonly property string ink:  Tokens.star.ink
        readonly property string line: Tokens.star.line
    }
    readonly property StarTokens star: StarTokens { }

    // ---- measurements ------------------------------------------------------
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
        // singleton with a NOTIFY of its own. That is W3's problem, and the call
        // sites do not change when it is solved.
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
    // `easing` is new here and it is the one thing this file adds rather than
    // republishes. theme.js says outright that it cannot hold `Easing.OutCubic`
    // because a QML enum cannot live in a plain JS library, so the house rule —
    // **OutCubic unless there is a stated reason** — is written literally at
    // some sixty call sites and enforced by nothing. A singleton can hold it.
    //
    // The sixty call sites are deliberately NOT changed in this commit: a
    // rewrite of every animation in the tree does not belong in a refactor
    // whose whole claim is that the pixels did not move. It is available; the
    // sweep through the call sites is the next commit.
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
    // the chart reads `Theme.ramp[root.metric.ramp].fill` and metrics.js is
    // what decides the key. gallery.js reads this table too, and it reads it
    // straight out of theme.js — a `.pragma library` cannot import a QML
    // singleton, which is the other half of why theme.js is still the file the
    // values live in.
    readonly property var ramp: Tokens.ramp
}
