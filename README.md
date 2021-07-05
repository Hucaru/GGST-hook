# Guilty Gear Strive Client Hook

Decreases the time taken to get to the main menu (slight speedup to quick match as arcsys go through the login process again when you select this option) . My time went from `2m 30s` to `1m 25s` (this depends on distance to server).

## Disclaimer

I have been using this since 02/07/2021. The likelyhood arcsys can even detect this is super small but on the off chance their server records how long a particular socket and http session is open for: Use at your own risk.

## How to build

Make sure visual studio is installed and edit the `VSTOOLS` variable in `build.bat` to point to your installed version of `vcvars64.bat`. Make sure you point to the 64-bit version.

Download the Microsoft Detours repo and compile the x64 lib and copy the compiled lib and header file into this directory.

Running `build.bat` should compile the DLL.

## How to inject

Once DLL has been compiled use a tool such as CFF explorer to add the DLL to the import table of the steamworks dll under the third party binaries in the Guilty Gear Strive folder.

## Further improvements

The `POST` uri `/api/statistics/set` request connections cannot be trivially cached as it causes an R-code upload error