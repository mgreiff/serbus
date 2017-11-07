#!/usr/bin/env bash
# This shell script runs an ADC SPI example on the BB, execute from /home/debian
BLUE='\033[0;34m'
NC='\033[0m'

cd juliapackages/serbus/examples

printf "${BLUE}\nCompile device tree overlay...\n${NC}"
dtc -O dtb -o BB-SPI1-01-00A0.dtbo -b 0 -@ BB-SPI1-01-00A0.dts
sudo cp BB-SPI1-01-00A0.dtbo /lib/firmware/

printf "${BLUE}Configuring pins...\n${NC}"
sudo config-pin P9.17 spi # SPI0 CS0
sudo config-pin P9.18 spi # SPI0 D1
sudo config-pin P9.21 spi # SPI0 D0
sudo config-pin P9.22 spi # SPI0 SLCK

printf "${BLUE}Visualize settings...\n${NC}"
config-pin -q P9.17  # SPI0 CS0
config-pin -q P9.18  # SPI0 D1
config-pin -q P9.21  # SPI0 D0
config-pin -q P9.22  # SPI0 SLCK

printf "${BLUE}Compile the SPI driver...\n${NC}"
make clean
make spi_MCP3903

printf "${BLUE}Run the example program...\n${NC}"
sudo bin/spi_MCP3903
