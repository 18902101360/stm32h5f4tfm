STM32H5F4 非安全侧 makefile 工程。

编译
  在本目录执行 make，生成 out/tfm_ns.bin，并由 sign_kit 签出 out/tfm_ns_signed.bin。
  MCU：STM32H5F4xx，BL2_TRAILER_SIZE=0x3000。
  USART6 PC6/PC7，115200。

签名（sign_kit，与 CubeIDE 工程同类，槽位按 H5F4）
  make 已自动签名。只签已有的未签名 bin：
    Linux:    ./sign_kit/sign.sh out/tfm_ns.bin
              → out/tfm_ns_signed.bin
    Windows:  sign_kit\sign.bat out\tfm_ns.bin
    安全侧:   ./sign_kit/sign.sh s sapp.bin
  文件名带 ns 按非安全签；带 sapp / tfm_s 按安全签。
  第一次会建 sign_kit/.venv 或复用本目录 .sign-venv。
  不要用仓库根目录 TF-M 的 .venv（cryptography 对不上会报
  “Loaded python version: 50.0.0, shared object version: b'50.0.1'”）。
  详细：sign_kit/README.md

槽位（签完大小）
  S  当前运行  512 KB  @ 0x0C078000    升级槽 @ 0x0C200000
  NS 当前运行 1024 KB  @ 0x0C0F8000    升级槽 @ 0x0C280000
  不要用 H573 的 0x0C088000 / 0x0C118000 / 0x0C168000。

SPE 配套
  s_veneers.o 必须和板上的 SPE 一起重新导出
  （trusted-firmware-m/build_s/api_ns/interface/lib）。
  换过 SPE 后请拷新的 s_veneers.o、
  api_ns/image_signing/layout_files/signing_layout_*.o，
  以及 sign_kit/layout/signing_layout_*.o。

烧录
  把签好的 bin 放到 windows-tfm-tools 后双击 tfm_update.bat，
  或 Linux 用仓库根目录 ./flash_stm32h5f4.sh。
  本工程 api_ns 已去掉 TFM_UPDATE.sh / regression.sh / TFM_BIN2HEX.sh。
  从 SPE 重新拷 api_ns 时不要把这几个脚本拷进来。
