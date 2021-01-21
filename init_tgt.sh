sudo scripts/rpc.py bdev_null_create Null0 $((16777216)) 4096

sudo scripts/rpc.py nvmf_create_transport -t TCP -c $((8*1024))

sudo scripts/rpc.py nvmf_create_subsystem nqn.2020-07.com.midhul:null0 -a -s SPDK00000000000003 -d SPDK_Controller3
sudo scripts/rpc.py nvmf_subsystem_add_ns nqn.2020-07.com.midhul:null0 Null0
sudo scripts/rpc.py nvmf_subsystem_add_listener nqn.2020-07.com.midhul:null0 -t TCP -s 4420 -a $1
