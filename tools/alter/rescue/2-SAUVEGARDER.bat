@echo off
cd /d "%~dp0"
echo === ETAPE 2 : LECTURE ET SAUVEGARDE de toute la memoire. RIEN NEST ECRIT sur la FM-1. ===
alter-fm1-secours.exe ALTER.fwsc --wait 30
echo.
echo Copiez tout ce texte pour Claude. Puis eteignez et rallumez la FM-1.
pause
