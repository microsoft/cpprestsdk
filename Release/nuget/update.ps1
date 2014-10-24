Param(
  [string]$OldVer,
  [string]$NewVer
)

Write-Host  "Copying Binaries folder"
Get-Item ..\..\Binaries\ | Copy-Item -Destination .\Binaries -rescure
Get-Item ..\include\ | Copy-Item -Destination .\include -rescure

Write-Host "Updating the cpprestsdk.nuspec version number"
(Get-Content .\cpprestsdk.nuspec) | ForEach-Object {$_ -replace $OldVer, $NewVer} | Set-Content .\cpprestsdk.nuspec