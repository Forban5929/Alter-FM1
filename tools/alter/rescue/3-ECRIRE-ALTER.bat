@echo off
cd /d "%~dp0"
echo === ETAPE 3 : ECRIRE ALTER (version corrigee) dans la zone du firmware. SEULEMENT apres une etape 2 reussie et laccord de Claude. ===
set /p OK=Tapez OUI en majuscules pour ecrire : 
if not "%OK%"=="OUI" (echo Annule, rien nest ecrit. & pause & exit /b 1)
alter-fm1-secours.exe ALTER.fwsc --write --wait 30
echo.
echo Copiez tout ce texte pour Claude.
pause
