@echo off
rem This file MUST use Windows CRLF line endings (LF-only breaks cmd.exe).
echo === Starting signing process ===
rem Standalone MCUboot signer for TF-M Secure / Non-Secure binaries (STM32H5F4).
rem Drop the unsigned .bin into this folder and run:
rem   sign.bat tfm_ns.bin
rem   sign.bat sapp.bin
rem SPDX-License-Identifier: BSD-3-Clause

setlocal EnableExtensions EnableDelayedExpansion

set "KIT=%~dp0"
if "%KIT:~-1%"=="\" set "KIT=%KIT:~0,-1%"

call :load_config
if errorlevel 1 exit /b 1

set "KIND="
set "IN_NAME="

if "%~1"=="" goto :usage_fail

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="-h" goto :usage_ok
if /I "%~1"=="--help" goto :usage_ok
if /I "%~1"=="/?" goto :usage_ok
if /I "%~1"=="ns" set "KIND=ns" & shift & goto :parse_args
if /I "%~1"=="nspe" set "KIND=ns" & shift & goto :parse_args
if /I "%~1"=="s" set "KIND=s" & shift & goto :parse_args
if /I "%~1"=="sapp" set "KIND=s" & shift & goto :parse_args
if /I "%~1"=="spe" set "KIND=s" & shift & goto :parse_args
set "IN_NAME=%~1"
shift
if not "%~1"=="" (
    echo 错误: 多余参数 %1
    exit /b 2
)

:args_done
if "%IN_NAME%"=="" goto :usage_fail

set "IN_BIN="
if exist "%IN_NAME%" (
    for %%I in ("%IN_NAME%") do set "IN_BIN=%%~fI"
) else if exist "%KIT%\%IN_NAME%" (
    set "IN_BIN=%KIT%\%IN_NAME%"
) else (
    echo 错误: 找不到文件: %IN_NAME%
    echo 请放到 "%KIT%" 或给出完整路径。
    exit /b 1
)

for %%I in ("%IN_BIN%") do (
    set "BASE=%%~nxI"
    set "STEM=%%~nI"
    set "IN_DIR=%%~dpI"
)
if "%IN_DIR:~-1%"=="\" set "IN_DIR=%IN_DIR:~0,-1%"

if not "%KIND%"=="" goto :kind_ready
echo %BASE%| findstr /I /C:"ns" >nul
if not errorlevel 1 (
    set "KIND=ns"
    goto :kind_ready
)
echo %BASE%| findstr /I /C:"sapp" /C:"tfm_s" /C:"_s.bin" /C:"_s_" >nul
if not errorlevel 1 (
    set "KIND=s"
    goto :kind_ready
)
echo 错误: 无法从文件名判断是 NS 还是 S
echo 请用:  sign.bat ns %BASE%
echo 或:    sign.bat s %BASE%
exit /b 1

:kind_ready
set "OUT_BIN=%IN_DIR%\%STEM%_signed.bin"

if /I "%KIND%"=="ns" (
    set "LAYOUT=%KIT%\layout\signing_layout_ns.o"
    set "KEY=%KIT%\keys\image_ns_signing_private_key.pem"
    set "VERSION=%MCUBOOT_IMAGE_VERSION_NS%"
    set "SEC_CNT=%MCUBOOT_SECURITY_COUNTER_NS%"
    set "DEP=(0, %MCUBOOT_S_IMAGE_MIN_VER%)"
    set "SLOT_HINT=NS  1200KB @ 0x0C0D0000"
    set "KIND_UP=NS"
) else (
    set "LAYOUT=%KIT%\layout\signing_layout_s.o"
    set "KEY=%KIT%\keys\image_s_signing_private_key.pem"
    set "VERSION=%MCUBOOT_IMAGE_VERSION_S%"
    set "SEC_CNT=%MCUBOOT_SECURITY_COUNTER_S%"
    set "DEP=(1, %MCUBOOT_NS_IMAGE_MIN_VER%)"
    set "SLOT_HINT=S   352KB @ 0x0C078000"
    set "KIND_UP=S"
)

if not exist "%LAYOUT%" echo 错误: 缺少签名文件: %LAYOUT% & exit /b 1
if not exist "%KEY%" echo 错误: 缺少签名文件: %KEY% & exit /b 1
if not exist "%KIT%\scripts\wrapper.py" echo 错误: 缺少签名文件: %KIT%\scripts\wrapper.py & exit /b 1
if not exist "%KIT%\bl2\macro_parser.py" echo 错误: 缺少签名文件: %KIT%\bl2\macro_parser.py & exit /b 1

call :find_python
if errorlevel 1 exit /b 1

"%PY%" %PY_EXTRA% -c "import click, cryptography, cbor2, intelhex" 2>nul
if errorlevel 1 (
    call :make_venv
    if errorlevel 1 exit /b 1
)

if /I "%MCUBOOT_HW_KEY%"=="ON" (set "PUB_FMT=full") else (set "PUB_FMT=hash")

