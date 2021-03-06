/* linux/arch/arm/mach-exynos/asv-exynos5.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>

#include <asm/system_info.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos5_cal.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>
#include <plat/pm.h>

#define ARM_DVFS_LEVEL_NR		(24)
#define ARM_ASV_GRP_NR			(16)
#define ARM_MAX_VOLT			(1350000)
#define KFC_DVFS_LEVEL_NR		(19)
#define KFC_ASV_GRP_NR			(16)
#define KFC_MAX_VOLT			(1375000)
#define G3D_DVFS_LEVEL_NR		(7)
#define G3D_ASV_GRP_NR			(16)
#define G3D_MAX_VOLT			(1050000)
#define MIF_DVFS_LEVEL_NR		(9)
#define MIF_ASV_GRP_NR			(16)
#define MIF_MAX_VOLT			(1125000)
#define INT_DVFS_LEVEL_NR		(7)
#define INT_ASV_GRP_NR			(16)
#define INT_MAX_VOLT			(1025000)
#define ISP_DVFS_LEVEL_NR		(4)
#define ISP_ASV_GRP_NR			(16)
#define ISP_MAX_VOLT			(1000000)

#ifdef CONFIG_ASV_MARGIN_TEST
static int set_arm_volt;
static int set_kfc_volt;
static int set_int_volt;
static int set_mif_volt;
static int set_g3d_volt;
static int set_isp_volt;

static int __init get_arm_volt(char *str)
{
	get_option(&str, &set_arm_volt);
	return 0;
}
early_param("arm", get_arm_volt);

static int __init get_kfc_volt(char *str)
{
	get_option(&str, &set_kfc_volt);
	return 0;
}
early_param("kfc", get_kfc_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_isp_volt(char *str)
{
	get_option(&str, &set_isp_volt);
	return 0;
}
early_param("isp", get_isp_volt);

static int exynos5_get_margin_test_param(enum asv_type_id target_type)
{
	int add_volt = 0;

	switch (target_type)
	case ID_ARM:
		add_volt = set_arm_volt;
		break;
	case ID_KFC:
		add_volt = set_kfc_volt;
		break;
	case ID_G3D:
		add_volt = set_g3d_volt;
		break;
	case ID_MIF:
		add_volt = set_mif_volt;
		break;
	case ID_INT:
		add_volt = set_int_volt;
		break;
	case ID_ISP:
		add_volt = set_isp_volt;
		break;
	default:
		return add_volt;
	}
	return add_volt;
}

#endif

static void exynos5_set_abb(struct asv_info *asv_inform)
{
	unsigned int target_value;
	target_value = asv_inform->abb_info->target_abb;

	if (asv_inform->ops_cal->set_abb == NULL) {
		pr_err("%s: ASV : Fail set_abb from CAL\n", __func__);
		return;
	}

	asv_inform->ops_cal->set_abb(asv_inform->asv_type, target_value);
}

static struct abb_common exynos5_abb_common = {
	.set_target_abb = exynos5_set_abb,
};

static unsigned int exynos5_get_asv_group(struct asv_common *asv_comm)
{
	int asv_group;
	if (asv_comm->ops_cal->get_asv_gr == NULL)
		return asv_group = 0;

	asv_group = (int) asv_comm->ops_cal->get_asv_gr();
	if (asv_group < 0) {
		pr_err("%s: Faile get ASV group from CAL\n", __func__);
		return 0;
	}
	return asv_group;
}

static void exynos5_set_asv_info(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	bool useABB;

	if (asv_inform->ops_cal->get_use_abb == NULL || asv_inform->ops_cal->get_abb == NULL)
		useABB = false;
	else
		useABB = asv_inform->ops_cal->get_use_abb(asv_inform->asv_type);

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table)
		* asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	if (useABB) {
		asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table)
			* asv_inform->dvfs_level_nr), GFP_KERNEL);
		if (!asv_inform->asv_abb) {
			pr_err("%s: Memory allocation failed for asv abb\n", __func__);
			kfree(asv_inform->asv_volt);
			return;
		}
	}

	if (asv_inform->ops_cal->get_freq == NULL || asv_inform->ops_cal->get_vol == NULL) {
		pr_err("%s: ASV : Fail get call back function for CAL\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = asv_inform->ops_cal->get_freq(asv_inform->asv_type, i) * 1000;
		if (asv_inform->asv_volt[i].asv_freq == 0)
			pr_err("ASV : Fail get Freq from CAL!\n");
#ifdef CONFIG_ASV_MARGIN_TEST
		if (asv_inform->ops_cal->get_vol(asv_inform->asv_type, i) == 0) {
			asv_inform->asv_volt[i].asv_value =
				asv_inform->ops_cal->get_vol(asv_inform->asv_type, i)
				+ exynos5_get_margin_test_param(asv_inform->asv_type);
		} else {
			pr_err("ASV : Fail get Voltage value from CAL!\n");
			asv_inform->asv_volt[i].asv_value = asv_inform->max_volt_value;
		}
#else
		asv_inform->asv_volt[i].asv_value = asv_inform->ops_cal->get_vol(asv_inform->asv_type, i);
		if (asv_inform->asv_volt[i].asv_value == 0) {
			pr_err("ASV : Fail get Voltage value from CAL!\n");
			asv_inform->asv_volt[i].asv_value = asv_inform->max_volt_value;
		}
#endif
		if (useABB) {
			asv_inform->asv_abb[i].asv_freq = asv_inform->asv_volt[i].asv_freq;
			asv_inform->asv_abb[i].asv_value = asv_inform->ops_cal->get_abb(asv_inform->asv_type, i);
		}
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			if (useABB)
				pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
						asv_inform->name, i,
						asv_inform->asv_volt[i].asv_freq,
						asv_inform->asv_volt[i].asv_value,
						asv_inform->asv_abb[i].asv_value);
			else
				pr_info("%s LV%d freq : %d volt : %d\n",
						asv_inform->name, i,
						asv_inform->asv_volt[i].asv_freq,
						asv_inform->asv_volt[i].asv_value);
		}
	}
}

static struct asv_ops exynos5_asv_ops = {
	.get_asv_group  = exynos5_get_asv_group,
	.set_asv_info   = exynos5_set_asv_info,
};

static struct asv_ops_cal exynos5_asv_ops_cal = {
	.get_max_lv = DVFS_GetMaxLevel,
	.get_min_lv = DVFS_GetMinLevel,
	.get_vol = DVFS_GetTypicalVoltage,
	.get_freq = DVFS_GetFreqMhz,
	.get_abb = DVFS_GetABB,
	.get_use_abb = DVFS_UseDynamicABB,
	.set_abb = DVFS_SetABB,
};

struct asv_info exynos5_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5_asv_ops,
		.ops_cal	= &exynos5_asv_ops_cal,
		.abb_info	= &exynos5_abb_common,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos5_asv_ops,
		.ops_cal	= &exynos5_asv_ops_cal,
		.abb_info	= &exynos5_abb_common,
		.asv_group_nr	= ASV_GRP_NR(KFC),
		.dvfs_level_nr	= DVFS_LEVEL_NR(KFC),
		.max_volt_value = MAX_VOLT(KFC),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos5_asv_ops,
		.ops_cal	= &exynos5_asv_ops_cal,
		.abb_info	= &exynos5_abb_common,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos5_asv_ops,
		.ops_cal	= &exynos5_asv_ops_cal,
		.abb_info	= &exynos5_abb_common,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos5_asv_ops,
		.ops_cal	= &exynos5_asv_ops_cal,
		.abb_info	= &exynos5_abb_common,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	}, {
		.asv_type	= ID_ISP,
		.name		= "VDD_ISP",
		.ops		= &exynos5_asv_ops,
		.ops_cal	= &exynos5_asv_ops_cal,
		.asv_group_nr	= ASV_GRP_NR(ISP),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ISP),
		.max_volt_value = MAX_VOLT(ISP),
	},
};

unsigned int exynos5_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos5_asv_member); i++)
		add_asv_member(&exynos5_asv_member[i]);

	return 0;
}


#ifdef CONFIG_PM
static struct sleep_save exynos5_abb_save[] = {
	SAVE_ITEM(EXYNOS5430_BB_CON0),
	SAVE_ITEM(EXYNOS5430_BB_CON1),
	SAVE_ITEM(EXYNOS5430_BB_CON2),
	SAVE_ITEM(EXYNOS5430_BB_CON3),
	SAVE_ITEM(EXYNOS5430_BB_CON4),
};

static int exynos5_asv_suspend(void)
{
	struct asv_info *exynos_asv_info;
	int i;

	s3c_pm_do_save(exynos5_abb_save,
			ARRAY_SIZE(exynos5_abb_save));

	for (i = 0; i < ARRAY_SIZE(exynos5_asv_member); i++) {
		exynos_asv_info = &exynos5_asv_member[i];
		exynos_asv_info->ops_cal
			->set_abb(exynos_asv_info->asv_type, ABB_BYPASS);
	}

	return 0;
}

static void exynos5_asv_resume(void)
{
	s3c_pm_do_restore_core(exynos5_abb_save,
			ARRAY_SIZE(exynos5_abb_save));
}
#else
#define exynos5_asv_suspend NULL
#define exynos5_asv_resume NULL
#endif

static struct syscore_ops exynos5_asv_syscore_ops = {
	.suspend	= exynos5_asv_suspend,
	.resume		= exynos5_asv_resume,
};

static struct asv_common_ops_cal exynos5_asv_ops_common_cal = {
	.init			= DVFS_Initialze,
	.get_table_ver		= CHIPID_GetAsvTableVersion,
	.is_fused_sp_gr		= CHIPID_IsFusedSpeedGroup,
	.get_asv_gr		= DVFS_GetIdsGroup,
	.get_ids		= CHIPID_GetFusedIdsEgl,
};

int exynos5_init_asv(struct asv_common *asv_info)
{
	asv_info->ops_cal = &exynos5_asv_ops_common_cal;

	asv_info->ops_cal->init();	/* CAL initiallize */

	if (asv_info->ops_cal->get_ids != NULL)
		pr_info("ASV: EGL IDS: %d \n", asv_info->ops_cal->get_ids());
	if (asv_info->ops_cal->get_asv_gr != NULL)
		pr_info("ASV: ASV_Group : %d \n", asv_info->ops_cal->get_asv_gr());

	if (asv_info->ops_cal->is_fused_sp_gr())
		pr_info("ASV: Use Speed Group\n");
	else
		pr_info("ASV: Use not Speed Group\n");

	register_syscore_ops(&exynos5_asv_syscore_ops);

	asv_info->regist_asv_member = exynos5_regist_asv_member;

	return 0;
}
