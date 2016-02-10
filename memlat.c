/*
 * lat_mem_rd.c - measure memory load latency
 *
 * usage: lat_mem_rd size-in-MB stride [stride ...]
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */

char	*id = "$Id$\n";
#define _GNU_SOURCE
#include <string.h>
#include	<assert.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <sys/time.h>
#include        <sched.h>


typedef unsigned long long uint64;

#define N       1000000	/* Don't change this */
#define STRIDE  (512/sizeof(char *))
#define	MEMTRIES	4
#define	LOWER	512

size_t	step(size_t k);


char last(char *s)
{
	while (*s++)
		;
	return (s[-2]);
}

size_t bytes(char *s)
{
	uint64	n;

	sscanf(s, "%llu", &n);

	if ((last(s) == 'k') || (last(s) == 'K'))
		n *= 1024;
	if ((last(s) == 'm') || (last(s) == 'M'))
		n *= (1024 * 1024);
	return ((size_t)n);
}

void tvsub(struct timeval * tdiff, struct timeval * t1, struct timeval * t0)
{
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0 && tdiff->tv_sec > 0) {
		tdiff->tv_sec--;
		tdiff->tv_usec += 1000000;
		assert(tdiff->tv_usec >= 0);
	}

	/* time shouldn't go backwards!!! */
	if (tdiff->tv_usec < 0 || t1->tv_sec < t0->tv_sec) {
		tdiff->tv_sec = 0;
		tdiff->tv_usec = 0;
	}
}

uint64 tvdelta(struct timeval *start, struct timeval *stop)
{
	struct timeval td;
	uint64	usecs;

	tvsub(&td, stop, start);
	usecs = td.tv_sec;
	usecs *= 1000000;
	usecs += td.tv_usec;
	return (usecs);
}

volatile uint64	use_result_dummy;	/* !static for optimizers. */

void use_pointer(void *result) 
{ 
  use_result_dummy += (int)result; 
}

static struct timeval start_tv, stop_tv;
/*
 * Start timing now.
 */
void
start(struct timeval *tv)
{
	if (tv == NULL) {
		tv = &start_tv;
	}
	(void) gettimeofday(tv, (struct timezone *) 0);
}

/*
 * Stop timing and return real time in microseconds.
 */
uint64 stop(struct timeval *begin, struct timeval *end)
{
	if (end == NULL) {
		end = &stop_tv;
	}
	(void) gettimeofday(end, (struct timezone *) 0);

	if (begin == NULL) {
		begin = &start_tv;
	}
	return tvdelta(begin, end);
}

uint64 now(void)
{
	struct timeval t;
	uint64	m;

	(void) gettimeofday(&t, (struct timezone *) 0);
	m = t.tv_sec;
	m *= 1000000;
	m += t.tv_usec;
	return (m);
}

double Now(void)
{
	struct timeval t;

	(void) gettimeofday(&t, (struct timezone *) 0);
	return (t.tv_sec * 1000000.0 + t.tv_usec);
}

void loads(char *addr, size_t range, size_t stride, int cpu)
{
	register char **p = 0 /* lint */;
	size_t	i;
	int	tries = 0;
	int	result = 0x7fffffff;
	double	time;
	unsigned long mask;
	unsigned int len = sizeof(mask);

     	if (stride & (sizeof(char *) - 1)) {
		printf("lat_mem_rd: stride must be aligned.\n");
		return;
	}
	
	if (range < stride) {
		return;
	}

	/*
	 * First create a list of pointers.
	 *
	 * This used to go forwards, we want to go backwards to try and defeat
	 * HP's fetch ahead.
	 *
	 * We really need to do a random pattern once we are doing one hit per 
	 * page.
	 */
	for (i = stride; i < range; i += stride) {
		*(char **)&addr[i] = (char*)&addr[i - stride];
	}
	*(char **)&addr[0] = (char*)&addr[i - stride];
	p = (char**)&addr[0];

	/*
	 * Now walk them and time it.
	 */
        for (tries = 0; tries < MEMTRIES; ++tries) {
                /* time loop with loads */
#define	ONE	p = (char **)*p;
#define	FIVE	ONE ONE ONE ONE ONE
#define	TEN	FIVE FIVE
#define	FIFTY	TEN TEN TEN TEN TEN
#define	HUNDRED	FIFTY FIFTY
		i = N;
                start(0);
                while (i >= 1000) {
			HUNDRED
			HUNDRED
			HUNDRED
			HUNDRED
			HUNDRED
			HUNDRED
			HUNDRED
			HUNDRED
			HUNDRED
			HUNDRED
			i -= 1000;
                }
		i = stop(0,0);
		use_pointer((void *)p);
		if (i < result) {
			result = i;
		}
	}
	/*
	 * We want to get to nanoseconds / load.  We don't want to
	 * lose any precision in the process.  What we have is the
	 * milliseconds it took to do N loads, where N is 1 million,
	 * and we expect that each load took between 10 and 2000
	 * nanoseconds.
	 *
	 * We want just the memory latency time, not including the
	 * time to execute the load instruction.  We allow one clock
	 * for the instruction itself.  So we need to subtract off
	 * N * clk nanoseconds.
	 *
	 * lmbench 2.0 - do the subtration later, in the summary.
	 * Doing it here was problematic.
	 *
	 * XXX - we do not account for loop overhead here.
	 */
	time = (double)result;
	time *= 1000.;				/* convert to nanoseconds */
	time /= (double)N;			/* nanosecs per load */

	fprintf(stderr, "%08d %.5f %.3f\n", cpu, range / (1024. * 1024), time);
}

size_t step(size_t k)
{
	if (k < 1024) {
		k = k * 2;
        } else if (k < 4*1024) {
		k += 1024;
	} else {
		size_t s;

		for (s = 32 * 1024; s <= k; s *= 2)
			;
		k += s / 16;
	}
	return (k);
}

int main(int ac, char **av)
{
	size_t	len;
	size_t	range;
	size_t	size;
	int	i;
	unsigned long mask = 0;
	unsigned int masklen = sizeof(mask);
        char   *addr;
	cpu_set_t core;
	int num_cpus = 1;

	if(ac > 1) num_cpus = atoi(av[1]);

       	for(i = 0; i < num_cpus; i++) {
	  
	  CPU_ZERO(&core);
	  CPU_SET(i, &core);

	  if (sched_setaffinity(0, sizeof(core), &core) < 0) {
	    perror("sched_setaffinity");
	    exit(-1);
	  }

	  len = 128 * 1024 * 1024;
	  addr = (char *)malloc(len);
	
	  fprintf(stderr, "\"stride=%d\n", STRIDE);
	  for (range = LOWER; range <= len; range = step(range)) {
	    loads(addr, range, STRIDE, i);
	  }
	  free(addr);
	}

	return(0);
}

