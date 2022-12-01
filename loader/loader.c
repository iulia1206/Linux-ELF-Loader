/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "exec_parser.h"

static so_exec_t *exec;
static int pageSize;
static struct sigaction oldButGold;
static char pathFile[100];

//reads exact x bytes from a file into buff
static size_t read_x_bytes(int offset, size_t x, void *buff)
{ 
	int fd;
	fd = open(pathFile, O_RDONLY);
	
	lseek(fd,offset,SEEK_SET);

	size_t bytesRead = 0;

	int i; 
	
	for(i=bytesRead;i<x;i++) {
		size_t bytesReadNow = read(fd,buff+bytesRead,x-bytesRead);

		if(bytesRead < 0)
			return -1;

		if(bytesRead == 0)
			return bytesRead;

		bytesRead =bytesRead + bytesReadNow;
	}

	close(fd);
	return bytesRead;
}

static void segv_handler(int signum, siginfo_t *info, void* context){

	if(signum != SIGSEGV){ 
		oldButGold.sa_sigaction(signum,info,context);
		return;
	}

	char* addr = (char*) info->si_addr;

	pageSize = getpagesize(); 

	int i;

	int segmentFound=0;

	//check if the page fault is located in one of the segments
	for(i=0;i<exec->segments_no;i++)
		if((uintptr_t)addr >= (uintptr_t)exec->segments[i].vaddr && (uintptr_t)addr <= (uintptr_t)exec->segments[i].vaddr + exec->segments[i].mem_size)
		{
			segmentFound=1;
			break;
		}
	if(segmentFound!=0)
	{
		//find the segment in which is located the page fault
		for(i=0;i<exec->segments_no;i++)
		{
			if((uintptr_t)addr >= (uintptr_t)exec->segments[i].vaddr && (uintptr_t)addr <= (uintptr_t)exec->segments[i].vaddr + exec->segments[i].mem_size)
			{
	
				//next we find the index of the page that caused the page fault
				uintptr_t page=((uintptr_t)addr-(uintptr_t)exec->segments[i].vaddr)/pageSize;
			
				//find the number of pages 
			    int numberOfPages = exec->segments[i].mem_size/pageSize  + 1;

				//we mark all the unmapped pages with 0
				exec->segments[i].data = calloc(numberOfPages,sizeof(uint8_t));
				
				//if the page is unmapped
				if(!(*((uint8_t *)exec->segments[i].data + page)))
				{ 
					
					int nextToRead;

					//check if there is anything left to read from the file
					if(page * pageSize > exec->segments[i].file_size) 
						nextToRead = 0;
					else
						//if there is more than one page to read
						if(pageSize < pageSize - (page + 1)* pageSize + exec->segments[i].file_size)
							nextToRead=pageSize;
						else
							//if there is one page left to read				
							nextToRead=pageSize - (page + 1)*pageSize + exec->segments[i].file_size;

					if(nextToRead>0)
					{
						char* buff = malloc(pageSize * sizeof(char));

						//reads a page into buff
						read_x_bytes(exec->segments[i].offset + page * pageSize, nextToRead, buff);

						//if there is less than a page
						if(nextToRead < pageSize)
							memset((void*)(buff+nextToRead), 0, pageSize - nextToRead);

						//mapping the page with the right permissions
						char* rc1 = mmap((void*)(exec->segments[i].vaddr+page*pageSize), pageSize, PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

						//copy buffer data in the mapped page
						memcpy(rc1,buff,pageSize);

						//set the right permissions
						mprotect(rc1,pageSize,exec->segments[i].perm); 

						

						free(buff);
					}
					else
					{
						//mapping the page with the right permissions
						char* rc = mmap((void*)(exec->segments[i].vaddr+page*pageSize), pageSize, PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

						//set the right permissions
						mprotect(rc,pageSize,exec->segments[i].perm); 

					}
					//mark the page as mapped
					*((uint8_t *)exec->segments[i].data + page) = 1;

				}
				//if the page is already mapped
				else
				{ 
					oldButGold.sa_sigaction(signum,info,context);
					return;
				}

				break;
			}
		}
	}
	//if the page fault is not located in any of the segments
	else
	{
		oldButGold.sa_sigaction(signum,info,context);
		return;
	}
}

int so_init_loader(void) 
{
	/* TODO: initialize on-demand loader */
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	strcpy(pathFile,path);

	so_start_exec(exec, argv);

	return 0;
}
