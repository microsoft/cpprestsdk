[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter(Mandatory=$true)]
    [String]$PublisherName
)

$ErrorActionPreference = "Stop"

$Paths = @(
    "$PSScriptRoot\BlackJack\BlackJack_UIClient\BlackJack_UIClient_TemporaryKey.pfx",
    "$PSScriptRoot\FacebookDemo\FacebookDemo_TemporaryKey.pfx",
    "$PSScriptRoot\OAuth2Live\OAuth2Live_TemporaryKey.pfx",
    "$PSScriptRoot\WindowsLiveAuth\WindowsLiveAuth_TemporaryKey.pfx"
)

$MakeCert = "C:\Program Files (x86)\Windows Kits\10\bin\x86\makecert.exe"
$Pvk2Pfx = "C:\Program Files (x86)\Windows Kits\10\bin\x86\pvk2pfx.exe"

pushd $PSScriptRoot

foreach($Path in $Paths)
{
    Remove-Item $Path -ErrorAction SilentlyContinue
    if ($PSCmdlet.ShouldProcess($PSScriptRoot, "MakeCert"))
    {
        & $MakeCert -n "CN=$PublisherName" -a sha512 -m 12 -r -h 0 -eku "1.3.6.1.5.5.7.3.3,1.3.6.1.4.1.311.10.3.13" /sv "Temp.pvk" "Temp.cer"
    }
    if ($PSCmdlet.ShouldProcess($Path, "Pvk2Pfx"))
    {
        & $Pvk2Pfx -pvk "Temp.pvk" -spc "Temp.cer" -pfx $Path
        Remove-Item "$PSScriptRoot\Temp.pvk"
        Remove-Item "$PSScriptRoot\Temp.cer"
    }
}
