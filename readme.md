# Trusted Firmware-M 项目

基于 STM32H573 的 TF-M（Trusted Firmware-M）移植与开发项目。

平台名：`stm/stm32h573i_dk`。

| 分支 | 签名算法 | 说明 |
|------|----------|------|
| `master` | **RSA-3072** | 默认主线 |
| `stm32h573p256` | **EC-P256** | 仅改 MCUboot 镜像签名算法与配套密钥 |
| `stm32H573P256-SPIFLASH` | **EC-P256** | 基于 `stm32h573p256`：NS 执行槽 1 MB，升级槽在外部 W25Q32 |
| `stm32H573P256-SPIFLASH-bl2-public-key` | **EC-P256** | 基于 `stm32H573P256-SPIFLASH`：BL2 OTP ROTPK 可只用 `keys/` 公钥 |
| `stm32H573P256-SPIFLASH-bl2-public-key-mbedtls` | **EC-P256** | 基于 `stm32H573P256-SPIFLASH-bl2-public-key`：CubeIDE Mbed TLS 4.1.1、CSR/签发；**PS 扩到 64 KB**，S 主槽改为 `0x0C044000` |

本文档所在分支为 **`stm32H573P256-SPIFLASH-bl2-public-key-mbedtls`**。升级路径、BL2 公钥 ROTPK 与父分支相同；**PS 为 64 KB，S 烧录地址相对父分支后移 48 KB**。

### 相对 `stm32H573P256-SPIFLASH-bl2-public-key` 改了什么（本分支）

1. 把仓库根目录原来的 `tfmcubeideproject.7z` **整份展开覆盖** `tfmcubeideproject/`，再删掉压缩包（不要再当源码树用）。
2. 用本仓库当前 SPE 导出和 `sign_kit` **覆盖 7z 里旧的布局**（7z 仍是旧的 320/576 KB、SWAP；当前必须是 **S 512 KB / NS 1 MB Bank2、`OVERWRITE_ONLY`**）。
3. NS 侧编入 Mbed TLS 4.1.1（PSA 客户端：密码学走 SPE `psa_*` + `s_veneers.o`）。
4. 打开 PKCS#10 **CSR 生成 / 解析** 和 **自行签发 X.509 证书**（见下一节）。
5. **Protected Storage 从 16 KB 扩到 64 KB**（8 个 8 KB 扇区）。ITS 仍 16 KB，因此 S 主槽起点从 `0x0C038000` 挪到 **`0x0C044000`**（仍 512 KB）；Bank1 空隙变为 `0x0C0C4000–0x0C0FFFFF`（240 KB）。NS 仍整块 Bank2。改完必须 **回归并重烧 BL2 + S + NS**；旧 PS 文件系统不能沿用。
6. **ITS / PS 对象上限**（`config_tfm_target.h`）：`ITS_MAX_ASSET_SIZE=512`，`ITS_NUM_ASSETS=12`；`PS_MAX_ASSET_SIZE=2048`，`PS_NUM_ASSETS=24`。ITS 12 个满额对象能同时放下。PS 24 是槽位数；加密后单对象约 2.1 KB，64 KB 里大约 **22 个满 2 KB 对象**能同时存在（其余槽给更小的资产）。

CubeIDE 工程说明：[`tfmcubeideproject/STM32CubeIDE/README.md`](./tfmcubeideproject/STM32CubeIDE/README.md)。

### NS Mbed TLS：解析 CSR、自行签发证书

配置在 `ns_crypto_user.h`：`MBEDTLS_PK_WRITE_C`、`MBEDTLS_PEM_WRITE_C`。Makefile / `.cproject` **编入** `x509write_csr.c`、`x509_csr.c`、`x509write_crt.c`、`pkwrite.c`、`psa_util.c`（仍排除 `net_sockets.c`、软件 AES/SHA、server/DTLS）。私钥留在 SPE，NS 用 `mbedtls_pk_wrap_psa` 签名。

| 场景 | 用哪个 API |
|------|------------|
| 设备当 CA，解析别人发来的 PKCS#10 CSR | `mbedtls_x509_csr_parse` / `mbedtls_x509_csr_parse_der` |
| 设备当 CA，用 SPE 里的 CA 密钥签发证书 | `mbedtls_x509write_crt_der` / `mbedtls_x509write_crt_pem` |
| 外部 CA 回的是证书（不是 CSR） | `mbedtls_x509_crt_parse` / `mbedtls_x509_crt_parse_der` |
| 本机生成 PKCS#10 交给外部 CA | `mbedtls_x509write_csr_der` / `mbedtls_x509write_csr_pem` |

