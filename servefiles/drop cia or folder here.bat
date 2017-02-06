@ECHO OFF
set /p DSIP="Enter the IP of your 3DS: "
python %~dp0servefiles.py %DSIP% "%~1"
pause
