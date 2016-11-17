set STINGRAY=C:\Work\stingray\build\binaries\engine\win64\dev\stingray_win64_dev.exe
set CORE=C:\work\stingray
%STINGRAY% --source-dir project --data-dir project_data\win32 --plugin-dir plugin\build\dlls --compile --continue --map-source-dir core %CORE%
