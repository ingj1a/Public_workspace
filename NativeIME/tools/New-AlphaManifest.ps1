# CI only: pin exact bytes of the upstream archives and first-party candidate.
param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $PackageRoot).Path
$sources=[ordered]@{
 'main.7z'='https://github.com/rime/librime/releases/download/1.13.1/rime-1c23358-Windows-msvc-x64.7z'
 'deps.7z'='https://github.com/rime/librime/releases/download/1.13.1/rime-deps-1c23358-Windows-msvc-x64.7z'
 'prelude.zip'='https://codeload.github.com/rime/rime-prelude/zip/082425ea0684bca36474415d4a0e8db9b016487e'
 'luna.zip'='https://codeload.github.com/rime/rime-luna-pinyin/zip/84685a7e7793e43278ca29368977b79d74d37a03'
 'stroke.zip'='https://codeload.github.com/rime/rime-stroke/zip/1e8fff9b9494ddec23b0cbc526bcfd8171a6fd48'
 'essay.zip'='https://codeload.github.com/rime/rime-essay/zip/d95eeae056df429ecf993038c325a216f0b28c5b'
}
$tmp=Join-Path $env:RUNNER_TEMP ('alpha-pin-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null
try{
 $upstream=@(foreach($name in $sources.Keys){
    $path=Join-Path $tmp $name
    if($name -eq 'main.7z'){Copy-Item "$env:NATIVEIME_RIME_ROOT\rime-msvc-x64.7z" $path}
    elseif($name -eq 'deps.7z'){Copy-Item "$env:NATIVEIME_RIME_ROOT\rime-deps-msvc-x64.7z" $path}
    else{Invoke-WebRequest -UseBasicParsing -Uri $sources[$name] -OutFile $path}
    @{name=$name;url=$sources[$name];sha256=(Get-FileHash -LiteralPath $path).Hash}
 })
 $files=@(Get-ChildItem -LiteralPath $root -File | Where-Object {$_.Extension -in '.exe','.dll'} | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 @{schema=1;build=$env:GITHUB_SHA;firstParty=$files;sources=$upstream} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'alpha-manifest.json') -Encoding UTF8
}finally{Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue}