`ns_app/main.c` 的 `test_csr()` 冒烟：SPE 生成叶密钥 → 写 CSR → 解析 CSR → 第二把 CA 密钥签发 CRT → 再解析 CRT。`test_tls_config()` 只做 `mbedtls_ssl_setup`，**没有 TCP/BIO，不会真正握手**。

命令行编 NS：

```bash
cd tfmcubeideproject/STM32CubeIDE/ns_app
make -j$(nproc)
cd ../sign_kit
./sign.sh ../ns_app/out/tfm_ns.bin
```

产物烧内部 Bank2 `0x0C100000`，必须与同一轮 `tfm_s` / `s_veneers.o` 配套。

### 相对 `stm32H573P256-SPIFLASH` 改了什么（`stm32H573P256-SPIFLASH-bl2-public-key`）

Flash 布局、升级路径、签名算法与 `stm32H573P256-SPIFLASH` 相同。只改 **编 BL2 时 OTP 里 ROTPK 哈希从哪来**：

`./buildtfm.sh` 在 cmake 之前仍调用 `scripts/sync_stm_otp_rotpk.py`。S / NS **各自独立**：

| 条件 | 该侧 ROTPK 来源 |
|------|-----------------|
| 存在 `keys/image_s_signing_public_key.pem` | 用该 **公钥** 算哈希 → `bl2_rotpk_0` |
| 存在 `keys/image_ns_signing_public_key.pem` | 用该 **公钥** 算哈希 → `bl2_rotpk_1` |
| 某一侧没有对应公钥 pem | 该侧仍用原来的私钥：`root-EC-P256.pem` / `root-EC-P256_1.pem`（或 `MCUBOOT_KEY_S/NS`） |

公钥须是 `-----BEGIN PUBLIC KEY-----`（`imgtool getpub -e pem`）。可以只放 S、只放 NS，或两个都放。

未改本地签名：没有量产私钥时，编出来的 `*_signed.bin` 仍可能是 dummy 钥签的。未签名 `tfm_s.bin` / `tfm_ns.bin` 可交给签名服务器，服务器私钥必须与放进 `keys/` 的公钥成对。换 ROTPK 后须 **回归并重烧 BL2 + 已签名 S/NS**。

`scripts/sync_stm_otp_rotpk.py` 现同时接受公钥 pem 和私钥 pem（公钥直接哈希，私钥先抽出公钥再哈希；算法仍是 EC-P256 的 SHA-256 + SPKI DER）。

### 相对 `stm32h573p256` 改了什么（`stm32H573P256-SPIFLASH`）

内部 Flash 仍 2 MB（Bank1 `0x00000–0xFFFFF`，Bank2 `0x100000–0x1FFFFF`）。升级策略 **overwrite-only**。S/NS **下载槽**在 SPI1 外接 **W25Q32**（4 MB，非 XIP），从 `0x100000` 起：S 下载 **512 KB**，NS 下载 **1 MB**。MCUboot 要求同一镜像的主槽和下载槽等大，因此内部 S 执行槽也是 512 KB（镜像填充；BL2 地址不变）。**NS 执行槽放在整个 Bank2**，不再跨 1 MB 银行边界。

| 内容 | 位置 |
|------|------|
| S 执行 | 内部 Bank1 `0x0C044000`，512 KB（父分支为 `0x0C038000`；本分支 PS 64 KB 后移） |
| NS 执行 | 内部 Bank2 `0x0C100000` / `0x08100000`，1 MB |
| PS | 内部 `0x0C030000`，**64 KB** |
| ITS | 内部 `0x0C040000`，16 KB |
| Bank1 空隙 | `0x0C0C4000–0x0C0FFFFF`（240 KB，SECWM1 保持 Secure） |
| S 下载 | W25Q32 `0x100000`，512 KB（命令偏移，不是片上 Bank2） |
| NS 下载 | W25Q32 `0x180000`，1 MB |
| 引脚 | SCK=PA5, MISO=PA6, MOSI=PA7, CS=PB2 |

