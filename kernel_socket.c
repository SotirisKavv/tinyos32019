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
	port_t port = socketcb->port;

	switch(socketcb->type){
		case PEER:
			if (socketcb->socket_properties.peer->peer != NULL)
			{
				pipe_close_writer(socketcb->socket_properties.peer->send_pipe);
				pipe_close_writer(socketcb->socket_properties.peer->recv_pipe);

				socketcb->socket_properties.peer->peer->socket_properties.peer->peer = NULL;
				socketcb->socket_properties.peer->peer->refcount--;
			}
			break;
		case LISTENER:

			while(!is_rlist_empty(& socketcb->socket_properties.listener->request_queue)){
				rlnode* temp = rlist_pop_front(& socketcb->socket_properties.listener->request_queue);
				temp->rcb->active = 0;
			}

			kernel_broadcast(& socketcb->socket_properties.listener->req_available);

			break;
		case UNBOUND:
		default:
			break;
	}

	socketcb->refcount--;

	if (socketcb->refcount <= 0 && socketcb != NULL)
	{
		free(socketcb);
	}

	PORT_MAP[port] = NULL;

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

	PORT_MAP[socket->port] = socket;

	

	/* Initialiaze the Listener */
	socket->type = LISTENER;
	socket->socket_properties.listener->req_available = COND_INIT;
	rlnode_init(&socket->socket_properties.listener->request_queue, NULL);

	PORT_MAP[socket->port] = socket;

	return 0;
}



Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

