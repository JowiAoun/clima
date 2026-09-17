#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the OpenSSL libraries the Android package has to carry.
#
#   scripts/android-openssl.sh              arm64-v8a, the ABI the release ships
#   scripts/android-openssl.sh x86_64       another ABI, for an emulator
#
# Output: build/android-openssl/<abi>/libcrypto_3.so and libssl_3.so, plus the
# OpenSSL licence beside them. cmake/ClimatAndroidTls.cmake looks there.
#
# Why this exists at all: Qt for Android ships its OpenSSL TLS backend but not
# OpenSSL itself, so an APK built without these two files can open no HTTPS
# connection. Every weather service this app talks to is HTTPS. Without this
# step the app installs, starts, and shows its cache forever.
#
# Built from a pinned source tarball with a pinned checksum, not downloaded as
# a prebuilt binary, for the same reason the flake pins Qt: a release has to
# be reproducible from something we can read. 3.5 is the current long-term
# support series. Bump `version` and `sha256` together when it moves; the
# checksum is the one OpenSSL publishes beside the tarball.
#
# The `_3` suffix and the patched sonames are what Qt looks for: Android loads
# the system's own libssl.so and libcrypto.so for a library without a suffix,
# and those are whatever version the phone shipped with.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

version="3.5.8"
sha256="a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2"

# 28 is QT_ANDROID_MIN_SDK_VERSION in app/CMakeLists.txt. The two have to agree:
# a library built for a newer API than the app's floor links against symbols
# an older phone does not have.
api=28

sdk="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
ndk="${ANDROID_NDK_ROOT:-$sdk/ndk/27.2.12479018}"
out="$root/build/android-openssl"
jobs="${CLIMAT_JOBS:-4}"

if [ ! -d "$ndk/toolchains/llvm/prebuilt" ]; then
  echo "android-openssl: no NDK at $ndk - set ANDROID_NDK_ROOT, or run \`scripts/android.sh deps\`" >&2
  exit 1
fi
for tool in perl patchelf; do
  if ! command -v "$tool" > /dev/null 2>&1; then
    echo "android-openssl: $tool not found - run this inside \`nix develop\`" >&2
    exit 1
  fi
done

abis=("$@")
[ "${#abis[@]}" -eq 0 ] && abis=(arm64-v8a)

mkdir -p "$out"
tarball="$out/openssl-$version.tar.gz"
if [ ! -f "$tarball" ]; then
  echo "android-openssl: downloading openssl-$version.tar.gz"
  curl -sS -L -o "$tarball.part" \
    "https://github.com/openssl/openssl/releases/download/openssl-$version/openssl-$version.tar.gz"
  mv "$tarball.part" "$tarball"
fi
echo "$sha256  $tarball" | sha256sum -c - > /dev/null || {
  echo "android-openssl: $tarball does not match the pinned checksum" >&2
  exit 1
}

src="$out/openssl-$version"
if [ ! -f "$src/Configure" ]; then
  tar -xzf "$tarball" -C "$out"
fi
cp "$src/LICENSE.txt" "$out/LICENSE-OpenSSL.txt"

host="$(uname -s | tr '[:upper:]' '[:lower:]')-x86_64"
export ANDROID_NDK_ROOT="$ndk"
export PATH="$ndk/toolchains/llvm/prebuilt/$host/bin:$PATH"

for abi in "${abis[@]}"; do
  case "$abi" in
    arm64-v8a)   target=android-arm64 ;;
    armeabi-v7a) target=android-arm ;;
    x86_64)      target=android-x86_64 ;;
    x86)         target=android-x86 ;;
    *) echo "android-openssl: unknown ABI '$abi'" >&2; exit 2 ;;
  esac

  build="$out/build-$abi"
  rm -rf "$build"
  mkdir -p "$build" "$out/$abi"

  echo "android-openssl: configuring $abi ($target, API $api)"
  # Out-of-tree, so two ABIs never share a build directory. No apps, tests or
  # docs: two libraries are the whole deliverable. max-page-size=16384 is the
  # 16 KB page alignment Google Play requires of every native library since
  # November 2025; the NDK's default is 4 KB for anything not built by Gradle.
  (
    cd "$build"
    perl "$src/Configure" "$target" shared no-tests no-apps no-docs \
      "-D__ANDROID_API__=$api" "-Wl,-z,max-page-size=16384" > configure.log 2>&1 \
      || { cat configure.log >&2; exit 1; }
    # SHLIB_VERSION_NUMBER empty gives libssl.so rather than libssl.so.3.
    # Android's loader does not load a versioned name at all.
    make -j "$jobs" SHLIB_VERSION_NUMBER= build_libs > build.log 2>&1 \
      || { tail -50 build.log >&2; exit 1; }
  )

  cp "$build/libcrypto.so" "$out/$abi/libcrypto_3.so"
  cp "$build/libssl.so"    "$out/$abi/libssl_3.so"
  patchelf --set-soname libcrypto_3.so "$out/$abi/libcrypto_3.so"
  patchelf --set-soname libssl_3.so    "$out/$abi/libssl_3.so"
  patchelf --replace-needed libcrypto.so libcrypto_3.so "$out/$abi/libssl_3.so"
  rm -rf "$build"

  echo "android-openssl: $abi -> $out/$abi/{libcrypto_3.so,libssl_3.so}"
done
