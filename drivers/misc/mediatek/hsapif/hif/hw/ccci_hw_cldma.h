// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_HW_CLDMA_H__
#define __CCCI_HW_CLDMA_H__


#include <mt-plat/sync_write.h>
#if 0
/* Must use DTS instead of definitions: h files are not existed in kernel 4.19 */
#include "dt-bindings/basereg/mt6297-mt_reg_base.h"
#include "dt-bindings/irqid/mt6297-mt_irqid.h"
#endif

#define DOMAIN_ID_SAP 0
#define DOMAIN_ID_PCIE 5
#define DOMAIN_ID_MD 7

#define DOMAIN_ID_LEFT_HAND_MASK               (0xFF)
#define DOMAIN_ID_RIGHT_HAND_MASK              (0xFF << 8)
#define DOMAIN_ID_PCIE_MASK                    (0xFF << 16)
#define DOMAIN_ID_MD_MASK                      (0xFF << 24)

#define DOMAIN_ID_LEFT_HAND_SHIFT              (0)
#define DOMAIN_ID_RIGHT_HAND_SHIFT             (8)
#define DOMAIN_ID_PCIE_SHIFT                   (16)
#define DOMAIN_ID_MD_SHIFT                     (24)

/*
please include ccci header file fisrt
this ccci header file expect contain mapped register_base_addr information

such as:
typedef _cldma_base_addr {
    void *cldma_ao_base; //always on domain register base
    void *cldma_pd_base; //power down domain register base
}cldma_base;
*/
#define cldma_write32(b, a, v)			mt_reg_sync_writel(v, (b)+(a))
#define cldma_write16(b, a, v)			mt_reg_sync_writew(v, (b)+(a))
#define cldma_write8(b, a, v)			mt_reg_sync_writeb(v, (b)+(a))

#define cldma_read32(b, a)			ioread32((void __iomem *)((b)+(a)))
#define cldma_read16(b, a)			ioread16((void __iomem *)((b)+(a)))
#define cldma_read8(b, a)			ioread8((void __iomem *)((b)+(a)))

#define REG_IO_READ32(a) cldma_read32(a,0)
#define REG_IO_WRITE32(a,b) cldma_write32(a, 0, b)

extern u64 g_CLDMA_AO_BASE;
extern u64 g_CLDMA_PD_BASE;

/*interrupt status bit meaning , bitmask*/
#define IRQ_TYPE_TXRX 1
#define IRQ_TYPE_EQ 2
#define STATUS_BITMASK 0xFFFF
#define EQ_STA_BIT_OFFSET 8
#define EMPTY_STATUS_BITMASK 0xFF00
#define TXRX_STA_BIT_OFFSET 0
#define TXRX_STATUS_BITMASK 0x00FF

/*bitmap*/
#define HSAPIF_CLDMA_BM_INT_ALL			0xFFFFFFFF
/* L2 interrupt */
#define HSAPIF_CLDMA_TX_INT_DONE		0x000000FF /* when the transmission if the GPD on the specified queue is done */
#define HSAPIF_CLDMA_TX_INT_QUEUE_EMPTY	0x0000FF00 /* when there is no GPD to be transmitted on the specified queue */
#define HSAPIF_CLDMA_TX_INT_ERROR		0x00FF0000 /* error occurred on the specified queue, check L3 for detail */
#define HSAPIF_CLDMA_TX_QE_OFFSET 4

/* HW address @sAP view for reference, eAP is the left user of CLDMA0/CLDMA1*/

/*For CLDMA0: MD keyword is "R hand[sAP]", AP is "L hand[Host]"*/
/*Ref coda file: CLDMA0_AO_INDMA_AO_MD 0x1004A000*/
#define CLDMA_R_AO_BASE 0x1004A000

/*Ref coda file: CLDMA0_AO_INDMA_PD_MD 0x1021E000*/
#define CLDMA_R_PD_BASE 0x1021E000

/*Ref AP Interrupt Investigate file: cldma0_md_int_ap && its APCPU API Naming: CLDMA0_MD_AP_IRQ_BIT*/
/*For kernel, it uses 2nd field (IRQSync_ID), it fill this value to DTS file*/
/*Then read irq_id = irq_of_parse_and_map() to pass irq_id to request_irq() when registering isr*/
#define CLDMA0_R_INT 285

#if 0
/* Must use DTS instead of definitions: h files are not existed in kernel 4.19 */
#define CLDMA_AO_BASE    CLDMA0_AO_AP_BASE// CLDMA0 LEFT_AO_BASE 0x10022000
#define CLDMA_PD_BASE    CLDMA0_AP_BASE// CLDMA0 LEFT_PD_BASE 0x1023C000

#define CLDMA1_AO_BASE    CLDMA1_AO_AP_BASE//CLDMA1 LEFT_AO_BASE 0x10024000
#define CLDMA1_PD_BASE    CLDMA1_AP_BASE//CLDMA1 LEFT_PD_BASE 0x1023E000

