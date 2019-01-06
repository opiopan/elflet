elflet
====

elflet is a home IoT controller based on [ESP-WROOM-32](https://www.espressif.com/en/products/hardware/modules). <br>
Both of it's hardware designe and firmware code are open source licenced. 
You are free to use any outcome of this project. And you are also free to modify hardware design and firmware code.<br>
elflet will join your home net work with WiFi.
Then elflet can controll home electronic devices through IR remote controll or Bluetooth LE. 
And this device can be behave as sensor node such as temperature sensor.<br>
Unlike other similar products, elflet highly abstructs IR remote controll protocol. Therefore host-side software to collaborate with elflet is very easy to implement.

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/elflet_description.jpg" width=750>
</p>

## Features

* **IR reciever**<br>
Recieving IR remotecontroller signal is analized such as following abstracted protocol data.
    ```json
    {
        "Protocol": "AEHA",
        "BitCount": 160,
        "Data": "11da270002000000000300001000000000000027"
    }
    ```
    Analized protocol data can be retrieved via 
    [REST interface](https://github.com/opiopan/elflet/blob/master/docs/REST.md), 
    or can be publishd via MQTT protocol.<br>
    **NOTE:** Supported protocols are NEC, SONY, and AEHA.

* **IR transmitter**<br> 
You can transmit IR remote controller signal by specifying the above level abstracted represatation.<br>
elflet supports two protocol for IR transmitter, 
[REST](https://github.com/opiopan/elflet/blob/master/docs/REST.md)
and binary protocol.
Regarding this binary protocol, please refer 
[these library code (irslib.h, irslib.c)](https://github.com/opiopan/elflet/tree/master/hosttool).

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
    [REST interface](https://github.com/opiopan/elflet/blob/master/docs/REST.md), and shadow device status change can be known real time as publicshed data via MQTT.<br>
    You can also change shadow's status by sending the above format data to elflet via 
    [REST interface](https://github.com/opiopan/elflet/blob/master/docs/REST.md). 
    In this case, elflet synthesize IR code acording to shadow definition then transmit that.<br>
    A couple of shadow definition examples are shown at
    [here](https://github.com/opiopan/elflet/blob/master/examples).

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
    [REST interface](https://github.com/opiopan/elflet/blob/master/docs/REST.md), and can recieve them periodically as MQTT published message.<br>
    elflet supports a SENSOR-ONLY mode to avoid own heat affecting to measuring temperature. In this mode, elflet is in deep sleep status except when sensor values is captured and they are published.
    You can transit elflet to SENSOR-ONLY mode by using
    [host tool](https://github.com/opiopan/elflet/blob/master/hosttool).

## PCB
PCB of elfet is designed by using [AUTODESK EAGLE](http://eagle.autodesk.com).
* PCB design files are placed [here](https://github.com/opiopan/elflet/blob/master/pcb/mainboard)
* You can also use [this gerver data](https://raw.githubusercontent.com/wiki/opiopan/elflet/pcb/elflet-mainboard.zip)
for production
* BOM list is [here](https://github.com/opiopan/elflet/blob/master/pcb/elflet-bom.xlsx?raw=true)

## Building Firmware
Firmware source codes are placed at 
[here](https://github.com/opiopan/elflet/blob/master/firmware).
To build firmware binary, please refer 
[this document](https://github.com/opiopan/elflet/blob/master/firmware/README.md).

## Inital Firmware Download
Once elflet firmware running on elflet board, you can download firmware binary via WiFi (OTA updating). However, you need to download firmware via UART at first time since OTA updating function does not work yet.<br>
elflet exports sevelal pins to service port as followings. You can download firmware by connection your USB serial dongle to there.

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/elflet-port.jpg" width=300>
</p>

Another way to download is using 
[this jig board](https://raw.githubusercontent.com/wiki/opiopan/elflet/pcb/elflet-jig.zip)
and Raspberry Pi to connect to elflet. And 
[these tools running on Raspberry Pi](https://github.com/opiopan/elflet/blob/master/raspitools)
help you also to download or debug elflet firmware.<br>
When use this jig board, please connect following direction.

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/elflet-jig.jpg" width=500>
</p>

## Configuration
When initial downloading firmware finish, LED on elflet starts to blink orange. In this state, elflet is working as WiFi access point and it provides Web based setup widzard interface.<br>
The SSD is same as node name of elflet, 
and the initial node name is "`elflet-000000`". Passphrase to connect elflet WiFi access point is same as administorator password, and the initial password is "`elflet00`".<br>
Once you connect to elflet WiFi access point, you can access to setup widzerd. Please try a URL "`http://elflet-000000.local/`".<br>

**NOTE:** Initial node name and initial administorator password can be change in step of building firmware. Please lefer
[this document](https://github.com/opiopan/elflet/blob/master/firmware/README.md)
regarding the detail. 

<p align="center">
<img alt="description" src="https://raw.githubusercontent.com/wiki/opiopan/elflet/images/wizard.jpg" width=300>
</p>

By this wizard, you can configure following items:

* WiFi SSID and passphrase to connect
* elflet node name
* administorator password

When finishing the wizard, elflet will restart and LED start to blink white.
After that, elflet connect to your home network if configuration is correct, and LED turns off.<br>
If WiFi configuration is not correct and elflet cannot connect to your home network in 30 seconds, elflet will fall back to configuration mode.
In this case, the LED start to blink orange again and you can access setup Wizard.

During elflet is connecting to your home network, setup widzard is not available. If you want to access setup wizard, please transit elflet to SETUP mode. You can proceed this transition by plessing a button till LED start to blink orange.

To change other detail configuration, please use a host tools, "`elflet-config`" and "`elflet-shadow`". Usage of these tools are described in [this document](https://github.com/opiopan/elflet/tree/master/hosttool/README.md).

## Application Example
My another project [Wakeserver](https://github.com/opiopan/wakeserver) is a good example of elfelt's application.