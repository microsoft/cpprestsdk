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

# set VisualStudioVersion - this is set by vcvarsall.bat under VS 11 and is needed to correctly build
# on a machine with both Dev10 and Dev11 installed.
$Env:VisualStudioVersion = "11.0"

# set environment variable to select between Dev10 and Dev11 projects in the various dirs.proj
$Env:DevToolsVersion = "110"