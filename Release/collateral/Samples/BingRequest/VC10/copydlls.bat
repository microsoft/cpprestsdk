@setlocal
@SET CASABLANCA_SDK=
@for /F "tokens=1,2*" %%i in ('reg query "HKEY_LOCAL_MACHINE\Software\Microsoft\Casablanca\OpenSourceRelease\100\SDK" /v "InstallDir"') DO (
    if "%%i"=="InstallDir" (
        SET "CASABLANCA_SDK=%%k"
    )
)

@copy "%CASABLANCA_SDK%\bin\%1\casablanca100.dll" %2
@copy "%CASABLANCA_SDK%\bin\%1\casablanca100.pdb" %2
