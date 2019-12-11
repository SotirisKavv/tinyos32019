#include "kernel_streams.h"
#include "kernel_dev.h"
#include "kernel_proc.h"
#include "tinyos.h"
#include "kernel_cc.h"
#include "util.h"
#include "kernel_socket.h"
#include "tinyos.h"

socket_CB *PORT_MAP[MAX_PORT+1] = {NULL};

socket_CB* getSCB(Fid_t sock){

	if(get_fcb(sock) == NULL){
		return NULL;
	}

	FCB* fcb = get_fcb(sock);
	
	return (socket_CB*) fcb->streamobj;
}

socket_CB* peer_init(socket_CB* socket, pipe_CB* pipe1, pipe_CB* pipe2){

	socket->socket_properties.peer = (peerSocket*)xmalloc(sizeof(peerSocket));
	socket->socket_properties.peer->recv_pipe = pipe2;
	socket->socket_properties.peer->send_pipe = pipe1;
	socket->type = PEER;

	return socket;
}

static file_ops socketOperations = {
	.Open = NULL,
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};

int socket_read(void* this, char* buff, unsigned int size){

	socket_CB* socketcb = (socket_CB*) this;

	if (socketcb == NULL || socketcb->type != PEER)
	{
		return -1;
	}

	pipe_CB* pipecb = socketcb->socket_properties.peer->recv_pipe;

	return pipe_read(pipecb, buff, size);
}

int socket_write(void* this, const char* buff, unsigned int size){

	socket_CB* socketcb = (socket_CB*) this;

	if (socketcb == NULL || socketcb->type != PEER)
	{
		return -1;
	}

	pipe_CB* pipecb = socketcb->socket_properties.peer->send_pipe;

	return pipe_write(pipecb, buff, size);
}

int socket_close(void* this){

	socket_CB* socketcb = (socket_CB*) this;
	
	if (socketcb == NULL)
	{
		return -1;
	}

	switch(socketcb->type){
		case UNBOUND:
			break;
		case PEER:
			if (socketcb->socket_properties.peer->peer != NULL)
			{
				pipe_close_writer(socketcb->socket_properties.peer->send_pipe);
				pipe_close_reader(socketcb->socket_properties.peer->recv_pipe);

				socketcb->socket_properties.peer->peer->socket_properties.peer->peer = NULL;
				socketcb->socket_properties.peer->peer->refcount--;
			}
			free(socketcb->socket_properties.peer);
			break;
		case LISTENER:
			PORT_MAP[socketcb->port] = NULL;
			while(!is_rlist_empty(& socketcb->socket_properties.listener->request_queue)){
				rlnode* temp = rlist_pop_front(& socketcb->socket_properties.listener->request_queue);
				kernel_broadcast(& temp->rcb->accept_socket);
			}
			free(socketcb->socket_properties.listener);			
			break;
		default:
			break;
	}

	socketcb->refcount--;

	free(socketcb);

	return 0;
}


Fid_t sys_Socket(port_t port)
{
	if (port < 0 || port > MAX_PORT)
	{
		return NOFILE;
	}

	socket_CB* socketcb = (socket_CB*)malloc(sizeof(socket_CB));
	assert(socketcb);

	if (FCB_reserve(1, & socketcb->fid, & socketcb->fcb)==0)
	{
		return NOFILE;
	}

	socketcb->refcount = 0;
	socketcb->type = UNBOUND;
	socketcb->port = port;
	
	socketcb->fcb->streamobj = socketcb;
	socketcb->fcb->streamfunc = &socketOperations;

	socketcb->refcount++;
	return socketcb->fid;
}

int sys_Listen(Fid_t sock)
{
	socket_CB* socket = getSCB(sock);

	if (socket == NULL || socket->port <= NOPORT || socket->port > MAX_PORT ||
		PORT_MAP[socket->port] != NULL || socket->type != UNBOUND)
	{
		return -1;
	}

	/* Initialiaze the Listener */
	socket->type = LISTENER;
	socket->socket_properties.listener = (listSocket*)malloc(sizeof(listSocket));
	assert(socket->socket_properties.listener);
	socket->socket_properties.listener->req_available = COND_INIT;
	rlnode_init(&socket->socket_properties.listener->request_queue, NULL);

	PORT_MAP[socket->port] = socket;

	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	socket_CB* listener = getSCB(lsock);

	if (listener == NULL || listener->type != LISTENER)
	{
		return NOFILE;
	}

	while (is_rlist_empty(& listener->socket_properties.listener->request_queue) && PORT_MAP[listener->port] != NULL){
		kernel_wait(& listener->socket_properties.listener->req_available, SCHED_PIPE);
	}

	if (PORT_MAP[listener->port] == NULL)
	{
		return NOFILE;
	}

	rlnode* request_node = rlist_pop_front(& listener->socket_properties.listener->request_queue);
	RCB* request = request_node->rcb;
	socket_CB* req_sock = request->socket_req;

	Fid_t acc_sock_id = sys_Socket(NOPORT);

	if (acc_sock_id == NOFILE)
	{
		return NOFILE;
	}

	socket_CB* acc_sock = getSCB(acc_sock_id);

	pipe_CB* pipe1 = pipe_init();
	pipe_CB* pipe2 = pipe_init();

	if (pipe1 == NULL || pipe2 == NULL)
	{
		return NOFILE;
	}

	pipe1->reader = req_sock->fcb;
	pipe1->writer = acc_sock->fcb;

	pipe2->reader = acc_sock->fcb;
	pipe2->writer = req_sock->fcb;

	//check for misplace
	req_sock = peer_init(req_sock, pipe1, pipe2);
	acc_sock = peer_init(acc_sock, pipe2, pipe1);

	request->served = 1;
	acc_sock->refcount++;
	req_sock->refcount++;
	kernel_broadcast(& listener->socket_properties.listener->req_available);

	return acc_sock_id;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{	
	socket_CB* peer = getSCB(sock);

	if (peer->port < NOPORT || peer->port > MAX_PORT)
	{
		return -1;
	}

	if (peer == NULL || PORT_MAP[peer->port] != NULL || peer->type != UNBOUND)
	{
		return -1;
	}

	if (port <= NOPORT || port > MAX_PORT)
		return -1;

	socket_CB* listener = PORT_MAP[port];

	if (listener == NULL || listener->type != LISTENER)
		return -1;

	RCB* requectCB = (RCB*)xmalloc(sizeof(RCB));
	assert(requectCB);
	requectCB->socket_req = peer;
	requectCB->accept_socket = COND_INIT;
	requectCB->served = 0;

	rlnode_init(& requectCB->node, requectCB);
	rlist_push_back(& listener->socket_properties.listener->request_queue, & requectCB->node);

	kernel_broadcast(& listener->socket_properties.listener->req_available);
	kernel_timedwait(& requectCB->accept_socket, SCHED_PIPE, timeout);

	if (requectCB->served == 0)
		return -1;

	rlist_remove(&requectCB->node);

	return 0;
}



int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	socket_CB* socket = getSCB(sock);

	if (socket == NULL)
	{
		return -1;
	}

	int control = -1;
	switch(how){
		case SHUTDOWN_READ:
			control = pipe_close_reader(socket->socket_properties.peer->recv_pipe);
			break;
		case SHUTDOWN_WRITE:
			control = pipe_close_writer(socket->socket_properties.peer->send_pipe);
			break;
		case SHUTDOWN_BOTH:
			control = socket_close(socket);
	}

	return control;
}

