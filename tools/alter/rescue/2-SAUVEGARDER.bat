@echo off
cd /d "%~dp0"
if not exist FM-1.fwsc (echo Il manque FM-1.fwsc : le fichier officiel M-VAVE V15, a mettre dans ce dossier. & pause & exit /b 1)
echo === ETAPE 2 : LECTURE ET SAUVEGARDE de toute la memoire. RIEN NEST ECRIT sur la FM-1. ===
alter-fm1-secours.exe FM-1.fwsc --wait 30
echo.
echo Copiez tout ce texte pour Claude. Puis eteignez et rallumez la FM-1.
pause
