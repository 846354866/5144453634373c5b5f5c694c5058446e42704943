/*!
\copyright  Copyright (c) 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_config.c
\brief	    Config data
*/

#include "av_headset_config.h"

#if defined(INCLUDE_PROXIMITY)
#if   defined(HAVE_VNCL3020)

#include "peripherals/vncl3020.h"
const struct __proximity_config proximity_config = {
    .threshold_low = 3000,
    .threshold_high = 3500,
    .threshold_counts = vncl3020_threshold_count_4,
    .rate = vncl3020_proximity_rate_7p8125_per_second,
    .i2c_clock_khz = 100,
    .pios = {
        /* The PROXIMITY_PIO definitions are defined in the platform x2p file */
        .on = PROXIMITY_PIO_ON,
        .i2c_scl = PROXIMITY_PIO_I2C_SCL,
        .i2c_sda = PROXIMITY_PIO_I2C_SDA,
        .interrupt = PROXIMITY_PIO_INT,
    },
};

#else
#error INCLUDE_PROXIMITY was defined, but no proximity sensor type was defined.
#endif   /* HAVE_VNCL3020 */
#endif /* INCLUDE_PROXIMITY */

#if defined(INCLUDE_ACCELEROMETER)
#if   defined(HAVE_ADXL362)

#include "peripherals/adxl362.h"
const struct __accelerometer_config accelerometer_config = {
    /* 250mg activity threshold, magic value from datasheet */
    .activity_threshold = 0x00FA,
    /* 150mg activity threshold, magic value from datasheet */
    .inactivity_threshold = 0x0096,
    /* Inactivity timer is about 5 seconds */
    .inactivity_timer = 30,
    .spi_clock_khz = 400,
    .pios = {
        /* The ACCELEROMETER_PIO definitions are defined in the platform x2p file */
        .on = ACCELEROMETER_PIO_ON,
        .spi_clk = ACCELEROMETER_PIO_SPI_CLK,
        .spi_cs = ACCELEROMETER_PIO_SPI_CS,
        .spi_mosi = ACCELEROMETER_PIO_SPI_MOSI,
        .spi_miso = ACCELEROMETER_PIO_SPI_MISO,
        .interrupt = ACCELEROMETER_PIO_INT,
    },
};
#else
#error INCLUDE_ACCELEROMETER was defined, but no accelerometer type was defined.
#endif   /* HAVE_ADXL362*/
#endif /* INCLUDE_ACCELEROMETER */
