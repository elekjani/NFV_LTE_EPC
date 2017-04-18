#!/bin/bash

sudo -E apt-get update
sudo -E apt-get install -q -y openvpn libsctp-dev openssl
sudo -E add-apt-repository -y "ppa:patrickdk/general-lucid"
sudo -E apt-get update
sudo -E apt-get install -q -y iperf3 iperf htop ipvsadm git libssl-dev g++ libboost-all-dev
cd kvstore/Implementation/LevelDB_Disk/server
sudo bash install_server.sh
cd ../client
make
sudo make install
echo "COMPLETED"
