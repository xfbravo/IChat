# IChat Windows Client Build Notes

## Runtime Topology

- Linux server runs `im_server` for login, chat, and call signaling.
- Linux server also runs `coturn` for STUN/TURN.
- Windows client uses Qt WebEngine for real WebRTC audio/video calls.
- `libdatachannel` is optional and only kept for the older native signaling adapter.

Do not link the Linux `libdatachannel.so` into the Windows client. If you enable the optional native adapter, build or install a Windows MinGW version of libdatachannel that matches the Qt MinGW kit.

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

## Optional libdatachannel Build With MSYS2 MinGW64

This is not required for the Qt WebEngine audio/video call path. Only install libdatachannel if you want to keep testing the optional native adapter:

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

The real audio/video call UI uses Qt WebEngine, so the selected Qt kit must include:

```text
Qt WebEngine
Qt WebChannel
```

In Qt Maintenance Tool, install the WebEngine component for the same Qt version and MinGW kit that builds the client.

Open the client project in Qt Creator. For the Qt WebEngine call path, keep `ICHAT_WITH_LIBDATACHANNEL` unset or set it to `OFF`.

In Qt Creator this is usually under:

```text
Projects -> Build Settings -> CMake -> Initial Configuration / Current Configuration
```

If Qt Creator has already configured the project before, clear the previous CMake cache or delete the build directory, then run CMake again.

If you explicitly enable the optional native adapter with `ICHAT_WITH_LIBDATACHANNEL=ON`, set:

```text
ICHAT_LIBDATACHANNEL_ROOT=C:/deps/libdatachannel
```

and make sure the CMake output contains:

```text
Found libdatachannel: C:/deps/libdatachannel
```
