#include "cc1101.h"
#include <string.h>

// Generous but bounded busy-wait for the post-CS-low "SO goes low when the
// crystal is running" handshake (datasheet: sub-millisecond in active mode,
// up to 150us waking from power-down) -- see spi_interface_wait_miso_low.
#define CC1101_READY_TIMEOUT_ITERATIONS 100000

// Asserts CS, waits for the chip-ready handshake, and clocks the header
// byte. Caller is responsible for any following data bytes and for raising
// CS afterward (spi_interface_cs_high).
static void cc1101_send_header(cc1101_t *dev, uint8_t header)
{
    spi_interface_cs_low(dev->spi);
    spi_interface_wait_miso_low(dev->spi, CC1101_READY_TIMEOUT_ITERATIONS);
    uint8_t status;
    spi_interface_transfer(dev->spi, &header, &status, 1);
}

void cc1101_strobe(cc1101_t *dev, uint8_t strobe_addr)
{
    cc1101_send_header(dev, strobe_addr);
    spi_interface_cs_high(dev->spi);
}

void cc1101_write_reg(cc1101_t *dev, uint8_t addr, uint8_t value)
{
    cc1101_send_header(dev, CC1101_WRITE_SINGLE(addr));
    spi_interface_transfer(dev->spi, &value, NULL, 1);
    spi_interface_cs_high(dev->spi);
}

uint8_t cc1101_read_reg(cc1101_t *dev, uint8_t addr)
{
    cc1101_send_header(dev, CC1101_READ_SINGLE(addr));
    uint8_t tx = 0x00, rx = 0;
    spi_interface_transfer(dev->spi, &tx, &rx, 1);
    spi_interface_cs_high(dev->spi);
    return rx;
}

void cc1101_write_burst(cc1101_t *dev, uint8_t addr, const uint8_t *data, size_t length)
{
    cc1101_send_header(dev, CC1101_WRITE_BURST(addr));
    spi_interface_transfer(dev->spi, data, NULL, length);
    spi_interface_cs_high(dev->spi);
}

void cc1101_read_burst(cc1101_t *dev, uint8_t addr, uint8_t *data, size_t length)
{
    uint8_t dummy_tx[8] = {0};
    cc1101_send_header(dev, CC1101_READ_BURST(addr));
    spi_interface_transfer(dev->spi, dummy_tx, data, length);
    spi_interface_cs_high(dev->spi);
}

uint8_t cc1101_read_status(cc1101_t *dev, uint8_t status_addr)
{
    // Status registers require the burst bit set even for a "single" read
    // (see datasheet Section 10.2: burst bit disambiguates status registers
    // from command strobes for addresses 0x30-0x3D).
    uint8_t dummy_tx = 0x00, rx = 0;
    cc1101_send_header(dev, CC1101_READ_BURST(status_addr));
    spi_interface_transfer(dev->spi, &dummy_tx, &rx, 1);
    spi_interface_cs_high(dev->spi);
    return rx;
}

bool cc1101_probe(cc1101_t *dev)
{
    cc1101_strobe(dev, CC1101_STROBE_SRES);

    // Every cc1101_* transaction independently does its own CS-low +
    // wait-for-ready before clocking a header, so this next call already
    // satisfies the datasheet's "wait for SO low again before the next
    // header byte" requirement after SRES -- no extra handling needed here.
    uint8_t ids[2];
    cc1101_read_burst(dev, CC1101_STATUS_PARTNUM, ids, sizeof(ids));
    dev->partnum = ids[0];
    dev->version = ids[1];

    const uint8_t test_value = 0xA5;
    cc1101_write_reg(dev, CC1101_REG_SYNC1, test_value);
    uint8_t readback = cc1101_read_reg(dev, CC1101_REG_SYNC1);

    return (dev->partnum == 0x00) && (readback == test_value);
}

bool cc1101_set_cw(cc1101_t *dev)
{
    // Unmodulated carrier via OOK/ASK with an all-ones data stream and
    // infinite packet length -- every register value here is derived and
    // cross-checked against the TI CC1101 datasheet (SWRS061I); see the
    // plan/commit message for the full register-by-register reasoning.
    // Data rate ~10kBaud (DRATE_E=8, DRATE_M=0x93 @ 26MHz XOSC) instead of
    // the ~115kBaud reset default -- at 115kBaud a 64-byte FIFO drains in
    // ~4.5ms, which a 1ms software refill loop can't reliably outrun (any
    // scheduling jitter underflows it, producing a carrier that cuts in and
    // out instead of staying continuous). At ~10kBaud the same FIFO lasts
    // ~51ms, giving a wide margin.
    cc1101_write_reg(dev, CC1101_REG_MDMCFG4, 0xC8);
    cc1101_write_reg(dev, CC1101_REG_MDMCFG3, 0x93);
    cc1101_write_reg(dev, CC1101_REG_MDMCFG2, 0x30); // MOD_FORMAT=ASK/OOK, SYNC_MODE=none
    cc1101_write_reg(dev, CC1101_REG_PKTCTRL0, 0x02); // no whitening/CRC, infinite packet length
    cc1101_write_reg(dev, CC1101_REG_FREND0, 0x11);   // PA_POWER=1 (PATABLE index for a '1' bit)
    cc1101_write_reg(dev, CC1101_REG_MCSM0, 0x14);    // FS_AUTOCAL on IDLE->TX

    // 433.92MHz @ 26MHz XOSC: FREQ = round(433.92e6 * 2^16 / 26e6) = 0x10B071
    cc1101_write_reg(dev, CC1101_REG_FREQ2, 0x10);
    cc1101_write_reg(dev, CC1101_REG_FREQ1, 0xB0);
    cc1101_write_reg(dev, CC1101_REG_FREQ0, 0x71);

    // PATABLE[0] = power for a '0' bit (off, never actually sent -- our
    // data is all-ones); PATABLE[1] = power for a '1' bit, 0x60 = 0dBm at
    // 433MHz (datasheet Table 39) -- a safe default for bench testing.
    const uint8_t patable[2] = {0x00, 0x60};
    cc1101_write_burst(dev, CC1101_REG_PATABLE, patable, sizeof(patable));

    uint8_t fifo_fill[CC1101_TX_FIFO_SIZE];
    memset(fifo_fill, 0xFF, sizeof(fifo_fill));
    cc1101_write_burst(dev, CC1101_REG_FIFO, fifo_fill, sizeof(fifo_fill));

    cc1101_strobe(dev, CC1101_STROBE_STX);
    return true;
}
