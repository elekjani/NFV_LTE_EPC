#ifndef SCTP_SERVER_H
#define SCTP_SERVER_H

#include "diameter.h"
#include "gtp.h"
#include "network.h"
#include "packet.h"
#include "s1ap.h"
#include "utils.h"

class SctpServer {
public:
  typedef int (*serve_client_t)(int, unsigned int, int);
private:
	/* Address parameters */
	int listen_fd;
	int port;
	string ip_addr;
	struct sockaddr_in sock_addr;

	/* Thread pool parameters */
	int workers_count;
	vector<thread> workers;	
  SctpServer::serve_client_t serve_client;

	/* Pipe parameter - for communication between main thread and worker threads */
	vector<int*> pipe_fds;

	void init(string, int, int, SctpServer::serve_client_t);
	void init_pipe_fds();
	void create_workers();
	void worker_func(int);
	void accept_clients();
	
public:
	SctpServer();
	void run(string, int, int, SctpServer::serve_client_t);
	void snd(int, Packet);
	void rcv(int, Packet&);
	~SctpServer();
};

#endif /* SCTP_SERVER_H */