#define MHCCIFRC MHCCIF_AO_RC_BASE//0x10027000
#endif

#define BASE_ADDR_CLDMA_AO    (g_CLDMA_AO_BASE)
#define BASE_ADDR_CLDMA_PD    (g_CLDMA_PD_BASE)

#define BASE_ADDR_CLDMA_IN        (BASE_ADDR_CLDMA_PD + 0x00000000)
#define BASE_ADDR_CLDMA_OUT       (BASE_ADDR_CLDMA_PD + 0x00000400)
#define BASE_ADDR_CLDMA_MISC      (BASE_ADDR_CLDMA_PD + 0x00000800)
#define BASE_ADDR_CLDMA_IN_AO     (BASE_ADDR_CLDMA_AO + 0x00000000)
#define BASE_ADDR_CLDMA_OUT_AO    (BASE_ADDR_CLDMA_AO + 0x00000400)
#define BASE_ADDR_CLDMA_MISC_AO   (BASE_ADDR_CLDMA_AO + 0x00000800)


/*CLDMA IN(Tx) AO*/
#define REG_CLDMA_UL_START_ADDRL_0_AO           (0x0004)
#define REG_CLDMA_UL_START_ADDRH_0_AO           (0x0008)

#define REG_CLDMA_UL_CURRENT_ADDRL_0_AO         (0x0044)
#define REG_CLDMA_UL_CURRENT_ADDRH_0_AO         (0x0048)

/*CLDMA IN(Tx) PD*/
#define REG_CLDMA_UL_START_ADDRL_0           (0x0004)
#define REG_CLDMA_UL_START_ADDRH_0           (0x0008)
#define REG_CLDMA_UL_CURRENT_ADDRL_0         (0x0044)
#define REG_CLDMA_UL_CURRENT_ADDRH_0         (0x0048)
#define REG_CLDMA_UL_STATUS                  (0x0084)
#define REG_CLDMA_UL_START_CMD               (0x0088)
#define REG_CLDMA_UL_RESUME_CMD              (0x008C)
#define REG_CLDMA_UL_STOP_CMD                (0x0090)
#define REG_CLDMA_UL_ERROR                   (0x0094)
#define REG_CLDMA_UL_CFG                     (0x0098)
#define REG_CLDMA_UL_DUMMY_0        		 (0x009C)

/*CLDMA OUT(Rx) PD*/
#define REG_CLDMA_SO_ERROR      		    (0x0400 + 0x0100)
#define REG_CLDMA_SO_START_CMD			    (0x0400 + 0x01BC)
#define REG_CLDMA_SO_RESUME_CMD 		    (0x0400 + 0x01C0)
#define REG_CLDMA_SO_STOP_CMD  			    (0x0400 + 0x01C4)
#define REG_CLDMA_SO_DUMMY_0			    (0x0400 + 0x0108)

/*CLDMA OUT(Rx) AO*/
#define REG_CLDMA_SO_CFG        		      (0x0400 + 0x0004)
#define REG_CLDMA_SO_START_ADDRL_0            (0x0400 + 0x0078)
#define REG_CLDMA_SO_START_ADDRH_0            (0x0400 + 0x007C)
#define REG_CLDMA_SO_CURRENT_ADDRL_0          (0x0400 + 0x00B8)
#define REG_CLDMA_SO_CURRENT_ADDRH_0          (0x0400 + 0x00BC)
#define REG_CLDMA_SO_STATUS     		      (0x0400 + 0x00F8)
#define REG_CLDMA_DEBUG_ID_EN  			      (0x0400 + 0x00FC)
#define REG_CLDMA_SO_LAST_UPDATE_ADDRL_0	  (0x0400 + 0x0100)
#define REG_CLDMA_SO_LAST_UPDATE_ADDRH_0	  (0x0400 + 0x0104)

