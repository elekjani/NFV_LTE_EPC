#include "mme_server.h"
#include "boost/program_options.hpp"

#define THREADS_COUNT "threads_count"
#define HSS_IP "hss_ip"
#define MME_IP "mme_ip"
#define HSS_PORT "hss_port"
#define SGW_S1_IP "sgw_s1_ip"
#define SGW_S11_IP "sgw_s11_ip"
#define SGW_S5_IP "sgw_s5_ip"
#define PGW_S5_IP "pgw_s5_ip"
#define DS_IP "ds_ip"
#define DS_PORT "ds_port"

#define TRAFMON_PORT "trafmon_port"
#define MME_PORT "mme_port"
#define SGW_S11_PORT "sgw_s11_port"
#define SGW_S1_PORT "sgw_s1_port"
#define SGW_S5_PORT "sgw_s5_port"
#define PGW_S5_PORT "pgw_s5_port"

Mme g_mme;
int g_workers_count;
vector<SctpClient> hss_clients;
vector<UdpClient> sgw_s11_clients;

void init() {
	hss_clients.resize(g_workers_count);
	sgw_s11_clients.resize(g_workers_count);
	g_mme.initialize_kvstore_clients(g_workers_count);

	signal(SIGPIPE, SIG_IGN);
}

void run() {
	int i;

	TRACE(cout << "MME server started" << endl;)

	for (i = 0; i < g_workers_count; i++) {
		hss_clients[i].conn(g_hss_ip_addr, g_hss_port);	
		sgw_s11_clients[i].conn(g_mme_ip_addr, g_sgw_s11_ip_addr, g_sgw_s11_port);
	}

	g_mme.server.run(g_mme_ip_addr, g_mme_port, g_workers_count, handle_ue);
}

int handle_ue(int conn_fd, int worker_id) {
	bool res;
	Packet pkt;

	g_mme.server.rcv(conn_fd, pkt);
	if (pkt.len <= 0) {
		TRACE(cout << "mmeserver_handleue:" << " Connection closed" << endl;)
				return 0;
	}
	pkt.extract_s1ap_hdr();
	if (pkt.s1ap_hdr.mme_s1ap_ue_id == 0) {
		switch (pkt.s1ap_hdr.msg_type) {
		/* Initial Attach request */
		case 1:
			TRACE(cout << "mmeserver_handleue:" << " case 1: initial attach" << endl;)
			g_mme.handle_initial_attach(conn_fd, pkt, hss_clients[worker_id],worker_id);
			break;

			/* For error handling */
		default:
			TRACE(cout << "mmeserver_handleue:" << " default case: new" << endl;)
			break;
		}		
	}
	else if (pkt.s1ap_hdr.mme_s1ap_ue_id > 0) {
		switch (pkt.s1ap_hdr.msg_type) {
		/* Authentication response */
		case 2:
			TRACE(cout << "mmeserver_handleue:" << " case 2: authentication response" << endl;)
			res = g_mme.handle_autn(conn_fd, pkt,worker_id);
			if (res) {
				g_mme.handle_security_mode_cmd(conn_fd, pkt,worker_id);
			}
			break;

			/* Security Mode Complete */
		case 3:
			TRACE(cout << "mmeserver_handleue:" << " case 3: security mode complete" << endl;)

			res = g_mme.handle_security_mode_complete(conn_fd, pkt,worker_id);
			if (res) {
				g_mme.handle_create_session(conn_fd, pkt, sgw_s11_clients[worker_id],worker_id);
			}
			break;

			/* Attach Complete */
		case 4:
			TRACE(cout << "mmeserver_handleue:" << " case 4: attach complete" << endl;)

			g_mme.handle_attach_complete(pkt,worker_id);
			g_mme.handle_modify_bearer(conn_fd, pkt, sgw_s11_clients[worker_id],worker_id);
			break;

			/* Detach request */
		case 5:
			TRACE(cout << "mmeserver_handleue:" << " case 5: detach request" << endl;)

			g_mme.handle_detach(conn_fd, pkt, sgw_s11_clients[worker_id],worker_id);

			break;

			/* For error handling */	
		default:
			TRACE(cout << "mmeserver_handleue:" << " default case: attached" << endl;)
			break;
		}				
	}		
	return 1;
}

