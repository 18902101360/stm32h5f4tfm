#!/usr/bin/env bash
# Standalone MCUboot signer for TF-M Secure / Non-Secure binaries (STM32H5F4).
# Matches tfmcubeideproject/STM32CubeIDE/sign_kit, with H5F4 slot sizes and
# a local venv (does not use the repo-root TF-M .venv).
#
#   ./sign.sh tfm_ns.bin
#   ./sign.sh sapp.bin
#
# SPDX-License-Identifier: BSD-3-Clause

set -euo pipefail

KIT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "$KIT/config"

usage() {
    cat <<'EOF'
把未签名的 S / NS 固件放到本目录，执行：

  ./sign.sh <文件名>

示例：
  ./sign.sh tfm_ns.bin          # 非安全
  ./sign.sh ns.bin
  ./sign.sh sapp.bin            # 安全
  ./sign.sh tfm_s.bin
  ./sign.sh ../out/tfm_ns.bin   # 输出写在输入文件旁边

文件名看不出类型时，显式指定：
  ./sign.sh ns  app.bin
  ./sign.sh s   app.bin

输出：<输入文件所在目录>/<文件名去扩展名>_signed.bin
EOF
}

die() { echo "错误: $*" >&2; exit 1; }

KIND=""
IN_NAME=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        ns|NS|nspe|NSPE)
            KIND="ns"
            shift
            ;;
        s|S|sapp|SAPP|spe|SPE)
            KIND="s"
            shift
            ;;
        -*)
            die "未知选项 $1（用 --help 查看用法）"
            ;;
        *)
            IN_NAME="$1"
            shift
            break
            ;;
    esac
done

