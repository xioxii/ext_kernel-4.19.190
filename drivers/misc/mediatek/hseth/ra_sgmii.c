// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2018-2019 MediaTek Inc.

/* A library for MediaTek SGMII circuit
 *
 * Author: Sean Wang <sean.wang@mediatek.com>
 *
 */

#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include "ra_soc.h"


void mtk_sgmii_phy_gen1(struct mtk_sgmii *ss, int id)
{
	unsigned int val;

	regmap_update_bits(ss->regmap[id], 0x24, GENMASK(31, 0), 0x1000fa);
	/* Release PHYA power down state */
	regmap_read(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, &val);
	val &= ~SGMII_PHYA_PWD;
	regmap_write(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, val);
	//0xb44[3] = 1'b0
	regmap_update_bits(ss->regmap_phy[id], 0xb44, BIT(3), (~(1<<3)) & 0xffffffff);
	//0x720[15] = 1'b1
	regmap_update_bits(ss->regmap_phy[id], 0x720, BIT(15), BIT(15));
	//0xc00[13:12] = 2'h2
	regmap_update_bits(ss->regmap_phy[id], 0xc00, GENMASK(13, 12), 0x2 << 12);
	//0xc00[17:16]=2'h2
	regmap_update_bits(ss->regmap_phy[id], 0xc00, GENMASK(17, 16), 0x2 << 16);
	//0xa00[10]=1'b1
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(10), BIT(10));
	//0xa00[16] = 1'b1
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(16), BIT(16));
	//0xc28[31:0]=32'hc04ec4ec
	//0xc2c[31:0]=32'hc04ec4ec
	//0xc30[31:0]=32'hc04ec4ec
	//0xc34[31:0]=32'hc04ec4ec
	regmap_update_bits(ss->regmap_phy[id], 0xc28, GENMASK(31, 0), 0xc04ec4ec);
	regmap_update_bits(ss->regmap_phy[id], 0xc2c, GENMASK(31, 0), 0xc04ec4ec);
	regmap_update_bits(ss->regmap_phy[id], 0xc30, GENMASK(31, 0), 0xc04ec4ec);
	regmap_update_bits(ss->regmap_phy[id], 0xc34, GENMASK(31, 0), 0xc04ec4ec);
	//0xc38[31:16]=16'h0221
	//0xc3c[15:0]=16'h0221
	//0xc3c[31:16]=16'h0221
	//0xc40[15:0]=16'h0221
	regmap_update_bits(ss->regmap_phy[id], 0xc38, GENMASK(31, 16), (0x0221) << 16);
	regmap_update_bits(ss->regmap_phy[id], 0xc3c, GENMASK(15, 0), 0x0221);
	regmap_update_bits(ss->regmap_phy[id], 0xc3c, GENMASK(31, 16), (0x0221) << 16);
	regmap_update_bits(ss->regmap_phy[id], 0xc40, GENMASK(15, 0), 0x0221);
	//0xc44[15:0]=16'h0213
	//0xc44[31:16]=16'h0213
	//0xc48[15:0]=16'h0213
	//0xc48[31:16]=16'h0213
	regmap_update_bits(ss->regmap_phy[id], 0xc44, GENMASK(15, 0), 0x0213);
	regmap_update_bits(ss->regmap_phy[id], 0xc44, GENMASK(31, 16), (0x0213) << 16);
	regmap_update_bits(ss->regmap_phy[id], 0xc48, GENMASK(15, 0), 0x0213);
	regmap_update_bits(ss->regmap_phy[id], 0xc48, GENMASK(31, 16), (0x0213) << 16);
	//0x97c[27:24]=4'h1
	//0x97c[31:28]=4'h1
	regmap_update_bits(ss->regmap_phy[id], 0x97c, GENMASK(27, 24), 0x1 << 24);
	regmap_update_bits(ss->regmap_phy[id], 0x97c, GENMASK(31, 28), 0x1 << 28);
	//0xc04[0]=1'h1
	//0xc04[1]=1'h1
	//0xc04[2]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0xc04, BIT(0), BIT(0));
	regmap_update_bits(ss->regmap_phy[id], 0xc04, BIT(1), BIT(1));
	regmap_update_bits(ss->regmap_phy[id], 0xc04, BIT(2), BIT(2));
	//0xc00[20]=1'h1
	//0xc00[21]=1'h0
	//0xc00[22]=1'h0
	regmap_update_bits(ss->regmap_phy[id], 0xc00, BIT(20), BIT(20));
	regmap_update_bits(ss->regmap_phy[id], 0xc00, BIT(21), (~(1<<21)) & 0xffffffff);
	regmap_update_bits(ss->regmap_phy[id], 0xc00, BIT(22), (~(1<<22)) & 0xffffffff);
	//0xc64[28]=1'h1
	//0xc64[29]=1'h1
	//0xc64[30]=1'h1
	//0xc64[31]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(28), BIT(28));
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(29), BIT(29));
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(30), BIT(30));
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(31), BIT(31));
	/*
	0xc80[0]=1'h1
	0xc80[2]=1'h1
	0xc80[4]=1'h1
	0xc80[6]=1'h1
	0xc80[12:11]=2'h2
	0xc80[15:14]=2'h2
	0xc80[18:17]=2'h2
	0xc80[21:20]=2'h2
	0xc80[27:26]=2'h0
	0xc80[30:29]=2'h0*/
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(0), BIT(0));
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(2), BIT(2));
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(4), BIT(4));
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(6), BIT(6));
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(12, 11), 0x2 << 11);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(15, 14), 0x2 << 14);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(18, 17), 0x2 << 17);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(21, 20), 0x2 << 20);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(27, 26), 0x0 << 26);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(30, 29), 0x0 << 29);
	//0xc84[1:0]=2'h0
	//0xc84[4:3]=2'h0
	regmap_update_bits(ss->regmap_phy[id], 0xc84, GENMASK(1, 0), 0x0);
	regmap_update_bits(ss->regmap_phy[id], 0xc84, GENMASK(4, 3), 0x0 <<3);
	//0x928[31]=1'h1
	//0x928[30]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x928, BIT(31), BIT(31));
	regmap_update_bits(ss->regmap_phy[id], 0x928, BIT(30), BIT(30));
	/*
	0xa78[4:0]=5'h04
	0xa78[10:6]=5'h01
	0xa78[16:12]=5'h04
	0xa78[22:18]=5'h04
	0xa78[28:24]=5'h01*/
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(4, 0), 0x4);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(10, 6), 0x1 << 6);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(16, 12), 0x4 << 12);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(22, 18), 0x4 << 18);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(28, 24), 0x1 << 24);
	//0xa7c[4:0] = 0x04
	regmap_update_bits(ss->regmap_phy[id], 0xa7c, GENMASK(4, 0), 0x4);
	//0xc88[7:4] = 0
	//0xc88[11:8]= 0
	//0xc88[15:12] = 0
	//0xc88[19:16] = 4'h0
	//0xc88[25:23]=3'h4
	//0xc88[28:26]=3'h4
	//0xc88[31:29]=3'h4
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(7, 4), 0x0 << 4);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(11, 8), 0x0 << 8);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(15, 12), 0x0 << 12);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(19, 16), 0x0 << 16);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(25, 23), 0x4 << 23);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(28, 26), 0x4 << 26);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(31, 29), 0x4 << 29);
	//0xc8c[2:0]=3'h4
	regmap_update_bits(ss->regmap_phy[id], 0xc8c, GENMASK(2, 0), 0x4);
	//0xb20[31:28]=4'h3
	regmap_update_bits(ss->regmap_phy[id], 0xb20, GENMASK(31, 28), 0x3 << 28);
	//0x9e8[19:15]=5'h14
	//0x9e8[14]=1'h1
	//0x9e8[25:21]=5'h00
	//0x9e8[20]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x9e8, GENMASK(19, 15), 0x14 << 15);
	regmap_update_bits(ss->regmap_phy[id], 0x9e8, BIT(14), BIT(14));
	regmap_update_bits(ss->regmap_phy[id], 0x9e8, GENMASK(25, 21), 0x0 << 21);
	regmap_update_bits(ss->regmap_phy[id], 0x9e8, BIT(20), BIT(20));
	//0x948[29:24]=6'h3f
	regmap_update_bits(ss->regmap_phy[id], 0x948, GENMASK(29, 24), 0x3f << 24);
	//0x994[18:16]=3'h0
	regmap_update_bits(ss->regmap_phy[id], 0x994, GENMASK(18, 16), 0x0 << 16);
	//0x938[15]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x938, BIT(15), BIT(15));
	//0x940[30]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x940, BIT(30), BIT(30));
	//0x960[24:15]=10'h0
	regmap_update_bits(ss->regmap_phy[id], 0x960, GENMASK(24, 15), 0x0 << 15);
	//0x720[13]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x720, BIT(13), BIT(13));
	//0x810[24]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x810, BIT(24), BIT(24));
	//usleep_range(200, 100);
	//0x940[11]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x940, BIT(11), BIT(11));
	//usleep_range(200, 100);
	//0xa00[6]=1'h1
	//0xa00[5]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(6), BIT(6));
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(5), BIT(5));
	//usleep_range(200, 100);
	//0x720[13]=1'h0
	regmap_update_bits(ss->regmap_phy[id], 0x720, BIT(13), (~(1<<13)) & 0xffffffff);
	//usleep_range(200, 100);
	//0x934[20]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x934, BIT(20), 0x1 << 20);

}