void readConfig(int ac, char *av[]) {
  namespace po = boost::program_options;
  using namespace std;

  po::options_description desc("Allowed options");
  desc.add_options()
    (THREADS_COUNT, po::value<int>(), "Number of threads")
    (HSS_IP, po::value<string>(), "IP addres of the HSS")
    (MME_IP, po::value<string>(), "IP addres of the MME")
    (HSS_PORT, po::value<int>()->default_value(g_hss_port), "Port of the HSS")
    (SGW_S1_IP, po::value<string>(), "IP address of SGW's S1 interface")
    (SGW_S11_IP, po::value<string>(), "IP address of SGW's S11 interface")
    (SGW_S5_IP, po::value<string>(), "IP address of SGW's S5 interface")
    (PGW_S5_IP, po::value<string>(), "IP address of PGW's S5 interface")
    (DS_IP, po::value<string>(), "IP address of datastore")
    (DS_PORT, po::value<int>()->default_value(8090), "Port of the datasoter")

    (TRAFMON_PORT, po::value<int>()->default_value(g_trafmon_port), "Port of the traffic monitor")
    (MME_PORT, po::value<int>()->default_value(g_mme_port), "Port of the MME")
    (SGW_S11_PORT, po::value<int>()->default_value(g_sgw_s11_port), "Port of the SGW's S11 interface")
    (SGW_S1_PORT, po::value<int>()->default_value(g_sgw_s1_port), "Port of the SGW's S1 interface")
    (SGW_S5_PORT, po::value<int>()->default_value(g_sgw_s5_port), "Port of the SGW's S5 interface")
    (PGW_S5_PORT, po::value<int>()->default_value(g_pgw_s5_port), "Port of the PGW's S5 interface")
    ;

  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  bool reqMissing = false;
  reqMissing |= vm.find(THREADS_COUNT) == vm.end();
  reqMissing |= vm.find(HSS_IP) == vm.end();
  reqMissing |= vm.find(MME_IP) == vm.end();
  reqMissing |= vm.find(SGW_S1_IP) == vm.end();
  reqMissing |= vm.find(SGW_S11_IP) == vm.end();
  reqMissing |= vm.find(SGW_S5_IP) == vm.end();
  reqMissing |= vm.find(DS_IP) == vm.end();
  reqMissing |= vm.find(PGW_S5_IP) == vm.end();
  if (reqMissing) {
    TRACE(cout << desc << endl;)
    exit(1);
  }


  g_workers_count = vm[THREADS_COUNT].as<int>();
  g_hss_ip_addr =  vm[HSS_IP].as<string>();
  g_mme_ip_addr =  vm[MME_IP].as<string>();
  g_hss_port = vm[HSS_PORT].as<int>();

  g_sgw_s11_ip_addr = vm[SGW_S11_IP].as<string>();
  g_sgw_s1_ip_addr = vm[SGW_S1_IP].as<string>();
  g_sgw_s5_ip_addr = vm[SGW_S5_IP].as<string>();
  g_pgw_s5_ip_addr = vm[PGW_S5_IP].as<string>();
  std::stringstream sstm;
  sstm << vm[DS_IP].as<string>() << ':' << vm[DS_PORT].as<int>();
  dsmme_path = sstm.str();

  g_trafmon_port = vm[TRAFMON_PORT].as<int>();
  g_mme_port = vm[MME_PORT].as<int>();
  g_sgw_s11_port = vm[SGW_S11_PORT].as<int>();
  g_sgw_s1_port = vm[SGW_S1_PORT].as<int>();
  g_sgw_s5_port = vm[SGW_S5_PORT].as<int>();
}

int main(int argc, char *argv[]) {
  readConfig(argc, argv);
  init();
  run();
  return 0;
}
