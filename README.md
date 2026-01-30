# ESP32 Wi-Wi Sniffer

This project aims at creating Wi-Fi's access points (AP) using ESP32.

## Prerequisites

* Install ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/#step-2-get-esp-idf
    * Make sure to use at least idf `v4.4` (which officially includes `esp32s3` support)
* Have an `ESP32-S3` board
* If flashing from linux, make sure you have permissions to access usb ports: 
    * ```sudo usermod -a -G dialout $USER``` 
    * Then reboot

## Run project

```
## Build
idf.py build -DSSID_VALUE=1

## Flash 
idf.py -p PORT flash

## Monitor
idf.py -p PORT monitor
```

You can also execute all above commands at once: `idf.py build -DSSID_VALUE=1 flash monitor`. \
The AP's ssid will be ```ESP32_AP<SSID_VALUE>```.

## Miscs

```
## Clean build folder
idf.py fullclean

## Set target
idf.py set-target esp32s3

## Configure board features
idf.py menuconfig
```
