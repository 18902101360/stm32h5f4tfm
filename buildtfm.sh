#!/usr/bin/env bash
# STM32H5F4  TF-M 一键编译（硬件浮点）
#
# 用法:
#   ./buildtfm.sh              # 交互选择 测试版 / 正式版
#   ./buildtfm.sh test         # 测试版：TEST_S/NS 全开，INFO 日志
#   ./buildtfm.sh prod         # 正式版：SPE 不带 S 测试分区，NS 测试程序可烧可跑，ERROR 日志
#   ./buildtfm.sh -h
#
# SPDX-License-Identifier: BSD-3-Clause

set -euo pipefail

usage() {
    cat <<'EOF'
STM32H5F4 TF-M 编译脚本（已启用硬件浮点 FPv5-SP-D16）

用法:
  ./buildtfm.sh              交互选择构建类型
  ./buildtfm.sh test         测试版（TEST_S + TEST_NS，INFO 日志）
  ./buildtfm.sh prod         正式版（TEST_S 关，TEST_NS 开，NS 测试可烧可跑，ERROR 日志）
  ./buildtfm.sh test --no-clean   不清 build 目录（增量，仍用本地依赖缓存）

默认每次编译会先调用 scripts/clean_tfm_build.sh：只清编译结果，
把已下载的 mcuboot/cmsis/… 留在 trusted-firmware-m/.deps-cache，不重新联网。

别名: test|debug|回归    prod|release|formal|正式
EOF
}

BUILD_TYPE=""
# 默认每次编译先清 build 产物并还原本地依赖缓存（避免 rm -rf 后重新下载）。
# 增量编译：./buildtfm.sh test --no-clean  或  BUILDTFM_NO_CLEAN=1 ./buildtfm.sh test
DO_CLEAN=1
[[ "${BUILDTFM_NO_CLEAN:-0}" == "1" ]] && DO_CLEAN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        test|debug|回归) BUILD_TYPE="test" ;;
        prod|release|formal|正式) BUILD_TYPE="prod" ;;
        clean|--clean) DO_CLEAN=1 ;;
        --no-clean|noclean) DO_CLEAN=0 ;;
        "")
            ;;
        *)
            echo "未知参数: $1"
            usage
            exit 2
            ;;
    esac
    shift || true
done

if [[ -z "${BUILD_TYPE}" ]]; then
    echo "请选择构建类型:"
    echo "  1) 测试版  — TEST_S / TEST_NS 全开，INFO 日志"
    echo "  2) 正式版  — 安全侧不带测试分区，NS 测试程序仍可烧可跑，ERROR 日志"
    echo -n "输入 1 或 2: "
    read -r choice
    case "${choice}" in
        1) BUILD_TYPE="test" ;;
        2) BUILD_TYPE="prod" ;;
        *) echo "无效选择"; exit 2 ;;
    esac
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 支持两种目录布局:
#   A) 脚本在仓库外:  WORK/buildtfm.sh  WORK/trusted-firmware-m  WORK/tf-m-tests
#   B) 脚本在仓库内:  TF-M/buildtfm.sh  上一级或旁路有 tf-m-tests
if [[ -f "${SCRIPT_DIR}/trusted-firmware-m/CMakeLists.txt" ]]; then
    WORK_ROOT="${SCRIPT_DIR}"
    TFM_ROOT="${WORK_ROOT}/trusted-firmware-m"
    TFM_TESTS="${WORK_ROOT}/tf-m-tests"
elif [[ -f "${SCRIPT_DIR}/CMakeLists.txt" && -d "${SCRIPT_DIR}/secure_fw" ]]; then
    TFM_ROOT="${SCRIPT_DIR}"
    WORK_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
    if [[ -f "${WORK_ROOT}/tf-m-tests/tests_reg/spe/CMakeLists.txt" ]]; then
        TFM_TESTS="${WORK_ROOT}/tf-m-tests"
    elif [[ -f "${TFM_ROOT}/../tf-m-tests/tests_reg/spe/CMakeLists.txt" ]]; then
        TFM_TESTS="$(cd "${TFM_ROOT}/../tf-m-tests" && pwd)"
    else
        TFM_TESTS="${WORK_ROOT}/tf-m-tests"
    fi
