#include <stdint.h>
#include <string.h>

#include <argentum/cpu.h>
#include <argentum/cprintf.h>
#include <argentum/drivers/eth.h>
#include <argentum/mm/memlayout.h>
#include <argentum/mm/page.h>
#include <argentum/irq.h>
#include <argentum/types.h>

// RX an TX FIFO ports, divided by 4 for use as uint32_t[] indices
#define RX_DATA_FIFO_PORT   (0x00 / 4)
#define TX_DATA_FIFO_PORT   (0x20 / 4)
#define RX_STATUS_FIFO_PORT (0x40 / 4)
#define RX_STATUS_FIFO_PEEK (0x44 / 4)
#define TX_STATUS_FIFO_PORT (0x48 / 4)
#define TX_STATUS_FIFO_PEEK (0x4C / 4)

// Direct address registers, divided by 4 for use as uint32_t[] indices
#define ID_REV              (0x50 / 4)    // Chip ID and Revision
#define IRQ_CFG             (0x54 / 4)    // Main Interrupt Configuration
#define   IRQ_INT             (1 << 12)   // Master Interrupt
#define   IRQ_EN              (1 << 8)    //   IRQ Enable
#define   IRQ_POL             (1 << 4)    //   IRQ Polarity
#define   IRQ_TYPE            (1 << 0)    //   IRQ Buffer Type
#define INT_STS             (0x58 / 4)    // Interrupt Status
#define   SW_INT              (1 << 31)   //   Software Interrupt
#define   TXSTOP_INT          (1 << 25)   //   TX Stopped
#define   RXSTOP_INT          (1 << 24)   //   RX Stopped
#define   RXDFH_INT           (1 << 23)   //   RX Dropped Frame Counter Halfway
#define   TX_IOC_INT          (1 << 21)   //   TX IOC Interrupt
#define   RXD_INT             (1 << 20)   //   RX DMA Interrupt
#define   GPT_INT             (1 << 19)   //   GP Timer
#define   PHY_INT             (1 << 18)   //   PHY
#define   PME_INT             (1 << 17)   //   Power Management Event Interrupt
#define   TXSO_INT            (1 << 16)   //   TX Status FIFO Overflow
#define   RWT_INT             (1 << 15)   //   Receive Watchdog Time-out
#define   RXE_INT             (1 << 14)   //   Receiver Error
#define   TXE_INT             (1 << 13)   //   Transmitter Error
#define   TDFO_INT            (1 << 10)   //   TX Data FIFO Overrun Interrupt
#define   TDFA_INT            (1 << 9)    //   TX Data FIFO Available Interrupt
#define   TSFF_INT            (1 << 8)    //   TX Status FIFO Full Interrupt
#define   TSFL_INT            (1 << 7)    //   TX Status FIFO Level Interrupt
#define   RXDF_INT            (1 << 6)    //   RX Dropped Frame Interrupt
#define   RSFF_INT            (1 << 4)    //   RX Status FIFO Full Interrupt
#define   RSFL_INT            (1 << 3)    //   RX Status FIFO Level Interrupt
#define   GPIO2_INT           (1 << 2)    //   GPIO Interrupt #2
#define   GPIO1_INT           (1 << 1)    //   GPIO Interrupt #1
#define   GPIO0_INT           (1 << 0)    //   GPIO Interrupt #0
#define INT_EN              (0x5C / 4)    // Interrupt Enable
#define BYTE_TEST           (0x64 / 4)    // Read-only byte order testing
#define FIFO_INT            (0x68 / 4)    // FIFO Level Interrupts
#define RX_CFG              (0x6C / 4)    // Receive Configuration
#define   RX_CFG_RX_DUMP      (1 << 15)   //   Force RX Discard
#define TX_CFG              (0x70 / 4)    // Transmit Configuration
#define   TX_CFG_TXD_DUMP     (1 << 14)   //   Force TX Data Discard
#define   TX_CFG_TXS_DUMP     (1 << 15)   //   Force TX Status Discard
#define   TX_CFG_TX_ON        (1 << 1)
#define   TX_CFG_STOP_TX      (1 << 0)
#define HW_CFG              (0x74 / 4)    // Hardware Configuration
#define   HW_CFG_SRST         (1 << 0)    //   Soft Reset
#define   HW_CFG_MBO          (1 << 20)   //   Must Be One
#define RX_DP_CTL           (0x78 / 4)    // RX Datapath Control
#define RX_FIFO_INF         (0x7C / 4)    // Receive FIFO Information
#define TX_FIFO_INF         (0x80 / 4)    // Transmit FIFO Information
#define PMT_CTRL            (0x84 / 4)    // Power Management Control
#define   PMT_CTRL_MODE_MASK  (3 << 12)   // Power Management Mode bitmask
#define   PMT_CTRL_PHY_RST    (1 << 10) 
#define   PMT_CTRL_PME_EN     (1 << 1)
#define   PMT_CTRL_READY      (1 << 0)
#define GPIO_CFG            (0x88 / 4)    // General Purpose IO Configuration
#define GPT_CFG             (0x8C / 4)    // General Purpose Timer Configuration
#define GPT_CNT             (0x90 / 4)    // General Purpose Timer Count
#define WORD_SWAP           (0x98 / 4)    // Word Swap Register
#define FREE_RUN            (0x9C / 4)    // Free Run Counter
#define RX_DROP             (0xA0 / 4)    // RX Dropped Frames Counter
#define MAC_CSR_CMD         (0xA4 / 4)    // MAC CSR Synchronizer Command
#define   MAC_CSR_CMD_BUSY    (1 << 31)   //   CSR Busy
#define   MAC_CSR_CMD_RNW     (1 << 30)   //   R/nW
#define   MAC_CSR_CMD_ADDR  0xFF          // CSR Address
#define MAC_CSR_DATA        (0xA8 / 4)    // MAC CSR Synchronizer Data
#define AFC_CFG             (0xAC / 4)    // Automatic Flow Control Config
#define E2P_CMD             (0xB0 / 4)    // EEPROM command
#define E2P_DATA            (0xB4 / 4)    // EEPROM Data

