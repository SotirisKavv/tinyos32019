#include "kernel_streams.h"
#include "kernel_dev.h"
#include "kernel_pipe.h"
#include "tinyos.h"
#include "kernel_cc.h"
#include "util.h"

typedef enum {
	UNBOUND,
	LISTENER,
	PEER
}SocketType;

typedef struct listener_socket listSocket;
typedef struct peer_socket peerSocket;

/**
  @brief Socket Control Block.

  This structure holds all information pertaining to a socket.
 */
typedef struct socket_control_block{
	
	uint refcount;
	FCB* fcb;
	Fid_t fid;
	SocketType type;
	port_t port;
	union{
		listSocket* listener;
		peerSocket* peer;
	}socket_properties;

} socket_CB;

typedef struct listener_socket
{
	rlnode request_queue;
	CondVar req_available;
}listSocket;

typedef struct peer_socket
{
	pipe_CB* send_pipe, *recv_pipe;
	socket_CB* peer;
}peerSocket;

typedef struct connection_request{
	socket_CB* socket_req;
	CondVar accept_socket;
	int served;
	int active;
	rlnode node;
}RCB;

int socket_read(void* this, char* buffer, unsigned int size);

int socket_write(void* this, const char* buffer, unsigned int size);

int socket_close(void* this);










