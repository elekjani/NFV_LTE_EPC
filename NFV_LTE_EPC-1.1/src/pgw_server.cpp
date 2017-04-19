#include "pgw_server.h"
#include "boost/program_options.hpp"

#define S5_THREADS_COUNT "s5_threads_count"
#define SGI_THREADS_COUNT "sgi_threads_count"

#define SGW_S5_IP "sgw_s5_ip"
#define PGW_S5_IP "pgw_s5_ip"
#define PGW_SGI_IP "pgw_sgi_ip"
#define SINK_IP_ADDR "sink_ip_addr"
#define DS_IP "ds_ip"
#define DS_PORT "ds_port"

#define SGW_S5_PORT "sgw_s5_port"
#define PGW_S5_PORT "pgw_s5_port"
#define PGW_SGI_PORT "pgw_sgi_port"
#define SINK_PORT "sink_port"

int g_s5_server_threads_count;
int g_sgi_server_threads_count;
vector<thread> g_s5_server_threads;
vector<thread> g_sgi_server_threads;
vector<UdpClient> sgw_s5_clients;
Pgw g_pgw;

void check_usage(int argc) {
	if (argc < 3) {
		TRACE(cout << "Usage: ./<pgw_server_exec> S5_SERVER_THREADS_COUNT SGI_SERVER_THREADS_COUNT" << endl;)
						g_utils.handle_type1_error(-1, "Invalid usage error: pgwserver_checkusage");
	}
}

void init() {
	g_s5_server_threads.resize(g_s5_server_threads_count);
	g_sgi_server_threads.resize(g_sgi_server_threads_count);
	g_pgw.initialize_kvstore_clients(g_s5_server_threads_count);

	signal(SIGPIPE, SIG_IGN);
}

void run() {
	int i;

	/* downlink clients */
	sgw_s5_clients.resize(g_s5_server_threads_count);
	for (i = 0; i < g_sgi_server_threads_count; i++) {
		sgw_s5_clients[i].conn(g_pgw_s5_ip_addr,g_sgw_s5_ip_addr,g_sgw_s5_port);
	}
	/* PGW S5 server */
	TRACE(cout << "PGW S5 server started" << endl;)
	g_pgw.s5_server.run(g_pgw_s5_ip_addr, g_pgw_s5_port);
	for (i = 0; i < g_s5_server_threads_count; i++) {
		g_s5_server_threads[i] = thread(handle_s5_traffic,i);
	}

	/* PGW SGI server */
	TRACE(cout << "PGW SGI server started" << endl;)
	g_pgw.sgi_server.run(g_pgw_sgi_ip_addr, g_pgw_sgi_port);
	for (i = 0; i < g_sgi_server_threads_count; i++) {
		g_sgi_server_threads[i] = thread(handle_sgi_traffic,i);
	}

	/* Joining all threads */
	for (i = 0; i < g_s5_server_threads_count; i++) {
		if (g_s5_server_threads[i].joinable()) {
			g_s5_server_threads[i].join();
		}
	}	
	for (i = 0; i < g_sgi_server_threads_count; i++) {
		if (g_sgi_server_threads[i].joinable()) {
			g_sgi_server_threads[i].join();
		}
	}				
}

