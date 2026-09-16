// SPDX-License-Identifier: GPL-2.0
/*
 * mediatek-mcupm-cpufreq.c - MCUPM based CPUFreq Driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Wei-Chia Su <Wei-Chia.Su@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/energy_model.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/uaccess.h>
#include <mcupm_ipi_id.h>
#include <mcupm_driver.h>
#include <linux/delay.h>
#include <linux/nvmem-consumer.h>

#define OFFS_WFI_S 0x037c
#define DVFS_D_LEN (4)
#define CSRAM_SIZE 0x1400
#define OFFS_LOG_S 0x03d0
#define DBG_REPO_NUM (CSRAM_SIZE / sizeof(u32))
#define REPO_I_LOG_S (OFFS_LOG_S / sizeof(u32))
#define ENTRY_EACH_LOG	5
#define VMIN_PREDICT_ENABLE (0)
#define NR_PI_VF 6
#define MAX_NR_FREQ 32
#define MIN_SIZE_SN_DUMP_CPE (7)
#define NUM_SN_CPU (8)
#define SIZE_SN_MCUSYS_REG (10)
#define SIZE_SN_DUMP_SENSOR (64)
#define IDX_HW_RES_SN (18)
#define DEVINFO_IDX_0 0xC8
#define DEVINFO_HRID_0 0x30
#define EEMSN_CSRAM_BASE 0x0011BC00
#define LOW_TEMP_OFF (8)
#define HIGH_TEMP_OFF (3)
#define AGING_VAL_CPU (0x6)
#define EEM_TAG  "[CPU][EEM]"
#define SUPPORT_PICACHU (1)
#if SUPPORT_PICACHU
#define PICACHU_SIG (0xA5)
#define PICACHU_SIGNATURE_SHIFT_BIT (24)
#define EEM_PHY_SPARE0 0x11278F20
#endif
#undef  BIT
#define BIT(bit) (1U << (bit))

#define eem_read(addr) __raw_readl((void __iomem *)(addr))

#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name ## _proc_fops = {		\
	.owner          = THIS_MODULE,					\
	.open           = name ## _proc_open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
	.write          = name ## _proc_write,				\
}

#define PROC_FOPS_RO(name)						\
static int name##_proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name##_proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name##_proc_fops = {		\
	.owner = THIS_MODULE,						\
	.open = name##_proc_open,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
	.release = single_release,					\
}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
#define PROC_ENTRY_DATA(name)	{__stringify(name), &name ## _proc_fops, g_ ## name}

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

enum eem_phase {
	EEM_PHASE_INIT01,
	EEM_PHASE_INIT02,
	EEM_PHASE_MON,
	EEM_PHASE_SEN,

	NR_EEM_PHASE,
};

enum eem_features {
	FEA_INIT01	= BIT(EEM_PHASE_INIT01),
	FEA_INIT02	= BIT(EEM_PHASE_INIT02),
	FEA_MON		= BIT(EEM_PHASE_MON),
	FEA_SEN	= BIT(EEM_PHASE_SEN),
};

enum {
	IPI_EEMSN_SHARERAM_INIT,
	IPI_EEMSN_INIT,
	IPI_EEMSN_PROBE,
	IPI_EEMSN_INIT01,
	IPI_EEMSN_GET_EEM_VOLT,
	IPI_EEMSN_INIT02,
	IPI_EEMSN_DEBUG_PROC_WRITE,
	IPI_EEMSN_SEND_UPOWER_TBL_REF,
	IPI_EEMSN_CUR_VOLT_PROC_SHOW,
	IPI_EEMSN_DUMP_PROC_SHOW,
	IPI_EEMSN_AGING_DUMP_PROC_SHOW,
	IPI_EEMSN_OFFSET_PROC_WRITE,
	IPI_EEMSN_SNAGING_PROC_WRITE,
	IPI_EEMSN_LOGEN_PROC_SHOW,
	IPI_EEMSN_LOGEN_PROC_WRITE,
	IPI_EEMSN_EN_PROC_SHOW,
	IPI_EEMSN_EN_PROC_WRITE,
	IPI_EEMSN_SNEN_PROC_SHOW,
	IPI_EEMSN_SNEN_PROC_WRITE,
	IPI_EEMSN_FAKE_SN_INIT_ISR,
	IPI_EEMSN_FORCE_SN_SENSING,
	IPI_EEMSN_PULL_DATA,
	IPI_EEMSN_FAKE_SN_SENSING_ISR,
	NR_EEMSN_IPI,
};

enum eemsn_det_id {
	EEMSN_DET_L,
	EEMSN_DET_CCI,

	NR_EEMSN_DET,
};

enum sn_det_id {
	SN_DET_L = 0,

	NR_SN_DET,
};

enum cpu_dvfs_ipi_type {
	IPI_DVFS_INIT,
	NR_DVFS_IPI,
};

static void __iomem *csram_base;
u32 *g_dbg_repo;
int ipi_ackdata;
unsigned char ctrl_agingload_enable;
static struct eemsn_dbg_log *eemsn_log;
unsigned int seq;
static LIST_HEAD(dvfs_info_list);
struct nvmem_device *nvmem_dev;
struct cdvfs_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} set_fv;
	} u;
};

struct mtk_cpu_dvfs_info {
	struct cpumask cpus;
	struct device *cpu_dev;
	struct list_head list_head;
	struct mutex lock;
	void __iomem *csram_base;
};

static struct mtk_cpu_dvfs_info *mtk_cpu_dvfs_info_lookup(int cpu)
{
	struct mtk_cpu_dvfs_info *info;
	struct list_head *list;

	list_for_each(list, &dvfs_info_list) {
		info = list_entry(list, struct mtk_cpu_dvfs_info, list_head);
		if (cpumask_test_cpu(cpu, &info->cpus))
			return info;
	}
	return NULL;
}

static int mtk_cpufreq_set_target(struct cpufreq_policy *policy,
				  unsigned int index)
{
	struct mtk_cpu_dvfs_info *info = policy->driver_data;
	unsigned int cluster_id = policy->cpu / 4;

	writel_relaxed(index, info->csram_base + (OFFS_WFI_S + (cluster_id * 4))
		       );
	arch_set_freq_scale(policy->related_cpus,
			    policy->freq_table[index].frequency,
			    policy->cpuinfo.max_freq);

	return 0;
}

static int mtk_cpu_dvfs_info_init(struct mtk_cpu_dvfs_info *info, int cpu)
{
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev)
		return -ENODEV;

	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, &info->cpus);
	if (ret)
		goto out_free_resources;

	ret = dev_pm_opp_of_cpumask_add_table(&info->cpus);
	if (ret)
		goto out_free_resources;

	info->cpu_dev = cpu_dev;
	mutex_init(&info->lock);

	return 0;

out_free_resources:
	return ret;
}

static void mtk_cpu_dvfs_info_release(struct mtk_cpu_dvfs_info *info)
{
	dev_pm_opp_of_cpumask_remove_table(&info->cpus);
}

static int mtk_cpufreq_init(struct cpufreq_policy *policy)
{
	struct mtk_cpu_dvfs_info *info;
	struct cpufreq_frequency_table *freq_table;
	struct em_data_callback em_cb = EM_DATA_CB(of_dev_pm_opp_get_cpu_power);
	int ret;

	info = mtk_cpu_dvfs_info_lookup(policy->cpu);
	if (!info)
		return -EINVAL;

	ret = dev_pm_opp_init_cpufreq_table(info->cpu_dev, &freq_table);
	if (ret)
		return -EINVAL;

	ret = dev_pm_opp_get_opp_count(info->cpu_dev);
	if (ret <= 0) {
		ret = -EINVAL;
		goto out_free_cpufreq_table;
	}

	cpumask_copy(policy->cpus, &info->cpus);
	em_register_perf_domain(policy->cpus, ret, &em_cb);
	policy->driver_data = info;
	policy->freq_table = freq_table;
	policy->transition_delay_us = 1000; /* us */

	return 0;

