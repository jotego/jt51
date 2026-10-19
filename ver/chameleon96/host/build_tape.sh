#!/bin/bash
main() {
    local source_dir output source_sha schedule_sha
    source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || return
    output=${1:?usage: build_tape.sh OUTPUT}
    source_sha=$(sha256sum "$source_dir/compile_tape.cpp") || return
    schedule_sha=$(sha256sum "$source_dir/cpu_schedule.hpp") || return
    "${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror \
        "-DTAPE_SOURCE_SHA=\"${source_sha%% *}\"" \
        "-DSCHEDULE_SOURCE_SHA=\"${schedule_sha%% *}\"" \
        "$source_dir/compile_tape.cpp" -lcrypto -o "$output"
}
main "$@"
