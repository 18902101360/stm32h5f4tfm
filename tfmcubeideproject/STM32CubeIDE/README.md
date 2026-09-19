# STM32CubeIDE NS 工程（STM32H573I-DK + TF-M + Mbed TLS 4.1.1）

工程名：`tfmminiproject`。目录：`tfmcubeideproject/STM32CubeIDE/`。

所在分支：`stm32H573P256-SPIFLASH-bl2-public-key-mbedtls`（基于 `stm32H573P256-SPIFLASH-bl2-public-key`）。仓库总览见根目录 [`readme.md`](../../readme.md)。

相对父分支：展开并删除根目录 `tfmcubeideproject.7z`；用当前 SPE/`sign_kit` 覆盖 7z 里旧的 320/576 KB、SWAP 布局；NS 编入 Mbed TLS 4.1.1；打开 CSR 解析与自行签发证书；**PS 64 KB**，S 主槽 **`0x0C044000`**。

NS 应用在 `ns_app/`。SPE 导出在 `spe/`（只链接，不要当 NS 源码编译）。`spe/` 与 `sign_kit/` 必须与本分支当前 SPE/BL2 一致：**S 512 KB @ `0x0C044000` / NS 1 MB（Bank2）**，`OVERWRITE_ONLY`，**PS 64 KB**。

## Mbed TLS 4.1.1（PSA 客户端）

NS 侧跑 TLS 1.2 / TLS 1.3 握手状态机和 X.509 解析。AES / SHA / 随机数等密码学走 **TF-M Crypto 分区**（`psa_*` + `s_veneers.o`），不要在 NS 再编一套软件 AES/SHA。

| 组件 | 作用 |
|---|---|
| `ns_app/mbedtls-4.1.1/library/` | TLS / X.509 |
| `tf-psa-crypto` 的 ASN.1 / PK / PEM / platform | 证书格式，不是第二套 PSA core |
| `spe/api_ns/.../tfm_crypto_api.c` | 真正的 `psa_*` 经 veneer 进 SPE |

配置叠加：

- `ns_crypto_user.h`（`TF_PSA_CRYPTO_USER_CONFIG_FILE`）：打开 `MBEDTLS_PSA_CRYPTO_CLIENT` 和 PK/ASN.1/PEM，**不要**定义 `MBEDTLS_ECP_LIGHT`
- `ns_mbedtls_user.h`（`MBEDTLS_USER_CONFIG_FILE`）
- 编译宏：`MBEDTLS_ALLOW_PRIVATE_ACCESS`

头文件顺序必须是 **Mbed TLS 4.1.1 在前，`api_ns` 在后**，否则会用到 SPE 旧的 `mbedtls/pk.h`。

CSR / CRT：`ns_crypto_user.h` 打开 `MBEDTLS_PK_WRITE_C` / `MBEDTLS_PEM_WRITE_C`。编入 `x509_create.c`、`x509write.c`、`x509write_csr.c`、`x509_csr.c`（解析 CSR）、`x509write_crt.c`（签发证书）、`pkwrite.c`、`psa_util.c`。私钥不导出 NS，签名走 `mbedtls_pk_wrap_psa`。

| 场景 | API |
|---|---|
| 解析 PKCS#10 CSR（设备当 CA） | `mbedtls_x509_csr_parse` / `_parse_der` |
| 自行签发证书 | `mbedtls_x509write_crt_der` / `_pem` |
| 解析 CA 发回的证书 | `mbedtls_x509_crt_parse` / `_parse_der` |
| 本机生成 CSR 交给外部 CA | `mbedtls_x509write_csr_der` / `_pem` |

`test_csr()` 冒烟：SPE 生成 P-256 → 写 PKCS#10 → 解析 CSR → 用第二把 CA 密钥签发 CRT，再解析该 CRT。

### 不要编译

- `tf-psa-crypto/core/psa_crypto*.c`
- `drivers/builtin/src` 里的 `aes.c` / `sha*.c` / `ecp.c` / `gcm.c` / `rsa.c` / `cipher.c` 等（只保留 `psa_util_internal.c`）
- `library/net_sockets.c`、`timing.c`、server / DTLS

