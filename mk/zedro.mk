# LIBS += -L/home/midhul/DZQ/end_hosts/build/lib 
LDFLAGS += -L/home/midhul/DZQ/end_hosts/build/lib -L/home/midhul/dpdk-stable-19.11.6/x86_64-native-linuxapp-gcc/lib

DPDK_LIBS = -Wl,-whole-archive -lrte_pmd_e1000 -Wl,-no-whole-archive
DPDK_LIBS += -Wl,-whole-archive -lrte_pmd_ixgbe -Wl,-no-whole-archive
DPDK_LIBS += -Wl,-whole-archive -lrte_mempool_ring -Wl,-no-whole-archive
DPDK_LIBS += -ldpdk
DPDK_LIBS += -lrte_eal
DPDK_LIBS += -lrte_ethdev
DPDK_LIBS += -lrte_hash
DPDK_LIBS += -lrte_mbuf
DPDK_LIBS += -lrte_mempool
DPDK_LIBS += -lrte_mempool
DPDK_LIBS += -lrte_mempool_stack
DPDK_LIBS += -lrte_ring

ifeq ($(ZEDRO_MLX4),y)
DPDK_LIBS += -Wl,-whole-archive -lrte_pmd_mlx4 -Wl,-no-whole-archive
DPDK_LIBS += -libverbs -lmlx4
endif

ifeq ($(ZEDRO_MLX5),y)
DPDK_LIBS += -Wl,-whole-archive -lrte_pmd_mlx5 -Wl,-no-whole-archive
DPDK_LIBS += -libverbs -lmlx5
endif

SYS_LIBS += -l:zedro -lm -lnuma
SYS_LIBS += $(DPDK_LIBS)
# SYS_LIBS += -l:zedro -lrte_mbuf -lrte_eal -lrte_mempool -lrte_ring -lrte_hash -lrte_kvargs -lrte_timer -lrte_ethdev -lrte_pmd_mlx4 -libverbs -lmlx4 -lm -lnuma
# SYS_LIBS += -lrte_flow_classify -Wl,--whole-archive -lrte_pipeline -Wl,--no-whole-archive -Wl,--whole-archive -lrte_table -Wl,--no-whole-archive -Wl,--whole-archive -lrte_port -Wl,--no-whole-archive -lrte_pdump -lrte_distributor -lrte_ip_frag -lrte_meter -lrte_fib -lrte_rib -lrte_lpm -lrte_acl -lrte_jobstats -lrte_metrics -lrte_bitratestats -lrte_latencystats -lrte_power -lrte_efd -lrte_bpf -lrte_ipsec -Wl,--whole-archive -lrte_cfgfile -lrte_gro -lrte_gso -lrte_hash -lrte_member -lrte_vhost -lrte_kvargs -lrte_mbuf -lrte_net -lrte_ethdev -lrte_bbdev -lrte_cryptodev -lrte_security -lrte_compressdev -lrte_eventdev -lrte_rawdev -lrte_timer -lrte_mempool -lrte_stack -lrte_mempool_ring -lrte_mempool_octeontx2 -lrte_ring -lrte_pci -lrte_eal -lrte_cmdline -lrte_reorder -lrte_sched -lrte_rcu -lrte_kni -lrte_common_cpt -lrte_common_octeontx -lrte_common_octeontx2 -lrte_common_dpaax -lrte_bus_pci -lrte_bus_vdev -lrte_bus_dpaa -lrte_bus_fslmc -lrte_mempool_bucket -lrte_mempool_stack -lrte_mempool_dpaa -lrte_mempool_dpaa2 -lrte_pmd_af_packet -lrte_pmd_ark -lrte_pmd_atlantic -lrte_pmd_avp -lrte_pmd_axgbe -lrte_pmd_bnxt -lrte_pmd_bond -lrte_pmd_cxgbe -lrte_pmd_dpaa -lrte_pmd_dpaa2 -lrte_pmd_e1000 -lrte_pmd_ena -lrte_pmd_enetc -lrte_pmd_enic -lrte_pmd_fm10k -lrte_pmd_failsafe -lrte_pmd_hinic -lrte_pmd_hns3 -lrte_pmd_i40e -lrte_pmd_iavf -lrte_pmd_ice -lrte_pmd_ixgbe -lrte_pmd_kni -lrte_pmd_lio -lrte_pmd_memif -lrte_pmd_mlx4 -libverbs -lmlx4 -lrte_pmd_nfp -lrte_pmd_null -lrte_pmd_octeontx2 -lrte_pmd_qede -lrte_pmd_ring -lrte_pmd_softnic -lrte_pmd_sfc_efx -lrte_pmd_tap -lrte_pmd_thunderx_nicvf -lrte_pmd_vdev_netvsc -lrte_pmd_virtio -lrte_pmd_vhost -lrte_pmd_ifc -lrte_pmd_vmxnet3_uio -lrte_bus_vmbus -lrte_pmd_netvsc -lrte_pmd_bbdev_null -lrte_pmd_bbdev_fpga_lte_fec -lrte_pmd_bbdev_turbo_sw -lrte_pmd_null_crypto -lrte_pmd_nitrox -lrte_pmd_octeontx_crypto -lrte_pmd_octeontx2_crypto -lrte_pmd_crypto_scheduler -lrte_pmd_dpaa2_sec -lrte_pmd_dpaa_sec -lrte_pmd_caam_jr -lrte_pmd_virtio_crypto -lrte_pmd_octeontx_zip -lrte_pmd_qat -lrte_pmd_skeleton_event -lrte_pmd_sw_event -lrte_pmd_dsw_event -lrte_pmd_octeontx_ssovf -lrte_pmd_dpaa_event -lrte_pmd_dpaa2_event -lrte_mempool_octeontx -lrte_pmd_octeontx -lrte_pmd_octeontx2_event -lrte_pmd_opdl_event -lrte_rawdev_skeleton -lrte_rawdev_dpaa2_cmdif -lrte_rawdev_dpaa2_qdma -lrte_bus_ifpga -lrte_rawdev_ioat -lrte_rawdev_ntb -lrte_rawdev_octeontx2_dma -Wl,--no-whole-archive -lrt -lm -lnuma -ldl -l:zedro
CFLAGS += -I/home/midhul/DZQ/end_hosts/include -I/home/midhul/dpdk-stable-19.11.6/x86_64-native-linuxapp-gcc/include