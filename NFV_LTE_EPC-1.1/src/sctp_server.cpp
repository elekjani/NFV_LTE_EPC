#include "sctp_server.h"

SctpServer::SctpServer() {
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	g_utils.handle_type1_error(listen_fd, "Socket error: sctpserver_sctpserver");
}

void SctpServer::run(string arg_ip_addr, int arg_port, int arg_workers_count, SctpServer::serve_client_t sc) {
	init(arg_ip_addr, arg_port, arg_workers_count, sc);
	create_workers();
	g_nw.set_sock_reuse(listen_fd);
	g_nw.bind_sock(listen_fd, sock_addr);
	accept_clients();
}

void SctpServer::init(string arg_ip_addr, int arg_port, int arg_workers_count, SctpServer::serve_client_t sc) {
	int status;

	port = arg_port;
	ip_addr = arg_ip_addr;
	g_nw.set_inet_sock_addr(ip_addr, port, sock_addr);
	workers_count = arg_workers_count;	
	workers.resize(workers_count);
	serve_client = sc;
	init_pipe_fds();
}

void SctpServer::init_pipe_fds() {
	int i;
	int status;

	pipe_fds.resize(workers_count);
	for (i = 0; i < workers_count; i++) {
		pipe_fds[i] = new int[2, 0];
		status = pipe(pipe_fds[i]);
		g_utils.handle_type1_error(status, "Pipe error: sctpserver_initpipefds");
	}
}

void SctpServer::create_workers() {
	int i;

	for (i = 0; i < workers_count; i++) {
		workers[i] = thread(&SctpServer::worker_func, this, i);
		workers[i].detach();
	}	
}

void SctpServer::worker_func(int worker_id) {
	int i;
	int max_fd;
	int status;
	int pipe_fd;
	int new_conn_fd;
	int new_conn_ip;
	int conn_fds_size;
	vector<int> conn_fds;
  vector<int> conn_ips;
	fd_set fds;
	Packet pkt;

	new_conn_fd = 0;
	new_conn_ip = 0;
	pipe_fd = pipe_fds[worker_id][0];
	max_fd = pipe_fd;
	while (1) {
		conn_fds_size = conn_fds.size();	
		FD_ZERO(&fds);
		if(fcntl(pipe_fd, F_GETFL)>=0){

			FD_SET(pipe_fd, &fds);
		}
		for (i = 0; i < conn_fds_size; i++) {

			if(fcntl(conn_fds[i], F_GETFL)<0){

				conn_fds.erase(conn_fds.begin() + i);
				conn_fds_size--;
				continue;
			}

			if(conn_fds[i] > 0) FD_SET(conn_fds[i], &fds);
			if(conn_fds[i] > max_fd) max_fd = conn_fds[i];
		}

		status = select(max_fd + 1, &fds, NULL, NULL, NULL);
		g_utils.handle_type2_error(status, "Select error: sctpserver_workerfunc:worker_id="+to_string(worker_id)+":");

		if (FD_ISSET(pipe_fd, &fds)) {
			status = g_nw.read_sctp_pkt(pipe_fd, pkt);
			g_utils.handle_type1_error(status, "Read connection fd error: sctpserver_workerfunc");
			pkt.extract_item(new_conn_fd);
			pkt.extract_item(new_conn_ip);
			conn_fds.push_back(new_conn_fd);
      conn_ips.push_back(new_conn_ip);
			max_fd = max(max_fd, new_conn_fd);
			new_conn_fd = 0;
		}

		for (i = 0; i < conn_fds_size; ) {

			status = 1;
			if (FD_ISSET(conn_fds[i], &fds)) {
				status = serve_client(conn_fds[i], conn_ips[i], worker_id);
			}

			if (status == 0) {
				close(conn_fds[i]);
				conn_fds.erase(conn_fds.begin() + i);
        conn_ips.erase(conn_ips.begin() + i);
				conn_fds_size--;
				max_fd = max(pipe_fd, g_utils.max_ele(conn_fds));						
			}
			else {
				i++;
			}
		}

	}
}

void SctpServer::accept_clients() {
	int turn;
	int status;
	int conn_fd;
	Packet pkt;
	struct sockaddr_in client_sock_addr;

	turn = 0;
	listen(listen_fd, 500);
	while (1) {
		conn_fd = accept(listen_fd, (struct sockaddr *)&client_sock_addr, &g_sock_addr_len);
		g_utils.handle_type1_error(conn_fd, "Accept error: sctpserver_acceptclients");
		pkt.clear_pkt();
		pkt.append_item(conn_fd);
		pkt.append_item(client_sock_addr.sin_addr.s_addr);
		status = g_nw.write_sctp_pkt(pipe_fds[turn][1], pkt);
		g_utils.handle_type1_error(status, "Thread communication error: sctpserver_acceptclients");
		turn = ((turn + 1) % workers_count);
	}
}

void SctpServer::snd(int conn_fd,  Packet pkt) {
	int status;

	while (1) {
		status = g_nw.write_sctp_pkt(conn_fd, pkt);
		if (errno == EPERM) {
			errno = 0;
			usleep(1000);
			continue;
		}
		else {
			break;
		}		
	}
	g_utils.handle_type2_error(status, "Write error: sctpserver_snd");
}

void SctpServer::rcv(int conn_fd, Packet &pkt) {
	int status;

	status = g_nw.read_sctp_pkt(conn_fd, pkt);
	g_utils.handle_type2_error(status, "Read error: sctpserver_rcv");
}

SctpServer::~SctpServer() {
	close(listen_fd);
}
