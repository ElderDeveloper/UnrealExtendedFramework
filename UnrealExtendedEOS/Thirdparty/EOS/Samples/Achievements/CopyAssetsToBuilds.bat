xcopy /D /Y /R ..\..\SDK\Bin\EOSSDK-Win64-Shipping.dll .\Bin\Win64\Debug_DX\
xcopy /D /Y /R ..\..\SDK\Bin\EOSSDK-Win64-Shipping.dll .\Bin\Win64\Release_DX\
xcopy /D /Y /R /Q ..\Shared\External\curl\Lib\Win64\libcurl.dll .\Bin\Win64\Debug_DX\
xcopy /D /Y /R /Q ..\Shared\External\curl\Lib\Win64\libcurl.dll .\Bin\Win64\Release_DX\
pause