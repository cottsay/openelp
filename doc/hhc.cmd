@"C:\Program Files (x86)\HTML Help Workshop\hhc.exe" %*

@IF %ERRORLEVEL% EQU 0 GOTO DONE

@IF EXIST *.chm GOTO DONE

@ECHO Expected .chm file was not generated!
@EXIT /B 1

:DONE

@EXIT /B 0
