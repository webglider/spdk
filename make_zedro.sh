export RTE_SDK=/home/midhul/dpdk-stable-19.11.6/
make -C /home/midhul/DZQ/end_hosts -f lib.mk clean && make -C /home/midhul/DZQ/end_hosts -f lib.mk && \
make -C /home/midhul/spdk/module/sock/posix clean && make -C /home/midhul/spdk/module/sock/posix CONFIG_ZEDRO=y && \
make -C /home/midhul/spdk/examples/nvme/perf clean && make -C /home/midhul/spdk/examples/nvme/perf && \
make -C /home/midhul/spdk/examples/nvmf/nvmf clean && make -C /home/midhul/spdk/examples/nvmf/nvmf