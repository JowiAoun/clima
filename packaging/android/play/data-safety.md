<!--
SPDX-FileCopyrightText: 2026 Jowi Aoun
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# Data safety form

What Climat does with data, in the shape the Play Console asks for it. Every
answer here is checkable against the source, and `docs/02-data-sources.md`
names each service the app talks to.

## Does your app collect or share any of the required user data types?

**No.**

Nothing leaves the phone except a request to a weather service, and that
request carries a place's coordinates and nothing about the person. There is
no account, no analytics library, no crash reporter, no advertising SDK, and
no server of ours in the middle. The app's User-Agent names the app and a
contact address, as MET Norway's terms require, and nothing else.

## Location

The app asks for approximate location (`ACCESS_COARSE_LOCATION`) only when the
reader taps "Use my location". The fix is turned into a city name on the phone
by a bundled place index, and the city's coordinates are what the forecast
request carries. The fix is not stored, not sent anywhere else, and not shared.
Under the form's definitions this is **not collection**: it is processed on
the device and used only for the request the reader asked for.

Precise location is not requested. Qt Positioning declares it and the manifest
removes it; `packaging/android/AndroidManifest.xml` says how.

## Network

`INTERNET` for the forecast, the air quality reading and the warnings.
`ACCESS_NETWORK_STATE` is declared by Qt Network and lets the app notice it is
offline rather than wait for a timeout.

## Data stored on the device

Preferences (units, clock format, theme) and a cache of the last forecast, in
the app's private storage. Nothing is written to shared storage. Uninstalling
the app removes all of it.

## Encryption in transit

Yes. Every service is HTTPS, and the package carries its own OpenSSL for the
purpose - see `scripts/android-openssl.sh`.

## Can users request that data be deleted?

There is nothing to delete on our side, because nothing is collected.
Uninstalling removes the on-device cache and preferences.

## Independent security review

No.

## Answers, one per form section

| Section | Answer |
|---|---|
| Data collection and security: collects or shares | No |
| Data encrypted in transit | Yes |
| Users can request deletion | Not applicable, nothing is collected |
| Location | Not collected (processed on device, on request) |
| Personal info, financial info, health, messages, photos, audio, files, calendar, contacts, activity, browsing, identifiers, installed apps, other | Not collected |
| Ads | None |
