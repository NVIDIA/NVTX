#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -uo pipefail

failures=0
current_year="${NVTX_VALIDATE_SOURCE_YEAR:-$(date +%Y)}"

source_pathspecs=(
    '*.c'
    '*.cpp'
    '*.h'
    '*.hpp'
    '*.cu'
    '*.py'
    '*.rs'
    '*.cmake'
    '*.in'
    ':(glob)**/CMakeLists.txt'
    '*.md'
    '*.toml'
    '*.sh'
    '*.ps1'
    '*.bat'
    '*.rst'
    ':(exclude)docs/**'
)

mapfile -d '' source_files < <(git ls-files -z -- "${source_pathspecs[@]}")
mapfile -d '' lf_line_ending_files < <(
    git ls-files -z -- \
        "${source_pathspecs[@]}" \
        ':(exclude)*.bat'
)
mapfile -d '' bat_files < <(git ls-files -z -- '*.bat' ':(exclude)docs/**')
mapfile -d '' ascii_files < <(
    git ls-files -z -- \
        "${source_pathspecs[@]}" \
        ':(exclude)*.md' \
        ':(exclude)*.py' \
        ':(exclude)*.rs'
)
mapfile -d '' sh_files < <(git ls-files -z -- '*.sh' ':(exclude)docs/**')
mapfile -d '' ps1_files < <(git ls-files -z -- '*.ps1' ':(exclude)docs/**')
mapfile -d '' script_files < <(git ls-files -z -- '*.sh' '*.ps1' '*.bat' ':(exclude)docs/**')
mapfile -d '' c_comment_files < <(
    git ls-files -z -- \
        '*.c' \
        '*.h' \
        ':(exclude)docs/**' \
        ':(exclude)tests/**' \
        ':(exclude)tools/**'
)
mapfile -d '' spdx_files < <(
    git ls-files -z -- \
        "${source_pathspecs[@]}" \
        ':(exclude)*.md'
)
mapfile -d '' public_header_files < <(
    git ls-files -z -- \
        'c/include/nvtx3/*.h' \
        'c/include/nvtx3/*.hpp' \
        'c/include/nvtx3/**/*.h' \
        'c/include/nvtx3/**/*.hpp'
)

record_failure() {
    local name="$1"
    local details="${2:-}"

    printf '\n%s\n' "$name"
    printf '%s\n' '----------------------------------------'
    if [[ -n "$details" ]]; then
        printf '%s\n' "$details"
    fi
    ((failures += 1))
}

