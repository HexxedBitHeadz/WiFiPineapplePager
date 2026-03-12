# WiFi Pineapple Pager — Games

Arcade games built for the Pager's 2.4" color display (480×222, RGB565) and D-pad controls.

## Games

| Game | Description |
|------|-------------|
| **Snake** | Classic snake — eat, grow, survive |
| **Pong** | Player vs AI paddle game (first to 11) |
| **Packet Catcher** | Catch DATA/BCN/AUTH packets, avoid MALWARE
| **NETRUNNER 2084** | Cyberpunk turn-based RPG — hack the megacorp!
| **Null-Buddy** | WiFi hacker buddy that levels up! |


## Quick Start

```
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"
cd ~/Desktop/WiFiPineapplePager/games
make clean && make CROSS_COMPILE=mipsel-linux-muslsf-
make deploy-payloads CROSS_COMPILE=mipsel-linux-muslsf-
```