out_free_cpufreq_table:	
	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &freq_table);

	return ret;
}

static int mtk_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct mtk_cpu_dvfs_info *info = policy->driver_data;

	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &policy->freq_table);

	return 0;
}

static struct cpufreq_driver mtk_cpufreq_driver = {
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		 CPUFREQ_HAVE_GOVERNOR_PER_POLICY | CPUFREQ_IS_COOLING_DEV,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = mtk_cpufreq_set_target,
	.init = mtk_cpufreq_init,
	.exit = mtk_cpufreq_exit,
	.name = "mtk-cpufreq",
	.attr = cpufreq_generic_attr,
};

struct A_Tused_VT {
	unsigned int A_Tused_SVT:8;
	unsigned int A_Tused_LVT:8;
	unsigned int A_Tused_ULVT:8;
	unsigned int A_Tused_RSV0:8;
};

struct dvfs_vf_tbl {
	unsigned short pi_freq_tbl[NR_PI_VF];
	unsigned char pi_volt_tbl[NR_PI_VF];
	unsigned char pi_vf_num;
};

struct sensing_stru {
#if defined(VMIN_PREDICT_ENABLE)
	uint64_t CPE_Vmin_HW;
#endif
	unsigned int SN_Vmin;
	int CPE_Vmin;
	unsigned int cur_volt;
#if !VMIN_PREDICT_ENABLE
	/* unsigned int count_cur_volt_HT; */
	int Sensor_Volt_HT;
	int Sensor_Volt_RT;
	int8_t CPE_temp;
	int8_t SN_temp;
	unsigned char T_SVT_current;
#endif
	unsigned short cur_temp;
	unsigned char cur_oppidx;
	unsigned char dst_volt_pmic;
};

struct eemsn_log_det {
	unsigned char mc50flag;
	unsigned char features;
	int8_t volt_clamp;
	int8_t volt_offset;
};

