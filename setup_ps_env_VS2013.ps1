function Get-Batchfile ($file) {
    $cmd = "`"$file`" & set"
    cmd /c $cmd | Foreach-Object {
        $p, $v = $_.split('=')
        Set-Item -path env:$p -value $v
    }
}

function VsVars32()
{
    $vs120comntools = (Get-ChildItem env:VS120COMNTOOLS).Value
    $batchFile = [System.IO.Path]::Combine($vs120comntools, "vsvars32.bat")
    Get-Batchfile $BatchFile
}

"Initializing C++ REST SDK Powershell VS2013 Environment"

# get VS tools
VsVars32

$Env:VisualStudioVersion = "12.0"
$Env:DevToolsVersion = "120"