/*CLDMA MISC PD*/
#define REG_CLDMA_L2TISAR0                   (0x0800 + 0x0010)
#define REG_CLDMA_L2TISAR1                   (0x0800 + 0x0014)
#define REG_CLDMA_L2TIMR0                    (0x0800 + 0x0018)
#define REG_CLDMA_L2TIMR1                    (0x0800 + 0x001C)
#define REG_CLDMA_L2TIMCR0                   (0x0800 + 0x0020)
#define REG_CLDMA_L2TIMCR1                   (0x0800 + 0x0024)
#define REG_CLDMA_L2TIMSR0                   (0x0800 + 0x0028)
#define REG_CLDMA_L2TIMSR1                   (0x0800 + 0x002C)
#define REG_CLDMA_L3TISAR0                   (0x0800 + 0x0030)
#define REG_CLDMA_L3TISAR1                   (0x0800 + 0x0034)
#define REG_CLDMA_L3TIMR0                    (0x0800 + 0x0038)
#define REG_CLDMA_L3TIMR1                    (0x0800 + 0x003C)
#define REG_CLDMA_L3TIMCR0                   (0x0800 + 0x0040)
#define REG_CLDMA_L3TIMCR1                   (0x0800 + 0x0044)
#define REG_CLDMA_L3TIMSR0                   (0x0800 + 0x0048)
#define REG_CLDMA_L3TIMSR1                   (0x0800 + 0x004C)
#define REG_CLDMA_L2RISAR0                   (0x0800 + 0x0050)
#define REG_CLDMA_L2RISAR1                   (0x0800 + 0x0054)
#define REG_CLDMA_L3RISAR0                   (0x0800 + 0x0070)
#define REG_CLDMA_L3RISAR1                   (0x0800 + 0x0074)
#define REG_CLDMA_L3RIMR0                    (0x0800 + 0x0078)
#define REG_CLDMA_L3RIMR1                    (0x0800 + 0x007C)
#define REG_CLDMA_L3RIMCR0                   (0x0800 + 0x0080)
#define REG_CLDMA_L3RIMCR1                   (0x0800 + 0x0084)
#define REG_CLDMA_L3RIMSR0                   (0x0800 + 0x0088)
#define REG_CLDMA_L3RIMSR1                   (0x0800 + 0x008C)
#define REG_CLDMA_IP_BUSY                    (0x0800 + 0x00B4)
#define REG_CLDMA_L3TISAR2                   (0x0800 + 0x00C0)
#define REG_CLDMA_L3TIMR2                    (0x0800 + 0x00C4)
#define REG_CLDMA_L3TIMCR2                   (0x0800 + 0x00C8)
#define REG_CLDMA_L3TIMSR2                   (0x0800 + 0x00CC)

/*CLDMA MISC AO*/
#define REG_CLDMA_L2RIMR0                    (0x0800 + 0x0058)
#define REG_CLDMA_L2RIMR1                    (0x0800 + 0x005C)
#define REG_CLDMA_L2RIMCR0                   (0x0800 + 0x0060)
#define REG_CLDMA_L2RIMCR1                   (0x0800 + 0x0064)
#define REG_CLDMA_L2RIMSR0                   (0x0800 + 0x0068)
#define REG_CLDMA_L2RIMSR1                   (0x0800 + 0x006C)

#define REG_CLDMA_DUMMY                      (0x0800 + 0x0150)

/*Note1: Colgin M80's INT_MASK and BUSY_MASK register is not same with Apollo M70*/
#define REG_CLDMA_INT_BUSY_MASK             (0x0800 + 0x0154)
#define REG_CLDMA_INT_SAP_MASK               (0x0800 + 0x0158)
/*Note1: Apollo M70's INT_MASK and BUSY_MASK register is not same with Colgin M80*/
/*Note2: The following two registers addresses are used only in M70*/
#define REG_CLDMA_INT_MASK                   (0x0800 + 0x0154)
#define REG_CLDMA_BUSY_MASK                  (0x0800 + 0x0158)


#define REG_CLDMA_DOMAIN_ID                  (0x0800 + 0x016C)
#define REG_CLDMA_DOMAIN_ID_LOCK             (0x0800 + 0x0170)

//#define REG_CLDMA_BUS_CFG                             (0x0800 + 0x0058)


#define R_CLDMA_TQ_IS_ACTIVE(base_addr, _qid) (REG_IO_READ32(base_addr+REG_CLDMA_UL_STATUS) &(1 <<(_qid)))
#define R_CLDMA_RQ_IS_ACTIVE(base_addr, _qid) (REG_IO_READ32(base_addr+REG_CLDMA_SO_STATUS) &(1 <<(_qid)))


#define CLDMA_AP_TQCPBAK_L(i)  (REG_CLDMA_UL_CURRENT_ADDRL_0_AO + (8 * (i)))
#define CLDMA_AP_TQCPBAK_H(i)  (REG_CLDMA_UL_CURRENT_ADDRH_0_AO + (8 * (i)))


/*L: low 32 bit register; H: high 32 bit register*/
#define REG_CLDMA_GET_QN_ADDRL(base_addr, _qid)  (base_addr + _qid*2*4)
#define REG_CLDMA_GET_QN_ADDRH(base_addr, _qid)  (base_addr + (_qid*2+1)*4))


/*L: low 32 bit register; H: high 32 bit register*/
#define REG_CLDMA_QSAR32L(base_addr, i) ((volatile unsigned *)((char *)(base_addr) + (i)*8))

#define REG_CLDMA_QSAR32H(base_addr, i) ((volatile unsigned *)((char *)(base_addr) + ((i)*8+4)))

#define W_CLDMA_QSAR32L(base_addr, i, value) REG_IO_WRITE32( REG_CLDMA_QSAR32L((base_addr), (i)), (u32)(value) )

#define W_CLDMA_QSAR32H(base_addr, i, value) REG_IO_WRITE32( REG_CLDMA_QSAR32H((base_addr), (i)), (u32)(((u64)(value))>>32) )


#endif  /* __CCCI_HW_CLDMA_H__ */