struct eemsn_vlog_det {
	unsigned int temp;
	unsigned short freq_tbl[MAX_NR_FREQ];
	unsigned short volt_tbl_pmic[MAX_NR_FREQ];
	unsigned char volt_tbl_orig[MAX_NR_FREQ];
	unsigned char volt_tbl_init2[MAX_NR_FREQ];
	/* unsigned char volt_tbl[NR_FREQ]; */
	unsigned char num_freq_tbl;
	unsigned char lock;
	unsigned char turn_pt;
	enum eemsn_det_id det_id;
};

struct eemsn_devinfo {
	/* M_HW_RES0 0x11c1_0580 */
	unsigned int FT_PGM:8;
	unsigned int FT_BIN:4;
	unsigned int RSV0_1:20;

	/* M_HW_RES1 */
	unsigned int CPU_B_MTDES:8;
	unsigned int CPU_B_INITEN:1;
	unsigned int CPU_B_MONEN:1;
	unsigned int CPU_B_DVFS_LOW:3;
	unsigned int CPU_B_SPEC:3;
	unsigned int CPU_B_BDES:8;
	unsigned int CPU_B_MDES:8;

	/* M_HW_RES2 */
	unsigned int CPU_B_HI_MTDES:8;
	unsigned int CPU_B_HI_INITEN:1;
	unsigned int CPU_B_HI_MONEN:1;
	unsigned int CPU_B_HI_DVFS_LOW:3;
	unsigned int CPU_B_HI_SPEC:3;
	unsigned int CPU_B_HI_BDES:8;
	unsigned int CPU_B_HI_MDES:8;

	/* M_HW_RES3 */
	unsigned int CPU_B_LO_MTDES:8;
	unsigned int CPU_B_LO_INITEN:1;
	unsigned int CPU_B_LO_MONEN:1;
	unsigned int CPU_B_LO_DVFS_LOW:3;
	unsigned int CPU_B_LO_SPEC:3;
	unsigned int CPU_B_LO_BDES:8;
	unsigned int CPU_B_LO_MDES:8;

	/* M_HW_RES4 */
	unsigned int CCI_LO_MTDES:8;
	unsigned int CCI_LO_INITEN:1;
	unsigned int CCI_LO_MONEN:1;
	unsigned int CCI_LO_DVFS_LOW:3;
	unsigned int CCI_LO_SPEC:3;
	unsigned int CCI_LO_BDES:8;
	unsigned int CCI_LO_MDES:8;

	/* M_HW_RES5 */
	unsigned int CPU_L_MTDES:8;
	unsigned int CPU_L_INITEN:1;
	unsigned int CPU_L_MONEN:1;
	unsigned int CPU_L_DVFS_LOW:3;
	unsigned int CPU_L_SPEC:3;
	unsigned int CPU_L_BDES:8;
	unsigned int CPU_L_MDES:8;

	/* M_HW_RES6 */
	unsigned int CPU_L_HI_MTDES:8;
	unsigned int CPU_L_HI_INITEN:1;
	unsigned int CPU_L_HI__MONEN:1;
	unsigned int CPU_L_HI_DVFS_LOW:3;
	unsigned int CPU_L_HI_SPEC:3;
	unsigned int CPU_L_HI_BDES:8;
	unsigned int CPU_L_HI_MDES:8;

	/* M_HW_RES7 */
	unsigned int CPU_B_HI_DCBDET:8;
	unsigned int CPU_B_HI_DCMDET:8;
	unsigned int CPU_B_DCBDET:8;
	unsigned int CPU_B_DCMDET:8;

	/* M_HW_RES8 */
	unsigned int CCI_LO_DCBDET:8;
	unsigned int CCI_LO_DCMDET:8;
	unsigned int CCI_HI_DCBDET:8;
	unsigned int CCI_HI_DCMDET:8;

	/* M_HW_RES */
	unsigned int CPU_L_LO_MTDES:8;
	unsigned int CPU_L_LO_INITEN:1;
	unsigned int CPU_L_LO_MONEN:1;
	unsigned int CPU_L_LO_DVFS_LOW:3;
	unsigned int CPU_L_LO_SPEC:3;
	unsigned int CPU_L_LO_BDES:8;
	unsigned int CPU_L_LO_MDES:8;

	/* M_HW_RES */
	unsigned int CCI_HI_MTDES:8;
	unsigned int CCI_HI_INITEN:1;
	unsigned int CCI_HI_MONEN:1;
	unsigned int CCI_HI_DVFS_LOW:3;
	unsigned int CCI_HI_SPEC:3;
	unsigned int CCI_HI_BDES:8;
	unsigned int CCI_HI_MDES:8;

	/* M_HW_RES9 */
	unsigned int GPU_HI_MTDES:8;
	unsigned int GPU_HI_INITEN:1;
	unsigned int GPU_HI_MONEN:1;
	unsigned int GPU_HI_DVFS_LOW:3;
	unsigned int GPU_HI_SPEC:3;
	unsigned int GPU_HI_BDES:8;
	unsigned int GPU_HI_MDES:8;

