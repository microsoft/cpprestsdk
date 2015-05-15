"Initializing C++ REST SDK Powershell VS2015 Environment"

# Add MSBuild to the path.
if (Test-Path "Env:ProgramFiles(x86)")
{
    $msbuildLocation = (Get-ChildItem "Env:ProgramFiles(x86)").Value
}
else
{
    $msbuildLocation = (Get-ChildItem "Env:ProgramFiles").Value
}

$msbuildLocation = [System.IO.Path]::Combine($msbuildLocation, "MSBuild", "14.0", "Bin")
$Env:Path += ";" + $msbuildLocation

$Env:VisualStudioVersion = "14.0"
$Env:DevToolsVersion = "140"
