
#include "tinyos.h"
#include "kernel_pipe.h"
#include "kernel_streams.h"

static file_ops readOperations = {
	.Open = NULL,
	.Read = pipe_read,
	.Write = NULL,
	.Close = pipe_close_reader
};

static file_ops writeOperations = {
	.Open = NULL,
	.Read = NULL,
	.Write = pipe_write,
	.Close = pipe_close_writer
};



int sys_Pipe(pipe_t* pipe)
{
	FCB* fcb[2];
	Fid_t fid[2];

	if (FCB_reserve(2, fid, fcb)==0)
	{
		return -1;
	}

	/*initialize pipe*/
	pipe_CB* pipecb = (pipe_CB*)malloc(sizeof(pipe_CB));
	assert(pipecb);

	pipecb->w = pipecb->r = 0;
	pipecb->full = 0;

	pipe->write = fid[0];
	pipe->read = fid[1];

	pipecb->writer = fcb[0];
	pipecb->reader = fcb[1];

	pipecb->hasSpace = COND_INIT;
	pipecb->hasData = COND_INIT;

	fcb[0]->streamobj = pipecb;
	fcb[0]->streamfunc = &writeOperations;

	fcb[1]->streamobj = pipecb;
	fcb[1]->streamfunc = &readOperations;

	return 0;
}

int pipe_read(void* this, char* buff, unsigned int size){

	pipe_CB* pipecb = (pipe_CB*) this;

	if (pipecb->reader == NULL || pipecb == NULL)
	{
		return -1;
	}

	int count = 0;
	while(count < size){

		if (pipecb->r == pipecb->w && pipecb->writer == NULL && !pipecb->full)
		{
			return count;
		}

		while(pipecb->r == pipecb->w && !pipecb->full){
			kernel_broadcast(& pipecb->hasData);
			kernel_wait(& pipecb->hasSpace, SCHED_PIPE);
		}

		buff[count] = pipecb->buffer[pipecb->r];
		pipecb->r = (pipecb->r + 1) % BUF_SIZE;
		pipecb->full = 0;

		count++;
	}

	return count;
}

int pipe_write(void* this, const char* buff, unsigned int size){
	
	pipe_CB* pipecb = (pipe_CB*) this;

	if (pipecb->writer == NULL || pipecb->reader == NULL || pipecb == NULL)
	{
		return -1;
	}

	int count = 0;
	while(count < size){

		if (pipecb->w + 1 == pipecb->r && pipecb->reader == NULL && !pipecb->full)
		{
			return count;
		}

		while(pipecb->r == pipecb->w && pipecb->full){
			kernel_broadcast(& pipecb->hasSpace);
			kernel_wait(& pipecb->hasData, SCHED_PIPE);
		}

		if (pipecb->r == pipecb->w)
			pipecb->full = 1;

		pipecb->buffer[pipecb->w] = buff[count];
		pipecb->w = (pipecb->w + 1) % BUF_SIZE;

		count++;
	}

	return count;
}

int pipe_close_reader(void* this){
	
	pipe_CB* pipecb = (pipe_CB*) this;
	
	if (pipecb == NULL)
	{
		return -1;
	}

	pipecb->reader = NULL;
	kernel_broadcast(& pipecb->hasData);

	if (pipecb->writer == NULL)
	{
		free(pipecb);
	}

	return 0;
}

int pipe_close_writer(void* this){
	
	pipe_CB* pipecb = (pipe_CB*) this;
	
	if (pipecb == NULL)
	{
		return -1;
	}

	pipecb->writer = NULL;
	kernel_broadcast(& pipecb->hasSpace);

	if (pipecb->reader == NULL)
	{
		free(pipecb);
	}

	return 0;
}