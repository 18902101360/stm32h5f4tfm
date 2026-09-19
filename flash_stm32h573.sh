#!/usr/bin/env bash
# STM32H573I-DK 一键回归 + 烧录（Linux）
#
#   ./flash_stm32h573.sh                 # 回归（option bytes + 全片擦除）后烧 BL2/S/NS
#   ./flash_stm32h573.sh download        # 只烧镜像（不擦片）
#   ./flash_stm32h573.sh regression      # 只跑回归
#   ./flash_stm32h573.sh all <ST-LINK SN>
#   ./flash_stm32h573.sh download <SN>
#
# 地址（安全别名）：BL2 0x0C00E000，S 0x0C044000，NS 0x0C100000（Bank2）
# BOOT_UBE=0xB4（OEM-iRoT）。需要 STM32_Programmer_CLI，且 SWD 用 AP=1。
# Windows 对应：windows-tfm-tools\tfm_update.bat
#
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
API_NS="${ROOT}/trusted-firmware-m/build_s/api_ns"
REGRESSION_SRC="${ROOT}/trusted-firmware-m/platform/ext/target/stm/stm32h573i_dk/regression.sh"
BL2_BIN="${API_NS}/bin/bl2.bin"
BL2_HEX="${API_NS}/bin/bl2.hex"
S_BIN="${API_NS}/bin/tfm_s_signed.bin"
NS_BIN_DEFAULT="${ROOT}/trusted-firmware-m/build_ns/bin/tfm_ns_signed.bin"
NS_BIN_FALLBACK="${ROOT}/windows-tfm-tools/tfm_ns_signed.bin"

BOOT_ADDR=0xc00e000
SLOT_S=0xc044000
SLOT_NS=0xc100000
# MCUboot image magic little-endian 0x96f3b83d
MAGIC_BYTES="3db8f396"

die() { echo "错误: $*" >&2; exit 1; }

usage() {
    cat <<'EOF'
用法:
  ./flash_stm32h573.sh                 # 一键：回归 + 烧录
  ./flash_stm32h573.sh all [SN]        # 同上
  ./flash_stm32h573.sh download [SN]   # 只烧 BL2 + S + NS
  ./flash_stm32h573.sh regression [SN] # 只写 option bytes（会全片擦除）
  ./flash_stm32h573.sh --help

先编译: ./buildtfm.sh test   （推荐 test：BL2 INFO 日志更全，便于看验签）
产物: trusted-firmware-m/build_s/api_ns/bin/{bl2,tfm_s_signed}.bin
      trusted-firmware-m/build_ns/bin/tfm_ns_signed.bin
EOF
}

find_stm32_cli() {
    if command -v STM32_Programmer_CLI >/dev/null 2>&1; then
        command -v STM32_Programmer_CLI
        return 0
    fi
    local d
    for d in \
        /usr/local/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin \
        "${HOME}/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin" \
        /opt/st/stm32cubeprogrammer/bin \
        /opt/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin
    do
        if [[ -x "${d}/STM32_Programmer_CLI" ]]; then
            echo "${d}/STM32_Programmer_CLI"
            return 0
        fi
    done
    return 1
}

resolve_ns_bin() {
    if [[ -n "${TFM_NS_BIN:-}" ]]; then
        printf '%s' "${TFM_NS_BIN}"
        return 0
    fi
    if [[ -f "${NS_BIN_DEFAULT}" ]]; then
        printf '%s' "${NS_BIN_DEFAULT}"
        return 0
    fi
    if [[ -f "${NS_BIN_FALLBACK}" ]]; then
        printf '%s' "${NS_BIN_FALLBACK}"
        return 0
    fi
    return 1
}