else
    echo "错误: 找不到 TF-M 源码（请把脚本放在仓库根目录或与仓库同级）"
    exit 1
fi

[[ -f "${TFM_ROOT}/CMakeLists.txt" ]] || { echo "错误: 缺少 ${TFM_ROOT}"; exit 1; }
[[ -f "${TFM_TESTS}/tests_reg/spe/CMakeLists.txt" ]] || { echo "错误: 缺少 ${TFM_TESTS}"; exit 1; }

cd "${TFM_ROOT}"

if [[ "${BUILD_TYPE}" == "test" ]]; then
    BUILD_LABEL="测试版"
    TEST_FLAGS=(-DTEST_S=ON -DTEST_NS=ON)
    LOG_FLAGS=(
        -DTFM_BL2_LOG_LEVEL=LOG_LEVEL_INFO
        -DTFM_SPM_LOG_LEVEL=LOG_LEVEL_INFO
        -DTFM_PARTITION_LOG_LEVEL=LOG_LEVEL_INFO
    )
else
    BUILD_LABEL="正式版"
    # 正式 SPE 不编安全侧测试分区；NS 仍是可烧、可跑的回归测试程序
    TEST_FLAGS=(-DTEST_S=OFF -DTEST_NS=ON)
    LOG_FLAGS=(
        -DTFM_BL2_LOG_LEVEL=LOG_LEVEL_ERROR
        -DTFM_SPM_LOG_LEVEL=LOG_LEVEL_ERROR
        -DTFM_PARTITION_LOG_LEVEL=LOG_LEVEL_ERROR
    )
fi

# Cortex-M33 单精度硬件 FPU；BL2 仍为 soft（TF-M 默认）
# NS cmake 只吃命令行/ spe_config，STM cpuarch 不设 FP，必须把配套开关一起传
FP_FLAGS=(
    -DCONFIG_TFM_ENABLE_FP=ON
    -DCONFIG_TFM_FP_ARCH=fpv5-sp-d16
    -DCONFIG_TFM_ENABLE_CP10CP11=ON
    -DCONFIG_TFM_FLOAT_ABI=hard
    -DCONFIG_TFM_LAZY_STACKING=ON
)

echo ">>> WORK_ROOT: ${WORK_ROOT}"
echo ">>> TFM_ROOT:  ${TFM_ROOT}"
echo ">>> TFM_TESTS: ${TFM_TESTS}"
echo ">>> 构建类型:  ${BUILD_LABEL}  (硬件浮点 ON, fpv5-sp-d16)"

LIB_EXT_S="${TFM_ROOT}/build_s/build-spe/lib/ext"
LIB_EXT_NS="${TFM_ROOT}/build_ns/lib/ext"
CLEAN_SH="${WORK_ROOT}/scripts/clean_tfm_build.sh"

