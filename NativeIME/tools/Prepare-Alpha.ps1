# NativeIME Alpha lab preparation. Run without elevation, in an isolated Windows VM.
[CmdletBinding()]
param([switch]$IsolatedVm, [string]$PackageRoot=$PSScriptRoot)
$ErrorActionPreference='Stop'
if (-not $IsolatedVm) { throw 'Explicit -IsolatedVm is required; never use this experimental package on a production PC.' }
if (-not [Environment]::Is64BitProcess) { throw 'Use 64-bit Windows PowerShell.' }
$root=(Resolve-Path -LiteralPath $PackageRoot).Path
$manifest=Get-Content -LiteralPath (Join-Path $root 'alpha-manifest.json') -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1 -or $manifest.build -notmatch '^[a-f0-9]{40}$') { throw 'Invalid build manifest.' }
function Assert-Hash($path,$hash) {
    if ($hash -notmatch '^[A-Fa-f0-9]{64}$' -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hash) { throw "Hash mismatch: $path" }
}
foreach($item in $manifest.firstParty) {
    if ($item.name -notmatch '^NativeIME[.A-Za-z0-9-]+\.(exe|dll)$') { throw 'Invalid first-party file name.' }
    Assert-Hash (Join-Path $root $item.name) $item.sha256
}
$allowed=[ordered]@{
    'main.7z'='https://github.com/rime/librime/releases/download/1.13.1/rime-1c23358-Windows-msvc-x64.7z'
    'deps.7z'='https://github.com/rime/librime/releases/download/1.13.1/rime-deps-1c23358-Windows-msvc-x64.7z'
    'prelude.zip'='https://codeload.github.com/rime/rime-prelude/zip/082425ea0684bca36474415d4a0e8db9b016487e'
    'luna.zip'='https://codeload.github.com/rime/rime-luna-pinyin/zip/84685a7e7793e43278ca29368977b79d74d37a03'
    'stroke.zip'='https://codeload.github.com/rime/rime-stroke/zip/1e8fff9b9494ddec23b0cbc526bcfd8171a6fd48'
    'essay.zip'='https://codeload.github.com/rime/rime-essay/zip/d95eeae056df429ecf993038c325a216f0b28c5b'
}
if (@($manifest.sources).Count -ne $allowed.Count) { throw 'Invalid source count.' }
$seen=@{}
foreach($item in $manifest.sources) {
    if (-not $allowed.Contains($item.name) -or $seen.ContainsKey($item.name) -or $allowed[$item.name] -cne $item.url) { throw 'Unexpected upstream source.' }
    $seen[$item.name]=$true
}
$tar=Join-Path $env:SystemRoot 'System32\tar.exe'
if (-not (Test-Path -LiteralPath $tar -PathType Leaf)) { throw 'Windows built-in tar.exe is required.' }
foreach($n in @('runtime','shared','upstream-notices','prepared.json')) {
    if (Test-Path -LiteralPath (Join-Path $root $n)) { throw 'Use a fresh extracted Alpha package; no existing data will be overwritten.' }
}
$tmp=Join-Path $root ('.prepare-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null
$moved=@()
try {
    foreach($item in $manifest.sources) {
        $archive=Join-Path $tmp $item.name
        Write-Host "Fetching pinned upstream $($item.name)"
        Invoke-WebRequest -UseBasicParsing -Uri $item.url -OutFile $archive
        Assert-Hash $archive $item.sha256
        $dest=Join-Path $tmp ([IO.Path]::GetFileNameWithoutExtension($item.name))
        New-Item -ItemType Directory -Path $dest | Out-Null
        # Verify the archive entry list before extracting a hash-pinned upstream archive.
        $entries=@(& $tar -tf $archive)
        if ($LASTEXITCODE -ne 0 -or $entries.Count -eq 0) { throw 'Archive listing failed.' }
        foreach($entry in $entries) {
            if ($entry -match '(^[/\\]|^[A-Za-z]:|(^|[/\\])\.\.([/\\]|$))') { throw 'Unsafe archive entry.' }
        }
        & $tar -xf $archive -C $dest
        if ($LASTEXITCODE -ne 0) { throw 'Archive extraction failed.' }
        if (Get-ChildItem -LiteralPath $dest -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint } | Select-Object -First 1) { throw 'Unexpected reparse point in archive.' }
    }
    $runtime=New-Item -ItemType Directory -Path (Join-Path $tmp 'runtime')
    $shared=New-Item -ItemType Directory -Path (Join-Path $tmp 'shared')
    $notices=New-Item -ItemType Directory -Path (Join-Path $tmp 'upstream-notices')
    foreach($dll in (Get-ChildItem -Path "$tmp\main","$tmp\deps" -Recurse -File -Filter '*.dll')) {
        $dest=Join-Path $runtime.FullName $dll.Name
        if (Test-Path -LiteralPath $dest) { Assert-Hash $dest (Get-FileHash -LiteralPath $dll.FullName).Hash }
        else { Copy-Item -LiteralPath $dll.FullName -Destination $dest }
    }
    foreach($name in @('prelude','luna','stroke','essay')) {
        $src=@(Get-ChildItem -LiteralPath (Join-Path $tmp $name) -Directory)
        if ($src.Count -ne 1) { throw "Unexpected source tree: $name" }
        foreach($file in (Get-ChildItem -LiteralPath $src[0].FullName -File | Where-Object { $_.Name -like '*.yaml' -or $_.Name -eq 'essay.txt' })) {
            $dest=Join-Path $shared.FullName $file.Name
            Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
        }
    }
    $opencc=Get-ChildItem -Path "$tmp\main","$tmp\deps" -Recurse -Directory -Filter 'opencc' | Select-Object -First 1
    if (-not $opencc) { throw 'OpenCC data missing.' }
    Copy-Item -LiteralPath $opencc.FullName -Destination (Join-Path $shared.FullName 'opencc') -Recurse
    foreach($name in @('main','deps','prelude','luna','stroke','essay')) {
        $dest=New-Item -ItemType Directory -Path (Join-Path $notices.FullName $name)
        $i=0
        Get-ChildItem -LiteralPath (Join-Path $tmp $name) -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE|COPYRIGHT|README)(\.|$)' } | ForEach-Object {
            $i++; Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $dest.FullName ("$i-"+$_.Name))
        }
    }
    if (-not (Test-Path "$($runtime.FullName)\rime.dll") -or -not (Test-Path "$($shared.FullName)\luna_pinyin.schema.yaml") -or -not (Test-Path "$($shared.FullName)\essay.txt")) { throw 'Required runtime or schema missing.' }
    $hashes=@(Get-ChildItem -Path $runtime.FullName,$shared.FullName,$notices.FullName -Recurse -File | ForEach-Object {
        [ordered]@{name=$_.FullName.Substring($tmp.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    foreach($name in @('runtime','shared','upstream-notices')) {
        Move-Item -LiteralPath (Join-Path $tmp $name) -Destination (Join-Path $root $name)
        $moved+=$name
    }
    @{schema=1;build=$manifest.build;files=$hashes} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'prepared.json') -Encoding UTF8
    Write-Host 'Preparation complete. No input profile or system configuration was changed.'
} catch {
    foreach($name in $moved) { Remove-Item -LiteralPath (Join-Path $root $name) -Recurse -Force }
    if(Test-Path -LiteralPath (Join-Path $root 'prepared.json')){Remove-Item -LiteralPath (Join-Path $root 'prepared.json') -Force}
    throw
} finally {
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