TrustZone 片上 Flash 的 S/NS 分界用 **FLASH SECWM**（H5 没有 GTZC-MPCWM 管内部 Flash）：

| 项 | 值 |
|----|----|
| SECWM1（Bank1） | STRT=0 END=127（整 bank Secure，含 S 后 288 KB） |
| SECWM2（Bank2） | STRT=127 END=0（整 bank NS） |
| SAU NS Flash | `0x08100000` … `0x081FFFFF`（随 `FLASH_AREA_1` / `FLASH_AREA_END_OFFSET`） |
| GTZC TZSC | SPI1 = NSEC+NPRIV；SRAM1 NS / SRAM2 S 不变 |

回归脚本会先把两 bank 写成全 Secure；DEV 模式下 BL2（`TFM_ENABLE_SET_OB`）按上面把 Bank2 改成全 NS。改布局后必须 **回归 + 重烧**。

外部窗口 **`0x100000-0x280000`**。BL2 / HDP / WRP 不变（scratch 48 KB 仍占位但不参与升级）。

升级路径：

1. NS 用 `w25q32_init` / `erase_4k` / `write` 把已签名的 `tfm_s_signed.bin`、`tfm_ns_signed.bin` 写到 W25Q32 `0x100000` / `0x180000`（先擦后写）。
2. 调用 `psa_fwu_request_reboot()` 或复位。BL2 **只读** NOR（不擦、不写外部 Flash）。
3. 签名正确、**版本不低于**当前内部主槽（`major.minor.revision`，不含 build），且哈希不同 → BL2 覆盖内部执行槽。
4. 签名错误、版本更低、或哈希与当前运行映像相同 → 不升级，继续从内部主槽启动。

签名时请抬版本（NS 默认 `0.0.0`，一直不改则版本相等，哈希不同仍会覆盖）。security counter 也不能比片上的小。

**PSA 查询 S 固件版本保留。** NS 用 `psa_fwu_query(FWU_COMPONENT_ID_SECURE)`（component `0`）读当前运行的 S 版本（BL2 写入共享区，不是去读 NOR）。NS 版本用 component `1`。`psa_fwu_start` / `write` / `install` 返回 `PSA_ERROR_NOT_SUPPORTED`。CubeProgrammer / `./flash_stm32h573.sh` 仍只烧内部 primary（BL2/S/NS）。

```c
psa_fwu_component_info_t info;
psa_status_t st = psa_fwu_query(FWU_COMPONENT_ID_SECURE, &info); /* 0 = S */
if (st == PSA_SUCCESS) {
    /* info.version = major.minor.patch[+build]，例如 2.3.0+0 */
}
```

### 从旧 CubeIDE 工程迁过来

把本分支编出来的 SPE 导出 **整份覆盖** 过去就行，`.cproject`、应用代码、链接脚本模板都不用改：

```text
trusted-firmware-m/build_s/api_ns/  →  tfmcubeideproject/STM32CubeIDE/spe/api_ns/
makefile 工程同理 →  tfmmakeproject/api_ns/
```

覆盖后重新编译。用下面几项确认这份 `api_ns` 是对的：

| 看哪里 | 对了是 |
|--------|--------|
| `flash_layout.h` | `FLASH_S_PARTITION_SIZE=0x80000`，`FLASH_NS_PARTITION_SIZE=0x100000` |
| `spe/out/appli_ns.pp.ld` 的 FLASH ORIGIN | `0x08100400` |
| 签完的 NS 大小、烧录地址 | **1 MB**，`0x0C100000`（旧值 `0x0C088000` 是错的） |
| `spe/api_ns/interface/lib/s_veneers.o` | 必须和板上 `tfm_s` **同一轮** SPE（只换 NS 会 NSC 跑飞） |
| `TFM_UPDATE.sh` / `TFM_BIN2HEX.sh` | `slot0=0xc044000`，`slot1=0xc100000` |

### 相对 `master` 改了什么（签名，继承自 `stm32h573p256`）

本支线相对 `master` **签名换成 EC-P256**。主要包括：

