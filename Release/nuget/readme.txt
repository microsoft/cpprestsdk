Here are the steps to follow to generate C++ Rest SDK's NuGet package. 

1. Run the update.ps1 to copy the binaries and update the cpprestsdk.nuspec version number. (i.g. update.ps1 -OldVer 2_2 -NewVer 2_3)
2. Copy the signed dlls under the .\Binaries.
3. Update the cpprestsdk.nuspec metadata to correct version description. (i.e. change the "<version>2.2.0</version>" to "<version>2.3.0</version>")
4. Run rename_stripped_pdbs.ps1 script to replace full symbols with stripped ones. 
5. Update the init.ps1 to correct version. (e.g. change  cpprest2_2.natvis to cpprest2_3.natvis).
6. Update the cpprestsdk.targets with the correct version number. (change the "<CppRestSDKVersionMinor>2</CppRestSDKVersionMinor>" to "<CppRestSDKVersionMinor>3</CppRestSDKVersionMinor>")
7. Download the Nuget command utility, run the command "nuget.exe pack" under the directory where this cpprestsdk.nuspec created.
8. Test generated NuGet package ***Important*** on ALL flavors, architectures, platforms(XP, store, phone, desktop), configurations, VS versions