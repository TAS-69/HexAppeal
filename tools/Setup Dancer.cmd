@echo off
py -3 "%~dp0tools\prepare_dancer_dependencies.py" --main-mod "%~dp0." --apply
if errorlevel 1 echo Dancer setup could not finish. Check the message above. Python 3.12 or newer with its launcher is required.
pause