static volatile uint32_t *eth;

#define MAC_CR              1
#define   MAC_CR_RXEN         (1 << 2)
#define   MAC_CR_TXEN         (1 << 3)
#define   MAC_CR_BCAST        (1 << 11)
#define   MAC_CR_PRMS         (1 << 18)
#define MAC_ADDRH           2
#define MAC_ADDRL           3
#define MAC_HASHH           4
#define MAC_HASHL           5
#define MAC_MII_ACC         6
#define   MAC_MII_ACC_BZY     (1 << 0)
#define   MAC_MII_ACC_WNR     (1 << 1)
#define MAC_MII_DATA        7
#define MAC_FLOW            8

static void eth_irq(void);

// Read from a MAC register
uint32_t
mac_read(uint8_t reg)
{
  uint32_t cmd, data;
  
  cmd = (reg & 0xFF) | MAC_CSR_CMD_BUSY | MAC_CSR_CMD_RNW;
  eth[MAC_CSR_CMD] = cmd;

  while (eth[MAC_CSR_CMD] & MAC_CSR_CMD_BUSY)
    ;

  data = eth[MAC_CSR_DATA];
  return data;
}

// Write to a MAC register
void
mac_write(uint8_t reg, uint32_t data)
{
  uint32_t cmd;
  
  cmd = (reg & 0xFF) | MAC_CSR_CMD_BUSY;

  eth[MAC_CSR_DATA] = data;
  eth[MAC_CSR_CMD]  = cmd;

  while (eth[MAC_CSR_CMD] & MAC_CSR_CMD_BUSY)
    ;
}

#define PHY_BCR             0
#define   PHY_BCR_FDPLX       (1 << 8)
#define   PHY_BCR_RSTAN       (1 << 9)
#define   PHY_BCR_PDOWN       (1 << 11)
#define   PHY_BCR_SS          (1 << 13)
#define   PHY_BCR_ANE         (1 << 12)
#define   PHY_BCR_RESET       (1 << 15)
#define PHY_BSR             1
#define   PHY_BSR_ANC         (1 << 5)
#define PHY_ID1             2
#define PHY_ID2             3
#define PHY_ANAR            4

// Read from a PHY register
uint16_t
eth_phy_read(uint8_t reg)
{
  uint32_t cmd, data;

  cmd = (1 << 11) | ((reg & 0x1F) << 6);
  mac_write(MAC_MII_ACC, cmd);

  while (mac_read(MAC_MII_ACC) & MAC_MII_ACC_BZY)
    ;

  data = mac_read(MAC_MII_DATA);
  return data;
}

// Write to a PHY register
void
eth_phy_write(uint8_t reg, uint16_t data)
{
  uint32_t cmd;
  
  cmd = (1 << 11) | ((reg & 0x1F) << 6) | MAC_MII_ACC_WNR;

  mac_write(MAC_MII_DATA, data);
  mac_write(MAC_MII_ACC, cmd);

  while (mac_read(MAC_MII_ACC) & MAC_MII_ACC_BZY)
    ;
}

uint8_t mac_addr[6];