# 测试版 <-> 正式版或 TEST_* 变化时清掉 SPE 缓存（须在离线检查之前）
STAMP="${TFM_ROOT}/build_s/.buildtfm_type"
# Include signature type so RSA <-> EC-P256 switches force a clean SPE rebuild
# (CMake caches MCUBOOT_KEY_S/NS paths derived from the old algorithm).
SIG_TYPE="$(sed -n 's/^[[:space:]]*set(MCUBOOT_SIGNATURE_TYPE[[:space:]]*"\([^"]*\)".*/\1/p' \
    "${TFM_ROOT}/platform/ext/target/stm/stm32h5f4/config.cmake" 2>/dev/null \
    | head -1)"
SIG_TYPE="${SIG_TYPE:-RSA-3072}"
STAMP_VAL="${BUILD_TYPE} ${TEST_FLAGS[*]} ${LOG_FLAGS[*]} SIG=${SIG_TYPE}"
if [[ -f "${STAMP}" ]] && [[ "$(cat "${STAMP}")" != "${STAMP_VAL}" ]]; then
    echo ">>> 构建配置已切换，将清除编译产物（保留依赖缓存）"
    DO_CLEAN=1
fi
echo ">>> MCUBOOT_SIGNATURE_TYPE: ${SIG_TYPE}"

# 若仓库根目录 keys/ 放了固定文件名的公私钥，则覆盖各工程同名文件 + BL2 root-EC/RSA*.pem。
# 缺失源文件或目标目录不存在只告警，不中断编译。
sync_user_signing_keys() {
    local sync_sh="${WORK_ROOT}/scripts/sync_user_signing_keys.sh"
    if [[ ! -f "${sync_sh}" ]]; then
        echo "警告: 缺少 ${sync_sh}，跳过 keys/ 用户密钥同步"
        return 0
    fi
    bash "${sync_sh}" "${WORK_ROOT}" "${SIG_TYPE}" || true
}

# 同步 STM OTP 表里的 bl2_rotpk_*（及 provisioning.c dummy 哈希）。
# 优先 keys/image_{s,ns}_signing_public_key.pem（S/NS 各自独立）；没有公钥则沿用
# root-EC/RSA*.pem 私钥（或 MCUBOOT_KEY_S/NS）。下次 ./buildtfm.sh 自动改 OTP。
sync_stm_otp_rotpk() {
    local sync_py="${WORK_ROOT}/scripts/sync_stm_otp_rotpk.py"
    local key_s key_ns
    local pub_s="${WORK_ROOT}/keys/image_s_signing_public_key.pem"
    local pub_ns="${WORK_ROOT}/keys/image_ns_signing_public_key.pem"
    if [[ ! -f "${sync_py}" ]]; then
        echo "警告: 缺少 ${sync_py}，跳过 OTP ROTPK 同步"
        return 0
    fi
    case "${SIG_TYPE}" in
        EC-P256)
            key_s="${TFM_ROOT}/bl2/ext/mcuboot/root-EC-P256.pem"
            key_ns="${TFM_ROOT}/bl2/ext/mcuboot/root-EC-P256_1.pem"
            ;;
        EC-P384)
            key_s="${TFM_ROOT}/bl2/ext/mcuboot/root-EC-P384.pem"
            key_ns="${TFM_ROOT}/bl2/ext/mcuboot/root-EC-P384_1.pem"
            ;;
        RSA-2048)
            key_s="${TFM_ROOT}/bl2/ext/mcuboot/root-RSA-2048.pem"
            key_ns="${TFM_ROOT}/bl2/ext/mcuboot/root-RSA-2048_1.pem"
            ;;
        RSA-3072)
            key_s="${TFM_ROOT}/bl2/ext/mcuboot/root-RSA-3072.pem"
            key_ns="${TFM_ROOT}/bl2/ext/mcuboot/root-RSA-3072_1.pem"
            ;;
        *)
            echo "警告: 未知 SIG=${SIG_TYPE}，跳过 OTP ROTPK 同步"
            return 0
            ;;
    esac
    [[ -n "${MCUBOOT_KEY_S:-}" ]] && key_s="${MCUBOOT_KEY_S}"
    [[ -n "${MCUBOOT_KEY_NS:-}" ]] && key_ns="${MCUBOOT_KEY_NS}"
    [[ -f "${pub_s}" ]] && key_s="${pub_s}"
    [[ -f "${pub_ns}" ]] && key_ns="${pub_ns}"
    "${PYTHON}" "${sync_py}" \
        --tfm-root "${TFM_ROOT}" \
        --sig-type "${SIG_TYPE}" \
        --key-s "${key_s}" \
        --key-ns "${key_ns}"
}

# Python：用 python -m pip，避免拷贝来的 venv shebang 失效
VENV_DIR="${WORK_ROOT}/.venv"
if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
    echo ">>> 创建 Python venv: ${VENV_DIR}"
    if ! python3 -m venv "${VENV_DIR}"; then
        echo "错误: 创建 venv 失败。Debian/Ubuntu 请先执行:"
        echo "  apt install -y python3-venv python3-pip"
        exit 1
    fi
