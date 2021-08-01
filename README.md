# Guilty Gear Strive Client Hook

Decreases the time all http network operations take. Unlike other solutions this is resilient to client version updates (unless they overhaul their networking) and does not requrie you to run a proxy application.

# How this works

By caching handles and requests the client makes. This means tls/tcp initialisation packets are no longer sent for every request

## How to get

Download the latest release, or alternatively build from source:

Make sure visual studio is installed and edit the `VSTOOLS` variable in `build.bat` to point to your installed version of `vcvars64.bat`. Make sure you point to the 64-bit version.

Download the Microsoft Detours repo and compile the x64 lib and copy the compiled lib and header file into this directory.

Running `build.bat` should compile the DLL.

## How to auto inject

- Download [CFF explorer](https://ntcore.com/files/CFF_Explorer.zip).
- Navigate to your steam install folder and go to `\SteamApps\common\GUILTY GEAR STRIVE\Engine\Binaries\ThirdParty\Steamworks\Steamv147\Win64`
- Make a copy of `steam_api64.dll`
- Place the `ggs_hook.dll` you either downloaded or built into the same folder
- Run CFF explorer and open `steam_api64.dll`
- On the left click on `Import Addr`
- Click `Add` and select `ggs_hook.dll`
- Select `dummyExport` and then press the `Import by Name` button
- Press the `Rebuild Import Table` button and then save the dll

![CFF explorer import table addition](/cff.png?raw=true)