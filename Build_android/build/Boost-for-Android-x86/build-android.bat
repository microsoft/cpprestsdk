@Echo Off

sh -c "echo MSYS found. Running ./build-android.sh"
If %ERRORLEVEL% EQU 0 GOTO MSYSOK
echo This script requires MSYS installed and path to its bin folder added to PATH variable
echo Read http://www.mingw.org/wiki/MSYS for more information
GOTO:EOF
:MSYSOK

sh -c "./build-android.sh %*"