fi
# shellcheck disable=SC1091
source "${VENV_DIR}/bin/activate"
export PATH="${VENV_DIR}/bin:${PATH}"
PYTHON="${VENV_DIR}/bin/python"
"${PYTHON}" -m pip install -q --upgrade pip setuptools wheel || true
# Always install TF-M Python deps into *this* venv. Do not use `command -v hex_generation`
# (may hit another venv on PATH and skip jinja2/pyyaml/…).
"${PYTHON}" -m pip install -q -e "${TFM_ROOT}"
"${PYTHON}" -c "import cryptography, jinja2, yaml" 2>/dev/null || {
    echo "错误: TF-M Python 依赖未装好（需要 cryptography/jinja2/pyyaml 等）"
    exit 1
}
"${PYTHON}" -c "import shutil,sys; sys.exit(0 if shutil.which('hex_generation') else 1)" \
    || { echo "错误: hex_generation 未安装"; exit 1; }

sync_user_signing_keys
sync_stm_otp_rotpk

# 若 versions/ 填了 S/NS 版本，同步到 sign_kit 并为 SPE/NS cmake 准备 -D 参数
VERSION_CMAKE_ARGS=()
sync_user_versions() {
    local sync_sh="${WORK_ROOT}/scripts/sync_user_versions.sh"
    local envf
    envf="$(mktemp)"
    if [[ ! -f "${sync_sh}" ]]; then
        echo "警告: 缺少 ${sync_sh}，跳过 versions/ 用户版本同步"
        rm -f "${envf}"
        return 0
    fi
    bash "${sync_sh}" "${WORK_ROOT}" "${envf}" || true
    if [[ -f "${envf}" ]] && grep -q 'VERSION_CMAKE_ARGS=' "${envf}"; then
        # shellcheck disable=SC1090
        source "${envf}"
    fi
    rm -f "${envf}"
}
sync_user_versions

# 清编译产物 / 还原本地依赖（避免每次 rm -rf build_s 后重新下载）
if [[ -f "${CLEAN_SH}" ]]; then
    if [[ "${DO_CLEAN}" -eq 1 ]]; then
        bash "${CLEAN_SH}" "${TFM_ROOT}"
    else
        echo ">>> --no-clean：保留 build 目录，仅确保依赖缓存可用"
        bash "${CLEAN_SH}" --save-only "${TFM_ROOT}" || true
        bash "${CLEAN_SH}" --restore-only "${TFM_ROOT}" || true
    fi
else
    echo "警告: 缺少 ${CLEAN_SH}，退回旧逻辑"
    if [[ "${DO_CLEAN}" -eq 1 ]]; then
        rm -rf "${TFM_ROOT}/build_s" "${TFM_ROOT}/build_ns"
    fi
fi

# cmake FetchContent 增量配置可能把已拉取的 MCUBoot 重置回 tag。
# 不要用 git apply 打 format-patch：可能返回 0 却不改文件。
apply_mcuboot_0002() {
    local helper="${TFM_ROOT}/lib/ext/mcuboot/apply_h5f4_swap_guard.py"
    local src misc
    local found=0
    [[ -f "${helper}" ]] || { echo "错误: 缺少 ${helper}"; exit 1; }
    while IFS= read -r src; do
        misc="${src}/boot/bootutil/src/swap_misc.c"
        [[ -f "${misc}" ]] || continue
        found=1
        python3 "${helper}" "${src}"
        grep -q 'H5F4SWP2' "${misc}" || { echo "错误: ${misc} 仍没有 H5F4SWP2"; exit 1; }
        touch "${misc}" "${src}/boot/bootutil/src/swap_scratch.c" \
            "${src}/boot/bootutil/src/image_validate.c"
        find "${TFM_ROOT}/build_s" \( -name 'swap_misc.c.o' -o -name 'swap_scratch.c.o' \
            -o -name 'image_validate.c.o' \) -delete 2>/dev/null || true
    done < <(find "${TFM_ROOT}/build_s" -type d -name 'mcuboot-src' 2>/dev/null)
    if [[ "${found}" -eq 0 ]]; then
        echo "错误: 找不到 mcuboot-src，无法打 MCUBoot swap 防护"
        echo "内层 TF-M configure 应已拉取 MCUBoot。请检查网络后: ${CLEAN_SH} && ./buildtfm.sh ${BUILD_TYPE}"
        exit 1
    fi
}