void mtk_sgmii_phy_gen2(struct mtk_sgmii *ss, int id)
{
	unsigned int val;

	regmap_update_bits(ss->regmap[id], 0x24, GENMASK(31, 0), 0x1000fa);

	/* Release PHYA power down state */
	regmap_read(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, &val);
	val &= ~SGMII_PHYA_PWD;
	regmap_write(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, val);
	//0xb44[3] = 1'b0
	regmap_update_bits(ss->regmap_phy[id], 0xb44, BIT(3), (~(1<<3)) & 0xffffffff);
	//0x720[15] = 1'b1
	//regmap_update_bits(ss->regmap_phy[id], 0x720, BIT(15), 1);

	regmap_read(ss->regmap_phy[id], 0x720, &val);
	val &= ~BIT(15);
	val |= BIT(15);
	regmap_write(ss->regmap_phy[id], 0x720, val);
	//0xc00[13:12] = 2'h2
	//0xc00[17:16] = 2'h2
	regmap_read(ss->regmap_phy[id], 0xc00, &val);
	pr_info("0xc00 = %x\n", val);
	//regmap_update_bits(ss->regmap_phy[id], 0xc00, GENMASK(13, 12), 0x2);
	//regmap_update_bits(ss->regmap_phy[id], 0xc00, GENMASK(17, 16), 0x2);
	
	regmap_read(ss->regmap_phy[id], 0xc00, &val);
	val &= ~GENMASK(13, 12);
	val |= (0x2 << 12);
	regmap_write(ss->regmap_phy[id], 0xc00, val);	

	regmap_read(ss->regmap_phy[id], 0xc00, &val);
	val &= ~GENMASK(17, 16);
	val |= (0x2 << 16);
	regmap_write(ss->regmap_phy[id], 0xc00, val);	


	
	//regmap_read(ss->regmap_phy[id], 0xc00, &val);
	pr_info("0xc00 after = %x\n", val);
	//0xa00[10]=1'b1
	//0xa00[16]=1'b1
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(10), BIT(10));
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(16), BIT(16));





	//0xc28[31:0]=32'hf0627627
	//0xc2c[31:0]=32'hf0627627
	//0xc30[31:0]=32'hf0627627
	//0xc34[31:0]=32'hf0627627
	regmap_update_bits(ss->regmap_phy[id], 0xc28, GENMASK(31, 0), 0xf0627627);
	regmap_update_bits(ss->regmap_phy[id], 0xc2c, GENMASK(31, 0), 0xf0627627);
	regmap_update_bits(ss->regmap_phy[id], 0xc30, GENMASK(31, 0), 0xf0627627);
	regmap_update_bits(ss->regmap_phy[id], 0xc34, GENMASK(31, 0), 0xf0627627);

	//0xc38[31:16]=16'h02aa
	//0xc3c[15:0]=16'h02aa
	//0xc3c[31:16]=16'h02aa
	//0xc40[15:0]=16'h02aa
	regmap_update_bits(ss->regmap_phy[id], 0xc38, GENMASK(31, 16), 0x02aa << 16);
	regmap_update_bits(ss->regmap_phy[id], 0xc3c, GENMASK(15, 0), 0x02aa);
	regmap_update_bits(ss->regmap_phy[id], 0xc3c, GENMASK(31, 16), 0x02aa << 16);
	regmap_update_bits(ss->regmap_phy[id], 0xc40, GENMASK(15, 0), 0x02aa);

	//0xc44[15:0]= 16'h0297
	//0xc44[31:16]=16'h0297
	//0xc48[15:0]= 16'h0297
	//0xc48[31:16]=16'h0297
	regmap_update_bits(ss->regmap_phy[id], 0xc44, GENMASK(15, 0), 0x0297);
	regmap_update_bits(ss->regmap_phy[id], 0xc44, GENMASK(31, 16), 0x0297<<16);
	regmap_update_bits(ss->regmap_phy[id], 0xc48, GENMASK(15, 0), 0x0297);
	regmap_update_bits(ss->regmap_phy[id], 0xc48, GENMASK(31, 16), 0x0297<<16);

	//0x97c[27:24]=4'h3
	//0x97c[31:28]=4'h3
	regmap_update_bits(ss->regmap_phy[id], 0x97c, GENMASK(27, 24), 0x3<<24);
	regmap_update_bits(ss->regmap_phy[id], 0x97c, GENMASK(31, 28), 0x3<<28);

	//0xc04[0]= 1'h1
	//0xc04[1]= 1'h1
	//0xc04[2]= 1'h1
	regmap_update_bits(ss->regmap_phy[id], 0xc04, BIT(0), BIT(0));
	regmap_update_bits(ss->regmap_phy[id], 0xc04, BIT(1), BIT(1));
	regmap_update_bits(ss->regmap_phy[id], 0xc04, BIT(2), BIT(2));

	//0xc00[20]=1'h1
	//0xc00[21]=1'h0
	//0xc00[22]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0xc00, BIT(20), BIT(20));
	regmap_update_bits(ss->regmap_phy[id], 0xc00, BIT(21),(~(1<<21)) & 0xffffffff);
	regmap_update_bits(ss->regmap_phy[id], 0xc00, BIT(22), BIT(22));

	//0xc64[28]=1'h1
	//0xc64[29]=1'h1
	//0xc64[30]=1'h1
	//0xc64[31]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(28), BIT(28));
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(29), BIT(29));
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(30), BIT(30));
	regmap_update_bits(ss->regmap_phy[id], 0xc64, BIT(31), BIT(31));

	//0xc80[0]= 1'h0
	//0xc80[2]= 1'h0
	//0xc80[4]= 1'h0
	//0xc80[6]= 1'h0
	//0xc80[12:11]=2'h1
	//0xc80[15:14]=2'h1
	//0xc80[18:17]=2'h1
	//0xc80[21:20]=2'h1
	//0xc80[27:26]=2'h1
	//0xc80[30:29]=2'h1
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(0), (~(1<<0)) & 0xffffffff);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(2), (~(1<<2)) & 0xffffffff);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(4), (~(1<<4)) & 0xffffffff);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, BIT(6), (~(1<<6)) & 0xffffffff);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(12, 11), 0x1 << 11);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(15, 14), 0x1 << 14);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(18, 17), 0x1<< 17);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(21, 20), 0x1 << 20);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(27, 26), 0x1<< 26);
	regmap_update_bits(ss->regmap_phy[id], 0xc80, GENMASK(30, 29), 0x1<< 29);

	//0xc84[1:0]=  2'h1
	//0xc84[4:3]=  2'h1
	regmap_update_bits(ss->regmap_phy[id], 0xc84, GENMASK(1, 0), 0x1);
	regmap_update_bits(ss->regmap_phy[id], 0xc84, GENMASK(4, 3), 0x1 <<3);

	//0xa78[4:0]=  5'h09
	//0xa78[10:6]= 5'h02
	//0xa78[16:12]=5'h09
	//0xa78[22:18]=5'h09
	//0xa78[28:24]=5'h02
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(4, 0), 0x9);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(10, 6), 0x2 << 6);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(16, 12), 0x9 <<12);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(22, 18), 0x9<<18);
	regmap_update_bits(ss->regmap_phy[id], 0xa78, GENMASK(28, 24), 0x2<<24);
	//0xa7c[4:0]=  5'h09

	regmap_update_bits(ss->regmap_phy[id], 0xa7c, GENMASK(4, 0), 0x9);
	//0xc88[7:4]=  4'h2
	//0xc88[11:8]= 4'h2
	//0xc88[15:12]=4'h2
	//0xc88[19:16]=4'h2
	//0xc88[25:23]=3'h3
	//0xc88[28:26]=3'h3
	//0xc88[31:29]=3'h3

	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(7, 4), 0x2 << 4);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(11, 8), 0x2 << 8);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(15, 12), 0x2 << 12);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(19, 16), 0x2 << 16);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(25, 23), 0x3 << 23);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(28, 26), 0x3 << 26);
	regmap_update_bits(ss->regmap_phy[id], 0xc88, GENMASK(31, 29), 0x3 << 29);
	

	//0xc8c[2:0]=3'h3
	regmap_update_bits(ss->regmap_phy[id], 0xc8c, GENMASK(2, 0), 0x3);
	//0xb20[31:28]=4'h1

	regmap_update_bits(ss->regmap_phy[id], 0xb20, GENMASK(31, 28), 0x1 << 28);
	//0x9e8[19:15]=5'h15
	//0x9e8[14]=1'h1
	//0x9e8[25:21]=5'h10
	//0x9e8[20]=1'h1

	regmap_update_bits(ss->regmap_phy[id], 0x9e8, GENMASK(19, 15), 0x15 <<15);
	regmap_update_bits(ss->regmap_phy[id], 0x9e8, BIT(14), BIT(14));
	regmap_update_bits(ss->regmap_phy[id], 0x9e8, GENMASK(25, 21), 0x10 << 21);
	regmap_update_bits(ss->regmap_phy[id], 0x9e8, BIT(20), BIT(20));

	//0x9ec[23:20]=4'h6
	//0x9ec[30:26]=5'h19
	regmap_update_bits(ss->regmap_phy[id], 0x9ec, GENMASK(23, 20), 0x6 << 20);
	regmap_update_bits(ss->regmap_phy[id], 0x9ec, GENMASK(30, 26), 0x19 << 26);

	//0x948[29:24]=6'h3f
	regmap_update_bits(ss->regmap_phy[id], 0x948, GENMASK(29, 24), 0x3f << 24);
	//0x994[18:16]=3'h1

	regmap_update_bits(ss->regmap_phy[id], 0x994, GENMASK(18, 16), 0x1 <<16);
	//0x938[15]=1'h1

	regmap_update_bits(ss->regmap_phy[id], 0x938, BIT(15), BIT(15));

	//0x940[30]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x940, BIT(30), BIT(30));

	//0x720[13]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x720, BIT(13), BIT(13));

	//0x810[24]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x810, BIT(24), BIT(24));

	usleep_range(1000, 1100);

	//0x940[11]=1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x940, BIT(11), BIT(11));
	//0xa00[6]= 1'h1
	//0xa00[5]= 1'h1
	usleep_range(100, 110);
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(6), BIT(6));
	regmap_update_bits(ss->regmap_phy[id], 0xa00, BIT(5), BIT(5));
	usleep_range(1000, 1100);

	//0x720[13]=1'h0
	regmap_update_bits(ss->regmap_phy[id], 0x720, BIT(13), (~(1<<13)) & 0xffffffff);
	usleep_range(1000, 1100);
	//0x934[20= 1'h1
	regmap_update_bits(ss->regmap_phy[id], 0x934, BIT(20), BIT(20));

}



