@echo off
cd /d "%~dp0"
echo === ETAPE 1 : la FM-1 en mode UBOOT est-elle vue ? (rien nest lu ni ecrit) ===
alter-fm1-secours.exe --info --wait 30
echo.
pause
