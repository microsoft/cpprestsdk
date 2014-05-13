Here are the steps to follow to generate Casablanca's NuGet package. 
Most important is to make sure all the binaries, libs, header files, etc... laid out in structure specified in cpprestsdk.autopkg.

1. Make sure includes in repository are up to date matching for release.
2. Copy the signed dlls under the Main\Casablanca\Release\nuget\Binaries.
3. Copy the libs and pdbs into Main\Binaries.
4. Replace full symbols with stripped ones by running rename_stripped_pdbs.ps1 script.
5. Open and edit cpprestsdk.autopkg to update for new version, release notes, description, etc...
6. Update init.ps1 which refers contains the version.
7. Create new package by running:  
    
        Write-NuGetPackage .\cpprestsdk.autopkg -SplitThreshold 1000
        
8. Test generated NuGet package ***Important*** on ALL flavors, architectures, platforms(XP, store, phone, desktop), configurations, VS versions