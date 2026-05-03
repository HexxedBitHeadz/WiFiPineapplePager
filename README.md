## Hexxed BitHeadz WiFi Pineapple Pager repo

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/DevinCleary)

#### UPDATE - APR 20206 >>>

evilginx portal added. This tools will ask for url, and redirect connected targets to it. See demo running evilginx via VPS: https://hexxedbitheadz.com/hackers-w-handhelds-v-2-0/


#### UPDATE - APR 20206 >>>

Merged all tools to one root folder "HeBi", makes it easier to manage compared to being under several different folders.

Added file_server! Fire this up to launch a web based file server on the Pager, features allow custom port asisgnment, authentication and ability to run in background. Allows for easy uploads / easy downloads!



Well it happened again, a neat new little gadget released and has stolen our attention from our everyday lives XD

The Hak5 WiFi Pineapple Pager released, and we cant stop poking at it. We've start learning about the device as quick as possible, begin working on some test tools and simple games to test it's capabilities.

So we have a few basic games built as early proof of concepts, we've learned how to accessible from the payloads screen so they can be launched and played directly on the pager.

This repo is totally experimental, and are used as early testing for us to learn more about the WiFi Pineapple Pager!

The source code is all here, to compile games, below are steps we took to compile and send to the pager while connected via USB

### Install dependencies
```
sudo apt update && sudo apt install git make build-essential -y
sudo wget -P /opt https://musl.cc/mipsel-linux-muslsf-cross.tgz
sudo tar xzf /opt/mipsel-linux-muslsf-cross.tgz -C /opt
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"
```

### Install games
```
git clone https://github.com/HexxedBitHeadz/WiFiPineapplePager && cd WiFiPineapplePager/games
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"
make clean && make CROSS_COMPILE=mipsel-linux-muslsf-
make deploy CROSS_COMPILE=mipsel-linux-muslsf-
```

It'll ask you for your Pager password, once entered, it will transfer the games over.

At this point as long as the device is plugged in and accessible via ssh root@172.16.52.1, the games should be seen in payloads > games from the main screen. 

There is also a test/test_hw.c tool that was used to assist getting the buttons figured out. We are including that tool here as well, in case a button test is needed.

### Install button test
```
git clone https://github.com/HexxedBitHeadz/WiFiPineapplePager && cd WiFiPineapplePager/test
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"
make clean && make CROSS_COMPILE=mipsel-linux-muslsf-
make deploy CROSS_COMPILE=mipsel-linux-muslsf-
```

Similar to before, just navigate to general > test_hw to see it in action!

Instllation and Null-Buddy demo:

![Null-Buddy-install](https://github.com/user-attachments/assets/83bf6559-608a-4803-bd09-6741542ac458)

Image of netrunner 2084 game:
![Description of image](images/netrunner_2084.png)

Image of packet capture game:
![Description of image](images/packet_capture.png)

Image of pong game:
![Description of image](images/pong.png)

Image of snake game:
![Description of image](images/snake.png)

Image of button test tool:
![Description of image](images/button_test.png)



