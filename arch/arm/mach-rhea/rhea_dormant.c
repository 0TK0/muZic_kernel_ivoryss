/******************************************************************************/
/* (c) 2011 Broadcom Corporation                                              */
/*                                                                            */
/* Unless you and Broadcom execute a separate written software license        */
/* agreement governing use of this software, this software is licensed to you */
/* under the terms of the GNU General Public License version 2, available at  */
/* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").                    */
/*                                                                            */
/******************************************************************************/

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/delay.h>

#include <mach/io_map.h>
#include <mach/rdb/brcm_rdb_scu.h>
#include <mach/rdb/brcm_rdb_csr.h>
#include <mach/rdb/brcm_rdb_chipreg.h>
#include <mach/rdb/brcm_rdb_root_clk_mgr_reg.h>
#include <mach/rdb/brcm_rdb_gicdist.h>
#include <mach/rdb/brcm_rdb_pwrmgr.h>
#include <mach/rdb/brcm_rdb_kproc_clk_mgr_reg.h>
#include <mach/rdb/brcm_rdb_axitp1.h>
#include <mach/rdb/brcm_rdb_swstm.h>
#include <mach/rdb/brcm_rdb_giccpu.h>
#include <mach/rdb/brcm_rdb_ptmr_wd.h>
#include <mach/rdb/brcm_rdb_cti.h>
#include <mach/rdb/brcm_rdb_gictr.h>
#include <mach/rdb/brcm_rdb_cstf.h>
#include <mach/rdb/brcm_rdb_pwrwdog.h>
#include <mach/rdb/brcm_rdb_a9cpu.h>
#include <mach/rdb/brcm_rdb_a9pmu.h>
#include <mach/rdb/brcm_rdb_a9ptm.h>
#include <mach/rdb/brcm_rdb_glbtmr.h>
#include <mach/pm.h>

#ifdef CONFIG_ROM_SEC_DISPATCHER
#include <mach/secure_api.h>
#endif

u32 dormant_base_va;
u32 dormant_base_pa;

/* Array of registers that needs to be saved and restored.  Please add to the
 * end if new data needs to be added.  Note that PLL DIV and TRIGGER registers
 * are moved to the top of the list to minimize issues at 156 mhz.
 */