check_git_grep_absent() {
    local name="$1"
    local grep_mode="$2"
    local pattern="$3"
    shift 3
    local files=("$@")
    local output
    local status

    if ((${#files[@]} == 0)); then
        return
    fi

    output=$(git grep -nI "$grep_mode" "$pattern" -- "${files[@]}" 2>&1)
    status=$?
    if ((status == 0)); then
        record_failure "$name" "$output"
    elif ((status != 1)); then
        record_failure "$name command failed" "$output"
    fi
}

check_exactly_one_final_newline() {
    local output

    output=$(python3 - "$@" <<'PY'
import pathlib
import sys

for file_name in sys.argv[1:]:
    data = pathlib.Path(file_name).read_bytes()
    expected = b"\r\n" if file_name.endswith(".bat") else b"\n"
    doubled = expected + expected
    if not data.endswith(expected):
        print(f"{file_name}: missing final newline")
    elif data.endswith(doubled):
        print(f"{file_name}: more than one final newline")
PY
)

    if [[ -n "$output" ]]; then
        record_failure 'Files must end with exactly one newline' "$output"
    fi
}

has_script_extension() {
    local file_name="$1"

    case "$file_name" in
        *.bat | *.ps1 | *.sh)
            return 0
            ;;
    esac

    return 1
}

has_shebang() {
    local file_name="$1"

    [[ "$(LC_ALL=C sed -n '1s/^#!.*/shebang/p;q' "$file_name")" == shebang ]]
}

check_chmod() {
    local output=""
    local file_name
    local mode

    for file_name in "$@"; do
        mode="$(git ls-files -s -- "$file_name" | awk '{print $1}')"
        if has_script_extension "$file_name"; then
            if [[ "$mode" != 100755 ]]; then
                output+="${file_name}: executable bit is not set"$'\n'
            fi
        elif [[ "$mode" == 100755 ]] && ! has_shebang "$file_name"; then
            output+="${file_name}: executable bit is set"$'\n'
        fi
    done

    if [[ -n "$output" ]]; then
        record_failure 'Unexpected executable permissions' "${output%$'\n'}"
    fi
}

check_script_shebangs() {
    local output=""
    local file_name
    local first_line

    for file_name in "${sh_files[@]}"; do
        first_line="$(LC_ALL=C sed -n '1{s/\r$//;p;q}' "$file_name")"
        if [[ "$first_line" != '#!/usr/bin/env bash' && "$first_line" != '#!/usr/bin/env zsh' ]]; then
            output+="${file_name}: expected #!/usr/bin/env bash or #!/usr/bin/env zsh"$'\n'
        fi
    done

    for file_name in "${ps1_files[@]}"; do
        first_line="$(LC_ALL=C sed -n '1{s/\r$//;p;q}' "$file_name")"
        if [[ "$first_line" != '#!/usr/bin/env pwsh' ]]; then
            output+="${file_name}: expected #!/usr/bin/env pwsh"$'\n'
        fi
    done

    if [[ -n "$output" ]]; then
        record_failure 'Script files must have valid shebangs' "${output%$'\n'}"
    fi
}

check_ascii() {
    local output

    output=$(python3 - "$@" <<'PY'
import pathlib
import sys

for file_name in sys.argv[1:]:
    data = pathlib.Path(file_name).read_bytes()
    allow_cr = file_name.endswith(".bat")
    found = False
    for line_number, line in enumerate(data.splitlines(keepends=True), 1):
        content = line.rstrip(b"\r\n")
        for byte in content:
            if 0x20 <= byte <= 0x7E:
                continue
            if allow_cr and byte == 0x0D:
                continue
            rendered = content.decode("ascii", errors="backslashreplace")
            print(f"{file_name}:{line_number}: non-ASCII byte 0x{byte:02X}")
            print(rendered)
            found = True
            break
        if found:
            break
PY
)

    if [[ -n "$output" ]]; then
        record_failure 'Only 7-bit ASCII characters are allowed' "$output"
    fi
}

check_no_byte_order_marks() {
    local output

    output=$(python3 - "$@" <<'PY'
import pathlib
import sys

boms = (
    (b"\xef\xbb\xbf", "UTF-8"),
)

for file_name in sys.argv[1:]:
    data = pathlib.Path(file_name).read_bytes()
    for marker, name in boms:
        if data.startswith(marker):
            print(f"{file_name}: starts with {name} byte order mark")
            break
PY
)

    if [[ -n "$output" ]]; then
        record_failure 'Byte order marks are not allowed' "$output"
    fi
}

check_lf_line_endings() {
    local output

    output=$(python3 - "$@" <<'PY'
import pathlib
import sys

for file_name in sys.argv[1:]:
    data = pathlib.Path(file_name).read_bytes()
    if b"\r" in data:
        print(f"{file_name}: contains CR or CRLF line endings")
PY
)

    if [[ -n "$output" ]]; then
        record_failure 'LF line endings are required' "$output"
    fi
}

check_bat_crlf_line_endings() {
    local output

    output=$(python3 - "$@" <<'PY'
import pathlib
import sys

for file_name in sys.argv[1:]:
    data = pathlib.Path(file_name).read_bytes()
    index = 0
    valid = True
    while index < len(data):
        byte = data[index]
        if byte == 0x0D:
            if index + 1 >= len(data) or data[index + 1] != 0x0A:
                valid = False
                break
            index += 2
            continue
        if byte == 0x0A:
            valid = False
            break
        index += 1
    if not valid:
        print(f"{file_name}: .bat files must use CRLF line endings")
PY
)

    if [[ -n "$output" ]]; then
        record_failure 'Batch files must use CRLF line endings' "$output"
    fi
}

check_no_cxx_comments_in_c_files() {
    local output

    output=$(python3 - "$@" <<'PY'
import pathlib
import sys

for file_name in sys.argv[1:]:
    text = pathlib.Path(file_name).read_text(encoding="utf-8", errors="replace")
    in_block_comment = False
    for line_number, line in enumerate(text.splitlines(), 1):
        index = 0
        in_string = False
        string_quote = ""
        escaped = False
        while index < len(line):
            pair = line[index:index + 2]
            char = line[index]

            if in_block_comment:
                if pair == "*/":
                    in_block_comment = False
                    index += 2
                else:
                    index += 1
                continue

            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == string_quote:
                    in_string = False
                index += 1
                continue

            if pair == "/*":
                in_block_comment = True
                index += 2
                continue
            if pair == "//":
                print(f"{file_name}:{line_number}:{line}")
                break
            if char in ('"', "'"):
                in_string = True
                string_quote = char
            index += 1
PY
)

    if [[ -n "$output" ]]; then
        record_failure 'C headers and C sources must not use // comments' "$output"
    fi
}

check_spdx() {
    local output=""
    local file_name
    local copyright_pattern
    local license_pattern
    local first_line
    local second_line
    local rust_copyright_pattern

    copyright_pattern='SPDX-FileCopyrightText: Copyright \(c\) ([0-9]{4}-)?'"${current_year}"' NVIDIA CORPORATION & AFFILIATES\. All rights reserved\.'
    license_pattern='SPDX-License-Identifier: Apache-2\.0 WITH LLVM-exception'
    rust_copyright_pattern='^// SPDX-FileCopyrightText: Copyright [(]c[)] ([0-9]{4}-)?'"${current_year}"' NVIDIA CORPORATION & AFFILIATES[.] All rights reserved[.]$'

    for file_name in "$@"; do
        if ! grep -Eq "$copyright_pattern" "$file_name"; then
            output+="${file_name}: missing current-year NVIDIA SPDX copyright line"$'\n'
        fi
        if ! grep -Eq "$license_pattern" "$file_name"; then
            output+="${file_name}: missing Apache-2.0 WITH LLVM-exception SPDX license line"$'\n'
        fi
        if [[ "$file_name" == *.rs ]]; then
            first_line="$(LC_ALL=C sed -n '1p;q' "$file_name")"
            second_line="$(LC_ALL=C sed -n '2{p;q}' "$file_name")"
            if ! [[ "$first_line" =~ $rust_copyright_pattern ]]; then
                output+="${file_name}: Rust SPDX copyright line must be first, before any #![...] attributes"$'\n'
            fi
            if [[ "$second_line" != '// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception' ]]; then
                output+="${file_name}: Rust SPDX license line must be second, before any #![...] attributes"$'\n'
            fi
        fi
    done

    if [[ -n "$output" ]]; then
        record_failure 'Missing or incorrect SPDX metadata' "${output%$'\n'}"
    fi
}

check_public_headers_are_system_headers() {
    local output=""
    local file_name

    for file_name in "$@"; do
        if ! grep -q 'NVTX_AS_SYSTEM_HEADER' "$file_name"; then
            output+="${file_name}: missing NVTX_AS_SYSTEM_HEADER"$'\n'
        fi
        if ! grep -q 'system_header' "$file_name"; then
            output+="${file_name}: missing system_header pragma"$'\n'
        fi
    done

    if [[ -n "$output" ]]; then
        record_failure 'Public C/C++ headers must support system_header pragmas' "${output%$'\n'}"
    fi
}

check_yamllint() {
    local output
    local status

    output=$(yamllint --strict .gitlab-ci.yml .github/*.yml .github/workflows/*.yml 2>&1)
    status=$?
    if ((status != 0)); then
        record_failure 'yamllint --strict failed' "$output"
    fi
}

check_copyright_start_years() {
    local output

    output=$(python3 - "$@" <<'PY'
import pathlib
import re
import subprocess
import sys

pattern = re.compile(
    r"SPDX-FileCopyrightText: Copyright \(c\) "
    r"(?P<first>[0-9]{4})(?:-[0-9]{4})? NVIDIA CORPORATION & AFFILIATES\\."
)

for file_name in sys.argv[1:]:
    text = pathlib.Path(file_name).read_text(encoding="utf-8", errors="replace")
    match = pattern.search(text)
    if not match:
        continue

    first_year = int(match.group("first"))
    try:
        history = subprocess.check_output(
            [
                "git",
                "log",
                "--diff-filter=A",
                "--format=%ad",
                "--date=format:%Y",
                "--",
                file_name,
            ],
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        continue

    years = [int(line) for line in history.splitlines() if line.strip().isdigit()]
    if not years:
        continue

    earliest_year = min(years)
    if first_year > earliest_year:
        print(
            f"{file_name}: first copyright year {first_year} is later "
            f"than earliest git history year {earliest_year}"
        )
PY
)

    if [[ -n "$output" ]]; then
        record_failure 'Copyright start years must cover git history' "$output"
    fi
}

check_git_grep_absent 'Trailing whitespace is not allowed' -P '[ \t]+$' "${source_files[@]}"
check_git_grep_absent 'Tabs are not allowed' -P '\t' "${source_files[@]}"
check_ascii "${ascii_files[@]}"
check_no_byte_order_marks "${source_files[@]}"
check_lf_line_endings "${lf_line_ending_files[@]}"
check_bat_crlf_line_endings "${bat_files[@]}"
check_exactly_one_final_newline "${source_files[@]}"
check_chmod "${source_files[@]}"
check_script_shebangs
check_no_cxx_comments_in_c_files "${c_comment_files[@]}"
check_public_headers_are_system_headers "${public_header_files[@]}"
typo_pattern="$(printf '%s' '(' 'n' 'vt' '[^xo]' '|' 'N' 'VT' '[^XO]' '|' 'nv[x]t' '|' 'NV[X]T' ')')"
check_git_grep_absent 'Possible NVTX typo found' -E "$typo_pattern" "${source_files[@]}"
check_spdx "${spdx_files[@]}"
check_copyright_start_years "${spdx_files[@]}"
check_yamllint

if ((failures != 0)); then
    printf '\n%d validation check(s) failed.\n' "$failures"
    exit 1
fi

printf 'All source validation checks passed.\n'
