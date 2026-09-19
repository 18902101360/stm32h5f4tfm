# STM32H5F4 独立签名工具包（EC-P256 / DEV-CCU）

把未签名的 Secure / Non-Secure `.bin` 放进本目录，执行脚本即可得到 MCUboot 签名镜像。

更完整的中文说明见同目录 **`使用说明.txt`**（含换密钥步骤）。

## 快速用法

```bash
cd sign_kit
python3 -m pip install -r requirements.txt   # 首次
./sign.sh tfm_ns.bin      # NS
./sign.sh sapp.bin        # S
```

Windows：

```bat
cd sign_kit
py -3 -m pip install -r requirements.txt
sign.bat tfm_ns.bin
sign.bat sapp.bin
```

也可把 `.bin` 拖到 `sign.bat` 上。

## 烧录地址（本分支布局）

| 镜像 | 主槽地址 | 签完槽大小 |
|------|----------|------------|
| `*_s_signed.bin` | `0x0C078000` | 352 KB |
| `*_ns_signed.bin` | `0x0C0D0000` | 1200 KB |

升级副槽：S `0x0C200000`，NS `0x0C258000`。不要用 H573 地址。

## 密钥

本包默认带 **DEV-CCU** EC-P256 公私钥（`keys/`）。须与板上 BL2 / OTP ROTPK 一致，否则会出现 `Image in the primary slot is not valid`。换钥见 `使用说明.txt`。
