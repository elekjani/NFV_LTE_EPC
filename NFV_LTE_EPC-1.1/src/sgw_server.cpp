#include "sgw_server.h"
#include "boost/program_options.hpp"

#define S11_THREADS_COUNT "s11_threads_count"
#define S1_THREADS_COUNT "s1_threads_count"
#define S5_THREADS_COUNT "s5_threads_count"

#define SGW_S11_IP_ADDR "sgw_s11_ip_addr"
#define SGW_S1_IP_ADDR "sgw_s1_ip_addr"
#define SGW_S5_IP_ADDR "sgw_s5_ip_addr"
#define PGW_S5_IP_ADDR "pgw_s5_ip_addr"
#define DS_IP "ds_ip"
#define DS_PORT "ds_port"
#define SGW_S11_PORT "sgw_s11_port"
#define SGW_S1_PORT "sgw_s1_port"
#define SGW_S5_PORT "sgw_s5_port"
#define PGW_S5_PORT "pgw_s5_port"

int g_s11_server_threads_count;
int g_s1_server_threads_count;
int g_s5_server_threads_count;
vector<thread> g_s11_server_threads;
vector<thread> g_s1_server_threads;
vector<thread> g_s5_server_threads;
vector<UdpClient> pgw_s5_clients;

Sgw g_sgw;

void init() {
	g_s11_server_threads.resize(g_s11_server_threads_count);
	g_s1_server_threads.resize(g_s1_server_threads_count);
	g_s5_server_threads.resize(g_s5_server_threads_count);
	g_sgw.initialize_kvstore_clients(g_s11_server_threads_count);
	signal(SIGPIPE, SIG_IGN);
}

void run() {
	int i;

	//uplink clients
	pgw_s5_clients.resize(g_s1_server_threads_count);
	for (i = 0; i < g_s1_server_threads_count; i++) {
		pgw_s5_clients[i].conn(g_sgw_s5_ip_addr,g_pgw_s5_ip_addr,g_pgw_s5_port);
	}
	/* SGW S11 server */
	TRACE(cout << "SGW S11 server started" << endl;)
	g_sgw.s11_server.run(g_sgw_s11_ip_addr, g_sgw_s11_port);
	for (i = 0; i < g_s11_server_threads_count; i++) {
		g_s11_server_threads[i] = thread(handle_s11_traffic,i);
	}	

	/* SGW S1 server */
	TRACE(cout << "SGW S1 server started" << endl;)
	g_sgw.s1_server.run(g_sgw_s1_ip_addr, g_sgw_s1_port);
	for (i = 0; i < g_s1_server_threads_count; i++) {
		g_s1_server_threads[i] = thread(handle_s1_traffic,i);
	}

	/* SGW S5 server */
	TRACE(cout << "SGW S5 server started" << endl;)
	g_sgw.s5_server.run(g_sgw_s5_ip_addr, g_sgw_s5_port);
	for (i = 0; i < g_s5_server_threads_count; i++) {
		g_s5_server_threads[i] = thread(handle_s5_traffic,i);
	}

	/* Joining all threads */
	for (i = 0; i < g_s11_server_threads_count; i++) {
		if (g_s11_server_threads[i].joinable()) {
			g_s11_server_threads[i].join();
		}
	}
	for (i = 0; i < g_s1_server_threads_count; i++) {
		if (g_s1_server_threads[i].joinable()) {
			g_s1_server_threads[i].join();
		}
	}	
	for (i = 0; i < g_s5_server_threads_count; i++) {
		if (g_s5_server_threads[i].joinable()) {
			g_s5_server_threads[i].join();
		}
	}				
}

