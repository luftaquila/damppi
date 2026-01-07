# damppi

Damppi is a pager device used to call others, physically.

## Prerequisites

### Software

* `git clone https://github.com/luftaquila/damppi.git`
* [OpenSCAD](https://openscad.org/downloads.html) (nightly build recommended).
* [ESP-IDF (Manual Installation)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#manual-installation)

### Hardware

* 1x Waveshare ESP32-C6-LCD-1.47
* 1x Cherry MX compatible mechanical keyboard switch
* 4x M2 8mm bolts
* 3D printed housing
    * Run the following commands to generate `top.stl` and `bottom.stl`:

```sh
cd damppi/3d
make
```

## Assembly

1. Solder two wires (approx. 5cm) to the `GPIO23` and `GND` pins on the ESP32 board.
1. Mount the ESP32 to the bottom housing using M2 bolts.
1. Install the MX switch into the top housing.
1. Solder the wires to the MX switch.
1. Glue the top and bottom housing together.

## Firmware

1. Connect the device to PC via USB cable.
1. Run following commands:

```sh
cd damppi/firmware
make flash
```

## Usage

1. Power the device. It will provision a Wi-Fi AP named `Damppi <MACADDR>`.
1. Connect to the AP. Your browser will automatically redirect to the configuration page.
    * If not, visit [http://192.168.4.1](http://192.168.4.1).
1. Set `Wi-Fi AP`, `Wi-Fi Password`, `Device Name` and `Server`, then click the `Save` button.
    * `Device Name`: The name displayed to others when you send a call.
    * `Server`: The MQTT broker. Refer to the `MQTT Broker` section below if you don't have one.
1. After the automatic reboot, the device will connect to the configured Wi-Fi.
1. Press the switch button once to wake up the screen, and twice to send a call.

## MQTT Broker

The device requires an MQTT broker to communicate.

Run following commands to host a broker:

```sh
cd damppi
sudo docker compose up -d
```

This will host the MQTT service on port 1883.

