# IChat Windows Client Build Notes

## Runtime Topology

- Linux server runs `im_server` for login, chat, and call signaling.
- Linux server also runs `coturn` for STUN/TURN.
- Windows client links Qt and Windows-build `libdatachannel`.

Do not link the Linux `libdatachannel.so` into the Windows client. Build or install a Windows MinGW version of libdatachannel that matches the Qt MinGW kit.

## Windows Environment Variables

Set these before launching the client or before starting it from Qt Creator:

```bat
set ICHAT_SERVER_HOST=61.184.13.118
set ICHAT_SERVER_PORT=8080
set ICHAT_TURN_HOST=61.184.13.118
set ICHAT_TURN_PORT=3478
set ICHAT_TURN_USER=ichat
set ICHAT_TURN_PASSWORD=dengni0425
```

Optional:

```bat
set ICHAT_STUN_URLS=stun:61.184.13.118:3478
set ICHAT_TURN_RELAY=udp
set ICHAT_RTC_FORCE_RELAY=0
```

Use `ICHAT_RTC_FORCE_RELAY=1` only when you want to force all WebRTC media through TURN for testing.

## Build With MSYS2 MinGW64

Install libdatachannel for Windows:

```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-openssl git
git clone --recursive https://github.com/paullouisageneau/libdatachannel.git
cd libdatachannel
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=C:/deps/libdatachannel \
  -DNO_EXAMPLES=ON \
  -DNO_TESTS=ON \
  -DNO_WEBSOCKET=ON
cmake --build build
cmake --install build
```

## Build From Qt Creator

If your libdatachannel source/build directory is:

```text
C:\msys64\home\21023\libdatachannel
```

open the client project in Qt Creator, then configure the CMake project with these variables:

```text
ICHAT_WITH_LIBDATACHANNEL=ON
ICHAT_LIBDATACHANNEL_ROOT=C:/msys64/home/21023/libdatachannel
```

In Qt Creator this is usually under:

```text
Projects -> Build Settings -> CMake -> Initial Configuration / Current Configuration
```

If Qt Creator has already configured the project before, clear the previous CMake cache or delete the build directory, then run CMake again.

The CMake output must contain:

```text
Found libdatachannel: enabling WebRTC PeerConnection adapter
```

If it still says `libdatachannel not found`, install libdatachannel to a clean prefix and point Qt Creator to that installed directory:

```bash
cd /home/21023/libdatachannel
cmake --install build --prefix C:/deps/libdatachannel
```

Then use this Qt Creator CMake variable instead:

```text
ICHAT_LIBDATACHANNEL_ROOT=C:/deps/libdatachannel
```

Build the client:

```bash
cmake -S im_client -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/mingw_64;C:/deps/libdatachannel" \
  -DICHAT_WITH_LIBDATACHANNEL=ON
cmake --build build
```

If `libdatachannel` is not found, the client still builds, but real WebRTC PeerConnection support is disabled.
