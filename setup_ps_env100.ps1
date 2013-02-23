Param([string]$TfsWorkspace='')

function Get-Batchfile ($file) {
    $cmd = "`"$file`" & set"
    cmd /c $cmd | Foreach-Object {
        $p, $v = $_.split('=')
        Set-Item -path env:$p -value $v
    }
}

function VsVars32()
{
    $vs100comntools = (Get-ChildItem env:VS100COMNTOOLS).Value
    $batchFile = [System.IO.Path]::Combine($vs100comntools, "vsvars32.bat")
    Get-Batchfile $BatchFile
}

"Initializing Casablanca Powershell VS2010 Environment"

# get VS tools
"Calling vsvars32"
VsVars32

# set environment variable to select between Dev10 and Dev11 projects in the various dirs.proj
$Env:DevToolsVersion = "100"
