$SOURCE_DIR = Convert-Path .
$BUILD_DIR = "$SOURCE_DIR\build"
$TESTRESULTS_DIR = "$BUILD_DIR\testresults"
$DEPENDENCIES_DIR = "$SOURCE_DIR\dependencies"
$BUILDTYPE="RelWithDebInfo"

$env:PATHEXT = $env:PATHEXT + ";.PY"
$env:VCINSTALLDIR = "C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC"
$env:VS140COMNTOOLS = "C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\Common7\\Tools"

#if (-not (Test-Path $SOURCE_DIR/ci)) {
#    throw "must run inside repository root"
#}

if ($env:DVAULT_DEPENDENCIES -eq $null) {
    throw "must define DVAULT_DEPENDENCIES environment variable with dependencies ZIPs"
}

if (-not (Test-Path $env:DVAULT_DEPENDENCIES)) {
    throw "didn't find dependencies directory $env:DVAULT_DEPENDENCIES"
}

function Get-VisualStudioVariables {
    # Set environment variables for Visual Studio Command Prompt
    # Found this piece of craziness on StackOverflow:
    #   http://stackoverflow.com/questions/2124753/
    #   how-i-can-use-powershell-with-the-visual-studio-command-prompt/2124759#2124759
    #
    # Note(rmeusel): we require an x64 build, hence *.bat is called with 'amd64'

    if (Test-Path ${env:VCINSTALLDIR}/vcvarsall.bat) {
        Push-Location ${env:VCINSTALLDIR}
        cmd /c "vcvarsall.bat amd64&set"
        Pop-Location
    } elseif (Test-Path ${env:VS140COMNTOOLS}/vsvars32.bat) {
        Push-Location ${env:VS140COMNTOOLS}
        cmd /c "vsvars32.bat amd64&set"
        Pop-Location
    } else {
        throw "neither vcvarsall.bat nor vsvars32.bat found"
    }
}

function Initialize-VisualStudio {
    "Setup the environment for Visual Studio building"
    Get-VisualStudioVariables |
    foreach {
      if ($_ -match "=") {
        $v = $_.split("="); set-item -force -path "ENV:\$($v[0])"  -value "$($v[1])"
      }
    }
}