static u32 addnl_save_reg_list[][2] = {
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PL310_DIV_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PL310_TRIGGER_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_POLICY0_MASK_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_POLICY1_MASK_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_POLICY2_MASK_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_POLICY3_MASK_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_INTEN_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_INTSTAT_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_LVM_EN_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_LVM0_3_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_LVM4_7_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_VLT0_3_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_VLT4_7_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_BUS_QUIESC_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_CORE0_CLKGATE_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_CORE1_CLKGATE_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_ARM_SWITCH_CLKGATE_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_ARM_PERIPH_CLKGATE_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_APB0_CLKGATE_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMA_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMB_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMC_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMCTRL0_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMCTRL1_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMCTRL2_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMCTRL3_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMCTRL4_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARMCTRL5_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLLARM_OFFSET_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_ARM_DIV_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_ARM_SEG_TRG_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_ARM_SEG_TRG_OVERRIDE_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_PLL_DEBUG_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_ACTIVITY_MON1_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_ACTIVITY_MON2_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_CLKGATE_DBG_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_APB_CLKGATE_DBG1_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_CLKMON_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_POLICY_DBG_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_POLICY_FREQ_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_POLICY_CTL_OFFSET), 0},
	{(KONA_PROC_CLK_VA + KPROC_CLK_MGR_REG_TGTMASK_DBG1_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_CONFIG_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_OUTIDS_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_CMD_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_FILTER_0_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRLOW_0_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRHIGH_0_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_FILTER_1_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRLOW_1_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRHIGH_1_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_FILTER_2_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRLOW_2_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRHIGH_2_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_FILTER_3_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRLOW_3_OFFSET), 0},
	{(KONA_AXITRACE1_VA + AXITP1_ATM_ADDRHIGH_3_OFFSET), 0},

	{(KONA_AXITRACE4_VA + AXITP1_ATM_CONFIG_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_OUTIDS_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_CMD_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_FILTER_0_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_ADDRLOW_0_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_ADDRHIGH_0_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_FILTER_1_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_ADDRLOW_1_OFFSET), 0},
	{(KONA_AXITRACE4_VA + AXITP1_ATM_ADDRHIGH_1_OFFSET), 0},

	{(KONA_AXITRACE16_VA + AXITP1_ATM_CONFIG_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_OUTIDS_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_CMD_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_FILTER_0_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_ADDRLOW_0_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_ADDRHIGH_0_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_FILTER_1_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_ADDRLOW_1_OFFSET), 0},
	{(KONA_AXITRACE16_VA + AXITP1_ATM_ADDRHIGH_1_OFFSET), 0},

	{(KONA_GICTR_VA + GICTR_GIC_CONFIG_OFFSET), 0},
	{(KONA_GICTR_VA + GICTR_GIC_OUTIDS_OFFSET), 0},
	{(KONA_GICTR_VA + GICTR_GIC_CMD_OFFSET), 0},

	{(KONA_FUNNEL_VA + CSTF_FUNNEL_CONTROL_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_PRIORITY_CONTROL_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_ITATBDATA0_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_ITATBCTR2_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_ITATBCTR1_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_ITATBCTR0_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_ITCTRL_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_CLAIM_TAG_SET_OFFSET), 0},
	{(KONA_FUNNEL_VA + CSTF_CLAIM_TAG_CLEAR_OFFSET), 0},

	{(KONA_SWSTM_VA + SWSTM_R_CONFIG_OFFSET), 0},

	{(KONA_SWSTM_ST_VA + SWSTM_R_CONFIG_OFFSET), 0},

	{(KONA_PWRWDOG_VA + PWRWDOG_PWRWDOG_CTRL_OFFSET), 0},
	{(KONA_PWRWDOG_VA + PWRWDOG_PWRWDOG_STAT_OFFSET), 0},
	{(KONA_PWRWDOG_VA + PWRWDOG_PWRWDOG_DLYEN_CNT_OFFSET), 0},
	{(KONA_PWRWDOG_VA + PWRWDOG_PWRWDOG_ACCU_CTRL_OFFSET), 0},
	{(KONA_PWRWDOG_VA + PWRWDOG_IRDROP_CTRL_OFFSET), 0},

	{(KONA_A9CPU0_VA + A9CPU_WFAR_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_VCR_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_DTRRX_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_DSCR_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_DTRTX_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BVR0_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BVR1_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BVR2_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BVR3_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BVR4_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BVR5_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BCR0_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BCR1_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BCR2_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BCR3_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BCR4_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_BCR5_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WVR0_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WVR1_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WVR2_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WVR3_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WVR4_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WVR5_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WCR0_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WCR1_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WCR2_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WCR3_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WCR4_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_WCR5_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_PRCR_OFFSET), 0},
	{(KONA_A9CPU0_VA + A9CPU_ICTRL_OFFSET), 0},

	{(KONA_A9PMU0_VA + A9PMU_PMEVCNTR0_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMEVCNTR1_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMEVCNTR2_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMEVCNTR3_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMEVCNTR4_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMEVCNTR5_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMCCNTR_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMXEVTYPER0_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMXEVTYPER1_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMXEVTYPER2_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMXEVTYPER3_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMXEVTYPER4_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMXEVTYPER5_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMCNTENSET_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMCNTENCLR_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMINTENSET_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMINTENCLR_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMOVSR_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMCR_OFFSET), 0},
	{(KONA_A9PMU0_VA + A9PMU_PMUSERENR_OFFSET), 0},

	{(KONA_A9PTM0_VA + A9PTM_ETMCR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMTRIGGER_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSSCR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMTEEVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMTECR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR2_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR3_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR4_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR5_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR6_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR7_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACVR8_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR2_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR3_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR4_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR5_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR6_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR7_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMACTR8_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTRLDVR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTRLDVR2_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTENR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTENR2_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTRLDEVR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTRLDEVR2_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTVR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCNTVR2_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSQ12EVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSQ21EVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSQ23EVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSQ31EVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSQ32EVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSQ13EVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSQR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMEXTOUTEVR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMEXTOUTEVR2_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCIDCVR1_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMCIDCMR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMSYNCFR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMEXTINSELR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMTSEVR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMAUXCR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ETMTRACEIDR_OFFSET), 0},
	{(KONA_A9PTM0_VA + A9PTM_ITCTRL_OFFSET), 0},

	{(KONA_A9CTI0_VA + CTI_CTICONTROL_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIAPPSET_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN0_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN1_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN2_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN3_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN4_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN5_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN6_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIINEN7_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN0_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN1_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN2_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN3_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN4_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN5_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN6_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTIOUTEN7_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_CTICHGATE_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_ASICCTL_OFFSET), 0},
	{(KONA_A9CTI0_VA + CTI_ICTRL_OFFSET), 0},

	{(KONA_GICCPU_VA + GICCPU_CONTROL_OFFSET), 0},
	{(KONA_GICCPU_VA + GICCPU_PRIORITY_MASK_OFFSET), 0},
	{(KONA_GICCPU_VA + GICCPU_BIN_PT_OFFSET), 0},
	{(KONA_GICCPU_VA + GICCPU_ALIAS_BIN_PT_NS_OFFSET), 0},

	{(KONA_PROFTMR_VA + GLBTMR_GLOB_LOW_OFFSET), 0},
	{(KONA_PROFTMR_VA + GLBTMR_GLOB_HI_OFFSET), 0},
	{(KONA_PROFTMR_VA + GLBTMR_GLOB_CTRL_OFFSET), 0},
	{(KONA_PROFTMR_VA + GLBTMR_GLOB_STATUS_OFFSET), 0},
	{(KONA_PROFTMR_VA + GLBTMR_GLOB_COMP_LOW_OFFSET), 0},
	{(KONA_PROFTMR_VA + GLBTMR_GLOB_COMP_HI_OFFSET), 0},
	{(KONA_PROFTMR_VA + GLBTMR_GLOB_INCR_OFFSET), 0},

	{(KONA_PTIM_VA + PTMR_WD_TIMER_LOAD_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_TIMER_COUNTER_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_TIMER_CTRL_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_TIMER_STATUS_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_WATCHDOG_LOAD_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_WATCHDOG_COUNTER_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_WATCHDOG_CTRL_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_WATCHDOG_STATUS_OFFSET), 0},
	{(KONA_PTIM_VA + PTMR_WD_WATCHDOG_RESET_STATUS_OFFSET), 0},

	{(KONA_GICDIST_VA + GICDIST_ENABLE_S_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY0_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY1_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY2_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY3_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY4_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY5_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY6_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_SECURITY7_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET0_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET1_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET2_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET3_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET4_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET5_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET6_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_ENABLE_SET7_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET0_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET1_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET2_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET3_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET4_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET5_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET6_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_SET7_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR0_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR1_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR2_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR3_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR4_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR5_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR6_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PENDING_CLR7_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL0_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL4_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL8_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL12_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL16_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL20_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL24_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL28_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL32_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL36_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL40_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL44_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL48_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL52_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL56_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_PRIORITY_LEVEL60_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET0_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET1_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET2_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET3_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET4_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET5_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET6_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET7_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET8_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET9_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET10_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET11_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET12_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET13_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET14_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET15_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET16_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET17_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET18_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET19_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET20_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET21_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET22_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET23_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET24_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET25_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET26_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET27_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET28_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET29_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET30_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET31_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET32_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET33_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET34_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET35_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET36_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET37_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET38_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET39_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET40_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET41_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET42_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET43_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET44_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET45_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET46_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET47_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET48_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET49_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET50_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET51_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET52_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET53_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET54_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET55_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET56_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET57_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET58_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET59_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET60_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET61_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET62_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_SPI_TARGET63_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG0_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG1_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG2_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG3_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG4_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG5_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG6_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG7_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG8_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG9_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG10_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG11_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG12_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG13_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG14_OFFSET), 0},
	{(KONA_GICDIST_VA + GICDIST_INT_CONFIG15_OFFSET), 0}
};