[[ $# -eq 0 ]] || die "多余参数: $*"
[[ -n "$IN_NAME" ]] || { usage >&2; exit 2; }

resolve_input() {
    local name="$1"
    if [[ -f "$name" ]]; then
        printf '%s' "$(cd "$(dirname "$name")" && pwd)/$(basename "$name")"
        return
    fi
    if [[ -f "$KIT/$name" ]]; then
        printf '%s' "$KIT/$name"
        return
    fi
    return 1
}

IN_BIN=""
if IN_BIN="$(resolve_input "$IN_NAME")"; then
    :
else
    die "找不到文件: $IN_NAME（请放到 $KIT 或给出路径）"
fi

base="$(basename "$IN_BIN")"
base_lc="$(printf '%s' "$base" | tr '[:upper:]' '[:lower:]')"

if [[ -z "$KIND" ]]; then
    if [[ "$base_lc" == *ns* ]]; then
        KIND="ns"
    elif [[ "$base_lc" == *sapp* || "$base_lc" == *tfm_s* || "$base_lc" == *_s.bin || "$base_lc" == *_s_* ]]; then
        KIND="s"
    else
        die "无法从文件名判断是 NS 还是 S，请用: ./sign.sh ns $base  或  ./sign.sh s $base"
    fi
fi

stem="${base%.*}"
OUT_BIN="$(dirname "$IN_BIN")/${stem}_signed.bin"

if [[ "$KIND" == "ns" ]]; then
    LAYOUT="$KIT/layout/signing_layout_ns.o"
    KEY="$KIT/keys/image_ns_signing_private_key.pem"
    VERSION="${MCUBOOT_IMAGE_VERSION_NS}"
    SEC_CNT="${MCUBOOT_SECURITY_COUNTER_NS}"
    DEP="(0, ${MCUBOOT_S_IMAGE_MIN_VER})"
    SLOT_HINT="NS  1200KB @ 0x0C0D0000"
else
    LAYOUT="$KIT/layout/signing_layout_s.o"
    KEY="$KIT/keys/image_s_signing_private_key.pem"
    VERSION="${MCUBOOT_IMAGE_VERSION_S}"
    SEC_CNT="${MCUBOOT_SECURITY_COUNTER_S}"
    DEP="(1, ${MCUBOOT_NS_IMAGE_MIN_VER})"
    SLOT_HINT="S   352KB @ 0x0C078000"
fi

for f in "$LAYOUT" "$KEY" "$KIT/scripts/wrapper.py" "$KIT/bl2/macro_parser.py"; do
    [[ -f "$f" ]] || die "缺少签名文件: $f"
done

py_ok() {
    local py="$1"
    [[ -x "$py" ]] || command -v "$py" >/dev/null 2>&1 || return 1
    "$py" -c "import click, cryptography, cbor2, intelhex" 2>/dev/null
}

ensure_python() {
    local cands=() p
    # Do not use the repo-root TF-M .venv (cryptography ABI mismatch).
    if [[ -n "${PYTHON:-}" ]]; then
        cands+=("$PYTHON")
    fi
    cands+=(
        "$KIT/.venv/bin/python"
        "$KIT/../.sign-venv/bin/python"
    )
    for p in "${cands[@]}"; do
        if py_ok "$p"; then
            if command -v "$p" >/dev/null 2>&1 && [[ ! -x "$p" ]]; then
                PYTHON="$(command -v "$p")"
            else
                PYTHON="$p"
            fi
            return 0
        fi
    done

    echo "imgtool: creating $KIT/.venv (not using a broken TF-M .venv)" >&2
    python3 -m venv "$KIT/.venv"
    "$KIT/.venv/bin/python" -m pip install -q -r "$KIT/requirements.txt"
    PYTHON="$KIT/.venv/bin/python"
    py_ok "$PYTHON" || die "Python 依赖安装失败。可手动: $PYTHON -m pip install -r $KIT/requirements.txt"
}

ensure_python

if [[ "${MCUBOOT_HW_KEY}" == "ON" ]]; then
    PUB_FMT="full"
else
    PUB_FMT="hash"
fi

ARGS=(
    --version "$VERSION"
    --layout "$LAYOUT"
    --key "$KEY"
    --public-key-format "$PUB_FMT"
    --align "$MCUBOOT_ALIGN_VAL"
    --pad
    --pad-header
    -H "$BL2_HEADER_SIZE"
    -s "$SEC_CNT"
    -L "$MCUBOOT_ENC_KEY_LEN"
    -d "$DEP"
)

if [[ "${MCUBOOT_UPGRADE_STRATEGY}" == "OVERWRITE_ONLY" ]]; then
    ARGS+=(--overwrite-only)
fi
if [[ "${MCUBOOT_CONFIRM_IMAGE}" == "ON" ]]; then
    ARGS+=(--confirm)
fi
if [[ "${MCUBOOT_MEASURED_BOOT}" == "ON" ]]; then
    ARGS+=(--measured-boot-record)
fi
if [[ "${MCUBOOT_ENC_IMAGES}" == "ON" ]]; then
    ENC_KEY="$KIT/keys/image_enc_${KIND}_key.pem"
    [[ -f "$ENC_KEY" ]] || die "已打开加密但找不到密钥: $ENC_KEY"
    ARGS+=(-E "$ENC_KEY")
fi

ARGS+=("$IN_BIN" "$OUT_BIN")

echo "签名 ${KIND^^} 镜像  ($SLOT_HINT)"
echo "  输入  $IN_BIN"
echo "  输出  $OUT_BIN"
echo "  版本  $VERSION  header $BL2_HEADER_SIZE  align $MCUBOOT_ALIGN_VAL  计数器 $SEC_CNT"
echo "  python $PYTHON"

# cwd 必须是 scripts/，wrapper.py 才会优先用自带的 imgtool
cd "$KIT/scripts"
PYTHONPATH="$KIT${PYTHONPATH:+:$PYTHONPATH}" "$PYTHON" "$KIT/scripts/wrapper.py" "${ARGS[@]}"

ls -l "$OUT_BIN"
echo "完成: $OUT_BIN"
