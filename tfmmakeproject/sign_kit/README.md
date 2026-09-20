# STM32H5F4 独立签名工具包（tfmmakeproject）

把未签名的 Secure / Non-Secure `.bin` 放进本目录（或给出路径），执行脚本即可。
必须和当前板上的 H5F4 BL2 / SPE 配套（`H5F4BL2` / `H5F4SWP2`）。

布局、密钥、imgtool 与本 makefile 工程 `api_ns` 导出一致：
NS 槽 1024 KB @ `0x0C0F8000`，S 槽 512 KB @ `0x0C078000`，`BL2_TRAILER_SIZE=0x3000`。

## 用法

Linux / macOS：

```bash
cd tfmmakeproject/sign_kit
./sign.sh tfm_ns.bin     # 非安全
./sign.sh sapp.bin       # 安全
```

也可以直接签 `out/` 里的未签名镜像（输出写在输入文件旁边）：

```bash
./sign.sh ../out/tfm_ns.bin
# -> ../out/tfm_ns_signed.bin
```

Windows（命令提示符或 PowerShell）：

```bat
cd tfmmakeproject\sign_kit
sign.bat tfm_ns.bin
sign.bat sapp.bin
```

也可以把 `.bin` 拖到 `sign.bat` 上。

第一次运行会在本目录创建 `.venv`（或复用上级 `.sign-venv`），不要用仓库根目录 TF-M 的 `.venv`（cryptography 版本经常对不上）。

文件名里带 `ns` 按非安全签；带 `sapp`、`tfm_s`、`_s.bin` 按安全签。看不出来时：

```bash
./sign.sh ns  app.bin
./sign.sh s   app.bin
```

## 烧录地址（STM32H5F4）

| 镜像 | 地址 | 签完大小 |
|---|---|---|
| `*_s_signed.bin` | `0x0C078000` | 512 KB |
| `*_ns_signed.bin` | `0x0C0F8000` | 1024 KB |

签完把 `tfm_ns_signed.bin` 放到 `windows-tfm-tools`，双击 `tfm_update.bat`。
Linux 也可用仓库根目录 `./flash_stm32h5f4.sh`。

升级下载（同一份 `*_signed.bin`，地址不同）：S `0x0C200000`，NS `0x0C280000`。不要用 H573 的 `0x0C118000` / `0x0C168000`。

本目录的密钥是 TF-M 开发用 dummy **EC-P256**（本分支 `stm32h5f4-p256`），和当前 SPE/BL2 配套。`stm32h5f4` 主线仍是 RSA-3072。量产请替换 `keys/` 并同步更新板上 ROTPK。
换过 SPE 后请拷新的 `layout/signing_layout_*.o`（来自 `trusted-firmware-m/build_s/api_ns/image_signing/layout_files`）。
