# TF-M 2.3.0 → 2.3.1 升级步骤

适用仓库：https://github.com/18902101360/stm32h5f4tfm  

NS CubeIDE / mbedtls / CSR **不用改代码**。槽位、升级协议、USART 与 2.3.0 相同。要换的是 **SPE（S）+ BL2**，以及 CubeIDE 里与 S 配套的 **`api_ns`**。

---

## 0. 先选对分支

| 板子 | 现在用的 2.3.0 | 升到的 2.3.1 |
|------|----------------|--------------|
| STM32H5F4 | `stm32h5f4p256-usart6-bl2-public-key-ps256-1MNS` | `stm32h5f4p256-usart6-bl2-public-key-ps256-1MNS-TFM2.3.1` |
| STM32H573 | `stm32H573P256-SPIFLASH-bl2-public-key-ps64` | `stm32H573P256-SPIFLASH-bl2-public-key-ps64-TFM2.3.1` |

不要用 `…-1MNS-test`（那是 panic 打印调试支线）。

---

## 1. 本机准备

1. 备份当前能烧、能跑的 2.3.0 目录（整份拷走即可）。
2. **删掉**准备用来编 2.3.1 的那份本地工程（连 `trusted-firmware-m/.deps-cache` 一起没掉），再重新 clone / checkout 上表 2.3.1 分支。  
   只 `git pull`、旧 `.deps-cache` 还在，可能仍用 TF-PSA-Crypto **1.1.0**。
3. `arm-none-eabi-gcc` 必须是 **Arm GNU 14.3.Rel1**。Ubuntu/MSYS 自带的 gcc 13 会被 TF-M 拒绝。
4. 另需：`cmake`、`ninja`、`python3` + venv。

```bash
git clone https://github.com/18902101360/stm32h5f4tfm.git
cd stm32h5f4tfm
# 只选一块板：
git checkout stm32h5f4p256-usart6-bl2-public-key-ps256-1MNS-TFM2.3.1
# 或
git checkout stm32H573P256-SPIFLASH-bl2-public-key-ps64-TFM2.3.1
```

Clone 若 HTTP/2 失败：改 HTTP/1.1，并加 `--depth 1`。

---

## 2. 编译 SPE / BL2（仓库根目录）

```bash
./buildtfm.sh prod    # 量产：S 不带测试分区
# 或
./buildtfm.sh test    # 要跑官方 NS 回归时用
```

成功应看到 `=== 编译完成`，并且：

- H5F4：`slot0=0xc078000` `slot1=0xc0f8000` `boot=0xc00e000`
- H573：`slot0=0xc044000` `slot1=0xc100000` `boot=0xc00e000`

产物目录：`trusted-firmware-m/build_s/api_ns/`  
（`bin/bl2.*`、`bin/tfm_s_signed.*`、`interface/lib/s_veneers.o`）

仓库里已带一份编好的 hex/bin，**仍建议自己再编一次**再烧，避免和本机密钥/`versions/` 不一致。

---

## 3. 覆盖 CubeIDE 的 `api_ns`

用 **第 2 步同一轮** 的导出覆盖：

**源：** `trusted-firmware-m/build_s/api_ns`  
**目标：** `tfmcubeideproject/STM32CubeIDE/spe/api_ns`  
（整目录覆盖。）

然后 **重编 NS 工程**。

不要动：

- `ns_app/`（应用、mbedtls-4.1.1）
- 槽位/密钥没变则 **`sign_kit/` 可以不换**

`s_veneers.o` 必须和即将烧进板的 `tfm_s` 同一轮。不要用 CubeIDE 这份 `api_ns` 里的 `TFM_UPDATE.sh` 当 H5F4 主烧录脚本。

---

## 4. 签名 NS（CubeIDE 仍走 sign_kit）

```text
tfmcubeideproject/STM32CubeIDE/sign_kit/sign.bat
```

或 Linux：`sign_kit/sign.sh` 指向编出的 `tfm_ns.bin`。

`versions/` 里 S 镜像版本默认仍是 **2.3.0 / counter 1**（产品计数，不是 TF-M 源码号）。要在升级策略里体现「新固件」，再改 `versions/` 后重编重签。

---

## 5. 烧录（必须成套）

至少烧：**BL2 + 已签名 S**。NS 建议用第 3～4 步新编的；这次 veneer 没变，旧 NS 多数能跑，但仍建议成套换，避免混烧。

### H5F4

- Linux：仓库根 `./flash_stm32h5f4.sh`（用 `build_s/api_ns`）
- Windows：`windows-tfm-tools\tfm_update.bat`（先把刚编的 `bl2` / `tfm_s_signed` / 可选 `tfm_ns_signed` 拷进该目录）

地址：

| 镜像 | 地址 |
|------|------|
| BL2 | `0x0C00E000` |
| S | `0x0C078000`（512 KB） |
| NS | `0x0C0F8000`（1 MB） |

串口 115200，USART6 PC6/PC7。正式版上电应有 **`H5F4BL2`**。  
INFO 里 `Bootloader chainload address offset: 0x78000` 表示跳到 S 槽（偏移，不是 S 只有 480KB）。  
不要单独烧拼接的 `tfm_s_ns_signed`（Bank1 空隙会对不齐）。

### H573

- Linux：`./flash_stm32h573.sh`
- Windows：`windows-tfm-tools\tfm_update.bat`

| 镜像 | 地址 |
|------|------|
| BL2 | `0x0C00E000` |
| S | `0x0C044000` |
| NS | `0x0C100000`（Bank2） |

外部 W25Q32 升级协议不变。

换过 ROTPK / `keys/` 必须 **回归 + 重烧 BL2**。

---

## 6. 上电检查

1. BL2 能起来（H5F4 看 `H5F4BL2`）。
2. 能 jump S：`chainload address offset` 对应该板 S 偏移（H5F4=`0x78000`，H573=`0x44000`）。
3. NS 应用起来；PSA / TLS / CSR 与 2.3.0 行为一致。
4. 若 NS HardFault / NSC 跑飞：多半是 **S 与 `s_veneers.o` 不是同一轮**，回到第 3、5 步。

---

## 7. 不用做的事

- 不要改 NS mbedtls、不要为 SHAKE / `psa_random_reseed` 等去改 CubeIDE。
- 不要为了 2.3.1 去改 `flash_layout.h`。
- 不要把 2.3.0 的 S 配 2.3.1 的 `api_ns`，或反过来（建议禁止混烧）。
- 量产密钥仍放 `keys/`，流程与 2.3.0 相同。

---

## 8. 最短路径（只升 H5F4、Windows）

1. 删本地旧目录，checkout `…-1MNS-TFM2.3.1`。
2. 装 GCC 14.3，`./buildtfm.sh prod`。
3. `build_s/api_ns` → 覆盖 CubeIDE `spe/api_ns`，重编 NS，`sign_kit` 签名。
4. 把 `bl2`、`tfm_s_signed`、`tfm_ns_signed` 拷进 `windows-tfm-tools`，跑 `tfm_update.bat`。
5. 串口确认 `H5F4BL2` 和 `0x78000` chainload。
