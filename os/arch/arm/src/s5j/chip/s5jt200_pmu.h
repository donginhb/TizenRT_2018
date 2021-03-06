/****************************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * arch/arm/src/s5j/chip/s5j200_pmu.h
 *
 *   Copyright (C) 2009-2010, 2014-2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_S5J_CHIP_S5JT200_PMU_H
#define __ARCH_ARM_SRC_S5J_CHIP_S5JT200_PMU_H

/****************************************************************************
 * Include
 ****************************************************************************/
#include "s5j_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/* Power Management Unit Register Offsets ***********************************/
#define S5J_PMU_WIFI_CTRL_NS					(S5J_PMU_BASE + 0x0140)	/* Non-Secure WIFI Control Register */
#define S5J_PMU_WIFI_CTRL_S						(S5J_PMU_BASE + 0x0144) /* Secure WIFI Control Register */
#define S5J_PMU_WIFI_STAT						(S5J_PMU_BASE + 0x0148) /* WIFI Status Register */
#define S5J_PMU_WIFI_DEBUG						(S5J_PMU_BASE + 0x014C) /* WIFI Debug Register */
#define S5J_PMU_CENTRAL_SEQ_WIFI_CONFIGURATION	(S5J_PMU_BASE + 0x0380) /* Central Sequencer Configuration Register for WIFI */
#define S5J_PMU_CENTRAL_SEQ_WIFI_STATUS			(S5J_PMU_BASE + 0x0384) /* Central Sequencer Status Register for WIFI */
#define S5J_PMU_SWRESET							(S5J_PMU_BASE + 0x0400) /* Software Reset Register */
#define S5J_PMU_MASK_WDT_RESET_REQUEST			(S5J_PMU_BASE + 0x040C) /* Mask the WDT Reset Request Register */
#define S5J_PMU_SPARE0							(S5J_PMU_BASE + 0x0900) /* PMU Spare Register 0 */
#define S5J_PMU_CLEANY_BUS_WIFI_SYS_PWR_REG		(S5J_PMU_BASE + 0x1324) /* CLEANY_BUS_WIFI System Power Configuration Register */
#define S5J_PMU_LOGIC_RESET_WIFI_SYS_PWR_REG	(S5J_PMU_BASE + 0x1328) /* LOGIC_RESET_WIFI System Power Configuration Register */
#define S5J_PMU_TCXO_GATE_WIFI_SYS_PWR_REG		(S5J_PMU_BASE + 0x132C) /* TCXO_GATE_WIFI System Power Configuration Register */
#define S5J_PMU_RESET_ASB_WIFI_SYS_PWR_REG		(S5J_PMU_BASE + 0x1330) /* RESET_ASB_WIFI System Power Configuration Register */

/* SWRESET Register Address *************************************************/
#define S5J_PMU_SYSTEM_SHIFT			0
#define S5J_PMU_SYSTEM_RESET_OFF		(0x0 << S5J_PMU_SYSTEM_SHIFT)
#define S5J_PMU_SYSTEM_RESET_ON			(0x1 << S5J_PMU_SYSTEM_SHIFT)

/* MASK_WDT_RESET_REQUEST Register Address **********************************/
#define S5J_PMU_CR4_WDTRESET_SHIFT		23
#define S5J_PMU_CR4_WDTRESET_MASK		(0x1 << S5J_PMU_CR4_WDTRESET_SHIFT)
#define S5J_PMU_CR4_WDTRESET_OFF		(0x0 << S5J_PMU_CR4_WDTRESET_SHIFT)
#define S5J_PMU_CR4_WDTRESET_ON			(0x1 << S5J_PMU_CR4_WDTRESET_SHIFT)

#endif /* __ARCH_ARM_SRC_S5J_CHIP_S5JT200_PMU_H */
