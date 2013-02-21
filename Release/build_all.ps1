 param (
    [string]$target = "build"
 )

$script:startTime = get-date

function GetElapsedTime() {
    $runtime = $(get-date) - $script:StartTime
    $retStr = [string]::format("{0} hours, {1} minutes, {2}.{3} seconds", `
        ($runtime.Days * 24 + $runtime.Hours), $runtime.Minutes, $runtime.Seconds, $runtime.Milliseconds)
    $retStr
    }

$config_params = @(("Win32", "Release"),
       ("Win32", "Debug"),
       ("x64", "Release"),
       ("x64", "Debug"),
       ("arm", "Release"),
       ("arm", "Debug"))

if ((Test-Path "dirs.proj") -eq $false)
{
    Write-Host ("Error: cannot find dirs.proj in the current directory") -foregroundcolor red
}
else {

    foreach($param in $config_params)
    {
        $plat = $param[0]
        $config = $param[1]
        Write-Host ("msbuild dirs.proj /p:Platform=$plat /p:Configuration=$config /t:$target") -foregroundcolor Cyan
        msbuild dirs.proj /p:Platform=$plat /p:Configuration=$config /t:$target
        if($LastExitCode -ne 0)
        {
            Write-Host ("Error: failure building $plat $config") -foregroundcolor red
            break
        }
    }
}
Write-Host "Elapsed Time: $(GetElapsedTime)"