1. **TF-M BL2**：`stm32h573i_dk/config.cmake` 设 `MCUBOOT_SIGNATURE_TYPE=EC-P256`（公钥编进 BL2）
2. **TF-M SPE 签名**：默认密钥改为 `root-EC-P256.pem` / `root-EC-P256_1.pem`；`buildtfm.sh` 带 `SIG=` stamp 并 `-UMCUBOOT_KEY_S/NS`
3. **tf-m-tests**：NS 测试镜像随 SPE 导出的 `api_ns` 密钥签名（无需单独改测试仓密钥）
4. **makefile 工程**：`tfmmakeproject/api_ns/image_signing/keys/`（及 `sign_kit/keys/`）
5. **CubeIDE 工程**：`sign_kit/keys/` 与 `spe/api_ns/image_signing/keys/`；本分支 NS 含 Mbed TLS 4.1.1（`tfmcubeideproject/STM32CubeIDE/ns_app/mbedtls-4.1.1`），可解析 CSR、自行签发 CRT，布局与 SPE/BL2 相同
6. **独立签名工具 / 压缩包**：根目录 `sign_kit.zip`、`ns_make_project.zip` 内密钥与样例签名镜像
7. **Linux 一键烧录**：根目录 `./flash_stm32h573.sh`（回归 + 烧 BL2/S/NS；Windows 仍用 `windows-tfm-tools\tfm_update.bat`）

### 密钥文件名对应（同内容、不同路径）

本支线默认 dummy 密钥下，下面两对文件 **内容相同**，只是名字和用途不同；换密钥时必须整对一起换，不能只改一边。

| TF-M / BL2 路径（编 SPE / 编进 BL2） | `sign_kit` / `api_ns` 路径（签镜像） | 用途 |
|--------------------------------------|--------------------------------------|------|
| `bl2/ext/mcuboot/root-EC-P256.pem` | `image_s_signing_private_key.pem` | Secure（S）私钥 |
| `bl2/ext/mcuboot/root-EC-P256_1.pem` | `image_ns_signing_private_key.pem` | Non-Secure（NS）私钥 |

编 SPE 时 TF-M 会把 `root-EC-P256*.pem` 拷成 `build_s/api_ns/image_signing/keys/image_*_signing_private_key.pem`；各工程 `sign_kit/keys/` 里同名文件应与之一致。

切换算法或更换密钥后必须 **整片重烧 BL2 + S + NS**。

### S / NS 签名版本修改说明

MCUboot 镜像头里的 **version**（以及可选的 **security counter**）在签名时写入。升级时 BL2 会按版本 / 计数器决定是否接受新镜像；改版本后需重新签名再烧录对应槽位。

| 镜像 | 默认版本（本仓库常见配置） | 默认 security counter |
|------|---------------------------|------------------------|
| Secure（S） | `2.3.0`（随 `TFM_VERSION` / `MCUBOOT_IMAGE_VERSION_S`） | `1` |
| Non-Secure（NS） | `0.0.0` | `1` |

版本字符串格式：`major.minor.revision[+build]`，例如 `1.2.0`、`1.2.0+3`。

#### 1. SPE / `./buildtfm.sh` 编出来的已签名镜像

TF-M 默认在 `bl2/ext/mcuboot/mcuboot_default_config.cmake`：

- `MCUBOOT_IMAGE_VERSION_S` ← 默认 `${TFM_VERSION}`
- `MCUBOOT_IMAGE_VERSION_NS` ← 默认 `0.0.0`
- `MCUBOOT_SECURITY_COUNTER_S` / `_NS` ← 默认 `1`（也可设为 `auto`）

改法（任选其一）：

```bash
# 方式 A：cmake 缓存（清 build 后重编）
rm -rf trusted-firmware-m/build_s trusted-firmware-m/build_ns
# 在 buildtfm / 平台 cmake 中增加，或首次配置时传入：
#   -DMCUBOOT_IMAGE_VERSION_S=2.4.0
#   -DMCUBOOT_IMAGE_VERSION_NS=1.0.0
#   -DMCUBOOT_SECURITY_COUNTER_S=2
#   -DMCUBOOT_SECURITY_COUNTER_NS=2
./buildtfm.sh test
```

也可在平台 `config.cmake`（`stm32h573i_dk`）里 `set(MCUBOOT_IMAGE_VERSION_S ... CACHE STRING "" FORCE)` 固化。改完必须重编 SPE（及需要的 NS 测试），再烧 **S / NS**（若只改 NS 版本则重签重烧 NS 即可；S 同理）。

