#ifndef KERNEL_SBCON_H
#define KERNEL_SBCON_H

/**
 * @file kernel/sbcon.h
 * 
 * Two-wire serial bus interface (SBCon).
 * 
 * PBX-A9 has two serial bus inerfaces (SBCon0 and SBCon1). SBCon0 provides
 * access to the Maxim DS1338 RTC on the baseboard.
 *
 * For more info on serial bus programming, see this tutorial:
 * @link https://www.robot-electronics.co.uk/i2c-tutorial
 */

#define SB_CON0       0x10002000      ///< SBCon0 memory base address

#define SCL           (1 << 0)        ///< Clock line
#define SDA           (1 << 1)        ///< Data line

/** @name SBRegs
 *  Serial bus registers, shifted right by 2 bits for use as uint32_t[] indices
 */
///@{
#define SB_CONTROL    (0x000 >> 2)    ///< Read serial control bits
#define SB_CONTROLS   (0x000 >> 2)    ///< Set serial control bits
#define SB_CONTROLC   (0x004 >> 2)    ///< Clear serial control bits
///@}

#define SB_RTC        0xD0            ///< RTC device address

/** @name RTCRegisters
 *  RTC Registers. For more info, see the data sheet for the Maxim DS1338 RTC
 */
///@{
#define RTC_SECONDS   0x00
#define RTC_MINUTES   0x01
#define RTC_HOURS     0x02
#define RTC_DAY       0x03
#define RTC_DATE      0x04
#define RTC_MONTH     0x05
#define RTC_YEAR      0x06
#define RTC_CONTROL   0x07
///@}

/**
 * Initialize the serial bus driver.
 */
void sb_init(void);

/**
 * Display time data from the Time-of-Year RTC chip.
 */
void sb_rtc_time(void);

#endif  // !KERNEL_SBCON_H
