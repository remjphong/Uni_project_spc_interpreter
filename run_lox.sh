#!/usr/bin/env bash
set -u

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 <input.lox> [report.txt]" >&2
    exit 2
fi

input=$1

if [[ $# -eq 2 ]]; then
    report=$2
else
    if [[ "$input" == *.lox ]]; then
        report="${input%.lox}.txt"
    else
        report="${input}.txt"
    fi
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 2

if ! mkdir -p "$(dirname -- "$report")"; then
    echo "[ERROR] Cannot create report directory." >&2
    exit 2
fi

build_dir="$script_dir/build"

if ! mkdir -p "$build_dir"; then
    echo "[ERROR] Cannot create build directory." >&2
    exit 2
fi

compiler=${CC:-gcc}
binary="$build_dir/lox_runner"
log_file=$(mktemp) || {
    echo "[ERROR] Cannot create compiler log." >&2
    exit 2
}

cleanup() {
    rm -f "$log_file"
}
trap cleanup EXIT

echo "[BUILD] Compiling Lox interpreter..."

if ! "$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    "$script_dir/lexer.c" \
    "$script_dir/parser.c" \
    "$script_dir/environment.c" \
    "$script_dir/resolver.c" \
    "$script_dir/interpreter.c" \
    "$script_dir/main.c" \
    -lm -o "$binary" 2>"$log_file"; then

    {
        echo "LOX INTERPRETER - TEST REPORT"
        echo
        echo "Input: $input"
        echo "Build status: FAILED"
        echo
        echo "COMPILER DIAGNOSTICS"
        echo "========================================"

        if [[ -s "$log_file" ]]; then
            cat "$log_file"
        else
            echo "Compiler failed without diagnostic output."
        fi

        echo
        echo "CONCLUSION: Build failed; the program was not executed."
    } > "$report"

    echo "[BUILD] FAILED"
    echo "[ERROR] Compilation diagnostics:"
    if [[ -s "$log_file" ]]; then
        cat "$log_file" >&2
    else
        echo "Compiler produced no diagnostic output." >&2
    fi
    echo "[REPORT] $report"
    exit 1
fi

echo "[BUILD] SUCCESS"

if [[ ! -f "$input" ]]; then
    {
        echo "LOX INTERPRETER - TEST REPORT"
        echo
        echo "Input: $input"
        echo "Build status: SUCCESS"
        echo "Execution status: FAILED"
        echo
        echo "ERROR: Input file does not exist."
    } > "$report"

    echo "[ERROR] Input file not found: $input" >&2
    echo "[REPORT] $report"
    exit 1
fi

if [[ "$input" != *.lox ]]; then
    {
        echo "LOX INTERPRETER - TEST REPORT"
        echo
        echo "Input: $input"
        echo "Build status: SUCCESS"
        echo "Execution status: FAILED"
        echo
        echo "ERROR: Input file must have the .lox extension."
    } > "$report"

    echo "[ERROR] Input must have a .lox extension." >&2
    echo "[REPORT] $report"
    exit 1
fi

LOX_C_BUILD_STATUS=SUCCESS "$binary" "$input" "$report"
status=$?

exit "$status"
