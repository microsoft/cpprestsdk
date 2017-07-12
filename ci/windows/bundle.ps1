# prepare bundle
if (Test-Path -Path usr) {
  Remove-Item -Path usr -Force -Recurse
}
New-Item -ItemType  Directory -Force -Path usr/local/lib
New-Item -ItemType  Directory -Force -Path usr/local/include
Copy-Item Binaries/x64/Release/* usr/local/lib/ -Force -Recurse
Copy-Item Release/include usr/local/ -Force -Recurse

# compress
Compress-Archive -Path usr -DestinationPath cpprestsdk.zip
