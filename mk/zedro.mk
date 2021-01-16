LIBS += -L/home/midhul/DZQ/end_hosts/build/lib 
LDFLAGS += -L/home/midhul/DZQ/end_hosts/build/lib -L/home/midhul/dpdk-stable-19.11.6/x86_64-native-linuxapp-gcc/lib
SYS_LIBS += -l:zedro -lrte_hash -lrte_mbuf -lrte_eal -lrte_mempool -lrte_ring -lrte_ethdev -lrte_hash -lrte_kvargs -lnuma
CFLAGS += -I/home/midhul/DZQ/end_hosts/src -I/home/midhul/dpdk-stable-19.11.6/x86_64-native-linuxapp-gcc/include