#### 2. 独立 `sign_kit`（CubeIDE post-build / 手动 `sign.sh`）

编辑对应工程下的 `sign_kit/config`（CubeIDE：`tfmcubeideproject/STM32CubeIDE/sign_kit/config`；根目录解压的 `sign_kit.zip` 同理）：

```text
MCUBOOT_IMAGE_VERSION_S=2.3.0
MCUBOOT_SECURITY_COUNTER_S=1
MCUBOOT_NS_IMAGE_MIN_VER=0.0.0+0

MCUBOOT_IMAGE_VERSION_NS=0.0.0
MCUBOOT_SECURITY_COUNTER_NS=1
MCUBOOT_S_IMAGE_MIN_VER=0.0.0+0
```

- 签 S 时用 `MCUBOOT_IMAGE_VERSION_S` + `MCUBOOT_SECURITY_COUNTER_S`
- 签 NS 时用 `MCUBOOT_IMAGE_VERSION_NS` + `MCUBOOT_SECURITY_COUNTER_NS`
- `*_IMAGE_MIN_VER` 是镜像依赖的对端最低版本（写入 dependency TLV），一般保持与板上已有镜像兼容，不要随意抬高

改完后重新执行 `./sign.sh` / `sign.bat`（或 CubeIDE 再编一次触发 post-build）。

#### 3. makefile 工程 `tfmmakeproject`

`Makefile` 里签名参数目前写死为 NS `--version 0.0.0`、`-s 1`（security counter）。改 NS 版本时改这两处后 `make` 重新生成 `out/tfm_ns_signed.bin`：

```makefile
--version 0.0.0 \
...
-s 1 \
-d "(0, 0.0.0+0)" \
```

`-d "(0, <S最低版本>)"` 表示 NS 镜像依赖的 S 镜像最低版本，须与板上 S 实际版本匹配。

#### 注意

- 仅抬高 version / security counter 做升级时：用**同一套**签名密钥签新镜像，烧到升级槽或按现有升级流程即可；**不必**因改版本而换密钥或重烧 BL2。
- 版本回退（比板上更低）在默认 MCUboot 策略下通常会被拒绝；需要回退请走你们自己的降级 / 确认流程，不要假设直接烧低版本一定能过。
- `sign_kit/config` 与 SPE cmake 里的版本应尽量一致，避免测试镜像和自签镜像版本语义混乱。

### 更换密钥（量产 / 自用）

> 推荐流程见上文「keys/、versions/ 与清编译」；本节保留细节说明。

算法必须仍是 **EC-P256**。

**推荐：只往仓库根目录 `keys/` 放四份固定文件名，编译时自动覆盖全库。**

| 固定文件名 | 含义 |
|------------|------|
| `keys/image_s_signing_private_key.pem` | Secure 私钥 |
| `keys/image_s_signing_public_key.pem` | Secure 公钥 |
| `keys/image_ns_signing_private_key.pem` | Non-Secure 私钥 |
| `keys/image_ns_signing_public_key.pem` | Non-Secure 公钥 |

```bash
imgtool keygen -k keys/image_s_signing_private_key.pem  -t ecdsa-p256
imgtool keygen -k keys/image_ns_signing_private_key.pem -t ecdsa-p256
imgtool getpub -k keys/image_s_signing_private_key.pem  -e pem > keys/image_s_signing_public_key.pem
imgtool getpub -k keys/image_ns_signing_private_key.pem -e pem > keys/image_ns_signing_public_key.pem

rm -rf trusted-firmware-m/build_s trusted-firmware-m/build_ns
./buildtfm.sh test
```

`./buildtfm.sh` 会：

1. 用 `keys/` 覆盖各工程里所有同名 `image_*_signing_*.pem`，以及 BL2 的 `root-EC-P256.pem` / `root-EC-P256_1.pem`
2. OTP ROTPK（`otp_rotpk_hashes.inc`）：某一侧若有 `keys/image_*_signing_public_key.pem` 则用该公钥哈希编进 BL2，否则仍用该侧私钥（`root-EC-P256*.pem`）计算
3. 某目标**目录不存在**只告警，**不中断编译**；`keys/` 为空则继续用仓库默认 dummy 密钥

