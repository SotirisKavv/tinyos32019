#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

#define BUF_SIZE 8192	//size of buffer

/**
  @brief Pipe Control Block.

  This structure holds all information pertaining to a pipe.
 */
typedef struct pipe_control_block{
	char buffer[BUF_SIZE];
	CondVar hasSpace, hasData;
	uint r, w;
	int full;
	FCB* writer, *reader;
} pipe_CB;

int pipe_read(void* this, char* buffer, unsigned int size);

int pipe_write(void* this, const char* buffer, unsigned int size);

int pipe_close_reader(void* this);

int pipe_close_writer(void* this);