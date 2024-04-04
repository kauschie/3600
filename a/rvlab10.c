/* 

Author: Michael Kausch
Assignment: Lab 10 (a)
Date: 4/4/24
Class: Operating Systems
Instructor: Gordon

 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[])
{
	char *src, *dst;
	int fd, fdout;
	struct stat sb;
	off_t offset, pa_offset;
	size_t length;
	ssize_t s;


	if (argc < 3 || argc > 5) {
		fprintf(stderr, "%s <infile> <outfile> [offset] [length]\n", argv[0]);
		exit(EXIT_FAILURE);
	} else if (argc < 5) {
		fprintf(stderr, "%s <infile> <outfile> [offset] [length]\n", argv[0]);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd == -1)
		handle_error("open");

	if ((fdout = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) {
		handle_error("open");
	}
	
	if (fstat(fd, &sb) == -1)         /* To obtain file size */
		handle_error("fstat");
	printf("\nfilesize: %ld bytes\n", sb.st_size); 

	if (argc >= 4) {	
		offset = atoi(argv[3]);
		/* offset for mmap() must be page aligned */
	} else {
		offset = 0;
	}

	pa_offset = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
	printf("pa_offset %ld\n", pa_offset); 

	if (offset >= sb.st_size) {
		fprintf(stderr, "offset is past end of file\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 5) {
		length = atoi(argv[4]);
		if (offset + length > sb.st_size)
			length = sb.st_size - offset;
		/* Can't display bytes past end of file */

	} else {    /* No length arg ==> display to end of file */
		length = sb.st_size - offset;
	}

	// printf("made it here, length: %ld, offset: %ld\n", length, offset);

	// get anon mapping for input file
	src = mmap(NULL, length + offset - pa_offset, PROT_READ,
			MAP_PRIVATE, fd, pa_offset);

	dst = mmap(NULL, length + offset - pa_offset, PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, fdout, pa_offset);

	if ((src == MAP_FAILED) || dst == MAP_FAILED)
		handle_error("mmap");

	int pagesz = getpagesize(); 
	pid_t mypid = getpid();
	printf("pid: %d pagesize: %d \n", mypid, pagesz);

	memcpy(dst, src + offset - pa_offset, length);

	s = write(fdout, dst, length);

	// printf("made it past write");
	if (s != length) {
		if (s == -1)
			handle_error("write");

		fprintf(stderr, "partial write: %ld bytes written to file.\n", s);
		exit(EXIT_FAILURE);
	}

	printf("%ld bytes written to file.\n", s);

	close(fd);
	close(fdout);
	munmap(src, length + offset - pa_offset);
	munmap(dst, length + offset - pa_offset);

	exit(EXIT_SUCCESS);
}