void mtk_w32(struct mtk_eth *eth, u32 val, unsigned reg)
{
	__raw_writel(val, eth->base + reg);
}
u32 mtk_r32(struct mtk_eth *eth, unsigned reg)
{
	return __raw_readl(eth->base + reg);
}



int mtk_sgmii_init(struct mtk_sgmii *ss, struct device_node *r, u32 ana_rgc3)
{
	struct device_node *np, *np_phy;
	const char *str;
	int i, err;

	ss->ana_rgc3 = ana_rgc3;

	for (i = 0; i < MTK_MAX_DEVS; i++) {
		np = of_parse_phandle(r, "mediatek,sgmiisys", i);
		if (!np)
			break;

		ss->regmap[i] = syscon_node_to_regmap(np);
		if (IS_ERR(ss->regmap[i]))
			return PTR_ERR(ss->regmap[i]);

		err = of_property_read_string(np, "mediatek,physpeed", &str);
		if (err)
			return err;

		if (!strcmp(str, "2500"))
			ss->flags[i] |= MTK_SGMII_PHYSPEED_2500;
		else if (!strcmp(str, "1000"))
			ss->flags[i] |= MTK_SGMII_PHYSPEED_1000;
		else if (!strcmp(str, "auto"))
			ss->flags[i] |= MTK_SGMII_PHYSPEED_AN;
		else
			return -EINVAL;

		np_phy = of_parse_phandle(r, "mediatek,sgmiisys_phy", i);
		if (!np_phy)
			break;

		ss->regmap_phy[i] = syscon_node_to_regmap(np_phy);
		if (IS_ERR(ss->regmap_phy[i]))
			return PTR_ERR(ss->regmap_phy[i]);

		pr_info("mtk_sgmii_init %d\n", i);
	}

	return 0;
}

