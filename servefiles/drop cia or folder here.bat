@ECHO OFF
set /p DSIP="Enter the IP of your 3DS: "
for %%a in (%*) do  (
python %~dp0servefiles.py %DSIP% "%%~a"
)
pause