	/* M_HW_RES10 */
	unsigned int GPU_LO_MTDES:8;
	unsigned int GPU_LO_INITEN:1;
	unsigned int GPU_LO_MONEN:1;
	unsigned int GPU_LO_DVFS_LOW:3;
	unsigned int GPU_LO_SPEC:3;
	unsigned int GPU_LO_BDES:8;
	unsigned int GPU_LO_MDES:8;

	/* M_HW_RES11 */
	unsigned int MD_VMODEM:32;

	/* M_HW_RES12 */
	unsigned int MD_VNR:32;

	/* M_HW_RES14 */
	unsigned int CPU_L_DCBDET:8;
	unsigned int CPU_L_DCMDET:8;
	unsigned int CPU_B_LO_DCBDET:8;
	unsigned int CPU_B_LO_DCMDET:8;

	/* M_HW_RES15 */
	unsigned int CPU_L_LO_DCBDET:8;
	unsigned int CPU_L_LO_DCMDET:8;
	unsigned int CPU_L_HI_DCBDET:8;
	unsigned int CPU_L_HI_DCMDET:8;

	/* M_HW_RES17 */
	unsigned int GPU_LO_DCBDET:8;
	unsigned int GPU_LO_DCMDET:8;
	unsigned int GPU_HI_DCBDET:8;
	unsigned int GPU_HI_DCMDET:8;

	/* M_HW_RES21 */
	unsigned int BCPU_A_T0_SVT:8;
	unsigned int BCPU_A_T0_LVT:8;
	unsigned int BCPU_A_T0_ULVT:8;
	unsigned int LCPU_A_T0_SVT:8;

	/* M_HW_RES22 */
	unsigned int LCPU_A_T0_LVT:8;
	unsigned int LCPU_A_T0_ULVT:8;
	unsigned int RES22_RSV:16;

	/* M_HW_RES23 */
	unsigned int FINAL_VMIN_BCPU:8;
	unsigned int FINAL_VMIN_LCPU:8;
	unsigned int ATE_TEMP:8;
	unsigned int SN_PATTERN:4;
	unsigned int SN_VERSION:4;

	/* M_HW_RES24 */
	unsigned int T_SVT_HV_BCPU:8;
	unsigned int T_SVT_LV_BCPU:8;
	unsigned int T_SVT_HV_LCPU:8;
	unsigned int T_SVT_LV_LCPU:8;

	/* M_HW_RES25 */
	unsigned int T_SVT_HV_BCPU_RT:8;
	unsigned int T_SVT_LV_BCPU_RT:8;
	unsigned int T_SVT_HV_LCPU_RT:8;
	unsigned int T_SVT_LV_LCPU_RT:8;

	/* M_HW_RES26 */
	unsigned int FPC_RECORVERY_BCPU:8;
	unsigned int CPE_VMIN_BCPU:8;
	unsigned int FPC_RECORVERY_LCPU:8;
	unsigned int CPE_VMIN_LCPU:8;
};

struct sn_log_data {
	unsigned long long timestamp;
	unsigned int reg_dump_cpe[MIN_SIZE_SN_DUMP_CPE];
	unsigned int reg_dump_sndata[SIZE_SN_DUMP_SENSOR];
	unsigned int reg_dump_sn_cpu[NUM_SN_CPU][SIZE_SN_MCUSYS_REG];
	struct sensing_stru sd[NR_SN_DET];
	unsigned int footprint[NR_SN_DET];
	unsigned int allfp;
#if VMIN_PREDICT_ENABLE
	unsigned int sn_cpe_vop;
#endif
};

struct sn_log_cal_data {
	int64_t cpe_init_aging;
	struct A_Tused_VT atvt;
	int TEMP_CAL;
	int volt_cross;
	short CPE_Aging;
	int8_t sn_aging;
	int8_t delta_vc;
	uint8_t T_SVT_HV_RT;
	uint8_t T_SVT_LV_RT;
};

struct sn_param {
	int Param_A_Tused_SVT;
	int Param_A_Tused_LVT;
	int Param_A_Tused_ULVT;
	int Param_A_T0_SVT;
	int Param_A_T0_LVT;
	int Param_A_T0_ULVT;
	int Param_ATE_temp;
	int Param_temp;
	int Param_INTERCEPTION;
	int8_t A_GB;
	int8_t sn_temp_threshold;
	int8_t Default_Aging;
	int8_t threshold_H;
	int8_t threshold_L;

	unsigned char T_GB;

	/* Formula for CPE_Vmin (Vmin prediction) */
	unsigned char CPE_GB;
	unsigned char MSSV_GB;

};

struct eemsn_log {
	struct dvfs_vf_tbl vf_tbl_det[NR_EEMSN_DET];
	unsigned char eemsn_enable;
	unsigned char sn_enable;
	unsigned char ctrl_aging_Enable;
	struct eemsn_log_det det_log[NR_EEMSN_DET];
};

