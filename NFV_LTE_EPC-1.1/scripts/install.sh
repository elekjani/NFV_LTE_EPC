#!/bin/bash

sudo apt-get update
sudo apt-get install -y openvpn libsctp-dev openssl
sudo add-apt-repository -y "ppa:patrickdk/general-lucid"
sudo apt-get update
sudo apt-get install -y iperf3 iperf htop ipvsadm git libssl-dev g++ libboost-all-dev
cd kvstore/Implementation/LevelDB_Disk/server
sudo bash install_server.sh
cd ../client
make
sudo make install
echo "COMPLETED"
