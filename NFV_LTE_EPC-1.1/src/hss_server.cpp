#include "hss_server.h"
#include "boost/program_options.hpp"
#include <iostream>

#define THREADS_COUNT "threads_count"
#define HSS_IP "hss_ip"
#define HSS_PORT "hss_port"
#define DS_IP "ds_ip"
#define DS_PORT "ds_port"

Hss g_hss;
int g_workers_count;

void init() {
	g_hss.initialize_kvstore_clients(g_workers_count); //error handling
	signal(SIGPIPE, SIG_IGN);

}


void run() {

	/* HSS server */
	TRACE(cout << "HSS server started" << endl;)
			g_hss.server.run(g_hss_ip_addr, g_hss_port, g_workers_count, handle_mme);
}

int handle_mme(int conn_fd, int worker_id) {
	Packet pkt;

	g_hss.server.rcv(conn_fd, pkt);
	if (pkt.len <= 0) {
		TRACE(cout << "hssserver_handlemme:" << " Connection closed" << endl;)
				return 0;
	}		
	pkt.extract_diameter_hdr();
	switch (pkt.diameter_hdr.msg_type) {
	/* Authentication info req */
	case 1:
		TRACE(cout << "hssserver_handlemme:" << " case 1: autn info req" << endl;)
		g_hss.handle_autninfo_req(conn_fd, pkt,worker_id);
		break;

		/* Location update */
	case 2:
		TRACE(cout << "hssserver_handlemme:" << " case 2: loc update" << endl;)
		g_hss.handle_location_update(conn_fd, pkt,worker_id);
		break;

		/* For error handling */	
	default:
		TRACE(cout << "hssserver_handlemme:" << " default case:" << endl;)
		break;
	}
	return 1;
}

void finish() {
}

void readConfig(int ac, char *av[]) {
  namespace po = boost::program_options;
  using namespace std;

  po::options_description desc("Allowed options");
  desc.add_options()
    (THREADS_COUNT, po::value<int>(), "Number of threads")
    (HSS_IP, po::value<string>(), "IP addres of the HSS")
    (DS_IP, po::value<string>(), "IP addres of the datastore")
    (DS_PORT, po::value<int>()->default_value(8090), "Port of the datastore")
    (HSS_PORT, po::value<int>()->default_value(g_hss_port), "Port of the HSS")
    ;

  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  if (vm.count(THREADS_COUNT) ||
      vm.count(DS_IP) ||
      vm.count(HSS_IP)) {
    TRACE(cout << desc << endl;)
  }

  g_workers_count = vm[THREADS_COUNT].as<int>();
  g_hss_ip_addr =  vm[HSS_IP].as<string>();
  g_hss_port = vm[HSS_PORT].as<int>();
  std::stringstream sstm;
  sstm << vm[DS_IP].as<string>() << ':' << vm[DS_PORT].as<int>();
  ds_path = sstm.str();
}

int main(int argc, char *argv[]) {
  readConfig(argc, argv);
  init();
  run();
  finish();
  return 0;
}