`keys/*.pem` 已 gitignore，勿把量产私钥提交进仓库。说明见 `keys/README.md`。

换密钥后仍须 **回归擦片并重烧 BL2 + S + NS**（BL2/`bl2.hex` 带 OTP 公钥哈希）。只换 `sign_kit`、不重编不重烧 BL2，板上会出现 `magic=good` 后 `Image in the primary slot is not valid`。

**可选：不用 `keys/` 时**，仍可手动覆盖 `trusted-firmware-m/bl2/ext/mcuboot/root-EC-P256*.pem` 与各 `sign_kit` / `image_signing/keys` 下同名文件，或传 `MCUBOOT_KEY_S` / `MCUBOOT_KEY_NS`。


## keys/、versions/ 与清编译

### keys/（更换签名密钥）

**本支线为 EC-P256。** 把两对固定文件名放到仓库根目录 `keys/`，再 `./buildtfm.sh`：

```bash
imgtool keygen -k keys/image_s_signing_private_key.pem  -t ecdsa-p256
imgtool keygen -k keys/image_ns_signing_private_key.pem -t ecdsa-p256
imgtool getpub -k keys/image_s_signing_private_key.pem  -e pem > keys/image_s_signing_public_key.pem
imgtool getpub -k keys/image_ns_signing_private_key.pem -e pem > keys/image_ns_signing_public_key.pem
./buildtfm.sh test
```

编译会覆盖各工程同名 pem、BL2 的 `root-EC-P256*.pem`。OTP ROTPK 优先用 `keys/` 里已有的公钥 pem，没有公钥的一侧仍从私钥计算。换密钥后须回归擦片并重烧 **BL2 + S + NS**。详见 `keys/README.md`。

### versions/（S / NS 镜像版本）

编辑 `versions/config`（或 `image_s_version.txt` / `image_ns_version.txt`），`./buildtfm.sh` 会把版本与 security counter 写进签名镜像，并同步各 `sign_kit/config`。

```bash
# 改 versions/config 后
./buildtfm.sh test
imgtool verify trusted-firmware-m/build_s/bin/tfm_s_signed.bin   # 看 Image version
imgtool verify trusted-firmware-m/build_ns/bin/tfm_ns_signed.bin
```

详见 `versions/README.md`。改版本只需重编重烧对应槽位，不必换密钥。

### 清编译（不重新下载依赖）

不要手动 `rm -rf trusted-firmware-m/build_s`。`./buildtfm.sh` 默认先跑 `scripts/clean_tfm_build.sh`：只清编译产物，依赖缓存在 `trusted-firmware-m/.deps-cache/`。增量：`./buildtfm.sh test --no-clean`。


### 一键回归烧录（Linux）

仓库根目录 `./flash_stm32h573.sh`：先写 option bytes（含全片擦除），再烧 **BL2 + S + NS**。需已安装 `STM32_Programmer_CLI`，板子用 ST-Link。

```bash
git checkout stm32H573P256-SPIFLASH-bl2-public-key-mbedtls
./buildtfm.sh test          # 或 prod
./flash_stm32h573.sh        # 一键：回归 + 烧录
# ./flash_stm32h573.sh download     # 只烧，不擦片
# ./flash_stm32h573.sh regression   # 只回归
# ./flash_stm32h573.sh all <ST-LINK SN>
```

| 镜像 | 地址 | 默认文件 |
|------|------|----------|
| BL2（含 OTP 区） | `0x0C00E000`（`bl2.hex` 另含 `0x0C028000` OTP） | `…/api_ns/bin/bl2.hex`（优先）或 `bl2.bin` |
| S | `0x0C044000` | `…/api_ns/bin/tfm_s_signed.bin` |
| NS | `0x0C100000` | `trusted-firmware-m/build_ns/bin/tfm_ns_signed.bin` |

可用环境变量 `TFM_NS_BIN=` 指定其它已签名 NS。`BOOT_UBE=0xB4`（OEM-iRoT）。串口 **115200**。

若串口已是 `sig_type: EC-P256` 且 primary `magic=good`，仍报 `Image in the primary slot is not valid`：多半是 OTP 里 ROTPK 不对——请 `git pull` 后重新 `./buildtfm.sh test`（或使用已修补的 `bl2.hex`），再 `./flash_stm32h573.sh` 做一次回归+烧录。

