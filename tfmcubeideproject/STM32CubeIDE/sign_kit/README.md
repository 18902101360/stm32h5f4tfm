# STM32H573 独立签名工具包

把未签名的 Secure / Non-Secure `.bin` 放进本目录，执行脚本并给出文件名即可。

## 用法

Linux / macOS：

```bash
cd sign_kit
./sign.sh tfm_ns.bin     # 非安全
./sign.sh sapp.bin       # 安全
```

Windows（命令提示符或 PowerShell）：

```bat
cd sign_kit
sign.bat tfm_ns.bin
sign.bat sapp.bin
```

也可以把 `.bin` 拖到 `sign.bat` 上。

输出在本目录：`tfm_ns_signed.bin` / `sapp_signed.bin`。

文件名里带 `ns` 按非安全签；带 `sapp`、`tfm_s`、`_s.bin` 按安全签。看不出来时：

```bash
./sign.sh ns  app.bin
./sign.sh s   app.bin
```

```bat
sign.bat ns  app.bin
sign.bat s   app.bin
```

## 首次依赖

```bash
python3 -m pip install -r requirements.txt
```

Windows：

```bat
py -3 -m pip install -r requirements.txt
```

已有 `.venv` 时脚本会自动用它。

## 烧录地址（STM32H573I-DK）

| 镜像 | 地址 | 签完大小 |
|---|---|---|
| `*_s_signed.bin` | `0x0C044000` | 512 KB |
| `*_ns_signed.bin` | `0x0C100000` | 1024 KB |

`layout/signing_layout_{s,ns}.o` 决定 `--pad` 后的槽大小，必须和 SPE 的 `flash_layout.h` 一致。`config` 里 `MCUBOOT_UPGRADE_STRATEGY=OVERWRITE_ONLY`（与本分支 BL2 相同）。

本目录的密钥是 TF-M 开发用 dummy **EC-P256**（本分支 `stm32h573p256`），和当前 SPE/BL2 配套。`master` 仍是 RSA-3072。

`root-EC-P256.pem`（TF-M/BL2）与 `image_s_signing_private_key.pem`（本目录）是**同一把 S 私钥**；`root-EC-P256_1.pem` 与 `image_ns_signing_private_key.pem` 是同一把 NS 私钥。量产请成对替换并同步更新板上 ROTPK。
