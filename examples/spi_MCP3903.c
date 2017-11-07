/**
 * @file spi_MCP3903.c
 * @author Marcus Greiff - <marcus.greiff@control.lth.se>
 *
 * @brief Uses serbus to control an MCP3903 ADC.
 *
 * Requires an SPI Kernel driver be loaded to expose a /dev/spidevX.Y
 * interface and an MCP3903 be connected on the SPI bus.
 *
 * The data sheet of the MCP3903 may be fund here
 *
 *  ([1] http://ww1.microchip.com/downloads/en/DeviceDoc/25048B.pdf)
 *
 * with the most important information summarized below in brevity
 *
 *   - The program is set to run on the SPI bus 1.                              (Chould be 0 for SPI 0, investigte!)
 *   - For proper operation and to keep ADC accuracy, AMCLK should be of
 *     1 - 5 MHz with BOOST bits off. Here we set it to 4 MHz. ([1], page 3,
       note 8).                                                                 (Investigate performance at varying rates)
 *   - Supported clock modes, (0,0)=0 or (1,1)=3 see ([1], page 1)              (We use the clock mode 3)
 *
 *
 */
#include "spidriver.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#define MCP3903_BUS       1        // Connected to /dev/spidev1.X bus
#define MCP3903_CS        0        // Using chip select 0 (/dev/spidev1.0)
#define MCP3903_FREQ      4000000  // SPI clock frequency in Hz
#define MCP3903_BITS      24       // SPI bits per word
#define MCP3903_CLOCKMODE 3        // SPI clock mode
#define OUTPUTDATA        0        // Print measured data in console

// Macro for setting bit in byte
#define SET_BIT(byte, n, x) byte ^= (-x ^ byte) & (1 << n)
// Macro for printing bytes
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
// Macro for visualizing a byte
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

/**
 * @brief Sets the given SPI bus per the MCP3903's required configuration
 *
 * @param spi_fd SPI bus file descriptor
 */
void MCP3903_SPIConfig(int spi_fd) {
  SPI_setMaxFrequency(spi_fd, MCP3903_FREQ);
  SPI_setBitsPerWord(spi_fd, MCP3903_BITS);
  SPI_setClockMode(spi_fd, MCP3903_CLOCKMODE);
  SPI_setCSActiveLow(spi_fd);
  SPI_setBitOrder(spi_fd, SPI_MSBFIRST);
}

/**
 * @brief Sets the given SPI bus per the MCP3903's required configuration
 *
 * @param spi_fd SPI bus file descriptor
 */
void MCP3903_SPIStatus(int spi_fd) {
  printf("\nSPI ioctl clock mode: %i", SPI_getMode(spi_fd));
  printf("\nSPI frequency mode: %i", SPI_getMaxFrequency(spi_fd));
  printf("\nSPI bits per word: %i\n", SPI_getBitsPerWord(spi_fd));
}

/**
 * @brief Read the value on the ADC
 *
 * @param spi_fd SPI bus file descriptor
 * @param value input to the ADC
 */
int MCP3903_getValue(int spi_fd) {
  int value = 0;
  SPI_read(spi_fd, (void*) &value, 1);
  return value;
}

int MCP3903_setValue(int spi_fd, int value) {
  SPI_read(spi_fd, (void*) &value, 1);
  return value;
}

/**
 * @brief Initialize the ADC for continuous operation.
 */
void MCP3903_initializeContinuous(int spi_fd){
  uint8_t controlByte = 0;

  SET_BIT(controlByte, 0, 1); // Address bit
  SET_BIT(controlByte, 1, 0); // Address bit
  SET_BIT(controlByte, 2, 1); // Register address bit
  SET_BIT(controlByte, 3, 0); // Register address bit
  SET_BIT(controlByte, 4, 0); // Register address bit
  SET_BIT(controlByte, 5, 0); // Register address bit
  SET_BIT(controlByte, 6, 0); // Register address bit
  SET_BIT(controlByte, 7, 1); // read/write bit ( Read=0, Write=1)

  printf("Control Byte: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(controlByte));

  // Write control packet to bus
  SPI_setBitsPerWord(spi_fd, 8);            // The control packet is a word of length 8
  MCP3903_setValue(spi_fd, controlByte);    // Write the control byte to SPI
  SPI_setBitsPerWord(spi_fd, MCP3903_BITS); // Reset word length
}

/**
 * @brief Called when Ctrl+C is pressed - triggers the program to stop.
 */
static uint8_t running;
void stopHandler(int sig) {
  running = 0;
}

int main() {
  int spi_fd;
  int counter;
  int value;

  // Open the SPI device file:
  spi_fd = SPI_open(MCP3903_BUS, MCP3903_CS);
  if (spi_fd < 0) {
    printf("Could not open SPI bus %d\n", MCP3903_BUS);
    exit(0);
  }

  // Configure the SPI bus:
  MCP3903_SPIConfig(spi_fd);

  // Print the current configuration as requested from the ioctl
  MCP3903_SPIStatus(spi_fd);

  // Enable ADC chip select CS0
  if (SPI_enableCS(spi_fd) < 0) {
    printf("Warning, could not enable chip select\n");
  } else {
    printf("CS enabled\n");
  }

  // Initialize the ADC in a continuous mode of operation
  MCP3903_initializeContinuous(spi_fd);

  // Loop until Ctrl+C pressed:
  running = 1;
  signal(SIGINT, stopHandler);

  // Read data from SPI and print words
  while(running) {
    value = MCP3903_getValue(spi_fd);
    if (OUTPUTDATA) {
      printf("Iteration %i. Read value: %i\n", counter, value);
    }
    sleep(0.5);
    counter++;
  }

  // Disable ADC chip select CS0 and close the SPI file descriptor:
  if (SPI_disableCS(spi_fd) < 0) {
    printf("Warning, could not disable chip select\n");
  } else {
    printf("CS disabled\n");
  }

  // Close device
  SPI_close(spi_fd);
  return 0;
}