Windows 一键：`windows-tfm-tools\tfm_update.bat`（会调 `regression.bat`）。预置镜像是 **S 512 KB + NS 1 MB**，脚本优先烧 `tfm_s_signed` 和 `tfm_ns_signed`。拼接的 `tfm_s_ns_signed` 没有 Bank1 空隙，NS 会落到错误偏移，不能单独当完整镜像烧。



## SPE Crypto 堆与 TLS 1.3 双 WSS（PSA -141）

NS 用 mbedTLS 4.x + PSA 走 TF-M Crypto 分区时，有两块独立的安全侧缓冲，都曾因 `-141`（`PSA_ERROR_INSUFFICIENT_MEMORY`）踩过：

| 宏 | 本平台值 | 用途 |
|----|----------|------|
| `CRYPTO_IOVEC_BUFFER_SIZE` | 20480 | Crypto IPC scratch；TLS 1.3 `psa_export_public_key()` |
| `CRYPTO_ENGINE_BUF_SIZE` | `0x8000`（32 KiB） | `mbedtls_mem_buf`，RSA 模幂 / 会话密钥 |

默认引擎堆 `0x3000` 只够空闲时做一次 RSA-4096 验签。Let's Encrypt 新根（Root YR / ISRG Root X1）是 4096 位，两路 WSS 并存时后建的那路会在 `psa_verify_hash()` 上失败；mbedTLS 把非 0 验签结果一律当成「证书不受信任」（`-0x2700`，flags 仅 `0x08`）。改这两项后必须 **重编并重烧 SPE**。定义在 `trusted-firmware-m/platform/ext/target/stm/stm32h573i_dk/config_tfm_target.h`。

## 文档


- [编译笔记索引](./编译笔记.txt) — 各工程笔记入口
- [TF-M 编译笔记](./tfm编译笔记.txt) — 编译环境、一键脚本、Flash 布局、烧录与 SPI 升级
- [SPE / BL2](./trusted-firmware-m/编译笔记.txt)
- [NS 回归测试](./tf-m-tests/编译笔记.txt)
- [makefile NS](./tfmmakeproject/编译笔记.txt)
- [CubeIDE NS 编译笔记](./tfmcubeideproject/编译笔记.txt)
- [CubeIDE NS + Mbed TLS / CSR](./tfmcubeideproject/STM32CubeIDE/README.md)
- 注意：如果编译不通过可以删除仓库根目录 `.venv` 后重新 `./buildtfm.sh`。

## 硬件平台

- 主控：STM32H573（Cortex-M33 + TrustZone）
- 调试器：ST-Link

## 代码提交 

- 执行命令: ./push_to_gitee.sh [提交说明]

- 增加非安全测试代码 nsdev.tar.xz ，在 ubuntu22.04 解压后执行make即可运行，这个工程不含硬件浮点计算。

- 增加 sign_kit.tar.xz 签名工具，只是用来对未加密固件进行签名使用。


- 增加 makefile 编译的非安全侧工程 tfmmakeproject ，可以使用make编译生成代码，正式版本关闭非安全侧测试，开启硬件浮点，使用内部晶振 PLL 240 MHZ

- 增加 tfmcubeideproject 非安全侧工程可以使用stm32cubeide开发，这是基于make工程 tfmmakeproject 移植而来。

- 本分支（`stm32H573P256-SPIFLASH-bl2-public-key-mbedtls`）相对 `stm32H573P256-SPIFLASH-bl2-public-key`：展开并删除 `tfmcubeideproject.7z`；CubeIDE NS 含 Mbed TLS 4.1.1（PSA 客户端）；`spe`/`sign_kit` 与当前 SPE/BL2 的 512 KB / 1 MB、`OVERWRITE_ONLY` 对齐；可生成/解析 PKCS#10 CSR，并可自行签发 X.509 证书。**PS 64 KB**，S 主槽 **`0x0C044000`**。密钥仍为 **EC-P256**（`master` 仍是 RSA-3072）。

- 增加 windows-tfm-tools 该工具是windows系统的使用的回归脚本和烧录工具。

- 本分支增加 Linux 一键回归烧录脚本 `flash_stm32h573.sh`（对应 Windows 的 `windows-tfm-tools\tfm_update.bat`）。

## 文件统计

3956 directories, 12622 files