void handle_s11_traffic(int worker_id) {
	UdpClient pgw_s5_client;
	struct sockaddr_in src_sock_addr;
	Packet pkt;

	pgw_s5_client.set_client(g_sgw_s5_ip_addr);

	while (1) {
		g_sgw.s11_server.rcv(src_sock_addr, pkt);
		pkt.extract_gtp_hdr();
		switch(pkt.gtp_hdr.msg_type) {
		/* Create session */
		case 1:
			TRACE(cout << "sgwserver_handles11traffic:" << " case 1: create session" << endl;)
			g_sgw.handle_create_session(src_sock_addr, pkt, pgw_s5_client,worker_id);
			break;

			/* Modify bearer */
		case 2:
			TRACE(cout << "sgwserver_handles11traffic:" << " case 2: modify bearer" << endl;)
			g_sgw.handle_modify_bearer(src_sock_addr, pkt,worker_id);
			break;

			/* Detach */
		case 3:
			TRACE(cout << "sgwserver_handles11traffic:" << " case 3: detach" << endl;)
			g_sgw.handle_detach(src_sock_addr, pkt, pgw_s5_client,worker_id);
			break;

			/* For error handling */
		default:
			TRACE(cout << "sgwserver_handles11traffic:" << " default case:" << endl;)
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
void handle_s1_traffic(int worker_id) {
	UdpClient pgw_s5_client;
	struct sockaddr_in src_sock_addr;
	Packet pkt;

	pgw_s5_client.set_client(g_sgw_s5_ip_addr);

	while (1) {
		g_sgw.s1_server.rcv(src_sock_addr, pkt);
		pkt.extract_gtp_hdr();
		switch(pkt.gtp_hdr.msg_type) {
		/* Uplink userplane data */
		case 1:
			TRACE(cout << "sgwserver_handles1traffic:" << " case 1: uplink udata" << endl;)
			g_sgw.handle_uplink_udata(pkt, pgw_s5_clients[getIndex(pkt)],worker_id);
			break;

			/* For error handling */
		default:
			TRACE(cout << "sgwserver_handles1traffic:" << " default case:" << endl;)
		}		
	}		
}

void handle_s5_traffic(int worker_id) {
	UdpClient enodeb_client;
	struct sockaddr_in src_sock_addr;
	Packet pkt;

	enodeb_client.set_client(g_sgw_s1_ip_addr);
	while (1) {
		g_sgw.s5_server.rcv(src_sock_addr, pkt);
		pkt.extract_gtp_hdr();
		switch(pkt.gtp_hdr.msg_type) {
		/* Downlink userplane data */
		case 3:
			TRACE(cout << "sgwserver_handles5traffic:" << " case 3: downlink udata" << endl;)
			g_sgw.handle_downlink_udata(pkt, enodeb_client,worker_id);
			break;

			/* For error handling */
		default:
			TRACE(cout << "sgwserver_handles5traffic:" << " default case:" << endl;	)
		}		
	}			
}

void readConfig(int ac, char *av[]) {
  namespace po = boost::program_options;
  using namespace std;

  po::options_description desc("Allowed options");
  desc.add_options()
    (S5_THREADS_COUNT, po::value<int>(), "Number of S5 server threads")
    (S11_THREADS_COUNT, po::value<int>(), "Number of S11 server threads")
    (S1_THREADS_COUNT, po::value<int>(), "Number of S1 server threads")

    (SGW_S11_IP_ADDR, po::value<string>(), "IP address of SGW's S11 interface")
    (SGW_S1_IP_ADDR, po::value<string>(), "IP address of SGW's S1 interface")
    (SGW_S5_IP_ADDR, po::value<string>(), "IP address of SGW's S5 interface")
    (PGW_S5_IP_ADDR, po::value<string>(), "IP address of PGW's S5 interface")
    (DS_IP, po::value<string>(), "IP address of the datastore")
    (DS_PORT, po::value<int>()->default_value(8090), "Port of the datastore")

    (SGW_S11_PORT, po::value<int>()->default_value(g_sgw_s11_port), "Port of the SGW's S11 interface")
    (SGW_S1_PORT, po::value<int>()->default_value(g_sgw_s1_port), "Port of the SGW's S1 interface")
    (SGW_S5_PORT, po::value<int>()->default_value(g_sgw_s5_port), "Port of the SGW's S5 interface")
    (PGW_S5_PORT, po::value<int>()->default_value(g_pgw_s5_port),"Port of the PGW-s S5 interface")
    ;
  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  bool reqMissing = false;
  reqMissing |= vm.find(S5_THREADS_COUNT) == vm.end();
  reqMissing |= vm.find(S11_THREADS_COUNT) == vm.end();
  reqMissing |= vm.find(S1_THREADS_COUNT) == vm.end();
  reqMissing |= vm.find(SGW_S11_IP_ADDR) == vm.end();
  reqMissing |= vm.find(SGW_S1_IP_ADDR) == vm.end();
  reqMissing |= vm.find(PGW_S5_IP_ADDR) == vm.end();
  reqMissing |= vm.find(DS_IP) == vm.end();
  reqMissing |= vm.find(SGW_S5_IP_ADDR) == vm.end();
  if (reqMissing) {
    TRACE(cout << desc << endl;)
    exit(1);
  }


  g_s11_server_threads_count = vm[S11_THREADS_COUNT].as<int>();
  g_s1_server_threads_count = vm[S1_THREADS_COUNT].as<int>();
  g_s5_server_threads_count = vm[S5_THREADS_COUNT].as<int>();

  g_sgw_s11_ip_addr = vm[SGW_S11_IP_ADDR].as<string>();
  g_sgw_s1_ip_addr = vm[SGW_S1_IP_ADDR].as<string>();
  g_sgw_s5_ip_addr = vm[SGW_S5_IP_ADDR].as<string>();
  g_pgw_s5_ip_addr = vm[PGW_S5_IP_ADDR].as<string>();
  std::stringstream sstm;
  sstm << vm[DS_IP].as<string>() << ':' << vm[DS_PORT].as<int>();
  dssgw_path = sstm.str();

  g_sgw_s11_port = vm[SGW_S11_PORT].as<int>();
  g_sgw_s1_port = vm[SGW_S1_PORT].as<int>();
  g_sgw_s5_port = vm[SGW_S5_PORT].as<int>();
  g_pgw_s5_port = vm[PGW_S5_PORT].as<int>();
}

int main(int argc, char *argv[]) {
  readConfig(argc, argv);
  init();cout<<"suc inint"<<endl;
  run();cout<<"suc inint2"<<endl;
  return 0;
}
