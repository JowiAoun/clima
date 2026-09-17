# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The OpenSSL libraries the Android package carries, and the check that it does.
#
# Qt for Android ships its OpenSSL TLS backend and not OpenSSL itself: Qt
# leaves the two libraries out of its packages for export-control reasons and
# expects the application to bundle them. An APK without them installs, starts,
# and cannot open one HTTPS connection - and every weather service this app
# reads is HTTPS. That is not a degraded build, it is an app that shows its
# cache forever with no error anywhere, which is why the check below is fatal
# rather than a warning.
#
# scripts/android-openssl.sh builds them from a pinned source tarball into
# build/android-openssl/<abi>/. They travel into the package through
# QT_ANDROID_EXTRA_LIBS, which is the property androiddeployqt reads to put
# extra native libraries beside Qt's own. Nothing links them: Qt's backend
# opens libssl_3.so and libcrypto_3.so by name at run time, which is what the
# `_3` suffix and the patched sonames in the script are for.

set(CLIMAT_ANDROID_OPENSSL_DIR "${CMAKE_SOURCE_DIR}/build/android-openssl" CACHE PATH
    "Where scripts/android-openssl.sh put libssl_3.so and libcrypto_3.so, one directory per ABI")

option(CLIMAT_ANDROID_BUNDLE_OPENSSL
    "Carry OpenSSL in the Android package. OFF builds a package that cannot reach any weather service; it exists for build experiments only" ON)

function(climat_android_bundle_tls target)
    if(NOT CLIMAT_ANDROID_BUNDLE_OPENSSL)
        message(WARNING
            "climat: CLIMAT_ANDROID_BUNDLE_OPENSSL is OFF. This package carries no TLS "
            "library and will not reach a single weather service. Do not ship it.")
        return()
    endif()

    set(libdir "${CLIMAT_ANDROID_OPENSSL_DIR}/${CMAKE_ANDROID_ARCH_ABI}")
    set(libs "${libdir}/libcrypto_3.so" "${libdir}/libssl_3.so")

    foreach(lib IN LISTS libs)
        if(NOT EXISTS "${lib}")
            message(FATAL_ERROR
                "climat: ${lib} is missing, so this Android package would carry no TLS "
                "library and could not reach any weather service.\n"
                "Build it first:  nix develop -c scripts/android-openssl.sh ${CMAKE_ANDROID_ARCH_ABI}\n"
                "or point CLIMAT_ANDROID_OPENSSL_DIR at a directory that has one "
                "<abi>/ subdirectory per ABI in QT_ANDROID_ABIS.")
        endif()
    endforeach()

    set_property(TARGET ${target} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS ${libs})
    message(STATUS "climat: Android package carries OpenSSL from ${libdir}")
endfunction()
