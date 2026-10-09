$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & gcc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -I stubs -I ../SRC/StdPeriphDriver/inc -I ../APP/include test_usb_keyboard.c -o test_usb_keyboard.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
    & ./test_usb_keyboard.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
} finally {
    Pop-Location
}
