# elflet firmware
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

## Requirement

- **Mac or Linux**<br>
Sorry, I could not verify that these souce codes are compiled well on Windows environment since I have no PC running Windows.

- **ESP-IDF**<br>
Please install the ESP-IDF according to 
[this procedure](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html).

## Building Firmware
1. **Preparing Key Files for Digital Signing**<br>
    When OTA firmware updating, elflet justify firmware image by digital signature scheme based on public-key encription.
    You have to prepare 1024 bit RSA key pair, private key to sign and public key to verify.<br>
    Please make dedicated key pair as below.

    ```shell
    $ mkdir ~/esp/deploy
    $ cd ~/esp/deploy
    $ openssl genrsa 1024 > signingkey.pem
    $ openssl rsa -in signingkey.pem -pubout -out verificationkey.pem
    ```

2. **Downloading source codes**<br>
    elflet repository include a couple of other projects as sub module.
    Run `git clone` command with `--recursive` option.
    ```shell
    $ git clone --recursive https://github.com/opiopan/elflet.git
    ```
3. **Configuration**<br>
    Execute configuration utility with:
    ```shell
    $ cd elflet/firmware
    $ make menuconfig
    ```
    Select `elflet Configuration` sub menu, then configure corresponding to your environment.<br>
    You need to specify path of key files for digital signing at least. You can also change initial administration password at here.

4. **Building**<br>
    Just execute `make` command.
    ```shell
    $ make
    ```

## Downloading Firmware
Please refer [this section](https://github.com/opiopan/elflet#inital-firmware-downloading).