int mtk_sgmii_setup_mode_an(struct mtk_sgmii *ss, int id)
{
	unsigned int val;

	if (!ss->regmap[id])
		return -EINVAL;

	/* Setup the link timer and QPHY power up inside SGMIISYS */
	regmap_write(ss->regmap[id], SGMSYS_PCS_LINK_TIMER,
		  SGMII_LINK_TIMER_DEFAULT);

	regmap_read(ss->regmap[id], SGMSYS_SGMII_MODE, &val);
	val |= SGMII_REMOTE_FAULT_DIS;
	regmap_write(ss->regmap[id], SGMSYS_SGMII_MODE, val);

	regmap_read(ss->regmap[id], SGMSYS_PCS_CONTROL_1, &val);
	val |= SGMII_AN_RESTART;
	regmap_write(ss->regmap[id], SGMSYS_PCS_CONTROL_1, val);

	regmap_read(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, &val);
	val &= ~SGMII_PHYA_PWD;
	regmap_write(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, val);

	return 0;
}

int mtk_sgmii_setup_mode_force(struct mtk_sgmii *ss, int id, struct END_DEVICE *ei_local)
{
	unsigned int val;
	int mode;

	if (!ss->regmap[id])
		return -EINVAL;

	regmap_read(ss->regmap[id], ss->ana_rgc3, &val);
	val &= ~GENMASK(3, 2);
	mode = ss->flags[id] & MTK_SGMII_PHYSPEED_MASK;
	val |= (mode == MTK_SGMII_PHYSPEED_1000) ? 0 : BIT(2);
	regmap_write(ss->regmap[id], ss->ana_rgc3, val);

	/* disable SGMII AN */
	regmap_read(ss->regmap[id], SGMSYS_PCS_CONTROL_1, &val);
	val &= ~BIT(12);
	regmap_write(ss->regmap[id], SGMSYS_PCS_CONTROL_1, val);

	/* SGMII force mode setting */
	val = 0x31120009;
	regmap_write(ss->regmap[id], SGMSYS_SGMII_MODE, val);

	/* Release PHYA power down state */
	regmap_read(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, &val);
	val &= ~SGMII_PHYA_PWD;
	regmap_write(ss->regmap[id], SGMSYS_QPHY_PWR_STATE_CTRL, val);

	/*SET SGMII PHY parameter*/
	if (MTK_HAS_CAPS(ei_local->soc->caps, MTK_SGMII_PHY)) {
		if(mode == MTK_SGMII_PHYSPEED_2500)
			mtk_sgmii_phy_gen2(ss, id);
		else
			mtk_sgmii_phy_gen1(ss, id);
	}

	return 0;
}