struct eemsn_det {
	int64_t		cpe_init_aging;
	int temp; /* det temperature */

	/* dvfs */
	unsigned int cur_volt;
	unsigned int *p_sn_cpu_coef;
	struct sn_param *p_sn_cpu_param;
	enum eemsn_det_id det_id;

	unsigned int volt_tbl_pmic[MAX_NR_FREQ]; /* pmic value */

	/* for PMIC */
	unsigned short eemsn_v_base;
	unsigned short eemsn_step;
	unsigned short pmic_base;
	unsigned short pmic_step;
	short cpe_volt_total_mar;

	/* dvfs */
	unsigned short freq_tbl[MAX_NR_FREQ];
	unsigned char volt_tbl_init2[MAX_NR_FREQ]; /* eem value */
	unsigned char volt_tbl_orig[MAX_NR_FREQ]; /* pmic value */
	unsigned char dst_volt_pmic;
	unsigned char volt_tbl0_min; /* pmic value */

	unsigned char features; /* enum eemsn_features */
	unsigned char cur_phase;
	unsigned char cur_oppidx;

	const char *name;

	unsigned char disabled; /* Disabled by error or sysfs */
	unsigned char num_freq_tbl;

	unsigned char turn_pt;
	unsigned char vmin_high;
	unsigned char vmin_mid;
	int8_t delta_vc;
	int8_t sn_aging;
	int8_t volt_offset;
	int8_t volt_clamp;
	/* int volt_offset:8; */
	unsigned int delta_ir:4;
	unsigned int delta_vdppm:5;
};

struct eemsn_dbg_log {
	struct dvfs_vf_tbl vf_tbl_det[NR_EEMSN_DET];
	unsigned char eemsn_enable;
	unsigned char sn_enable;
	unsigned char ctrl_aging_Enable;
	struct eemsn_log_det det_log[NR_EEMSN_DET];
	struct eemsn_vlog_det det_vlog[NR_EEMSN_DET];
	struct sn_log_data sn_log;
	struct sn_log_cal_data sn_cal_data[NR_SN_DET];
	struct sn_param sn_cpu_param[NR_SN_DET];
	struct eemsn_devinfo efuse_devinfo;
	unsigned int efuse_sv;
	unsigned int efuse_sv2;
	unsigned int picachu_sn_mem_base_phys;
	unsigned char init2_v_ready;
	unsigned char init_vboot_done;
	unsigned char segCode;
	unsigned char lock;
};


struct eem_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} data;
	} u;
};

#define det_to_id(det)	((det) - &eemsn_detectors[0])

struct eemsn_det eemsn_detectors[NR_EEMSN_DET] = {
	[EEMSN_DET_L] = {
		.name			= "EEM_DET_L",
		.det_id    = EEMSN_DET_L,
		.features	= FEA_INIT01 | FEA_INIT02 | FEA_MON | FEA_SEN,
		.volt_offset = 0,
	},
	[EEMSN_DET_CCI] = {
		.name			= "EEM_DET_CCI",
		.det_id    = EEMSN_DET_CCI,
		.features	= FEA_INIT02 | FEA_MON,
		.volt_offset = 0,
	},
};

#define for_each_det(det) \
	for (det = eemsn_detectors; \
	     det < (eemsn_detectors + ARRAY_SIZE(eemsn_detectors)); \
	     det++)

static unsigned int eem_to_cpueb(unsigned int cmd,
					struct eem_ipi_data *eem_data)
{
	unsigned int ret;

	pr_debug("to_cpueb, cmd:%d\n", cmd);
	eem_data->cmd = cmd;

	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_EEMSN,
		/*IPI_SEND_WAIT*/IPI_SEND_POLLING, eem_data,
		sizeof(struct eem_ipi_data)/MBOX_SLOT_SIZE, 2000);

	return ret;
}

unsigned int detid_to_dvfsid(struct eemsn_det *det)
{
	unsigned int cpudvfsindex;
	enum eemsn_det_id detid = det_to_id(det);

	if (detid == EEMSN_DET_L)
		cpudvfsindex = MT_CPU_DVFS_LL;
	else
		cpudvfsindex = MT_CPU_DVFS_CCI;

	pr_debug("[%s] id:%d, cpudvfsindex:%d\n", __func__,
		detid, cpudvfsindex);

	return cpudvfsindex;
}

static int dbg_repo_proc_show(struct seq_file *m, void *v)
{
	int i;
	u32 *repo = m->private;
	char ch;

	for (i = 0; i < DBG_REPO_NUM; i++) {
		if (i >= REPO_I_LOG_S && (i - REPO_I_LOG_S) %
		    ENTRY_EACH_LOG == 0)
			ch = ':';	/* timestamp */
		else
			ch = '.';

		seq_printf(m, "%4d%c%08x%c",
			   i, ch, repo[i], i % 4 == 3 ? '\n' : ' ');
	}

	return 0;
}
/*
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */

/*
 * show current EEM stauts
 */