# tests_reg/spe 只生成 wrapper。MCUBoot 是内层 TF-M cmake（FetchContent）下的，
# 第一次或切 test/prod 清掉 build_s 之后还不存在。先跑 TF-M-configure 再打补丁。
ensure_mcuboot_src() {
    local src misc
    while IFS= read -r src; do
        misc="${src}/boot/bootutil/src/swap_misc.c"
        if [[ -f "${misc}" ]]; then
            return 0
        fi
    done < <(find "${TFM_ROOT}/build_s" -type d -name 'mcuboot-src' 2>/dev/null)
    echo ">>> 内层 TF-M configure（拉取 MCUBoot）"
    ninja -C build_s TF-M-configure
}

# 有 lib/ext 就离线，没有就自动在线下载
OFFLINE=1
for lib in qcbor mcuboot cmsis t_cose tf-psa-crypto tf-m-extras; do
    [[ -d "${LIB_EXT_S}/${lib}-src" ]] || OFFLINE=0
done
[[ -f "${LIB_EXT_S}/qcbor-src/src/qcbor_encode.c" ]] || OFFLINE=0

if [[ "${OFFLINE}" -eq 1 ]]; then
    echo ">>> 离线模式"
    FETCH_OFF=(-DFETCHCONTENT_FULLY_DISCONNECTED=ON)
else
    echo ">>> 在线模式（首次会下载依赖，较慢）"
    FETCH_OFF=()
fi

# 关掉 tf-m-tests 版本检查（只打一次补丁）
CV="${TFM_TESTS}/cmake/check_version.cmake"
if [[ -f "${CV}" ]] && ! grep -q 'buildtfm: skip version check' "${CV}"; then
    sed -i '8i\return() # buildtfm: skip version check' "${CV}"
fi

echo ">>> build_s (${BUILD_LABEL})"
cmake -S "${TFM_TESTS}/tests_reg/spe" -B build_s -GNinja \
    -DCONFIG_TFM_SOURCE_PATH="${TFM_ROOT}" \
    -DTFM_PLATFORM=stm/stm32h5f4 \
    -DTFM_TOOLCHAIN_FILE="${TFM_ROOT}/toolchain_GNUARM.cmake" \
    -DTFM_PSA_API=ON \
    -DTFM_ISOLATION_LEVEL=1 \
    -DBL2_TRAILER_SIZE=0x3000 \
    -DMCUBOOT_SIGNATURE_TYPE="${SIG_TYPE}" \
    "${VERSION_CMAKE_ARGS[@]}" \
    "${TEST_FLAGS[@]}" \
    "${FP_FLAGS[@]}" \
    "${LOG_FLAGS[@]}" \
    "${FETCH_OFF[@]}"

# tests_reg/spe 是一层 wrapper，已有的 build-spe CMakeCache 不会吃上面的
# -DBL2_TRAILER_SIZE。必须再配一次内层 TF-M，否则仍是 0x2000。
# Drop cached key paths when switching RSA <-> EC so defaults rematerialize.
ensure_mcuboot_src
if [[ -f "${TFM_ROOT}/build_s/build-spe/CMakeCache.txt" ]]; then
    echo ">>> 内层 TF-M: BL2_TRAILER_SIZE=0x3000 SIG=${SIG_TYPE}"
    cmake -UMCUBOOT_KEY_S -UMCUBOOT_KEY_NS \
        -DBL2_TRAILER_SIZE=0x3000 \
        -DMCUBOOT_SIGNATURE_TYPE="${SIG_TYPE}" \
        "${VERSION_CMAKE_ARGS[@]}" \
        "${TFM_ROOT}/build_s/build-spe"
fi

apply_mcuboot_0002

ninja -C build_s install -j"$(nproc)"
mkdir -p "$(dirname "${STAMP}")"
echo "${STAMP_VAL}" > "${STAMP}"
# 编译成功后刷新本地依赖缓存，供下次清目录后离线使用
if [[ -f "${CLEAN_SH}" ]]; then
    bash "${CLEAN_SH}" --save-only "${TFM_ROOT}" || true
fi

