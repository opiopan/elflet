elflet
====

elflet is a home IoT controller based on [ESP-WROOM-32](https://www.espressif.com/en/products/hardware/modules) (ESP32). <br>
Both of hardware designe and firmware code are open source licenced. 
You are free to use any outcome from this project. And you are also free to modify hardware design and firmware code.<br>
elflet will join your home net work with WiFi.
Then elflet can controll home electronic devices through IR remote controll protocol or Bluetooth LE. 
And this device can be behave as sensor node such as temperature sensor.<br>
Unlike other similar products, elflet highly abstructs IR remote controll protocol. Therefore host-side software to collaborate with elflet is very easy to implement.

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/elflet_description.jpg" width=750>
</p>

## Features

* **IR receiver**<br>
Receivd IR remote controller signal is analized such as following abstracted protocol data.

    ```json
    {
        "Protocol": "AEHA",
        "BitCount": 160,
        "Data": "11da270002000000000300001000000000000027"
    }
    ```
    Analized protocol data can be retrieved via 
    [REST interface](docs/REST.md), 
    or can be publishd via MQTT protocol.<br>
    **NOTE:** Supported protocols are NEC, SONY, and AEHA.

* **IR transmitter**<br> 
You can transmit IR remote controller signal by specifying the above level abstracted represatation.<br>
elflet supports two protocols for IR transmitter, 
The first one is 
[REST](docs/REST.md)
, and the other one is original binary protocol.
Regarding this binary protocol, please refer 
[these host-side library codes (`irslib.h`, `irslib.c`)](hosttool).

* **Device Shadow**<br>
Device shadow is designed to help complecated IR command device management such as air conditioner.<br>
elflet analize IR command code pattern according to registered shadow definition, then reflect device status to shadow representation managed in elflet. Following JSON data is a example of shadow status representation.
    ```json
    {
        "NodeName": "elflet-living", 
        "ShadowName": "ac",
        "IsOn": true,
        "Attributes":{
            "Mode": "Cooler",
            "Temperature": 26.5,
            "Humidity": 50
        }
    }
    ```

    Shadow status can be refered via 
    [REST interface](docs/REST.md), and status change of shadow device can be known immediately as publicshed data via MQTT.<br>
    You can also change shadow's status by sending the above format data to elflet via 
    [REST interface](docs/REST.md). 
    In this case, elflet synthesize IR code acording to shadow definition then transmit that.<br>
    A couple of shadow definition examples are shown at
    [here](examples).

* **BLE Keyboard Emulator**<br>
elflet can behave as Bluetooth LE (BLE) keyboard. You can send any key code to a device paired with elflet via Bluetooth LE.<br>
The typical use case of this function is to wake up a PC or server which does not accept WOL packet.

* **Environment Sensor**<br>
Following sensors are installed on elflet.
    * temperature sensor
    * humidity sensor
    * atmospheric pressure sensor
    * luminocity sensors

    You can retrieve sensor values via
    [REST interface](docs/REST.md),
    and can also receive them periodically as MQTT published message.<br>
    elflet supports a SENSOR-ONLY mode to avoid own heat affecting to measuring temperature. In this mode, elflet stays in deep sleep status except when sensor values is captured and they are published.
    You can transit elflet to SENSOR-ONLY mode by using
    [host tool](hosttool).

## PCB
PCB of elfet is designed by using [AUTODESK EAGLE](http://eagle.autodesk.com).
* PCB design files are placed [here](pcb/mainboard)
* You can also use [this gerver data](https://raw.githubusercontent.com/wiki/opiopan/elflet/pcb/elflet-mainboard.zip)
for production
* BOM list is [here](pcb/elflet-bom.xlsx?raw=true)

## Building Firmware
Firmware source codes are placed at 
[here](firmware).
To build firmware binary, please refer 
[this document](firmware/README.md).

## Inital Firmware Downloading
Once elflet firmware runs on elflet board, you can download firmware binary via WiFi (OTA updating). However, you need to download firmware via UART at first time since OTA updating function does not work yet.<br>
Folowing four binary files must be download at correct address in ESP-WROOM-32's SPI flash.

|file to download                        | adress  |
|:---------------------------------------|--------:|
| firmware/buld/bootloader/bootloader.bin|   0x1000|
| firmware/buld/partitions.bin           |   0x8000|
| firmware/build/ota_data_initial.bin    |   0xd000|
| firmware/buld/elflet.bin               |  0x10000|
| firmware/buld/spiffsimage.bin          | 0x3f0000|

elflet exports sevelal ESP32 pins to service port as below. You can download firmware by connection your USB serial dongle to there.

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/elflet-port.jpg" width=450>
</p>

Another way to download firmware is using 
[this jig board](https://raw.githubusercontent.com/wiki/opiopan/elflet/pcb/elflet-jig.zip)
and Raspberry Pi. 
[These tools running on Raspberry Pi](raspitools)
may help you to download or debug elflet firmware.<br>
When use this jig board, please connect following direction.

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/elflet-jig.jpg" width=500>
</p>

## Configuration
When initial firmware downloading finish, LED on elflet starts to blink orange. In this state, elflet is working as WiFi access point and it provides Web based setup widzard interface.<br>
The SSID is same as node name of elflet, 
and the initial node name is "`elflet-000000`". Passphrase to connect elflet WiFi access point is same as administorator password, and the initial password is "`elflet00`".
Please connect to elflet WiFi access point by specifing these SSID and passphrase<br>

Once you connect to elflet WiFi access point, you can access to setup widzard. <br>
Since this wizard behaves as same mechanism of [captive portal](https://en.wikipedia.org/wiki/Captive_portal), typical OS shows that wizard window automatically when you connect to elflet WiFi access point.<br>
If no wizard window is shown, try a URL "```http://elflet.setup/```"

**NOTE:** Initial node name and initial administorator password can be change in step of building firmware. Please refer
[this document](firmware/README.md)
regarding the detail. 

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/wizard.jpg" width=800>
</p>

By this wizard, you can configure following items:

* WiFi SSID and passphrase to connect
* elflet node name
* administorator password

When the wizard is completed, elflet restarts and LED start to blink white.
After that, elflet connect to your home network if configuration is correct, and LED turns off.<br>
If WiFi configuration is not correct and elflet cannot connect to your home network in 30 seconds, elflet will fall back to CONFIGURATION mode.
In this case, the LED start to blink orange again and you can access setup Wizard.

During elflet is connecting to your home network, setup widzard is not available. If you want to access setup wizard, please transit elflet to CONFIGURATION mode. You can proceed this transition by pressing a button till LED start to blink orange.

To change other detail configuration, please use a host tools, "`elflet-config`" and "`elflet-shadow`". Usage of these tools are described in [this document](hosttool/README.md).


## Licencing
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

elflet is released under dual open source licence.<br>
[The elfle firmware](firmware)
is in accordance with 
[GNU General Public License, version 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
The other program source files, PCB design, and all documents are coverd by
[Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0).

## Application Example
My another project [Wakeserver](https://github.com/opiopan/wakeserver) is a good example of elfelt's application.