void handle_s5_traffic(int worker_id) {
	UdpClient sink_client;
	struct sockaddr_in src_sock_addr;
	Packet pkt;

	sink_client.set_client(g_pgw_sgi_ip_addr);
	while (1) {
		g_pgw.s5_server.rcv(src_sock_addr, pkt);
		pkt.extract_gtp_hdr();
		switch(pkt.gtp_hdr.msg_type) {
		/* Create session */
		case 1:
			TRACE(cout << "pgwserver_handles5traffic:" << " case 1: create session" << endl;	)
			g_pgw.handle_create_session(src_sock_addr, pkt,worker_id);
			break;

			/* Uplink userplane data */
		case 2:
			TRACE(cout << "pgwserver_handles5traffic:" << " case 2: uplink udata" << endl;	)
			g_pgw.handle_uplink_udata(pkt, sink_client,worker_id);
			break;

			/* Detach */
		case 4:
			TRACE(cout << "pgwserver_handles5traffic:" << " case 4: detach" << endl;	)
			g_pgw.handle_detach(src_sock_addr, pkt,worker_id);
			break;

			/* For error handling */
		default:
			TRACE(cout << "pgwserver_handles5traffic:" << " default case:" << endl;	)
		}		
	}
}
int getIndex(Packet pkt){
	int size = g_s5_server_threads_count;
	string ip = g_nw.get_dst_ip_addr(pkt);
	ip = ip.substr(ip.find_last_of('.')+1, ip.size());
	int index = stoi(ip);
	index = index % size;
	return index;
}
void handle_sgi_traffic(int worker_id) {
	UdpClient sgw_s5_client;
	struct sockaddr_in src_sock_addr;
	Packet pkt;

	sgw_s5_client.set_client(g_pgw_s5_ip_addr);
	while (1) {
		g_pgw.sgi_server.rcv(src_sock_addr, pkt);

		/* Downlink userplane data */
		TRACE(cout << "pgwserver_handlesgitraffic: downlink udata" << endl;	)
		g_pgw.handle_downlink_udata(pkt, sgw_s5_clients[getIndex(pkt)],worker_id);
	}	
}

void readConfig(int ac, char *av[]) {
  namespace po = boost::program_options;
  using namespace std;

  po::options_description desc("Allowed options");
  desc.add_options()
    (S5_THREADS_COUNT, po::value<int>(), "Number of S5 server threads")
    (SGI_THREADS_COUNT, po::value<int>(), "Number of SGI server threads")
    (SGW_S5_IP, po::value<string>(), "IP address of SGW's S5 interface")
    (PGW_S5_IP, po::value<string>(), "IP address of PGW's S5 interface")
    (PGW_SGI_IP, po::value<string>(), "IP address of PGW's SGI interface")
    (SINK_IP_ADDR, po::value<string>(), "IP address of the sink")
    (DS_IP, po::value<string>(), "IP address of the datastore")
    (DS_PORT, po::value<int>()->default_value(8090), "Port of the datastore")

    (SGW_S5_PORT, po::value<int>()->default_value(g_sgw_s5_port), "Port of the SGW's S5 interface")
    (PGW_S5_PORT, po::value<int>()->default_value(g_pgw_s5_port), "Port of the PGW's S5 interface")
    (PGW_SGI_PORT, po::value<int>()->default_value(g_pgw_sgi_port), "Port of the PGW's SGI interface")
    (SINK_PORT, po::value<int>()->default_value(g_sink_port), "Port of the sink")
    ;


  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  if (vm.count(S5_THREADS_COUNT) ||
      vm.count(SGI_THREADS_COUNT) ||
      vm.count(SGW_S5_IP) ||
      vm.count(PGW_S5_IP) ||
      vm.count(PGW_SGI_IP) ||
      vm.count(DS_IP) ||
      vm.count(SINK_IP_ADDR)) {
    TRACE(cout << desc << endl;)
  }

  g_s5_server_threads_count = vm[S5_THREADS_COUNT].as<int>();
  g_sgi_server_threads_count = vm[SGI_THREADS_COUNT].as<int>();

  g_sgw_s5_ip_addr = vm[SGW_S5_IP].as<string>();
  g_pgw_s5_ip_addr = vm[PGW_S5_IP].as<string>();
  g_pgw_sgi_ip_addr = vm[PGW_SGI_IP].as<string>();
  g_sink_ip_addr = vm[SINK_IP_ADDR].as<string>();
  std::stringstream sstm;
  sstm << vm[DS_IP].as<string>() << ':' << vm[DS_PORT].as<int>();
  dspgw_path = sstm.str();

  g_sgw_s5_port = vm[SGW_S5_PORT].as<int>();
  g_pgw_s5_port = vm[PGW_S5_PORT].as<int>();
  g_pgw_sgi_port = vm[PGW_SGI_PORT].as<int>();
  g_sink_port = vm[SINK_PORT].as<int>();

}

int main(int argc, char *argv[]) {
  readConfig(argc, argv);
  init();
  run();
  return 0;
}