Makefile 已按上面裁源；CubeIDE `.cproject` 的 `sourceEntries` 排除项也对照过。命令行请用 Makefile。

## 命令行编译（与 CubeIDE 硬浮点一致）

```bash
cd tfmcubeideproject/STM32CubeIDE/ns_app
make -j$(nproc)
```

产物：`ns_app/out/tfm_ns.elf`、`tfm_ns.bin`。仍须用 `sign_kit` 签名后再烧录，**不要**直接烧未签名 ELF。

```bash
cd tfmcubeideproject/STM32CubeIDE/sign_kit
./sign.sh ../ns_app/out/tfm_ns.bin
```

## CubeIDE

1. 导入本目录工程 `tfmminiproject`
2. Debug / Release 已加 Mbed TLS 头路径、三个宏、以及 mbedtls 源码排除
3. 构建后走原来的 `sign_kit/sign.bat` post-build。若 Windows 上签名一步报 `1KIT:~0,-1"` / `'et' 不是内部或外部命令`，更新 `sign_kit/sign.bat` 后再编（GitHub zip 是 LF 换行，旧写法会被 `cmd` 拆行）。
4. 若 CubeIDE 仍去编译 `mbedtls-4.1.1` 下不该编的 `.c`，对照 `.cproject` 的 `sourceEntries` 排除项，或改用上面的 Makefile

浮点 ABI 必须与 SPE 一致：`fpv5-sp-d16` + hard float（`CONFIG_TFM_FLOAT_ABI=2`）。覆盖 SPE 后重新编译；`s_veneers.o` 必须和板上 `tfm_s` 同一轮。

## 板上测试

当前 `main.c` 的 `test_tls_config()` 只做 `mbedtls_ssl_config_defaults` + min TLS1.2 / max TLS1.3 + `mbedtls_ssl_setup`，**没有 TCP/BIO，不会真正握手**。`test_csr()` 会生成 CSR、解析 CSR，并用 SPE 里的 CA 密钥签发一张 CRT。

烧录（与本分支 SPE/BL2 配套，内部 Flash）：

| 镜像 | 地址 |
|---|---|
| `bl2.bin` | `0x0C00E000` |
| `tfm_s_signed.bin` | `0x0C044000`（512 KB） |
| `tfm_ns_signed.bin` | `0x0C100000`（1 MB，Bank2；旧值 `0x0C088000` 已作废） |

ST-Link VCP CN10：115200 8N1，**不要插 JP1**。UBE 保持 OEM-iRoT `0xB4`。

串口应看到 `Mbed TLS 4.1.1 (PSA client)` 以及 `mbedtls_ssl_setup` 的 PASS，然后仍是原来的 PSA ITS / FWU smoke。

## 多任务

本工程仍是 `tfm_ns_interface_bare_metal.c`。上 RTOS 前应改用官方 `tfm_ns_interface_rtos.c`，并实现 4 个 `os_wrapper_mutex_*` / `os_wrapper_is_kernel_started`。不要给 bare-metal 接口打补丁互斥锁。FreeRTOS 请设 `configENABLE_TRUSTZONE = 0`；每个任务独立 `mbedtls_ssl_context`；ISR 里不要调 PSA / Mbed TLS。

| 文件 | 下载地址 | 执行地址（向量表 / VTOR） | 谁跳进去 |
|------|----------|---------------------------|----------|
| `bl2.bin` | `0x0C00E000` | `0x0C010000` | 复位。Option Bytes SECBOOTADD = `0xC0100`（即 `0x0C010000 >> 8`） |
| `tfm_s_signed.bin` | `0x0C044000` | `0x0C044400` | BL2 验签通过后跳 SPE |
| `tfm_ns_signed.bin` | `0x0C100000`（安全别名，和 `0x08100000` 同一块 Flash） | `0x08100400` | SPE 切到 NS 后跳 NS 应用 |
