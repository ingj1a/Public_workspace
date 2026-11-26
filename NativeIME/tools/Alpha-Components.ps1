# Shared first-party manifest contract. This file has no side effects.
function Get-AlphaComponents {
    @('NativeIME.exe','NativeIME.TSF.dll','NativeIME.EngineService.exe',
      'NativeIME.EngineSupervisor.exe','NativeIME.Session.exe',
      'NativeIME.DesktopLab.exe','NativeIME.TsfDesktopE2E.exe')
}
function Assert-AlphaComponents($items) {
    $expected=@(Get-AlphaComponents)
    if (@($items).Count -ne $expected.Count) { throw 'Incomplete or extra first-party components.' }
    $seen=@{}
    foreach($item in $items){
        $name=[string]$item.name
        if ($expected -cnotcontains $name -or $seen.ContainsKey($name) -or $item.sha256 -notmatch '^[A-Fa-f0-9]{64}$') {
            throw "Invalid or duplicate first-party component: $name"
        }
        $seen[$name]=$true
    }
}
