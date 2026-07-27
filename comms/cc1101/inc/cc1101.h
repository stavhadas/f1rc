#ifndef CC1101_H
#define CC1101_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "spi_interface.h"

// ── Configuration registers (0x00-0x2E, R/W, burst-capable) ────────────────
#define CC1101_REG_IOCFG2    0x00
#define CC1101_REG_IOCFG1    0x01
#define CC1101_REG_IOCFG0    0x02
#define CC1101_REG_FIFOTHR   0x03
#define CC1101_REG_SYNC1     0x04
#define CC1101_REG_SYNC0     0x05
#define CC1101_REG_PKTLEN    0x06
#define CC1101_REG_PKTCTRL1  0x07
#define CC1101_REG_PKTCTRL0  0x08
#define CC1101_REG_ADDR      0x09
#define CC1101_REG_CHANNR    0x0A
#define CC1101_REG_FSCTRL1   0x0B
#define CC1101_REG_FSCTRL0   0x0C
#define CC1101_REG_FREQ2     0x0D
#define CC1101_REG_FREQ1     0x0E
#define CC1101_REG_FREQ0     0x0F
#define CC1101_REG_MDMCFG4   0x10
#define CC1101_REG_MDMCFG3   0x11
#define CC1101_REG_MDMCFG2   0x12
#define CC1101_REG_MDMCFG1   0x13
#define CC1101_REG_MDMCFG0   0x14
#define CC1101_REG_DEVIATN   0x15
#define CC1101_REG_MCSM2     0x16
#define CC1101_REG_MCSM1     0x17
#define CC1101_REG_MCSM0     0x18
#define CC1101_REG_FOCCFG    0x19
#define CC1101_REG_BSCFG     0x1A
#define CC1101_REG_AGCCTRL2  0x1B
#define CC1101_REG_AGCCTRL1  0x1C
#define CC1101_REG_AGCCTRL0  0x1D
#define CC1101_REG_WOREVT1   0x1E
#define CC1101_REG_WOREVT0   0x1F
#define CC1101_REG_WORCTRL   0x20
#define CC1101_REG_FREND1    0x21
#define CC1101_REG_FREND0    0x22
#define CC1101_REG_FSCAL3    0x23
#define CC1101_REG_FSCAL2    0x24
#define CC1101_REG_FSCAL1    0x25
#define CC1101_REG_FSCAL0    0x26
#define CC1101_REG_RCCTRL1   0x27
#define CC1101_REG_RCCTRL0   0x28
#define CC1101_REG_FSTEST    0x29
#define CC1101_REG_PTEST     0x2A
#define CC1101_REG_AGCTEST   0x2B
#define CC1101_REG_TEST2     0x2C
#define CC1101_REG_TEST1     0x2D
#define CC1101_REG_TEST0     0x2E

// ── Command strobes (0x30-0x3D, header byte only, no data) ─────────────────
#define CC1101_STROBE_SRES     0x30
#define CC1101_STROBE_SFSTXON  0x31
#define CC1101_STROBE_SXOFF    0x32
#define CC1101_STROBE_SCAL     0x33
#define CC1101_STROBE_SRX      0x34
#define CC1101_STROBE_STX      0x35
#define CC1101_STROBE_SIDLE    0x36
#define CC1101_STROBE_SWOR     0x38
#define CC1101_STROBE_SPWD     0x39
#define CC1101_STROBE_SFRX     0x3A
#define CC1101_STROBE_SFTX     0x3B
#define CC1101_STROBE_SWORRST  0x3C
#define CC1101_STROBE_SNOP     0x3D

// ── Status registers (0x30-0x3D with R=1,B=1 -- same addresses as the
//    strobes above, disambiguated by the burst bit in the header) ──────────
#define CC1101_STATUS_PARTNUM         0x30
#define CC1101_STATUS_VERSION         0x31
#define CC1101_STATUS_FREQEST         0x32
#define CC1101_STATUS_LQI             0x33
#define CC1101_STATUS_RSSI            0x34
#define CC1101_STATUS_MARCSTATE       0x35
#define CC1101_STATUS_WORTIME1        0x36
#define CC1101_STATUS_WORTIME0        0x37
#define CC1101_STATUS_PKTSTATUS       0x38
#define CC1101_STATUS_VCO_VC_DAC      0x39
#define CC1101_STATUS_TXBYTES         0x3A
#define CC1101_STATUS_RXBYTES         0x3B
#define CC1101_STATUS_RCCTRL1_STATUS  0x3C
#define CC1101_STATUS_RCCTRL0_STATUS  0x3D

#define CC1101_REG_PATABLE 0x3E
#define CC1101_REG_FIFO    0x3F

#define CC1101_TX_FIFO_SIZE 64

// Header byte construction: bit7=R/W-bar, bit6=burst, bits5:0=address.
#define CC1101_WRITE_SINGLE(addr) ((uint8_t)(addr))
#define CC1101_WRITE_BURST(addr)  ((uint8_t)((addr) | 0x40))
#define CC1101_READ_SINGLE(addr)  ((uint8_t)((addr) | 0x80))
#define CC1101_READ_BURST(addr)   ((uint8_t)((addr) | 0xC0))

typedef struct
{
    spi_interface_t *spi;
    uint8_t partnum;
    uint8_t version;
} cc1101_t;

void cc1101_strobe(cc1101_t *dev, uint8_t strobe_addr);
void cc1101_write_reg(cc1101_t *dev, uint8_t addr, uint8_t value);
uint8_t cc1101_read_reg(cc1101_t *dev, uint8_t addr);
void cc1101_write_burst(cc1101_t *dev, uint8_t addr, const uint8_t *data, size_t length);
// length must be <= 8 (see cc1101.c: uses a small fixed dummy TX buffer).
void cc1101_read_burst(cc1101_t *dev, uint8_t addr, uint8_t *data, size_t length);
uint8_t cc1101_read_status(cc1101_t *dev, uint8_t status_addr);

/**
 * Resets the chip (SRES), reads back PARTNUM/VERSION (stored in dev), and
 * does a scratch-register write/read-back round trip to confirm
 * bidirectional SPI. Returns true only if PARTNUM reads the expected 0x00
 * and the round-trip matches.
 */
bool cc1101_probe(cc1101_t *dev);

/**
 * Programs the chip for an unmodulated carrier (OOK/ASK, all-ones data,
 * infinite packet length -- see comms/cc1101/cc1101.c for the full recipe
 * and datasheet references), fills the TX FIFO once, and issues STX. Called
 * by cc1101_start_bringup_thread() (cc1101_thread.h), which also keeps the
 * FIFO topped up (and recovers from underflow) so the carrier is sustained
 * indefinitely instead of stopping after one FIFO's worth.
 */
bool cc1101_set_cw(cc1101_t *dev);

#endif /* CC1101_H */
