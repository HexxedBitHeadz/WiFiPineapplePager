# WiFi Pineapple Pager — Games

Arcade games built for the Pager's 2.4" color display (480×222, RGB565) and D-pad controls.

## Games

| Game | Description | Controls |
|------|-------------|----------|
| **Snake** | Classic snake — eat, grow, survive | D-pad: move, A: pause |
| **Pong** | Player vs AI paddle game (first to 11) | Up/Down: paddle, A: pause |
| **Packet Catcher** | Catch DATA/BCN/AUTH packets, avoid MALWARE | Left/Right: move, A: pause |


## Quick Start

```bash
cd /opt
sudo wget https://musl.cc/mipsel-linux-muslsf-cross.tgz
sudo tar xzf mipsel-linux-muslsf-cross.tgz
```


```bash
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"
```

```bash
mipsel-linux-muslsf-gcc --version
```


```bash
cd games
```

```bash
make clean && make CROSS_COMPILE=mipsel-linux-muslsf-
```

    === All games built successfully ===
    Run 'make deploy' to install on Pager


Connect usb to VM 

# On Kali, after syncing the updated files:

```bash
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"
cd games
make clean && make CROSS_COMPILE=mipsel-linux-muslsf-
make deploy CROSS_COMPILE=mipsel-linux-muslsf-
```