static int eem_debug_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] %s\n",
		   ((char *)(det->name) + 8),
		   det->disabled ? "disabled" : "enable"
		  );

	return 0;
}

/*
 * set EEM status by procfs interface
 */
static ssize_t eem_debug_proc_write(struct file *file,
				    const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_det *det = (struct eemsn_det *)PDE_DATA(file_inode(file));
	struct eem_ipi_data eem_data;
	int ipi_ret = 0;

	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = enabled;
		ipi_ret = eem_to_cpueb(IPI_EEMSN_DEBUG_PROC_WRITE, &eem_data);
		det->disabled = enabled;

	} else
		ret = -EINVAL;

out:
	free_page((unsigned long)buf);

	return (ret < 0) ? ret : count;
}

static int eem_aging_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	int ipi_ret = 0;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_AGING_DUMP_PROC_SHOW, &eem_data);
	seq_printf(m, "ipi_ret:%d\n", ipi_ret);

	return 0;
}

static int eem_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;
	unsigned char i, j;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_DUMP_PROC_SHOW, &eem_data);
	seq_printf(m, "ipi_ret:%d\n", ipi_ret);

	for (i = 0; i < NR_EEMSN_DET; i++) {
		seq_printf(m, "id:%d, vf_tbl_det pi_vf_num:%d\n",
			   i, eemsn_log->vf_tbl_det[i].pi_vf_num);
		if (eemsn_log->vf_tbl_det[i].pi_vf_num <= NR_PI_VF)
			for (j = 0; j < eemsn_log->vf_tbl_det[i].pi_vf_num; j++)
				seq_printf(m, "idx:%d, f:%d, v:0x%x\n",
					   j, eemsn_log->vf_tbl_det[i].pi_freq_tbl[j],
					   eemsn_log->vf_tbl_det[i].pi_volt_tbl[j]);
	}

	return 0;
}

static int eem_hrid_proc_show(struct seq_file *m, void *v)
{
	unsigned int i;
	int devinfo = 0;

	for (i = 0; i < 4; i++) {
		nvmem_device_read(nvmem_dev, DEVINFO_HRID_0 + 0x4 * i, sizeof(__u32), &devinfo);
		seq_printf(m, "%s[HRID][%d]: 0x%08X\n", EEM_TAG, i, devinfo);
	}

	return 0;
}

static int eem_efuse_proc_show(struct seq_file *m, void *v)
{
	unsigned int i;
	int devinfo = 0;
	for (i = 0; i < 25; i++) {
		nvmem_device_read(nvmem_dev, DEVINFO_IDX_0 + 0x4 * i, sizeof(__u32), &devinfo);
		seq_printf(m, "%s[PTP_DUMP] ORIG_RES%d: 0x%08X\n", EEM_TAG, i, devinfo);
	}

	return 0;
}

static int eem_mar_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s[CPU_L][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
		   EEM_TAG, LOW_TEMP_OFF, 0,
		   HIGH_TEMP_OFF, AGING_VAL_CPU);

	seq_printf(m, "%s[CPU_CCI][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
		   EEM_TAG, LOW_TEMP_OFF, 0,
		   HIGH_TEMP_OFF, AGING_VAL_CPU);

	return 0;
}

/*
 * show current voltage
 */
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;
	u32 i;

	seq_printf(m, "EEM[%s] read current voltage fail\n", det->name);

	if (det->features != 0) {
		for (i = 0; i < MAX_NR_FREQ; i++)
			seq_printf(m, "[%d],eem = [%x], pmic = [%x]\n",
				   i,
				   eemsn_log->det_vlog[det->det_id].volt_tbl_init2[i],
				   eemsn_log->det_vlog[det->det_id].volt_tbl_pmic[i]);
	}

	return 0;
}

static int eem_force_sensing_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_FORCE_SN_SENSING, &eem_data);
	seq_printf(m, "ret:%d\n", ipi_ret);

	return 0;
}

static int eem_pull_data_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_PULL_DATA, &eem_data);
	seq_printf(m, "ret:%d\n", ipi_ret);

	return 0;
}

/*
 * show EEM offset
 */
static int eem_offset_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	seq_printf(m, "%d\n", det->volt_offset);

	return 0;
}

/*
 * set EEM offset by procfs
 */
static ssize_t eem_offset_proc_write(struct file *file,
				     const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct eemsn_det *det = (struct eemsn_det *)PDE_DATA(file_inode(file));
	unsigned int ipi_ret = 0;
	struct eem_ipi_data eem_data;

	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = offset;
		ipi_ret = eem_to_cpueb(IPI_EEMSN_OFFSET_PROC_WRITE, &eem_data);
		/* to show in eem_offset_proc_show */
		det->volt_offset = (signed char)offset;
		pr_debug("set volt_offset %d(%d)\n", offset, det->volt_offset);
	} else {
		ret = -EINVAL;
		pr_debug("bad argument_1!! argument should be \"0\"\n");
	}

out:
	free_page((unsigned long)buf);

	return (ret < 0) ? ret : count;
}