set "EXTRA="
if /I "%MCUBOOT_UPGRADE_STRATEGY%"=="OVERWRITE_ONLY" set "EXTRA=!EXTRA! --overwrite-only"
if /I "%MCUBOOT_CONFIRM_IMAGE%"=="ON" set "EXTRA=!EXTRA! --confirm"
if /I "%MCUBOOT_MEASURED_BOOT%"=="ON" set "EXTRA=!EXTRA! --measured-boot-record"
if /I "%MCUBOOT_ENC_IMAGES%"=="ON" (
    set "ENC_KEY=%KIT%\keys\image_enc_%KIND%_key.pem"
    if not exist "!ENC_KEY!" (
        echo 错误: 已打开加密但找不到密钥: !ENC_KEY!
        exit /b 1
    )
    set "EXTRA=!EXTRA! -E "!ENC_KEY!""
)

echo 签名 %KIND_UP% 镜像  (%SLOT_HINT%)
echo   输入  %IN_BIN%
echo   输出  %OUT_BIN%
echo   版本  %VERSION%  header %BL2_HEADER_SIZE%  align %MCUBOOT_ALIGN_VAL%  计数器 %SEC_CNT%
echo   python %PY% %PY_EXTRA%

rem cwd 必须是 scripts\，wrapper.py 才会优先用自带的 imgtool
pushd "%KIT%\scripts" || exit /b 1
set "PYTHONPATH=%KIT%;%PYTHONPATH%"
"%PY%" %PY_EXTRA% "%KIT%\scripts\wrapper.py" ^
    --version "%VERSION%" ^
    --layout "%LAYOUT%" ^
    --key "%KEY%" ^
    --public-key-format "%PUB_FMT%" ^
    --align "%MCUBOOT_ALIGN_VAL%" ^
    --pad ^
    --pad-header ^
    -H "%BL2_HEADER_SIZE%" ^
    -s "%SEC_CNT%" ^
    -L "%MCUBOOT_ENC_KEY_LEN%" ^
    -d "%DEP%" ^
    %EXTRA% ^
    "%IN_BIN%" ^
    "%OUT_BIN%"
set "ERR=%ERRORLEVEL%"
popd
if not "%ERR%"=="0" (
    echo 错误: 签名失败，退出码 %ERR%
    exit /b %ERR%
)

echo 完成: %OUT_BIN%
exit /b 0

:usage_ok
call :usage
exit /b 0

:usage_fail
call :usage
exit /b 2

:usage
echo 把未签名的 S / NS 固件放到本目录，执行：
echo.
echo   sign.bat 文件名
echo.
echo 示例：
echo   sign.bat tfm_ns.bin          非安全
echo   sign.bat ns.bin
echo   sign.bat sapp.bin            安全
echo   sign.bat tfm_s.bin
echo.
echo 文件名看不出类型时，显式指定：
echo   sign.bat ns  app.bin
echo   sign.bat s   app.bin
echo.
echo 输出：输入文件所在目录下的 文件名_signed.bin
exit /b 0

:load_config
if not exist "%KIT%\config" (
    echo 错误: 找不到配置文件: %KIT%\config
    exit /b 1
)
for /f "usebackq eol=# tokens=1,* delims==" %%A in ("%KIT%\config") do (
    if not "%%A"=="" set "%%A=%%B"
)
exit /b 0

:find_python
set "PY_EXTRA="
if defined PYTHON (
    set "PY=%PYTHON%"
    if exist "%PYTHON%" goto :python_ok
    %PYTHON% -c "import sys" 2>nul
    if not errorlevel 1 goto :python_ok
)
if exist "%KIT%\.venv\Scripts\python.exe" (
    set "PY=%KIT%\.venv\Scripts\python.exe"
    goto :python_ok
)
if exist "%KIT%\..\.sign-venv\Scripts\python.exe" (
    set "PY=%KIT%\..\.sign-venv\Scripts\python.exe"
    goto :python_ok
)
py -3 -c "import sys" 2>nul
if not errorlevel 1 (
    set "PY=py"
    set "PY_EXTRA=-3"
    goto :python_ok
)
python -c "import sys" 2>nul
if not errorlevel 1 (
    set "PY=python"
    goto :python_ok
)
python3 -c "import sys" 2>nul
if not errorlevel 1 (
    set "PY=python3"
    goto :python_ok
)
echo 错误: 找不到 Python。请安装 Python 3 并勾选 Add python.exe to PATH。
exit /b 1

:python_ok
exit /b 0

:make_venv
echo imgtool: creating %KIT%\.venv
py -3 -m venv "%KIT%\.venv" 2>nul
if errorlevel 1 python -m venv "%KIT%\.venv"
if errorlevel 1 (
    echo 错误: 无法创建 %KIT%\.venv
    exit /b 1
)
set "PY=%KIT%\.venv\Scripts\python.exe"
set "PY_EXTRA="
"%PY%" -m pip install -q -r "%KIT%\requirements.txt"
if errorlevel 1 (
    echo 错误: 缺少 Python 依赖。先执行:
    echo   "%PY%" -m pip install -r "%KIT%\requirements.txt"
    exit /b 1
)
"%PY%" -c "import click, cryptography, cbor2, intelhex" 2>nul
if errorlevel 1 (
    echo 错误: 签名依赖安装后仍无法 import click/cryptography/cbor2/intelhex
    exit /b 1
)
exit /b 0
