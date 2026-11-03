@echo off
cd /d "%~dp0"
if not exist FM-1.fwsc (echo Il manque FM-1.fwsc. & pause & exit /b 1)
echo === ETAPE 3 : ECRIRE le firmware officiel V15. A faire SEULEMENT apres une etape 2 reussie et laccord de Claude. ===
set /p OK=Tapez OUI en majuscules pour ecrire : 
if not "%OK%"=="OUI" (echo Annule, rien nest ecrit. & pause & exit /b 1)
alter-fm1-secours.exe FM-1.fwsc --write --wait 30
echo.
pause
