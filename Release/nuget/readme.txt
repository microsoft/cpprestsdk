Here are the steps to follow to generate Casablanca's NuGet package:

1. Have all the binaries, libs, header files, etc... laid out in structure specified in cpprestsdk.autopkg.
2. Open and edit cpprestsdk.autopkg to update for new version, release notes, description, etc...
3. Update init.ps1 which refers contains the version.
3. Create new package by running:  
    
        PS C:\NativeCloud\Main\Tools\NuGet> Write-NuGetPackage .\cpprestsdk.autopkg -SplitThreshold 1000
        
4. Test generated NuGet package ***Important*** on ALL flavors, architectures, platforms(XP, store, phone, desktop), configurations, VS versions