#if SUPPORT_PICACHU
static void get_picachu_efuse(void)
{
	/* int *val; */
	phys_addr_t picachu_mem_base_phys;
	phys_addr_t picachu_mem_size;
	phys_addr_t picachu_mem_base_virt = 0;
	unsigned int sig;
	void __iomem *addr_ptr;
	void __iomem *spare1phys;

	/* val = (int *)&eem_devinfo; */

	picachu_mem_size = 0x80000;
	spare1phys = ioremap(EEM_PHY_SPARE0, 0);
	picachu_mem_base_phys = eem_read(spare1phys);
	if ((void __iomem *)picachu_mem_base_phys != NULL)
		picachu_mem_base_virt =
			(phys_addr_t)(uintptr_t)ioremap_wc(
			picachu_mem_base_phys,
			picachu_mem_size);

#if 0
	eem_error("phys:0x%llx, size:0x%llx, virt:0x%llx\n",
		(unsigned long long)picachu_mem_base_phys,
		(unsigned long long)picachu_mem_size,
		(unsigned long long)picachu_mem_base_virt);
#endif
	if ((void __iomem *)(picachu_mem_base_virt) != NULL) {
		/* 0x60000 was reserved for eem efuse using */
		addr_ptr = (void __iomem *)(picachu_mem_base_virt
			+ 0x60000);

		/* check signature */
		sig = (eem_read(addr_ptr) >>
			PICACHU_SIGNATURE_SHIFT_BIT) & 0xff;

		if (sig == PICACHU_SIG) {
			ctrl_agingload_enable = eem_read(addr_ptr) & 0x1;
			addr_ptr += 4;
			memcpy(eemsn_log->vf_tbl_det,
				addr_ptr, sizeof(eemsn_log->vf_tbl_det));

#if 0
			/* check efuse data */
			for (i = 1; i < cnt; i++) {
				if (((i == 3) || (i == 4) || (i == 7)) &&
				(eem_read(addr_ptr + i * 4) == 0)) {
					eem_error("Wrong PI-OD%d: 0x%x\n",
						i, eem_read(addr_ptr + i * 4));
					return;
				}
			}

			for (i = 1; i < cnt; i++)
				val[i] = eem_read(addr_ptr + i * 4);
#endif
		}
	}
}

#endif

/*
 * All
 */
PROC_FOPS_RO(dbg_repo);
PROC_FOPS_RW(eem_debug);
PROC_FOPS_RO(eem_cur_volt);
PROC_FOPS_RW(eem_offset);
PROC_FOPS_RO(eem_dump);
PROC_FOPS_RO(eem_aging_dump);
PROC_FOPS_RO(eem_hrid);
PROC_FOPS_RO(eem_efuse);
PROC_FOPS_RO(eem_mar);
PROC_FOPS_RO(eem_force_sensing);
PROC_FOPS_RO(eem_pull_data);

static int __init mtk_cpufreq_preinit(void)
{
	struct cdvfs_data eem_data;
	int err = 0;
	phys_addr_t eem_log_phy_addr, eem_log_virt_addr;

	err = mtk_ipi_register(&mcupm_ipidev, CH_S_EEMSN, NULL, NULL,
			                               (void *) &ipi_ackdata);
	if (err != 0) {
		pr_info("%s error ret:%d\n", __func__, err);
		return 0;
	}

	eem_log_phy_addr =
		mcupm_reserve_mem_get_phys(MCUPM_EEMSN_MEM_ID);
	eem_log_virt_addr =
		mcupm_reserve_mem_get_virt(MCUPM_EEMSN_MEM_ID);

	if (eem_log_virt_addr != 0)
		eemsn_log = (struct eemsn_dbg_log *)eem_log_virt_addr;
	else
		pr_info("mcupm_reserve_mem_get_virt fail\n");

	memset(&eem_data, 0, sizeof(struct cdvfs_data));
	eem_data.u.set_fv.arg[0] = eem_log_phy_addr;
	eem_data.u.set_fv.arg[1] = sizeof(struct eemsn_log);

	/* eemsn_log->efuse_sv = eem_read(EEM_TEMPSPARE1); */

#if SUPPORT_PICACHU
	get_picachu_efuse();
#endif

#if defined(MC50_LOAD)
	/* force set freq table */
	memcpy(eemsn_log->vf_tbl_det,
		mc50_tbl, sizeof(eemsn_log->vf_tbl_det));
#endif

	eem_data.cmd = IPI_EEMSN_SHARERAM_INIT;

	mtk_ipi_send_compl(&mcupm_ipidev, CH_S_EEMSN,
			   /*IPI_SEND_WAIT*/IPI_SEND_POLLING, &eem_data,
			   sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE, 2000);

	return 0;
}

