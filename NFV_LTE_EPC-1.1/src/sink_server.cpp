#include "sink_server.h"
#include "boost/program_options.hpp"

#define THREADS_COUNT "threads_count"
#define PGW_SGI_IP_ADDR "pgw_sgi_ip_addr"
#define SINK_IP_ADDR "sink_ip_addr"
#define PGW_SGI_PORT "pgw_sgi_port"
#define SINK_PORT "sink_port"

int g_threads_count;
vector<thread> g_threads;
thread g_mon_thread;
TrafficMonitor g_traf_mon;
vector<UdpClient> pgw_sgi_clients;

void traffic_monitor() {
	fd_set rcv_set;
	int max_fd;
	int status;

	max_fd = max(g_traf_mon.server.conn_fd, g_traf_mon.tun.conn_fd);
	while (1) {
		FD_ZERO(&rcv_set);
		FD_SET(g_traf_mon.server.conn_fd, &rcv_set); 
		FD_SET(g_traf_mon.tun.conn_fd, &rcv_set); 

		status = select(max_fd + 1, &rcv_set, NULL, NULL, NULL);
		g_utils.handle_type1_error(status, "select error: sinkserver_trafficmonitor");		

		if (FD_ISSET(g_traf_mon.server.conn_fd, &rcv_set)) {
			g_traf_mon.handle_uplink_udata();
		}
		else if (FD_ISSET(g_traf_mon.tun.conn_fd, &rcv_set)) {
			g_traf_mon.handle_downlink_udata(pgw_sgi_clients);
		}
	}
}

void sink(int sink_num) {
	string cmd;
	int port;

	port = (sink_num + 55000);
	cmd = "iperf3 -s -B 172.16.0.2 -p " + to_string(port);
	cout<<cmd<<endl;
	system(cmd.c_str());
}

void init() {
	g_threads.resize(g_threads_count);
	pgw_sgi_clients.resize(g_threads_count);

	for (int i = 0; i < g_threads_count; i++) {

		pgw_sgi_clients[i].conn(g_sink_ip_addr, g_pgw_sgi_ip_addr, g_pgw_sgi_port);

	}
}
void run() {
	int i;

	g_nw.add_itf(0, "172.16.0.2/8");

	/* Tun */
	g_traf_mon.tun.set_itf("tun1", "172.16.0.1/16");
	g_traf_mon.tun.conn("tun1");

	/* Traffic monitor server */
	TRACE(cout << "Traffic monitor server started" << endl;)
	g_traf_mon.server.run(g_sink_ip_addr, g_sink_port);

	g_mon_thread = thread(traffic_monitor);
	g_mon_thread.detach();
	for (i = 0; i < g_threads_count; i++) {
		g_threads[i] = thread(sink, i);
	}	
	for (i = 0; i < g_threads_count; i++) {
		if (g_threads[i].joinable()) {
			g_threads[i].join();
		}
	}	
}

void readConfig(int ac, char *av[]) {
  namespace po = boost::program_options;
  using namespace std;

  po::options_description desc("Allowed options");
  desc.add_options()
    (THREADS_COUNT, po::value<int>(), "Number of server threads")
    (PGW_SGI_IP_ADDR, po::value<string>(), "IP addres of PGW's SGI interface")
    (SINK_IP_ADDR, po::value<string>(), "IP address of the sink")

    (PGW_SGI_PORT, po::value<int>()->default_value(g_pgw_sgi_port), "Port of the PGW's SGI interface")
    (SINK_PORT, po::value<int>()->default_value(g_sink_port), "Port of the sink")
    ;
  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  if (vm.count(THREADS_COUNT) ||
      vm.count(PGW_SGI_IP_ADDR) ||
      vm.count(SINK_IP_ADDR)) {
    TRACE(cout << desc << endl;)
  }

  g_threads_count = vm[THREADS_COUNT].as<int>();
  g_pgw_sgi_ip_addr = vm[PGW_SGI_IP_ADDR].as<string>();
  g_sink_ip_addr = vm[SINK_IP_ADDR].as<string>();
  g_pgw_sgi_port = vm[PGW_SGI_PORT].as<int>();
  g_sink_port = vm[SINK_PORT].as<int>();
}

int main(int argc, char *argv[]) {
  readConfig(argc, argv);
  init();
  run();
  return 0;
}
