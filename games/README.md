# WiFi Pineapple Pager — Games

Arcade games built for the Pager's 2.4" color display (480×222, RGB565) and D-pad controls.

## Games

| Game | Description | Controls |
|------|-------------|----------|
| **Snake** | Classic snake — eat, grow, survive | D-pad: move, A: pause |
| **Pong** | Player vs AI paddle game (first to 11) | Up/Down: paddle, A: pause |
| **Packet Catcher** | Catch DATA/BCN/AUTH packets, avoid MALWARE | Left/Right: move, A: pause |
| **NETRUNNER 2084** | Cyberpunk turn-based RPG — hack the megacorp! | D-pad: move, A: confirm, B: inventory, Power: pause |
| **Launcher** | Menu to browse and launch installed games | Up/Down: navigate, A: launch |

## Quick Start

```
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"
cd ~/Desktop/WiFiPineapplePager/games
make clean && make CROSS_COMPILE=mipsel-linux-muslsf-
make deploy-payloads CROSS_COMPILE=mipsel-linux-muslsf-
```
