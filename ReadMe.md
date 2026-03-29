# aitaonmatalin

- The most simple game imaginable: jump over a fence
- Use arrows to move, space to jump

# Notes:

Dependencies needed for building on Linux:

```
sudo apt update && sudo apt install \
libwayland-dev wayland-protocols libxkbcommon-dev libegl1-mesa-dev \
libx11-dev libxrandr-dev libxcursor-dev libxi-dev \
libgl1-mesa-dev libglu1-mesa-dev libudev-dev \
libfreetype6-dev libvorbis-dev libogg-dev libflac-dev
```

You may need to add your user to the input group and set permissions for uinput:

``sudo usermod -aG input $USER``
``sudo chgrp input /dev/uinput``
``sudo chmod 660 /dev/uinput``

Note that the chgrp and chmod are volatile.