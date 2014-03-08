param($installPath, $toolsPath, $package) 

function Copy-Natvis($DestFolder)
{
    if ((Test-Path $DestFolder) -eq $True)
    {
        # Update casablanca version for each release here.
        $DestFile = Join-Path -path $DestFolder -childpath "cpprest2_0.natvis";

        # Check to see if cpp rest natvis file for this version already exists
        # if not, then copy into user profile for Visual Studio to pick up
        if ((Test-Path $DestFile) -eq $False)
        {
            $SrcFile = Join-Path -path $toolsPath -childpath "cpprest.natvis";
            Copy-Item $SrcFile $DestFile;
        }
    }
}

$VS2012Folder = Join-Path -path $env:userprofile -childpath "Documents\Visual Studio 2012\Visualizers";
$VS2013Folder = Join-Path -path $env:userprofile -childpath "Documents\Visual Studio 2013\Visualizers";
Copy-Natvis $VS2012Folder;
Copy-Natvis $VS2013Folder;