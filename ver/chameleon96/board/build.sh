#!/bin/bash
set -euo pipefail

main() {
    [[ $# == 1 ]] || { echo "Usage: $0 NEW_BUILD_DIRECTORY" >&2; exit 2; }
    : "${JTFRAME:?Source setprj.sh first}"
    local src dut out source_hash file
    src=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
    dut=$(cd "$src/../.." && pwd)
    out=$(realpath -m "$1")
    [[ ! -e $out ]] || { echo "Build directory must be new: $out" >&2; exit 2; }
    for file in quartus_sh quartus_cpf dtc; do command -v "$file" >/dev/null; done
    [[ -s $src/rtl/c96_jt51_top.v && -s $src/rtl/jt51_replay_lane.v ]]
    mkdir -p "$out"
    export C96_JT51_SRC="$src" C96_JT51_BUILD="$out"
    local -a files=("$dut"/hdl/*.v "$src"/rtl/*.v
        "$JTFRAME"/hdl/ram/jtframe_ram.v "$JTFRAME"/hdl/ram/jtframe_dual_ram.v)
    for file in "${files[@]}"; do [[ -s $file ]]; done
    printf '%s\n' "${files[@]}" | LC_ALL=C sort > "$out/hdl-files.txt"
    while IFS= read -r file; do sha256sum "$file"; done < "$out/hdl-files.txt" > "$out/hdl-inputs.sha256"
    for file in "$src/board/build.sh" "$src/board/build.tcl" "$src/board/c96.sdc" "$src/board/c96.dts"; do
        sha256sum "$file"
    done > "$out/build-inputs.sha256"
    cat "$out/hdl-inputs.sha256" "$out/build-inputs.sha256" | sed "s|$dut/|JT51/|g; s|$JTFRAME/|JTFRAME/|g" > "$out/source-signature.txt"
    source_hash=$(sha256sum "$out/source-signature.txt"); source_hash=${source_hash%% *}
    printf '\140define JT51_FPGA_SOURCE_HASH256 256\047h%s\n' "$source_hash" > "$out/build.vh"
    printf '%s\n' "$source_hash" > "$out/source-hash.txt"
    cd "$out"
    quartus_sh -t "$src/board/build.tcl" > project.log 2>&1
    quartus_sh --flow compile c96_jt51 > compile.log 2>&1
    sha256sum -c hdl-inputs.sha256 > source-recheck.log
    sha256sum -c build-inputs.sha256 >> source-recheck.log
    quartus_cpf -c output_files/c96_jt51.sof output_files/c96-jt51.rbf > cpf.log 2>&1
    dtc -@ -I dts -O dtb -o output_files/c96-jt51.dtbo "$src/board/c96.dts"
    sha256sum output_files/c96-jt51.rbf output_files/c96-jt51.dtbo > output-files.sha256
    cat output-files.sha256
}

main "$@"
