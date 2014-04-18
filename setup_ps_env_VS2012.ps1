function Get-Batchfile ($file) {
    $cmd = "`"$file`" & set"
    cmd /c $cmd | Foreach-Object {
        $p, $v = $_.split('=')
        Set-Item -path env:$p -value $v
    }
}

function VsVars32()
{
    $vs110comntools = (Get-ChildItem env:VS110COMNTOOLS).Value
    $batchFile = [System.IO.Path]::Combine($vs110comntools, "vsvars32.bat")
    Get-Batchfile $BatchFile
}

"Initializing Casablanca Powershell VS2012 Environment"

# get VS tools
VsVars32

$Env:VisualStudioVersion = "11.0"
$Env:DevToolsVersion = "110"