static int create_cpufreq_debug_fs(void)
{
	int i;
	struct proc_dir_entry *dir = NULL;
	struct proc_dir_entry *eem_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	struct eemsn_det *det;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
		void *data;
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(eem_debug),
		PROC_ENTRY(eem_cur_volt),
		PROC_ENTRY(eem_offset),
	};
	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_dump),
		PROC_ENTRY(eem_aging_dump),
		PROC_ENTRY(eem_hrid),
		PROC_ENTRY(eem_efuse),
		PROC_ENTRY(eem_mar),
		PROC_ENTRY(eem_force_sensing),
		PROC_ENTRY(eem_pull_data),
	};

	dir = proc_mkdir("cpuhvfs", NULL);
	if (!dir)
		return -ENOMEM;

	if (!proc_create_data("dbg_repo", 0664, dir, &dbg_repo_proc_fops, csram_base))
		return -EIO;

	eem_dir = proc_mkdir("eem", NULL);
	for (i = 0; i < ARRAY_SIZE(eem_entries); i++)
		if(!proc_create(eem_entries[i].name, 0664, eem_dir, eem_entries[i].fops))
			return -3;

	for_each_det(det) {
		if(det->features == 0)
			continue;

		det_dir = proc_mkdir(det->name, eem_dir);
		if (!det_dir)
			return -2;

		for(i = 0 ; i<ARRAY_SIZE(det_entries); i++)
			if(!proc_create_data(det_entries[i].name, 0644, det_dir, det_entries[i].fops, det))
				return -3;
	}

	return 0;
}

static int get_devinfo(struct platform_device *pdev)
{
	int *val, ret = 1, i = 0, devinfo = 0;

	val = (int *)&eemsn_log->efuse_devinfo;

	nvmem_dev = nvmem_device_get(&pdev->dev, "mtk_efuse");
	if (IS_ERR(nvmem_dev))
		return PTR_ERR(nvmem_dev);

	for (i = 0; i < 12; i++) {
		ret = nvmem_device_read(nvmem_dev, DEVINFO_IDX_0 + 0x4 * i, sizeof(__u32), &devinfo);
		if (ret != sizeof(__u32)) {
			ret = -EINVAL;
			goto release_nvmem;
		}
		val[i]= devinfo;
	}

	ret = 1;

release_nvmem:
	nvmem_device_put(nvmem_dev);

	return ret;
}

static int mtk_cpufreq_probe(struct platform_device *pdev)
{
	struct mtk_cpu_dvfs_info *info;
	struct list_head *list, *tmp;
	int cpu, ret;
	struct cdvfs_data cdvfs_d;
	uint32_t cpufreq_buf[4];

	ret = mtk_ipi_register(&mcupm_ipidev, CH_S_CPU_DVFS, NULL, NULL,
			 (void *) &cpufreq_buf);
	if (ret)
		return -EINVAL;

	cdvfs_d.cmd = IPI_DVFS_INIT;
	cdvfs_d.u.set_fv.arg[0] = 0;
	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_CPU_DVFS,
				 IPI_SEND_POLLING, &cdvfs_d,
				 sizeof(struct cdvfs_data)/MBOX_SLOT_SIZE,
				 2000);
	if (ret)
		return 0;

	mtk_cpufreq_preinit();

	ret = get_devinfo(pdev);
	if(ret <= 0)
		return ret;

	for_each_possible_cpu(cpu) {
		info = mtk_cpu_dvfs_info_lookup(cpu);
		if (info)
			continue;

		info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
		if (!info) {
			ret = -ENOMEM;
			goto release_dvfs_info_list;
		}

		info->csram_base = of_iomap(pdev->dev.of_node, 0);
		if (!info->csram_base) {
			ret = -ENOMEM;
			goto release_dvfs_info_list;
		}

		ret = mtk_cpu_dvfs_info_init(info, cpu);
		if (ret)
			goto release_dvfs_info_list;

		list_add(&info->list_head, &dvfs_info_list);
	}

	ret = cpufreq_register_driver(&mtk_cpufreq_driver);
	if (ret)
		goto release_dvfs_info_list;

	csram_base = of_iomap(pdev->dev.of_node, 0);
	create_cpufreq_debug_fs();

	return 0;

release_dvfs_info_list:
	list_for_each_safe(list, tmp, &dvfs_info_list) {
		info = list_entry(list, struct mtk_cpu_dvfs_info, list_head);
		mtk_cpu_dvfs_info_release(info);
		list_del(list);
	}

	return ret;
}

static const struct of_device_id mtk_cpufreq_machines[] = {
	{ .compatible = "mediatek,mcupm-dvfsp", },
	{ }
};

MODULE_DEVICE_TABLE(of, mtk_cpufreq_machines);

static struct platform_driver mtk_cpufreq_platdrv = {
	.probe		= mtk_cpufreq_probe,
	.driver = {
		.name	= "dvfsp",
		.of_match_table = of_match_ptr(mtk_cpufreq_machines),
	},
};
module_platform_driver(mtk_cpufreq_platdrv);

MODULE_AUTHOR("Wei-Chia Su <Wei-Chia.Su@mediatek.com>");
MODULE_DESCRIPTION("Medaitek MCUPM CPUFreq Platform driver");
MODULE_LICENSE("GPL v2");
