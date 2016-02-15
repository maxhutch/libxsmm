@ECHO OFF
SETLOCAL

cd ..
bash -c "make realclean ; make M='1 2 3 4 5 23' PREFETCH=1 headers sources"
cd ide

SET LIBXSMMROOT=%~d0%~p0..

CALL %~d0"%~p0"_vs.bat vs2015
IF NOT "%VS_IDE%" == "" (
  START "" "%VS_IDE%" "%~d0%~p0_vs2015.sln"
) ELSE (
  START %~d0"%~p0"_vs2015.sln
)

ENDLOCAL