#ifdef CONFIG_ROM_SEC_DISPATCHER_LIB

void *hw_mmu_physical_address_get(void *x)
{
	void *hw_mmu_pa_get(void *x);
	return hw_mmu_pa_get(x);
}

u32 hw_sec_pub_dispatcher(u32 service_id, u32 flags, ...)
{
	u32 ret_val;
	va_list list;

	va_start(list, flags);
	ret_val = hw_sec_rom_pub_bridge(service_id, flags, list);
	va_end(list);

	return ret_val;
}

#endif

static void dormant_save_addnl_reg(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(addnl_save_reg_list); i++)
		addnl_save_reg_list[i][1] = readl_relaxed(
						addnl_save_reg_list[i][0]);
}

static void dormant_restore_addnl_reg(void)
{
	int i;
	u32 reg_val, reg_val1, val1, val2;
	u32 insurance = 0;

	/* Allow write access to the CCU registers */
	writel_relaxed(0xA5A501, (KONA_PROC_CLK_VA +
	       KPROC_CLK_MGR_REG_WR_ACCESS_OFFSET));

	/* Disable A9 CCU policy engine before restoring registers.
	 * Ensures that writing values onto policy registers doesnt
	 * take any immediate effect while restoring.
			 */
			writel_relaxed(readl(KONA_PROC_CLK_VA +
			       KPROC_CLK_MGR_REG_LVM_EN_OFFSET) |
			       KPROC_CLK_MGR_REG_LVM_EN_POLICY_CONFIG_EN_MASK,
			       KONA_PROC_CLK_VA +
			       KPROC_CLK_MGR_REG_LVM_EN_OFFSET);

			/* Wait for HW confirmation of policy lock */
			do {
				udelay(1);
				reg_val = readl(KONA_PROC_CLK_VA +
					KPROC_CLK_MGR_REG_LVM_EN_OFFSET);
				insurance++;
	} while ((reg_val & KPROC_CLK_MGR_REG_LVM_EN_POLICY_CONFIG_EN_MASK) &&
			insurance < 10000);
			WARN_ON(insurance >= 10000);

	for (i = 0; i < ARRAY_SIZE(addnl_save_reg_list); i++) {
		/* Restore the saved data */
		writel_relaxed(addnl_save_reg_list[i][1],
			       addnl_save_reg_list[i][0]);

		/* Write the go bit to trigger the frequency change */
		if (addnl_save_reg_list[i][0] == (KONA_PROC_CLK_VA +
		    KPROC_CLK_MGR_REG_TGTMASK_DBG1_OFFSET)) 
			writel_relaxed(KPROC_CLK_MGR_REG_POLICY_CTL_GO_AC_MASK |
			       KPROC_CLK_MGR_REG_POLICY_CTL_GO_MASK,
			       KONA_PROC_CLK_VA +
			       KPROC_CLK_MGR_REG_POLICY_CTL_OFFSET);
	}

	/* Wait until the new frequency takes effect */
	do {
		udelay(1);
		val1 =
		    readl(KONA_PROC_CLK_VA +
			  KPROC_CLK_MGR_REG_POLICY_CTL_OFFSET) &
			  KPROC_CLK_MGR_REG_POLICY_CTL_GO_MASK;
		val2 =
		    readl(KONA_PROC_CLK_VA +
			  KPROC_CLK_MGR_REG_POLICY_CTL_OFFSET) &
			  KPROC_CLK_MGR_REG_POLICY_CTL_GO_MASK;
	} while (val1 | val2);
	
	/* Lock CCU registers */
	writel_relaxed(0xA5A500, (KONA_PROC_CLK_VA +
	       KPROC_CLK_MGR_REG_WR_ACCESS_OFFSET));

}