void
eth_init(void)
{
  uint32_t mac_addr_lo, mac_addr_hi, mac_cr;

  eth = (volatile uint32_t *) PA2KVA(PHYS_ETH);

  // Write BYTE_TEST to wake chip up in case it is in sleep mode.
  eth[BYTE_TEST] = 0;

  // Software reset.
  eth[HW_CFG] = HW_CFG_SRST;
  while (eth[HW_CFG] & HW_CFG_SRST)
    ;

  // Enable PME_EN & PME_POL to active low
  eth[PMT_CTRL] |= PMT_CTRL_PME_EN;

  // Disable all interrupts, clear any pending status, and configure IRQ_CFG.
  eth[INT_EN]  = 0;
  eth[INT_STS] = 0xFFFFFFFF;
  eth[IRQ_CFG] = IRQ_EN | IRQ_POL | IRQ_TYPE;

  // Read the mac address
  mac_addr_lo = mac_read(MAC_ADDRL);
  mac_addr_hi = mac_read(MAC_ADDRH);
  mac_addr[0] = mac_addr_lo & 0xFF;
  mac_addr[1] = (mac_addr_lo >> 8) & 0xFF;
  mac_addr[2] = (mac_addr_lo >> 16) & 0xFF;
  mac_addr[3] = (mac_addr_lo >> 24) & 0xFF;
  mac_addr[4] = mac_addr_hi & 0xFF;
  mac_addr[5] = (mac_addr_hi >> 8) & 0xFF;

  // Reset the PHY
  eth_phy_write(PHY_BCR, eth_phy_read(PHY_BCR) | PHY_BCR_RESET);
  while (eth_phy_read(PHY_BCR) & PHY_BCR_RESET)
    ;

  // Setup TLI store-and-forward, and preserve TxFifo size
  eth[HW_CFG] = (eth[HW_CFG] & ((0xF << 16) | 0xFFF)) | HW_CFG_MBO;
  
  // Set transmit configuration
  eth[TX_CFG] = TX_CFG_TX_ON;

  // Set receive configuration
  eth[RX_CFG] = 0x200;

  // Setup MAC for TX and RX
  mac_cr = mac_read(MAC_CR);
  mac_cr |= (MAC_CR_TXEN | MAC_CR_RXEN);
  mac_write(MAC_CR, mac_cr);

  eth[FIFO_INT] = 0xFF000000;

  // Enable interrupts
  eth[INT_EN] |= RSFL_INT;

  irq_attach(IRQ_ETH, eth_irq, 0);
}

static void
eth_rx(void)
{
  uint32_t rx_used;
  
  rx_used = (eth[RX_FIFO_INF] >> 16) & 0xFF;

  while(rx_used > 0) {
    uint32_t rx_status, packet_len;

    rx_status = eth[RX_STATUS_FIFO_PORT];
    packet_len = (rx_status >> 16) & 0x3FFF;

    if (rx_status & (1 << 15)) {
      // Packet has error: discard and update status
      uint32_t i, tmp;

      for (i = ROUND_UP(packet_len, sizeof(uint32_t)); i > 0; i--)
        tmp = eth[RX_DATA_FIFO_PORT];
      (void) tmp;
    } else {
      uint32_t i;
      struct Page *p;
      uint32_t *data;
      uint8_t *packet;
  
      p = page_alloc_one(PAGE_ALLOC_ZERO);
      packet = (uint8_t *) page2kva(p);
      data = (uint32_t *) page2kva(p);

      for (i = ROUND_UP(packet_len, sizeof(uint32_t)); i > 0; i--)
        *data++ = eth[RX_DATA_FIFO_PORT];

      cprintf("Received packet (length = %d):\n", packet_len);
      for (uint32_t i = 0; i < packet_len; i++) {
        cprintf(" %02x", packet[i + 2]);
        if ((i % 26 == 25) || (i == packet_len - 1))
          cprintf("\n");
      }

      page_free_one(p);
    }

    rx_used = (eth[RX_FIFO_INF] >> 16) & 0xFF;
  }
}

static void
eth_irq(void)
{
  uint32_t status;

  cpu_irq_save();

  // Wake up
  while (!(eth[PMT_CTRL]) & PMT_CTRL_READY)
    eth[BYTE_TEST] = 0xFFFFFFFF;

  if (!(eth[IRQ_CFG] & IRQ_INT))
    warn("Unexpected IRQ");

  status = eth[INT_STS] & eth[INT_EN];

  if (status & RSFL_INT) {
    eth_rx();
    eth[INT_STS] |= RSFL_INT;
  }

  if (status & ~(RSFL_INT))
    panic("Unexpected interupt %x", status & ~(RSFL_INT));

  cpu_irq_restore();
}

void
eth_write(const void *buf, size_t n)
{
  static int last_tag;
  
  uint32_t *data;
  uint32_t cmd_a, cmd_b;

  data = (uint32_t *) buf;

  cmd_a = (((uintptr_t) buf & 0x3) << 16) | 0x00003000 | (n + 14);
  cmd_b = (last_tag++ << 16) | (n + 14);

  cpu_irq_save();

  eth[TX_DATA_FIFO_PORT] = cmd_a;
  eth[TX_DATA_FIFO_PORT] = cmd_b;

  for (uint32_t i = (n+14+3)/sizeof(uint32_t); i > 0; i--)
    eth[TX_DATA_FIFO_PORT] = *data++;

  eth[TX_CFG] = TX_CFG_TX_ON;

  eth[TX_CFG] = TX_CFG_STOP_TX;

  cpu_irq_restore();
}