SPE_CONFIG="${TFM_ROOT}/build_s/api_ns/cmake/spe_config.cmake"
[[ -f "${SPE_CONFIG}" ]] && \
    sed -i 's/^set(CHECK_TFM_TESTS_VERSION.*$/set(CHECK_TFM_TESTS_VERSION OFF)/' "${SPE_CONFIG}"

echo ">>> build_ns (回归测试程序，可烧录可跑)"
# 只清 NS 编译树；依赖从 .deps-cache / SPE 还原，不联网
if [[ -f "${CLEAN_SH}" ]]; then
    if [[ -d "${TFM_ROOT}/build_ns" ]]; then
        bash "${CLEAN_SH}" --save-only "${TFM_ROOT}" || true
        rm -rf "${TFM_ROOT}/build_ns"
    fi
    bash "${CLEAN_SH}" --restore-only "${TFM_ROOT}" || true
else
    rm -rf build_ns
    mkdir -p "${LIB_EXT_NS}"
    for lib in qcbor t_cose; do
        [[ -d "${LIB_EXT_S}/${lib}-src" ]] && cp -a "${LIB_EXT_S}/${lib}-src" "${LIB_EXT_NS}/"
    done
fi
# 再确保 NS 有 qcbor/t_cose（restore 可能只写了 cache）
mkdir -p "${LIB_EXT_NS}"
for lib in qcbor t_cose; do
    if [[ ! -d "${LIB_EXT_NS}/${lib}-src" && -d "${LIB_EXT_S}/${lib}-src" ]]; then
        cp -a "${LIB_EXT_S}/${lib}-src" "${LIB_EXT_NS}/"
    fi
done

cmake -S "${TFM_TESTS}/tests_reg" -B build_ns -GNinja \
    -DCONFIG_SPE_PATH="${TFM_ROOT}/build_s/api_ns" \
    -DTFM_TOOLCHAIN_FILE="${TFM_ROOT}/build_s/api_ns/cmake/toolchain_ns_GNUARM.cmake" \
    "${VERSION_CMAKE_ARGS[@]}" \
    "${FP_FLAGS[@]}" \
    "${FETCH_OFF[@]}"

ninja -C build_ns -j"$(nproc)"

echo ">>> postbuild"
cd build_s/api_ns
chmod +x postbuild.sh regression.sh TFM_UPDATE.sh preprocess.sh 2>/dev/null || true
./postbuild.sh "$(command -v arm-none-eabi-gcc)"

BL2_BIN="${TFM_ROOT}/build_s/api_ns/bin/bl2.bin"
[[ -f "${BL2_BIN}" ]] || { echo "错误: 找不到 ${BL2_BIN}"; exit 1; }
# H5F4BL2 is a static string (always linked). "Starting bootloader S-sec=" is
# BOOT_LOG_INF and is compiled out in prod (LOG_LEVEL_ERROR).
if ! grep -a -F -q "H5F4BL2" "${BL2_BIN}"; then
    echo "错误: ${BL2_BIN} 没有 H5F4BL2 标记（编译产物不是当前源码）"
    strings "${BL2_BIN}" | grep -E "Starting bootloader|H5F4BL2" || true
    exit 1
fi
if [[ "${BUILD_TYPE}" == "test" ]] && ! grep -a -F -q "Starting bootloader S-sec=" "${BL2_BIN}"; then
    echo "错误: ${BL2_BIN} 没有 S-sec 标记（测试版 INFO 日志应编进该串）"
    strings "${BL2_BIN}" | grep -E "Starting bootloader|H5F4BL2" || true
    exit 1
fi
if ! grep -a -F -q "H5F4SWP2" "${BL2_BIN}"; then
    echo "错误: ${BL2_BIN} 没有 MCUBoot 0002 标记 H5F4SWP2（image 0 会在 0x30180000 BusFault）"
    echo "cmake 可能冲掉了补丁。请再跑一次 ./buildtfm.sh ${BUILD_TYPE}；仍失败则: ${CLEAN_SH} && ./buildtfm.sh ${BUILD_TYPE}"
    exit 1
