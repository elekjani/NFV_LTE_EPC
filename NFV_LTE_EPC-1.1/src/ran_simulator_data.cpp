#include "ran_simulator.h"
#include "boost/program_options.hpp"

#define THREADS_COUNT "threads_count"
#define DURATION "duration"
#define RATE "rate"

#define RAN_IP_ADDR "ran_ip_addr"
#define TRAFMON_IP_ADDR "trafmon_ip_addr"
#define MME_S1_IP_ADDR "mme_ip_addr"
#define TRAFMON_PORT "trafmon_port"
#define MME_PORT "mme_port"
#define SGW_S1_IP_ADDR "sgw_s1_ip_addr"
#define SGW_S1_PORT "sgw_s1_port"

time_t g_start_time;
int g_threads_count;
uint64_t g_req_dur;
uint64_t g_req_rate;
uint64_t g_run_dur;
int g_tot_regs;
uint64_t g_tot_regstime;
pthread_mutex_t g_mux;
vector<thread> g_umon_thread;
vector<thread> g_dmon_thread;
vector<thread> g_threads;
thread g_rtt_thread;
TrafficMonitor g_traf_mon;


void utraffic_monitor() {

	while (1) {
		g_traf_mon.handle_uplink_udata();
	}

}

void dtraffic_monitor() {
	while (1) {

		g_traf_mon.handle_downlink_udata();		
	}
}
void ping(){
	string cmd;

	cmd = "ping -I 172.16.1.3 172.16.0.2 -c 60 | grep \"^rtt\" >> ping.txt";
	cout << cmd << endl;
	system(cmd.c_str());
}

void simulate(int arg) {
	CLOCK::time_point mstart_time;
	CLOCK::time_point mstop_time;
	MICROSECONDS mtime_diff_us;		
	Ran ran;
	int status;
	int ran_num;
	bool ok;
	bool time_exceeded;

	ran_num = arg;
	time_exceeded = false;
	ran.init(ran_num);
	ran.conn_mme();

	while (1) {
		// Run duration check
		g_utils.time_check(g_start_time, g_req_dur, time_exceeded);
		if (time_exceeded) {
			break;
		}

		// Start time
		mstart_time = CLOCK::now();	

		// Initial attach
		ran.initial_attach();

		// // Authentication
		ok = ran.authenticate();
		if (!ok) {
			TRACE(cout << "ransimulator_simulate:" << " autn failure" << endl;)
		 					return;
		}

		// // Set security
		ok = ran.set_security();
		if (!ok) {
			TRACE(cout << "ransimulator_simulate:" << " security setup failure" << endl;)
		 					return;
		}

		// // Set eps session
		ok = ran.set_eps_session(g_traf_mon);
		if (!ok) {
			TRACE(cout << "ransimulator_simulate:" << " eps session setup failure" << endl;)
		 					return;
		}

		/*
		// To find RTT
		if (ran_num == 0) {
			g_rtt_thread = thread(ping);
			g_rtt_thread.detach();
		}


		/* Data transfer */
		ran.transfer_data(g_req_dur, g_req_rate);

		// Detach
		ok = ran.detach();
		if (!ok) {
			TRACE(cout << "ransimulator_simulate:" << " detach failure" << endl;)
		 					return;
		}

		// Stop time
		mstop_time = CLOCK::now();

		// Response time
		mtime_diff_us = std::chrono::duration_cast<MICROSECONDS>(mstop_time - mstart_time);

		/* Updating performance metrics */
		g_sync.mlock(g_mux);
		g_tot_regs++;
		g_tot_regstime += mtime_diff_us.count();		
		g_sync.munlock(g_mux);


	}
}

void init() {
	g_start_time = time(0);
	g_tot_regs = 0;
	g_tot_regstime = 0;
	g_sync.mux_init(g_mux);	
	g_umon_thread.resize(NUM_MONITORS);
	g_dmon_thread.resize(NUM_MONITORS);
	g_threads.resize(g_threads_count);
	signal(SIGPIPE, SIG_IGN);
}

