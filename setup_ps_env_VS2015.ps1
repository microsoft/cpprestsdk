function Get-Batchfile ($file) {
    $cmd = "@echo off & `"$file`" & set"
    cmd /c $cmd | Foreach-Object {
        $p, $v = $_.split('=')
        Set-Item -path env:$p -value $v
    }
}

function VsVars32()
{
    $vs140comntools = (Get-ChildItem env:VS140COMNTOOLS).Value
    $batchFile = [System.IO.Path]::Combine($vs140comntools, "vsvars32.bat")
    Get-Batchfile $BatchFile
}

"Initializing Casablanca Powershell VS2015 Environment"

# get VS tools
VsVars32

$Env:VisualStudioVersion = "14.0"
$Env:DevToolsVersion = "140"
