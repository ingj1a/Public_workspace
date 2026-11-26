# Experimental installation/rollback. Use only on a disposable Windows VM.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Install','Remove')][string]$Action,
      [switch]$IsolatedVm)
$ErrorActionPreference='Stop'
if(-not $IsolatedVm){throw 'Explicit -IsolatedVm required.'}
if(-not [Environment]::Is64BitProcess){throw 'Use 64-bit Windows PowerShell.'}
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Run in an elevated 64-bit PowerShell in the disposable VM.'}
$root=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'NativeIME-AlphaLab'
$dll=Join-Path $root 'NativeIME.TSF.dll'
$marker=Join-Path $root 'alpha-owned.json'
$cls='{9D61A6D1-2C5E-4B6B-9C8A-7A8E1F2D1A10}'
$reg=Join-Path $env:SystemRoot 'System32\regsvr32.exe'
$machine="Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\$cls"
$user="Registry::HKEY_CURRENT_USER\SOFTWARE\Classes\CLSID\$cls"
$tipMachine="Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\CTF\TIP\$cls"
$tipUser="Registry::HKEY_CURRENT_USER\SOFTWARE\Microsoft\CTF\TIP\$cls"
function Existing { (Test-Path $machine) -or (Test-Path $user) -or (Test-Path $tipMachine) -or (Test-Path $tipUser) }
function RegisteredPath {
    $key=Get-Item -LiteralPath "$machine\InprocServer32" -ErrorAction SilentlyContinue
    if($key){return [string]$key.GetValue('')}
    return ''
}
function RunReg([bool]$remove) {
    $args=@('/s');if($remove){$args+='/u'};$args+=('"'+$dll+'"')
    $p=Start-Process -FilePath $reg -ArgumentList $args -Wait -PassThru
    if($p.ExitCode -ne 0){throw "regsvr32 returned $($p.ExitCode)."}
}
function Assert-NoReparse($path){
    if((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unexpected reparse point.'}
}
if($Action -eq 'Remove'){
    if(-not (Test-Path -LiteralPath $marker -PathType Leaf)){throw 'No owned Alpha installation marker; nothing will be removed.'}
    Assert-NoReparse $root; Assert-NoReparse $marker; Assert-NoReparse $dll
    $owned=Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
    if($owned.schema -ne 1 -or $owned.build -notmatch '^[a-f0-9]{40}$' -or (Get-FileHash -LiteralPath $dll).Hash -ne $owned.dllSha256){throw 'Ownership/integrity check failed.'}
    if(Test-Path $user){throw 'Conflicting per-user COM registration; no changes made.'}
    if((Existing) -and (RegisteredPath) -ine $dll){throw 'Existing input profile is not owned by this installation.'}
    if(Existing){RunReg $true}
    if(Existing){throw 'Registration still present; keep installation and investigate.'}
    # Do not erase locked DLLs, user dictionaries, reports or unrelated files.
    Write-Host 'NativeIME registration removed. Switch to another input method before removal and sign out/restart if Windows still holds the DLL.'
    Write-Host "Protected files retained at $root for manual cleanup after restart; user data not removed."
    return
}
if((Test-Path -LiteralPath $root) -or (Existing)){throw 'Existing Alpha folder or NativeIME registration detected. Refusing to overwrite.'}
$src=$PSScriptRoot
$m=Get-Content -LiteralPath (Join-Path $src 'alpha-manifest.json') -Raw | ConvertFrom-Json
$p=Get-Content -LiteralPath (Join-Path $src 'prepared.json') -Raw | ConvertFrom-Json
if($m.schema -ne 1 -or $p.schema -ne 1 -or $m.build -ne $p.build -or $m.build -notmatch '^[a-f0-9]{40}$'){throw 'Prepared package/build mismatch.'}
function CopyChecked($item,[bool]$first){
    $name=[string]$item.name
    if($first){
        if($name -notmatch '^NativeIME[.A-Za-z0-9-]+\.(exe|dll)$'){throw 'Unexpected binary name.'}
    }else{
        if($name -notmatch '^(runtime|shared|upstream-notices)/[A-Za-z0-9_. /-]+$' -or ($name -split '/') -contains '..' -or ($name -split '/') -contains '.'){throw 'Unexpected prepared file name.'}
    }
    if($item.sha256 -notmatch '^[A-Fa-f0-9]{64}$'){throw 'Invalid SHA256.'}
    $from=Join-Path $src $name
    Assert-NoReparse $from
    if((Get-FileHash -LiteralPath $from).Hash -ne $item.sha256){throw "Source hash mismatch: $name"}
    $to=Join-Path $root $name
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $to) | Out-Null
    Copy-Item -LiteralPath $from -Destination $to
    if((Get-FileHash -LiteralPath $to).Hash -ne $item.sha256){throw "Staged hash mismatch: $name"}
}
$attempted=$false
try{
    New-Item -ItemType Directory -Path $root | Out-Null
    foreach($item in $m.firstParty){CopyChecked $item $true}
    foreach($item in $p.files){CopyChecked $item $false}
    if(-not (Test-Path -LiteralPath $dll) -or -not (Test-Path -LiteralPath "$root\runtime\rime.dll") -or -not (Test-Path -LiteralPath "$root\NativeIME.exe")){throw 'Required product components missing.'}
    @{schema=1;build=$m.build;dllSha256=(Get-FileHash -LiteralPath $dll).Hash} | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding UTF8
    # The preflight established no existing NativeIME registration.
    $attempted=$true;RunReg $false
    if((RegisteredPath) -ine $dll){throw 'COM registration ownership verification failed.'}
    Write-Host "Alpha registered: $root\NativeIME.exe"
    Write-Host 'No default IME, login startup or input language change. Manually enable/select NativeIME in Windows only when testing.'
}catch{
    if($attempted){
        try{RunReg $true}catch{Write-Warning 'Rollback failed; keep protected files and inspect registration.'}
    }
    if(-not (Existing) -and (Test-Path -LiteralPath $root)){Remove-Item -LiteralPath $root -Recurse -Force}
    throw
}