static int dormant_pass;
module_param_named(dormant_pass, dormant_pass, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

static int dormant_fail;
module_param_named(dormant_fail, dormant_fail, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

int dormant_attempt;
module_param_named(dormant_attempt, dormant_attempt, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

void dormant_enter(void)
{
	bool ret = false;

	if (dormant_base_va != 0) {
		instrument_dormant_entry();

		/* Count of total number of times dormant entry was
		 * attempted.
		 */
		dormant_attempt++;
		dormant_save_addnl_reg();

		ret = dormant_start();

		if (ret == true) {
			/* Dormant entry succeeded */
			dormant_pass++;
			dormant_restore_addnl_reg();
		} else {
			dormant_fail++;
		}

		instrument_dormant_exit();
	}
}

static int __init rhea_dormant_init(void)
{
	void *vptr = NULL;

#ifdef CONFIG_ROM_SEC_DISPATCHER
	dma_addr_t drmt_buf_phy;

	vptr = dma_alloc_coherent(NULL, SZ_4K, &drmt_buf_phy, GFP_ATOMIC);
	if (vptr == NULL) {
		pr_info("%s: dormant dma buffer alloc failed\n", __func__);
		return -ENOMEM;
	}

	dormant_base_va = (u32) vptr;
	dormant_base_pa = (u32) drmt_buf_phy;
#else
	vptr = kmalloc(SZ_4K, GFP_ATOMIC);
	if (vptr == NULL) {
		pr_info("%s: dormant buffer alloc failed\n", __func__);
		return -ENOMEM;
	}

	dormant_base_va = (u32) vptr;
	dormant_base_pa = (u32) vptr;
#endif

	pr_info("dormant base @ va: 0x%08x, pa: 0x%08x\n", dormant_base_va,
		dormant_base_pa);

	return 0;
}

module_init(rhea_dormant_init);
