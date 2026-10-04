#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py
    # 字库清单同理：新增界面文案后忘了重跑字库生成器，charset.txt 就会与源文件
    # 脱节。这一步要 fontTools 才能逐个码点核对母字体覆盖，而 CI 镜像不装
    # fontTools —— 缺依赖时明确跳过并说明，不假装通过。
    if python3 -c "import fontTools" >/dev/null 2>&1; then
        PYTHONDONTWRITEBYTECODE=1 python3 tools/astock-quote/gen_font.py --check
    else
        echo "skip: tools/astock-quote/gen_font.py --check（当前 python3 无 fontTools；字库清单同步未校验）" >&2
    fi

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/sq-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_demo_navigation.c main/demo_navigation.c \
        -o "${test_dir}/test_demo_navigation"
    "${test_dir}/test_demo_navigation"

    # 本应用的纯逻辑层：代码推断、行情解析（东财 JSON）、交易时段、自选
    # 名单、音量与折行。它们刻意不依赖 ESP-IDF/LVGL，宿主机直接编译。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_sq_symbol.c main/sq_symbol.c \
        -o "${test_dir}/test_sq_symbol"
    "${test_dir}/test_sq_symbol"

    # 行情解析：真实报文形状、停牌占位（"-" 必须无效而不是 0）、坏 JSON、
    # 快照序列化回读（cJSON 用 tests/cjson 的单文件同源版）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain -Itests/cjson \
        tests/test_sq_quote.c main/sq_quote.c tests/cjson/cJSON.c \
        -o "${test_dir}/test_sq_quote"
    "${test_dir}/test_sq_quote"

    # 交易时段边界（集合竞价/午休/收盘后/周末，北京时间）。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_sq_market.c main/sq_market.c \
        -o "${test_dir}/test_sq_market"
    "${test_dir}/test_sq_market"

    # 自选名单：预设、增删去重（同码两域并存）、上限 8、序列化回读。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_sq_watch.c main/sq_watch.c main/sq_symbol.c \
        -o "${test_dir}/test_sq_watch"
    "${test_dir}/test_sq_watch"

    # 音量档位与折行。
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_sq_volume.c main/sq_volume.c \
        -o "${test_dir}/test_sq_volume"
    "${test_dir}/test_sq_volume"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_sq_wrap.c main/sq_wrap.c \
        -o "${test_dir}/test_sq_wrap"
    "${test_dir}/test_sq_wrap"

    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_display_rounding.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_display_rounding"
    "${test_dir}/test_bsp_display_rounding"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_es8311_sleep_check.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_es8311_sleep_check"
    "${test_dir}/test_bsp_es8311_sleep_check"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_button.c -o "${test_dir}/test_bsp_button"
    "${test_dir}/test_bsp_button"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_lvgl_init.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_lvgl_init"
    "${test_dir}/test_bsp_lvgl_init"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/audio_stubs -Icomponents/bsp/include -Icomponents/bsp/src \
        tests/test_bsp_audio_recovery.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_audio_recovery"
    "${test_dir}/test_bsp_audio_recovery"
    for demo in audio low_power ble wifi; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
            -ffunction-sections -fdata-sections -Itests/demo_stubs -Imain \
            "tests/test_demo_${demo}_runtime.c" -Wl,--gc-sections \
            -o "${test_dir}/test_demo_${demo}_runtime"
        "${test_dir}/test_demo_${demo}_runtime"
    done
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_deep_sleep_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_check_repo.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_verify_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_archive_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_install_passport_skills.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/sq-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/sq-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/archive_firmware.py create \
        "${validation_build_dir}" --archive-root "${repo_root}/build/firmware"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