void run() {
	int i;

	///* Tun
	g_traf_mon.tun.set_itf("tun1", "172.16.0.1/16");
	g_traf_mon.tun.conn("tun1");

	/* Traffic monitor server //*/
	TRACE(cout << "Traffic monitor server started" << endl;)
	g_traf_mon.server.run(g_trafmon_ip_addr, g_trafmon_port);	

	//preparing data transfer clients
	cout<<"sgwclient initiated"<<endl;

	cout<<"sgwclient initiated"<<endl;
	g_traf_mon.client_count = g_threads_count;
	g_traf_mon.initialize(); //initializing sgw client heads
	// Uplink traffic monitor
	for (i = 0; i < NUM_MONITORS; i++) {
		g_umon_thread[i] = thread(utraffic_monitor);
		g_umon_thread[i].detach();		
	}

	// Downlink traffic monitor
	for (i = 0; i < NUM_MONITORS; i++) {
		g_dmon_thread[i] = thread(dtraffic_monitor);
		g_dmon_thread[i].detach();			
	}

	// Simulator threads */
	for (i = 0; i < g_threads_count; i++) {
		g_threads[i] = thread(simulate, i);
	}	
	for (i = 0; i < g_threads_count; i++) {
		if (g_threads[i].joinable()) {
			g_threads[i].join();
		}
	}	
}

void print_results() {
	g_run_dur = difftime(time(0), g_start_time);

	cout << endl << endl;
	cout << "Requested duration has ended. Finishing the program." << endl;
	cout << "Total number of registrations is " << g_tot_regs << endl;
	cout << "Total time for registrations is " << g_tot_regstime * 1e-6 << " seconds" << endl;
	cout << "Total run duration is " << g_run_dur << " seconds" << endl;
	cout << "Latency is " << ((double)g_tot_regstime/g_tot_regs) * 1e-6 << " seconds" << endl;
	cout << "Throughput is " << ((double)g_tot_regs/g_run_dur) << endl;	
}

void readConfig(int ac, char *av[]) {
  namespace po = boost::program_options;
  using namespace std;

  po::options_description desc("Allowed options");
  desc.add_options()
    (THREADS_COUNT, po::value<int>(), "Number of threads")
    (DURATION, po::value<int>(), "Duration in seconds")
    (RATE, po::value<int>(), "Rate of sent traffic by iperf in Mbps")
    (RAN_IP_ADDR, po::value<string>(), "IP address of the simulator")
    (TRAFMON_IP_ADDR, po::value<string>(), "IP address of the traffic monitor")
    (MME_S1_IP_ADDR, po::value<string>(), "IP address of the MME's S1 interface")
    (TRAFMON_PORT, po::value<int>()->default_value(g_trafmon_port), "Port of the trraffic monitor")
    (MME_PORT, po::value<int>()->default_value(g_mme_port), "Port of the MME")
    (SGW_S1_IP_ADDR, po::value<string>(), "IP address of SGW's S1 interface")
    (SGW_S1_PORT, po::value<int>()->default_value(sgw_s1_port), "Port of SGW's S1 interface")
    ;
  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  bool reqMissing = false;
  reqMissing |= vm.find(THREADS_COUNT) == vm.end();
  reqMissing |= vm.find(DURATION) == vm.end();
  reqMissing |= vm.find(RATE) == vm.end();
  reqMissing |= vm.find(RAN_IP_ADDR) == vm.end();
  reqMissing |= vm.find(TRAFMON_IP_ADDR) == vm.end();
  reqMissing |= vm.find(MME_S1_IP_ADDR) == vm.end();
  reqMissing |=  vm.find(SGW_S1_IP_ADDR) == vm.end();
  if (reqMissing) {
    TRACE(cout << desc << endl;)
    exit(1);
  }

  g_req_dur = vm[DURATION].as<int>();;
  g_req_rate = vm[RATE].as<int>();;
  g_threads_count = vm[THREADS_COUNT].as<int>();;

  g_ran_ip_addr = vm[RAN_IP_ADDR].as<string>();
  g_trafmon_ip_addr = vm[TRAFMON_IP_ADDR].as<string>();
  g_mme_s1_ip_addr = vm[MME_S1_IP_ADDR].as<string>();
  g_trafmon_port = vm[TRAFMON_PORT].as<int>();
  g_mme_port = vm[MME_PORT].as<int>();
  g_sgw_s1_ip_addr = vm[SGW_S1_IP_ADDR].as<string>();
  sgw_s1_port = vm[SGW_S1_PORT].as<int>();
}

int main(int argc, char *argv[]) {
  readConfig(argc, argv);
  init();
  run();
  print_results();
  return 0;
}
