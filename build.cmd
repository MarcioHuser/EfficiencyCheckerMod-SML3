:: @echo off

setlocal

pushd "%~dp0"

for %%1 in (*.uplugin) do set PROJECT=%%~n1

if exist ..\..\Saved\ArchivedPlugins\%PROJECT%\*.zip del /q ..\..\Saved\ArchivedPlugins\%PROJECT%\*.zip

"C:/Program Files/Unreal Engine - CSS/Engine/Build/BatchFiles/RunUAT.bat" ^
    -ScriptsForProject="%cd%/../../FactoryGame.uproject" PackagePlugin ^
    -project="%cd%/../../FactoryGame.uproject" ^
    -clientconfig=Shipping ^
    -serverconfig=Shipping ^
    -utf8output ^
    -DLCName=%PROJECT% ^
    -build ^
    -platform=Win64 ^
    -server ^
    -serverplatform=Win64+Linux ^
    -server ^
    -server ^
    -nocompileeditor ^
    -installed ^
    -merge

if not exist ..\..\Saved\ArchivedPlugins\%PROJECT%\*.zip exit /b 1
