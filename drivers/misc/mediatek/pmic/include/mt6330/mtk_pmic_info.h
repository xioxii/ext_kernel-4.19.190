/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MT_PMIC_INFO_H_
#define _MT_PMIC_INFO_H_

/*
 * The CHIP INFO
 */
#define PMIC6330_CID_CODE_H       0x30
#define PMIC6330_E1_CID_CODE_L    0x10
#define PMIC6330_E2_CID_CODE_L    0x20

#if 0
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define IPIMB
#endif
#endif

extern unsigned int pmic_ipi_test_code(void);

/*
 * Debugfs
 */
#define PMICTAG                "[PMIC] "
extern unsigned int gPMICDbgLvl;
extern unsigned int gPMICHKDbgLvl;
extern unsigned int gPMICIRQDbgLvl;
extern unsigned int gPMICREGDbgLvl;

#define PMIC_LOG_DBG     4
#define PMIC_LOG_INFO    3
#define PMIC_LOG_NOT     2
#define PMIC_LOG_WARN    1
#define PMIC_LOG_ERR     0

#define PMICLOG(fmt, arg...) do { \
	if (gPMICDbgLvl >= PMIC_LOG_DBG) \
		pr_notice(PMICTAG "%s: " fmt, __func__, ##arg); \
} while (0)

#define HKLOG(fmt, arg...) do { \
	if (gPMICHKDbgLvl >= PMIC_LOG_DBG) \
		pr_notice(PMICTAG "%s: " fmt, __func__, ##arg); \
} while (0)

#define IRQLOG(fmt, arg...) do { \
	if (gPMICIRQDbgLvl >= PMIC_LOG_DBG) \
		pr_notice(PMICTAG "%s: " fmt, __func__, ##arg); \
} while (0)

#define RGLTRLOG(fmt, arg...) do { \
	if (gPMICREGDbgLvl >= PMIC_LOG_DBG) \
		pr_notice(PMICTAG "%s: " fmt, __func__, ##arg); \
} while (0)

extern void wk_pmic_enable_sdn_delay(void);

#endif				/* _MT_PMIC_INFO_H_ */
