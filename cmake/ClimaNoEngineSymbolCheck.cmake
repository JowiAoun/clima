# SPDX-FileCopyrightText: 2026 Jowi Aoun
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Reads the symbol table of a linked binary and fails if the engine's fetching
# half is in it. Run by ctest, not by the build:
#
#   cmake -DCLIMA_BINARY=<path> -DCLIMA_NM=<nm> -P ClimaNoEngineSymbolCheck.cmake
#
# ---- why a symbol check and not `ldd` ----------------------------------------
#
# The plan for this guard was `ldd clima-widget | grep -qv libclima_providers`,
# and that check cannot work: libclima is a STATIC archive. There is no
# `libclima_providers.so` to find, and DT_NEEDED says nothing about which of an
# archive's members were pulled in — an unused provider adds no entry to it and
# a used one adds no entry either.
#
# What a static link *does* leave is a symbol. `clima::HttpClient::get` either
# ended up in the binary or it did not, and that is the question worth asking:
# not "did we link a library" but "can this process reach the network".
#
# ---- what it looks for -------------------------------------------------------
#
# The fetching half of libclima, by mangled-name substring: the HTTP client, the
# cache, the provider registry and the provider classes themselves. Not the
# domain types — `clima::Forecast` and the WMO tables are pure values, the
# widget host compiles two of those files on purpose, and banning them would be
# banning the thing that keeps the tables single-sourced.
#
# ---- a check that finds nothing must fail ------------------------------------
#
# A stripped binary has no symbol table, and a grep over an empty table passes
# every test ever written. So this asserts a POSITIVE CONTROL first: a symbol
# that is definitely in the binary because the widget host calls it. If that is
# missing, the table is unreadable and the script says so rather than reporting
# a clean bill of health it has no evidence for.

if(NOT CLIMA_BINARY)
    message(FATAL_ERROR "ClimaNoEngineSymbolCheck: pass -DCLIMA_BINARY=<path>")
endif()

if(NOT EXISTS "${CLIMA_BINARY}")
    message(FATAL_ERROR "ClimaNoEngineSymbolCheck: no such file: ${CLIMA_BINARY}")
endif()

if(NOT CLIMA_NM OR NOT EXISTS "${CLIMA_NM}")
    message(FATAL_ERROR "ClimaNoEngineSymbolCheck: CLIMA_NM is not a usable program: ${CLIMA_NM}")
endif()

execute_process(
    COMMAND "${CLIMA_NM}" --defined-only "${CLIMA_BINARY}"
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE  symbol_errors
    RESULT_VARIABLE symbol_status
)

if(NOT symbol_status EQUAL 0)
    message(FATAL_ERROR "ClimaNoEngineSymbolCheck: nm failed: ${symbol_errors}")
endif()

# ---- the positive control ----------------------------------------------------
#
# `clima::scales::uvBand` is compiled into the widget host by
# widgets/CMakeLists.txt and called by the UV dial. If it is not in the table
# then the table is not telling us anything.
if(NOT symbols MATCHES "clima6scales")
    message(FATAL_ERROR
        "\n"
        "  ClimaNoEngineSymbolCheck could not read a symbol table from\n"
        "  ${CLIMA_BINARY}.\n"
        "\n"
        "  It expected to find clima::scales, which the UV and air-quality tiles\n"
        "  call. Not finding it means the binary is stripped, or nm is reading a\n"
        "  different file than the one that was built.\n"
        "\n"
        "  This is reported as a failure on purpose: a symbol search over an empty\n"
        "  table passes every check that could ever be written against it.\n")
endif()

# ---- the things that must not be there ---------------------------------------
#
# Matched against mangled names, so these are the Itanium-ABI spellings:
# `5clima10HttpClient` for clima::HttpClient, and so on. Substrings rather than
# anchored patterns, because the surrounding mangling depends on the member.
set(forbidden
    "clima10HttpClient"
    "clima10CacheStore"
    "clima16ProviderRegistry"
    "clima24OpenMeteoForecastProvider"
    "clima20MetNoForecastProvider"
    "clima26OpenMeteoAirQualityProvider"
    "clima17EcccAlertProvider"
    "clima16NwsAlertProvider"
    "clima13GeoNamesIndex"

    # And the two that catch linking libclima *without* calling into it.
    #
    # This was found by injecting the defect rather than reasoned out: adding
    # libclima to the link line and merely naming a type left no provider symbol
    # at all — the compiler folded the reference away and the archive
    # contributed nothing. Qt's resource initialisers are different. They are
    # force-linked whether or not anything reads them, so `libclima` on a link
    # line always drags in the bundled GeoNames index and the recorded
    # fixtures, and always leaves these two behind.
    #
    # That is worth failing on for its own sake and not only as a proxy: those
    # are megabytes of data compiled into a process whose entire payload is
    # four hundred pixels of tile.
    "qInitResources_clima_geonames"
    "qInitResources_clima_fixtures"
)

set(offenders "")
foreach(banned IN LISTS forbidden)
    if(symbols MATCHES "${banned}")
        list(APPEND offenders "${banned}")
    endif()
endforeach()

if(offenders)
    string(REPLACE ";" "\n    " printable "${offenders}")
    message(FATAL_ERROR
        "\n"
        "  ${CLIMA_BINARY} carries the engine's fetching half, and it must not.\n"
        "\n"
        "  Found (mangled-name fragments):\n    ${printable}\n"
        "\n"
        "  A widget process draws what clima-daemon fetched. Six tiles must not be\n"
        "  six things opening sockets against a non-commercial free tier (R5) and\n"
        "  writing one SQLite file that has a single writer.\n"
        "\n"
        "  The usual cause is a new target_link_libraries(clima-widget … libclima)\n"
        "  line. If a tile needs something out of libclima, compile that FILE into\n"
        "  the widget module the way widgets/CMakeLists.txt already does for\n"
        "  domain/weathercode.cpp and domain/scales.cpp — and only if it is a pure\n"
        "  table with no I/O in it.\n")
endif()

message(STATUS "ClimaNoEngineSymbolCheck: ${CLIMA_BINARY} carries no provider, cache or HTTP symbol")