fi
if ! grep -q '^slot2=0xc200000$' TFM_UPDATE.sh; then
    echo "错误: TFM_UPDATE.sh 的 slot2 必须是 0xc200000（S secondary 在 bank 2）"
    grep -E '^slot[0-3]=' TFM_UPDATE.sh || true
    exit 1
fi
if ! grep -q '^slot1=0xc0d0000$' TFM_UPDATE.sh; then
    echo "错误: TFM_UPDATE.sh 的 slot1 必须是 0xc0d0000（S primary 已扩到 352 KB）"
    grep -E '^slot[0-3]=' TFM_UPDATE.sh || true
    exit 1
fi

S_SIGNED="${TFM_ROOT}/build_s/api_ns/bin/tfm_s_signed.bin"
[[ -f "${S_SIGNED}" ]] || { echo "错误: 找不到 ${S_SIGNED}"; exit 1; }
"${PYTHON}" - "${S_SIGNED}" <<'PY'
import struct, sys
from pathlib import Path
data = Path(sys.argv[1]).read_bytes()
if len(data) < 32:
    raise SystemExit(f"错误: {sys.argv[1]} 太小")
magic, _load, hdr, prot, img, _flags = struct.unpack_from("<IIHHII", data, 0)
if magic != 0x96F3B83D:
    raise SystemExit(f"错误: {sys.argv[1]} 没有 MCUBoot 头 magic")
off = hdr + img
if off + 4 > len(data):
    raise SystemExit(f"错误: {sys.argv[1]} TLV 超出文件")
mag, tot = struct.unpack_from("<HH", data, off)
end = off + tot
if mag == 0x6908:
    if end + 4 > len(data):
        raise SystemExit(f"错误: {sys.argv[1]} 未保护 TLV 超出文件")
    _umag, utot = struct.unpack_from("<HH", data, end)
    end = end + utot
# MCUBoot SWAP_USING_SCRATCH trailer for 150 NS sectors, write_sz=16, align=16.
trailer = 150 * 3 * 16 + 80
max_img = len(data) - trailer
if end > max_img:
    raise SystemExit(
        f"错误: S 镜像 TLV 到 0x{end:x}，MCUBoot 上限 0x{max_img:x} "
        f"(slot=0x{len(data):x} trailer=0x{trailer:x})。加大 FLASH_S_PARTITION_SIZE。"
    )
print(f">>> S 镜像 TLV end=0x{end:x} max=0x{max_img:x} slot=0x{len(data):x}")
PY

echo ""
grep -E '^boot=|^slot0=|^slot1=|^slot2=|^slot3=' TFM_UPDATE.sh || true
echo ""
echo "=== 编译完成（${BUILD_LABEL}，硬件浮点 ON）==="
echo "************************************************************"
echo "* 只能烧下面这个目录，不要用 tfmcubeideproject /"
echo "* tfmmakeproject / windows-tfm-tools 里的旧脚本和 hex"
echo "* ${TFM_ROOT}/build_s/api_ns"
echo "*"
echo "* cd ${TFM_ROOT}/build_s/api_ns"
echo "* ./regression.sh"
echo "* STM32_Programmer_CLI -c port=SWD mode=HotPlug -ob BOOT_UBE=0xB4"
echo "* ./TFM_UPDATE.sh"
echo "*"
echo "* 烧完串口第一行必须有: H5F4BL2（BOOT_LOG_ERR，正式版也会打）"
echo "* 如果没有 H5F4BL2，BL2 没写进去"
echo "************************************************************"
SLOT1="$(sed -n 's/^slot1=//p' TFM_UPDATE.sh | head -n1)"
SLOT3="$(sed -n 's/^slot3=//p' TFM_UPDATE.sh | head -n1)"
echo "NS 测试程序: ${TFM_ROOT}/build_ns/bin/tfm_ns_signed.bin  地址 0x${SLOT1#0x}"
if [[ -n "${SLOT3}" ]]; then
    ns_sec=$((SLOT3))
    ns_end=$((ns_sec + 0x12C000))
    user_sz=$((0x0C400000 - ns_end))
    printf 'NS 用户 Flash 数据区: 0x%X  大小 %d KB（到 4 MB 末尾）\n' "${ns_end}" "$((user_sz / 1024))"
fi
