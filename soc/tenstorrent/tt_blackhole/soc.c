/*
 * Copyright (c) 2025 Tenstorrent AI ULC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <stdint.h>

static inline void delay_spin(uint32_t count)
{
	volatile uint32_t i = count;

	while (i--) {
		/* Spin */
	}
}

void soc_early_init_hook(void)
{
	/* Enable first level interrupts for DW INCT controllers */
	if (DT_HAS_COMPAT_STATUS_OKAY(snps_designware_spi) &&
	    IS_ENABLED(CONFIG_FLASH)) {
		/* Manually toggle the SPI reset control bits */
		volatile uint32_t *RESET_UNIT_SPI_CNTL = (uint32_t *)0x800300F8;
		*RESET_UNIT_SPI_CNTL |= BIT(4);
		/* Delay a few cycles- pre kernel so just use a spin loop */
		delay_spin(1000);
		*RESET_UNIT_SPI_CNTL &= ~BIT(4);
		/* Enable the SPI */
		*RESET_UNIT_SPI_CNTL |= BIT(0);
		/* Disable DDR mode */
		*RESET_UNIT_SPI_CNTL &= ~BIT(1);
	}
}
