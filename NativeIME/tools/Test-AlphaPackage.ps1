[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageRoot,[switch]$Prepared)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Alpha-Components.ps1')
$root=(Resolve-Path -LiteralPath $PackageRoot).Path
$manifest=Get-Content -LiteralPath (Join-Path $root 'alpha-manifest.json') -Raw | ConvertFrom-Json
if($manifest.schema -ne 1 -or $manifest.build -notmatch '^[a-f0-9]{40}$'){throw 'Invalid Alpha package manifest.'}
Assert-AlphaComponents $manifest.firstParty
foreach($item in $manifest.firstParty){
    $path=Join-Path $root $item.name
    if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing first-party file: $($item.name)"}
    if((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Reparse point: $($item.name)"}
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.sha256){throw "First-party SHA256 mismatch: $($item.name)"}
}
$expectedSources=[ordered]@{
    'main.7z'='https://github.com/rime/librime/releases/download/1.13.1/rime-1c23358-Windows-msvc-x64.7z'
    'deps.7z'='https://github.com/rime/librime/releases/download/1.13.1/rime-deps-1c23358-Windows-msvc-x64.7z'
    'prelude.zip'='https://codeload.github.com/rime/rime-prelude/zip/082425ea0684bca36474415d4a0e8db9b016487e'
    'luna.zip'='https://codeload.github.com/rime/rime-luna-pinyin/zip/84685a7e7793e43278ca29368977b79d74d37a03'
    'stroke.zip'='https://codeload.github.com/rime/rime-stroke/zip/1e8fff9b9494ddec23b0cbc526bcfd8171a6fd48'
    'essay.zip'='https://codeload.github.com/rime/rime-essay/zip/d95eeae056df429ecf993038c325a216f0b28c5b'
}
if(@($manifest.sources).Count -ne $expectedSources.Count){throw 'Pinned source count mismatch.'}
$sourceSeen=@{}
foreach($item in $manifest.sources){
    if(-not $expectedSources.Contains($item.name) -or $expectedSources[$item.name] -cne $item.url -or
       $item.sha256 -notmatch '^[A-Fa-f0-9]{64}$' -or $sourceSeen.ContainsKey($item.name)){throw "Invalid pinned source: $($item.name)"}
    $sourceSeen[$item.name]=$true
}
foreach($required in @('Prepare-Alpha.ps1','Install-Alpha.ps1','Alpha-Components.ps1','Test-AlphaPackage.ps1','ALPHA-LAB.md','DESKTOP-ACCEPTANCE.md','DIAGNOSTICS.md')){
    if(-not (Test-Path -LiteralPath (Join-Path $root $required) -PathType Leaf)){throw "Missing package entry point or documentation: $required"}
}
$firstPartyNames=@($manifest.firstParty | ForEach-Object {$_.name})
$allExecutables=@(Get-ChildItem -LiteralPath $root -File -Recurse -Include '*.exe','*.dll' | ForEach-Object {$_.FullName.Substring($root.Length+1).Replace('\','/')})
if(-not $Prepared){
    if(Test-Path -LiteralPath (Join-Path $root 'prepared.json')){throw 'Unprepared package unexpectedly contains prepared.json.'}
    foreach($dir in @('runtime','shared','upstream-notices')){if(Test-Path -LiteralPath (Join-Path $root $dir)){throw "Unexpected prepared directory: $dir"}}
    if(@($allExecutables | Where-Object {$firstPartyNames -cnotcontains $_}).Count -gt 0){throw 'Unmanifested binary in Alpha package.'}
    if(@($allExecutables).Count -ne $firstPartyNames.Count){throw 'First-party binary count mismatch.'}
    Write-Host 'PASS: clean Alpha package, exact first-party inventory, pinned sources and SHA256 manifest.'
    return
}
$preparedManifest=Get-Content -LiteralPath (Join-Path $root 'prepared.json') -Raw | ConvertFrom-Json
if($preparedManifest.schema -ne 1 -or $preparedManifest.build -cne $manifest.build -or @($preparedManifest.files).Count -eq 0){throw 'Prepared package/build mismatch.'}
$seen=@{}
foreach($item in $prepared.files){
    $name=[string]$item.name
    $parts=@($name -split '/')
    if($name -notmatch '^(runtime|shared|upstream-notices)/[A-Za-z0-9_. /-]+$' -or
       $parts -contains '..' -or $parts -contains '.' -or
       @($parts | Where-Object {$_ -eq '' -or $_ -match '[. ]$'}).Count -gt 0 -or
       $seen.ContainsKey($name) -or $item.sha256 -notmatch '^[A-Fa-f0-9]{64}$'){throw "Unsafe or duplicate prepared path: $name"}
    $seen[$name]=$true
    $path=Join-Path $root ($name.Replace('/','\'))
    if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing prepared component: $name"}
    if((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Prepared reparse point: $name"}
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.sha256){throw "Prepared SHA256 mismatch: $name"}
}
foreach($dir in @('runtime','shared','upstream-notices')){
    if(-not (Test-Path -LiteralPath (Join-Path $root $dir) -PathType Container)){throw "Missing prepared directory: $dir"}
}
foreach($required in @('runtime/rime.dll','shared/luna_pinyin.schema.yaml','shared/essay.txt','shared/opencc')){
    $requiredPath=Join-Path $root ($required.Replace('/','\'))
    if(-not (Test-Path -LiteralPath $requiredPath)){throw "Missing prepared runtime/data: $required"}
}
$actual=@(foreach($dir in @('runtime','shared','upstream-notices')){
    Get-ChildItem -LiteralPath (Join-Path $root $dir) -File -Recurse | ForEach-Object {$_.FullName.Substring($root.Length+1).Replace('\','/')}
})
if($actual.Count -ne $seen.Count -or @($actual | Where-Object {-not $seen.ContainsKey($_)}).Count -gt 0){throw 'Prepared payload contains unmanifested files.'}
Write-Host 'PASS: prepared Alpha runtime, schema data, notices, exact inventory and SHA256 hashes.'
