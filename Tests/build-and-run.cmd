@echo off
setlocal
for %%I in ("%~dp0..") do set "PROJECT=%%~fI"
if defined VSINSTALLDIR goto ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 2
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT exit /b 2
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 2
:ready
msbuild "%PROJECT%\GameBaseDx11.sln" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
if errorlevel 1 exit /b 1
if not exist "%~dp0build" mkdir "%~dp0build"
set "OBJ=%PROJECT%\GameBaseDx11\x64\Debug"
cl /nologo /EHsc /MTd /std:c++17 /I"%PROJECT%\Engine" "%~dp0Smoke.cpp" "%OBJ%\Fbx.obj" "%OBJ%\FbxParts.obj" "%OBJ%\Model.obj" "%OBJ%\Texture.obj" "%OBJ%\TextureManager.obj" "%OBJ%\Transform.obj" "%OBJ%\Camera.obj" "%OBJ%\Direct3D.obj" "%OBJ%\ufbx.obj" "%OBJ%\Image.obj" "%OBJ%\Sprite.obj" "%OBJ%\GameObject.obj" "%OBJ%\Collider.obj" "%OBJ%\BoxCollider.obj" "%OBJ%\SphereCollider.obj" "%OBJ%\VFX.obj" "%OBJ%\BillBoard.obj" "%OBJ%\TestScene.obj" /Fe:"%~dp0build\Smoke.exe" /Fo:"%~dp0build\Smoke.obj" /Fd:"%~dp0build\Smoke.pdb" /link ole32.lib user32.lib gdi32.lib /OPT:NOREF /OPT:NOICF
if errorlevel 1 exit /b 1
pushd "%PROJECT%\Assets"
"%~dp0build\Smoke.exe" "DebugCollision\BoxCollider.fbx" "DebugCollision\SphereCollider.fbx" "%~dp0Fixtures\blender_suzanne_multimaterial_7400_binary.fbx" "%~dp0Fixtures\max_geometry_transform_instances_7700_ascii.fbx" "%~dp0Fixtures\max_transformed_skin_7500_ascii.fbx" "%~dp0Fixtures\max2009_cube_anim_6100_ascii.fbx" "%~dp0Fixtures\maya_textured_cube_7500_ascii.fbx" "%~dp0Fixtures\maya_transformed_skin_7700_ascii.fbx" "%~dp0Fixtures\orientation_rh.fbx"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%

