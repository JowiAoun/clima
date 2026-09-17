<!--
SPDX-FileCopyrightText: 2026 Jowi Aoun
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Google Play listing

The text and the pictures the Play Console asks for, kept here so that the
store page is reviewed like the README is. Everything under `screenshots/`
and `feature-graphic.png` is rendered by a script and gated in CI; the words
are edited by hand.

## App details

| Field | Value |
|---|---|
| App name | Climat |
| Package name | `io.github.JowiAoun.Climat` |
| Default language | English (Canada) |
| Category | Weather |
| Free or paid | Free |
| Contains ads | No |
| App icon | `packaging/icons/climat-512.png` |
| Feature graphic | `feature-graphic.png` (1024 by 500) |
| Phone screenshots | `screenshots/01-today.png` to `06-hourly-light.png` (1080 by 1920) |
| Privacy policy | the URL of `privacy-policy.md`, published on the docs site |
| Contact email | the address in the package's Maintainer field |
| Website | https://github.com/JowiAoun/climat |

## Short description

80 characters at most.

> Forecasts as charts you can read, and official weather warnings. No ads, no account.

## Full description

4000 characters at most. Plain text; Play ignores Markdown.

> Climat is a weather app with nothing in it but the weather.
>
> Two weeks of forecast as charts rather than a grid of numbers: temperature,
> feels like, rain and snow, wind, humidity, pressure, visibility, UV and air
> quality, hour by hour and day by day. Sunrise, sunset and the moon on their
> own cards. Severe weather warnings from Environment and Climate Change
> Canada and the United States National Weather Service, shown on the screen
> where they matter.
>
> No ads. No news feed. No account. No tracking. Climat asks two weather
> services for the forecast and nothing else, and it says which ones on its
> own settings screen.
>
> It works offline: the last forecast stays on screen until a new one
> arrives, so a service being down is never an empty page.
>
> One thing it does not do yet: check for warnings while it is closed. A
> warning reaches you when you open the app, and the settings screen says so.
>
> Climat is free software under the GNU GPL. The source is at
> github.com/JowiAoun/climat, and so is the list of what is not built yet.

## Content rating

The IARC questionnaire: no user-generated content, no violence, no sexual
content, no gambling, no controlled substances, no user interaction, no
location sharing with other users. Expected rating: Everyone.

## Target audience

18 and over. Not designed for children, and not a "family" app: that
programme has its own review and Climat gains nothing from it.

## Data safety

`data-safety.md` beside this file has the answers, one per question.

## Release notes

Written per release, in the voice of the AppStream release note in
`packaging/linux/climat.metainfo.xml.in`, which says the same things to a
Linux software centre. Copy it; do not paste the changelog.
