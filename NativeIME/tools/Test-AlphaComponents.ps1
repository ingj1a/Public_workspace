$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Alpha-Components.ps1')
$good=@(Get-AlphaComponents | ForEach-Object { @{name=$_;sha256=('a'*64)} })
Assert-AlphaComponents $good
function Expect-Reject($items){
    $rejected=$false
    try { Assert-AlphaComponents $items } catch { $rejected=$true }
    if(-not $rejected){throw 'Malformed manifest was accepted.'}
}
Expect-Reject @($good | Select-Object -Skip 1)
Expect-Reject @($good + $good[0])
Expect-Reject @($good[0..5] + @{name='..\NativeIME.exe';sha256=('a'*64)})
Expect-Reject @($good[0..5] + @{name='NativeIME.TsfDesktopE2E.exe';sha256='invalid'})
Write-Host 'Manifest exact component list, missing, duplicate, traversal and invalid hash checks passed.'
