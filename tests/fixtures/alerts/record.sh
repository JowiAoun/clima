#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# How the fixtures beside this file were captured. Provenance you can run
# rather than provenance you have to believe.
#
# NOT part of the build and NOT run by CI. Nothing in tests/ may reach a
# network (docs/04-architecture.md §4.11); this is a developer action taken
# once, whose *output* is what the tests use.
#
#   ./record.sh            write into this directory
#   ./record.sh /tmp/new   write somewhere else, to diff against what is here
#
# ---- these are harder to re-record than the forecast fixtures ---------------
#
# A forecast fixture can be captured for any coordinate at any time. An alert
# fixture can only be captured where an alert is actually in force, which means
# the coordinates below were chosen on 2026-08-05 by asking each service what
# it had and picking the interesting answers. Running this script tomorrow at
# the same points will very likely record empty collections.
#
# So: read README.md first. If you re-record, expect to re-choose the points,
# and expect every literal in tests/tst_alerts*.cpp to need re-checking by hand.
set -euo pipefail

out="${1:-$(dirname "$0")}"
mkdir -p "$out/eccc" "$out/nws"

# The same shape HttpClient sends. api.weather.gov answers 403 to an empty
# User-Agent — see libclima/net/httpclient.h — so this is not decoration.
ua='Clima/0.1.0 (+https://github.com/JowiAoun/clima; recording test fixtures)'

eccc='https://api.weather.gc.ca/collections/weather-alerts/items'
nws='https://api.weather.gov/alerts/active'

grab() {
    local name="$1" url="$2"
    # --fail is deliberately absent for the out-of-bounds case, which is
    # recorded *because* it is a 400. Status is printed instead.
    local code
    code="$(curl --silent --show-error --user-agent "$ua" \
        --write-out '%{http_code}' --output "$out/$name" "$url")"
    printf '  %3s  %7d B  %s\n' "$code" "$(stat -c%s "$out/$name")" "$name"
}

echo "Environment and Climate Change Canada — GeoMet-Weather"

# One warning at a point, in the commonest shape there is: yellow, "continued".
grab eccc/annapolis-heat.json \
    "$eccc?f=json&bbox=-65.2007,44.6487,-65.2007,44.6487"

# The other end of the risk scale that was in force that day: orange, and the
# only feature in the whole 58-alert national set with impact "High". Recorded
# because a severity mapping tested against one colour is not tested.
grab eccc/fraser-valley-air-quality.json \
    "$eccc?f=json&bbox=-121.9686,49.2552,-121.9686,49.2552"

# A point with nothing in force. An empty FeatureCollection, HTTP 200 — which
# is the answer the parser must not confuse with a failure.
grab eccc/toronto-clear.json \
    "$eccc?f=json&bbox=-79.3832,43.6532,-79.3832,43.6532"

echo
echo "National Weather Service — api.weather.gov"

# Four alerts at one coordinate, which is what the banner's "+3 more" is for.
grab nws/seattle-four.json "$nws?point=47.6062,-122.3321"

# severity Severe, the tier above everything else recorded here.
grab nws/phoenix-extreme-heat.json "$nws?point=33.4484,-112.0740"

# THE expiry fixture. expires 2026-08-06T05:00-07:00, ends 2026-08-06T23:00-07:00
# — eighteen hours apart, with the hazard outlasting the message.
grab nws/siskiyou-heat-advisory.json "$nws?point=41.5,-122.5"

# severity Unknown, twice. A real CAP value, and the one a parser is most
# likely to map to something confident.
grab nws/denver-air-quality.json "$nws?point=39.7392,-104.9903"

# Nothing in force.
grab nws/minneapolis-clear.json "$nws?point=44.9778,-93.2650"

# HTTP 400 "out of bounds" — the answer to a coordinate outside the United
# States. Not an empty list. Recorded so the routing test has the real body.
grab nws/out-of-bounds.json "$nws?point=44.7,-65.3"

echo
echo "Recorded into $out. Now re-check every literal in tests/tst_alerts*.cpp."
