$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
$generatedInput = Join-Path $PSScriptRoot 'hidkbd_input.generated.tmp'
try {
    & gcc -std=c99 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -I stubs -I ../SRC/StdPeriphDriver/inc -I ../APP/include test_usb_keyboard.c -o test_usb_keyboard.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
    & ./test_usb_keyboard.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }

    # Compile the real input branch and key map, with only TMOS/transport mocked.
    # The BLE SDK cannot link into a host executable. Avoid copying its logic.
    $source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../APP/hidkbd.c'), [Text.Encoding]::GetEncoding(28591))
    $stateStart = $source.IndexOf('static uint8_t modifier_key_flag')
    $stateEnd = $source.IndexOf('static void    hidEmu_ProcessTMOSMsg', $stateStart)
    $branchStart = $source.IndexOf('    if(events & START_TOUCH_KEY_PROCESS_EVT)')
    $branchEnd = $source.IndexOf("`n    return 0;", $branchStart)
    if ($stateStart -lt 0 -or $stateEnd -lt 0 -or $branchStart -lt 0 -or $branchEnd -lt 0) {
        throw 'Cannot locate firmware input branch for regression test'
    }
    $inputCode = $source.Substring($stateStart, $stateEnd - $stateStart) + "`nstatic uint16_t process_input(uint16_t events) {`n" + $source.Substring($branchStart, $branchEnd - $branchStart) + "`nreturn 0;`n}`n"
    [IO.File]::WriteAllText($generatedInput, $inputCode, [Text.Encoding]::GetEncoding(28591))
    & gcc -std=c99 -Wall -Wextra -Werror -I ../APP/include test_keyboard_input.c -o test_keyboard_input.exe
    if ($LASTEXITCODE -ne 0) { throw 'Input test compilation failed' }
    & ./test_keyboard_input.exe
    if ($LASTEXITCODE -ne 0) { throw 'Input tests failed' }
} finally {
    if (Test-Path -LiteralPath $generatedInput) { Remove-Item -LiteralPath $generatedInput }
    Pop-Location
}
