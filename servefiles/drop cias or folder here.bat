@ECHO OFF
set /p DSIP="Enter the IP of your 3DS: "
for %%a in (%*) do (
	if "%%~xa"==".cia" (
		python "%~dp0servefiles.py" %DSIP% "%%~a"
	)
	if "%%~xa"=="" (
		python "%~dp0servefiles.py" %DSIP% "%%~a"
	)
)
pause