MODE="all"
SN=""
case "${1:-}" in
    ""|all)
        MODE="all"
        SN="${2:-}"
        ;;
    download|flash|update)
        MODE="download"
        SN="${2:-}"
        ;;
    regression|erase|regress)
        MODE="regression"
        SN="${2:-}"
        ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        if [[ "${1}" =~ ^[0-9A-Fa-f]+$ ]] && [[ $# -eq 1 ]]; then
            MODE="all"
            SN="$1"
        else
            usage >&2
            die "未知参数: $*"
        fi
        ;;
esac

sn_option=""
[[ -n "${SN}" ]] && sn_option="sn=${SN}"

CLI="$(find_stm32_cli)" || die "找不到 STM32_Programmer_CLI（请安装 STM32CubeProgrammer 并加入 PATH）"
# H5 + TZEN 后烧录用 HotPlug + AP=1 更稳；UR 也保留给 hardRst
CONNECT_UR="-c port=SWD ap=1 ${sn_option} mode=UR"
CONNECT_HP="-c port=SWD ap=1 ${sn_option} mode=HotPlug"

run_regression() {
    echo "=== H573 回归（写 option bytes，会全片擦除）==="
    if [[ -n "${SN}" ]]; then
        echo "ST-LINK SN=${SN}"
    fi
    if [[ -f "${REGRESSION_SRC}" ]]; then
        export PATH="$(dirname "${CLI}"):${PATH}"
        bash "${REGRESSION_SRC}" ${SN:+"${SN}"}
    elif [[ -x "${API_NS}/regression.sh" ]]; then
        export PATH="$(dirname "${CLI}"):${PATH}"
        (cd "${API_NS}" && ./regression.sh ${SN:+"${SN}"})
    else
        die "找不到 regression.sh（期望 ${REGRESSION_SRC}）"
    fi
    echo ">>> BOOT_UBE=0xB4（OEM-iRoT）"
    "${CLI}" ${CONNECT_HP} -ob BOOT_UBE=0xB4 || true
}

readback_magic() {
    local addr="$1"
    local label="$2"
    # CubeProgrammer 2.x: -u/--upload writes memory to a .bin/.hex file.
    # (-r without full args fails on v2.23 with "missing some arguments")
    local tmp
    tmp="$(mktemp --suffix=.bin)"
    echo ">>> 回读 ${label} @ ${addr}（检查 MCUboot magic）"
    "${CLI}" ${CONNECT_HP} -u "${addr}" 0x10 "${tmp}" \
        || die "回读 ${label} 失败（可能没烧上，或 AP/连接不对）"
    local got
    got="$(od -An -tx1 -N4 "${tmp}" | tr -d ' \n')"
    rm -f "${tmp}"
    echo "    片上前 4 字节: ${got}"
    if [[ "${got}" != "${MAGIC_BYTES}" ]]; then
        die "${label} 没有 MCUboot magic（期望 ${MAGIC_BYTES}）。烧录未成功，请检查上方 Download/Verify 是否 OK。"
    fi
    echo "    ${label} magic OK"
}

run_download() {
    local ns_bin
    [[ -f "${BL2_BIN}" ]] || die "缺少 ${BL2_BIN}，请先: ./buildtfm.sh test"
    [[ -f "${S_BIN}" ]] || die "缺少 ${S_BIN}，请先: ./buildtfm.sh test"
    ns_bin="$(resolve_ns_bin)" || die "缺少 NS 已签名镜像。请 ./buildtfm.sh test，或设置 TFM_NS_BIN=/path/to/tfm_ns_signed.bin"

    echo "=== H573 下载 BL2 + SPE + NS ==="
    echo "S    ${SLOT_S}  <- ${S_BIN}"
    echo "NS   ${SLOT_NS}  <- ${ns_bin}"
    if [[ -f "${BL2_HEX}" ]]; then
        echo "BL2  (hex 内含地址) <- ${BL2_HEX}"
    else
        echo "BL2  ${BOOT_ADDR}  <- ${BL2_BIN}"
    fi
    if [[ -n "${SN}" ]]; then
        echo "ST-LINK SN=${SN}"
    fi

    echo ">>> BOOT_UBE=0xB4"
    "${CLI}" ${CONNECT_HP} -ob BOOT_UBE=0xB4 || true

    # 先烧应用、后烧 BL2（与 ST TFM_UPDATE / Windows tfm_update 一致）
    echo ">>> Write Secure（须出现 Download verified successfully）"
    "${CLI}" ${CONNECT_HP} -d "${S_BIN}" ${SLOT_S} -v
    echo ">>> Write Non-Secure"
    "${CLI}" ${CONNECT_HP} -d "${ns_bin}" ${SLOT_NS} -v
    echo ">>> Write BL2"
    if [[ -f "${BL2_HEX}" ]]; then
        "${CLI}" ${CONNECT_HP} -d "${BL2_HEX}" -v
    else
        "${CLI}" ${CONNECT_HP} -d "${BL2_BIN}" ${BOOT_ADDR} -v
    fi

    readback_magic "${SLOT_S}" "Secure primary"
    readback_magic "${SLOT_NS}" "Non-Secure primary"

    echo ">>> hard reset"
    "${CLI}" ${CONNECT_UR} -hardRst || true
    echo "download Done"
    echo
    echo "若仍报 Image in the primary slot is not valid："
    echo "  1) 必须烧含 OTP 区的 bl2.hex（本脚本默认用它）；勿只烧旧 RSA 的 BL2"
    echo "  2) 从 RSA 切到 EC-P256 后先 ./flash_stm32h573.sh（回归全片擦除）再烧"
    echo "  3) 串口应有: PSA Crypto init done, sig_type: EC-P256"
    echo "  4) S/NS 须与 BL2 同一次 ./buildtfm.sh test 产物"
    echo "串口 ST-Link VCP 115200 8N1。"
}

case "${MODE}" in
    regression)
        run_regression
        ;;
    download)
        run_download
        ;;
    all)
        run_regression
        run_download
        echo "全部完成。"
        ;;
esac
