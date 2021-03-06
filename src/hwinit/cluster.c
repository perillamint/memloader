/*
* Copyright (c) 2018 naehrwert
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cluster.h"
#include "clock.h"
#include "timer.h"
#include "pmc.h"
#include "flow.h"
#include "t210.h"
#include "i2c.h"
#include "max7762x.h"
#include "max77620.h"

void _cluster_enable_power()
{
	u8 tmp = max77620_recv_byte(MAX77620_REG_AME_GPIO);
	max77620_send_byte(MAX77620_REG_AME_GPIO, tmp & 0xDF);
	max77620_send_byte(MAX77620_REG_GPIO5, 0x09);

	//Enable cores power.
	i2c_send_byte(I2C_5, 0x1B, 0x2, 0x20);
	i2c_send_byte(I2C_5, 0x1B, 0x3, 0x8D);
	i2c_send_byte(I2C_5, 0x1B, 0x0, 0xB7);
	i2c_send_byte(I2C_5, 0x1B, 0x1, 0xB7);
}

int _cluster_pmc_enable_partition(u32 part, u32 toggle)
{
	//Check if the partition has already been turned on.
	if (PMC(APBDEV_PMC_PWRGATE_STATUS) & part)
		return 1;

	u32 startTime = get_tmr();
	while (PMC(APBDEV_PMC_PWRGATE_TOGGLE) & 0x100)
	{
		if (get_tmr() - startTime >= 5000) //only wait for 5ms
			return 0;
	}

	PMC(APBDEV_PMC_PWRGATE_TOGGLE) = toggle | 0x100;

	startTime = get_tmr();
	while ((PMC(APBDEV_PMC_PWRGATE_STATUS) & part) == 0)
	{
		if (get_tmr() - startTime >= 5000) //only wait for 5ms
			return 0;
	}

	return 1;
}

void cluster_boot_cpu0(u32 entry)
{
	struct flow_ctlr* const flow = (void *)FLOW_CTLR_BASE;

	//Set ACTIVE_CLUSER to FAST.
	flow->cluster_control &= 0xFFFFFFFE;

	_cluster_enable_power();

	if (!(CLOCK(CLK_RST_CONTROLLER_PLLX_BASE) & 0x40000000))
	{
		CLOCK(CLK_RST_CONTROLLER_PLLX_MISC_3) &= 0xFFFFFFF7;
		sleep(2);
		CLOCK(CLK_RST_CONTROLLER_PLLX_BASE) = 0x80404E02;
		CLOCK(CLK_RST_CONTROLLER_PLLX_BASE) = 0x00404E02;
		CLOCK(CLK_RST_CONTROLLER_PLLX_MISC) |= 0x40000;
		CLOCK(CLK_RST_CONTROLLER_PLLX_BASE) = 0x40404E02;
	}
	while (!(CLOCK(CLK_RST_CONTROLLER_PLLX_BASE) & 0x8000000)) {}

	//Configure MSELECT source and enable clock.
	CLOCK(CLK_RST_CONTROLLER_CLK_SOURCE_MSELECT) = (CLOCK(CLK_RST_CONTROLLER_CLK_SOURCE_MSELECT) & 0x1FFFFF00) | 6;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_V) |= CLK_V_MSELECT;

	//Configure initial CPU clock frequency and enable clock.
	CLOCK(CLK_RST_CONTROLLER_CCLK_BURST_POLICY) = 0x20008888;
	CLOCK(CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER) = 0x80000000;
	CLOCK(CLK_RST_CONTROLLER_CLK_ENB_V_SET) = CLK_V_CPUG;

	clock_enable_coresight();

	//CAR2PMC_CPU_ACK_WIDTH should be set to 0.
	CLOCK(CLK_RST_CONTROLLER_CPU_SOFTRST_CTRL2) &= 0xFFFFF000;

	//Enable CPU rail.
	_cluster_pmc_enable_partition(1, 0);
	//Enable cluster 0 non-CPU.
	_cluster_pmc_enable_partition(0x8000, 15);
	//Enable CE0.
	_cluster_pmc_enable_partition(0x4000, 14);

	//Request and wait for RAM repair.
	flow->ram_repair = 1;
	while (!(flow->ram_repair & 2)) {}

	EXCP_VEC(0x100) = 0;
	
	//Keep bootrom accessible after cluster boot
	SB(SB_PIROM_START) = 96*1024;	
	//Set reset vector.
	SB(SB_AA64_RESET_LOW) = entry | 1;
	SB(SB_AA64_RESET_HIGH) = 0;
	//Non-secure reset vector write disable.
	SB(SB_CSR) = 2;
	(void)SB(SB_CSR);

	//Clear MSELECT reset.
	CLOCK(CLK_RST_CONTROLLER_RST_DEVICES_V) &= ~CLK_V_MSELECT;
	//Clear NONCPU reset.
	CLOCK(CLK_RST_CONTROLLER_RST_CPUG_CMPLX_CLR) = 0x20000000;
	//Clear CPU{0,1,2,3} POR and CORE, CX0, L2, and DBG reset.
	CLOCK(CLK_RST_CONTROLLER_RST_CPUG_CMPLX_CLR) = 0x411F000F;
}
