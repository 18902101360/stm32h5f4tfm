STM32H5F4 非安全侧 STM32CubeIDE 工程。

在 STM32CubeIDE 里打开 STM32CubeIDE/ 下的 tfmminiproject（工程名不变）。
MCU：STM32H5F4ZJTx，宏 STM32H5F4xx，BL2_TRAILER_SIZE=0x3000。
板子选 genericBoard。若 CubeIDE 器件库还没有 H5F4，GCC 仍可按现有工程编译。

编译生成 Debug/tfm_ns.bin（或 make 的 build/tfm_ns.bin）。
CubeIDE post-build 会调用 sign_kit\sign.bat，签完写在 sign_kit\tfm_ns_signed.bin（不在 Debug\）。
Linux 命令行：在 STM32CubeIDE/ 下执行 make，再 ./sign_kit/sign.sh build/tfm_ns.bin
（CubeIDE 这份 sign_kit 同样把 tfm_ns_signed.bin 写到 sign_kit/ 目录）。

第一次签名会自动建 sign_kit/.venv，不要用仓库根目录 TF-M 的 .venv。
Windows 的 sign.bat 必须是 CRLF；若 post-build 报 1KIT:~0,-1 或 et 不是内部命令，先 git pull 更新 sign_kit。
详细：STM32CubeIDE/sign_kit/README.md

烧录：把 sign_kit/tfm_ns_signed.bin 放到 windows-tfm-tools 后双击 tfm_update.bat，或 Linux 用仓库根目录 ./flash_stm32h5f4.sh。
本工程 spe/api_ns 已去掉 TFM_UPDATE.sh / regression.sh / TFM_BIN2HEX.sh。从 SPE 重新拷 api_ns 时不要把这几个脚本拷进来。

s_veneers.o 必须和板上的 SPE 一起重新导出（trusted-firmware-m/build_s/api_ns/interface/lib）。换过 SPE 后请拷新的 s_veneers.o 和 sign_kit/layout/signing_layout_*.o。

NS 槽 1024 KB @ 0x0C0F8000（升级 0x0C280000）；S 槽 512 KB @ 0x0C078000（升级 0x0C200000）。USART6 PC6/PC7，115200。

工程已带 mbedtls 4.1.1（ns_app/mbedtls-4.1.1）：编 TLS/X.509 辅助模块，不编第二套 PSA crypto core。
密码仍走 SPE 的 PSA（s_veneers.o）。配置见 ns_app/ns_crypto_user.h、ns_mbedtls_user.h。
已打开 PKCS#10 CSR 解析/生成和自行签发证书：ns_crypto_user.h 打开 MBEDTLS_PK_WRITE_C / MBEDTLS_PEM_WRITE_C，
Makefile 与 .cproject 编入 x509_create.c、x509_csr.c、x509write_*.c、pkwrite.c；core/ 仍只编 psa_util.c。
main.c 的 test_csr() 用 SPE 里的 P-256 冒烟（写 CSR → 解析 → 签发 CRT）。
不要把 net_sockets.c、ssl_*_server.c、builtin aes/ecp 等排除文件加回源文件列表，除非同步改配置。
core/ 只编 psa_util.c（ECDSA raw/DER），不要把 PSA crypto core 的其它 .c 加进来。

NS 侧是 TLS 1.3 客户端：mbedtls_ssl_set_bio() 接到 ns_mbedtls_bio.c（send/recv）。
没有 POSIX mbedtls_net_*；有真实 TCP 时替换这两个回调即可。
动态内存走 mbedtls 自带的 memory_buffer_alloc（MBEDTLS_MEMORY_BUFFER_ALLOC_C），
不经过 newlib malloc/_sbrk。ssl_setup 前调用 mbedtls_memory_buffer_alloc_init()。

串口日志里搜 `TLS 1.3 + bio`。无对端时握手应停在 WANT_READ（ClientHello 已从 bio send 打出）。
