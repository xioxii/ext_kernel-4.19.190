ifeq ($(CONFIG_64BIT),y)

obj-$(CONFIG_NET_MEDIATEK_HSETH) += raeth.o
raeth-objs := raether.o raether_pdma.o raether_edma.o ra_mac.o mii_mgr.o ra_switch.o ra_dbg_proc.o
raeth-objs += raether_qdma.o
raeth-objs += raether_rss.o
raeth-objs += ra_path.o ra_sgmii.o

ifeq ($(CONFIG_RAETH_ETHTOOL),y)
raeth-objs += ra_ethtool.o
endif

raeth-objs += raether_hwlro.o
raeth-objs += ra_dbg_hwlro.o
raeth-objs += ra_dbg_hwioc.o

ccflags-y += -DCONFIG_MEDIATEK_HSETH

ifeq ($(CONFIG_RAETH_PDMA_DVT),y)
raeth-objs += dvt/raether_pdma_dvt.o
obj-m += dvt/pkt_gen.o
obj-m += dvt/pkt_gen_udp_frag.o
obj-m += dvt/pkt_gen_tcp_frag.o
endif

#ccflags-y += -Werror

endif
