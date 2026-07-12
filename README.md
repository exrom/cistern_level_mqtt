# Fill volume measurement for a cistern for integration into home assistant with MQTT

This project is an arduino based fill level/fill volume measurement system water cisterns.
The measure values are published by MQTT and can be observed by http. For connectivity, an W5500 based ethernet hat is connected to the arduino nano.
The level sensor is connected through a current loop by two wires. The loop current of 4-20mA is digitized on arduino side.
The firmware is developed in VSCode with platformIO.


## Requirements

- support for multiple cisterns
- use of 4..20mA loop current sensors - to support long cables and off-site electronics
- robust design (no WLAN, no DHCP, static IP)
- MQTT support with authentication
- precision: 1%
- a http server with same information published via MQTT


## parts list

- arduino nano V3 with Atmega328 MCU
- ethernet shield with W5500 chip set
- level sensors ALS-MPM-2F  https://www.amazon.de/dp/B0CF3PWPP3?th=1  (5m-Version)
- optional analog instruments with 4..20mA 0..100% reading
- power supply 24V


## Test circuit

supply 24V:   4..20mA  == 6k..1k2 Ohm
220 Shunt:              5k78...980  - 50% (12mA) 1k28


## hardware setup

https://docs.arduino.cc/libraries/ethernet/

+--------- Arduino nano pin
|
|                                                           W5500 Pin
D13     SCLK    SPI clock                                       33
D12     MISO    SPI master input slave(W5500) output            34
D11     MOSI    SPI master output slave(W5500) input            35
        SPS     SPI chip select                                 32

SS =? SCS  "On the Mega [...] is not used to select the Ethernet controller chip"
SPS not connect with an AZ delivery eth module

