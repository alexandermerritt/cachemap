/*
 * Copyright 2015 The University of Adelaide
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


#ifdef __APPLE__
#include <mach/vm_statistics.h>
#undef PAGE_SIZE
#endif

#include "pageset.h"
#include "timestats.h"
#include "sysinfo.h"

#ifdef VM_FLAGS_SUPERPAGE_SIZE_ANY
#define MAP_LARGEPAGES	VM_FLAGS_SUPERPAGE_SIZE_ANY
#define MAP_ROUNDSIZE	(2*1024*1024)
#define SETINDEX_BITS	17
#endif

#ifdef MAP_HUGETLB
#define MAP_LARGEPAGES	MAP_HUGETLB
#define MAP_ROUNDSIZE	(2*1024*1024)
#define SETINDEX_BITS	17
#endif

#define PAGE_BITS	12
#define PAGE_SIZE	(1 << PAGE_BITS)
#define PAGE_MASK	(PAGE_SIZE - 1)
#define PAGE_LBITS	(PAGE_BITS - CLBITS)
#define PAGE_LINES	(PAGE_SIZE >> CLBITS)

#ifndef SETINDEX_BITS
#error Expected large pages
#define MAP_LARGEPAGES	0
#define MAP_ROUNDSIZE	4096
#define SETINDEX_BITS	12
#endif



#define CLBITS	6
#define CLSIZE (1 << CLBITS)


#define SETINDEX_SIZE	(1 << SETINDEX_BITS)
#define SETINDEX_MASK	(SETINDEX_SIZE - 1)
#define SETINDEX_LBITS	(SETINDEX_BITS - CLBITS)
#define SETINDEX_LINES	(SETINDEX_SIZE >> CLBITS)


// Number of probes to find the set index of a virtual address
#define SETINDEX_NPROBE	64

#define MAX_SLICES 32

static int debug = 0;

union cacheline {
  union cacheline *next;
  union cacheline *cl_links[CLSIZE/8];
  char bytes[CLSIZE];
#define cl_next cl_links[0]
#define cl_ebnext cl_links[1]
};

union setindex {
  union setindex *next;
  union cacheline cachelines[SETINDEX_LINES];
  char bytes[SETINDEX_SIZE];
};

typedef union cacheline *cacheline_t;
typedef union setindex *setindex_t;

typedef char setindexmap[SETINDEX_LINES];

static struct probeinfo {
  uint64_t ebsetindices;
  setindex_t eb;
} probeinfo;

void probe_clflush(volatile void *p) {
  asm __volatile__ ("clflush 0(%0)" : : "r" (p):);
}

void probe_access(volatile void *p) {
  asm __volatile__ ("mov (%0), %%ebx" : : "r" (p) : "%ebx");
}

static void walk(cacheline_t cl, int ind) {
  while (cl != NULL) 
    cl = cl->cl_links[ind];
}


void probe_evict(int si) {
  walk (&probeinfo.eb[0].cachelines[si], 0);
}

int probe_npages() {
  return probeinfo.ebsetindices;
}

int probe_noffsets() {
  return SETINDEX_LINES;
}

int probe_nways() {
  return NWAYS;
}

int probe_ncores() {
  return NCORES;
}

int probe_pagesize() {
  return SETINDEX_SIZE;
}



int probe_time(volatile void *p) {
  volatile int rv;
  asm __volatile__ (
      "xorl %%eax, %%eax\n"
      "cpuid\n"
      "rdtsc\n"
      "mov %%eax, %%esi\n"
      "mov (%%rdi), %%rdi\n"
      "rdtscp\n"
      "sub %%eax, %%esi\n"
      "xorl %%eax, %%eax\n"
      "cpuid\n"
      "xorl %%eax, %%eax\n"
      "subl %%esi, %%eax\n"
      : "=a" (rv) : "D" (p) : "%rbx", "%rcx", "%rdx", "%rsi");
  return rv;
}

int probe_setindex(void *p) {
  int page = ((uint32_t)p & PAGE_MASK) / PAGE_LINES;
  if (PAGE_SIZE == SETINDEX_SIZE)
    return page;
  ts_t ts = ts_alloc();

  int max = 0;
  int maxsi = -1;
  for (int si = page; si < SETINDEX_LINES; si+= PAGE_LINES) {
    for (int i = 0; i < SETINDEX_NPROBE; i++) {
      probe_access(p);
      probe_evict(si);
      ts_add(ts, probe_time(p));
    }
    int median = ts_median(ts);
    if (median > max) {
      max = median;
      maxsi = si;
    }
    ts_clear(ts);
  }
  ts_free(ts);
  return maxsi;
}

static pageset_t ebpageset() {
  pageset_t rv = ps_new();
  for (int i = 0; i < probeinfo.ebsetindices; i++) 
    ps_push(rv, i);
  ps_randomise(rv);
  return rv;
}

void evictmeasureloop(pageset_t ps, int candidate, int si, int link, ts_t ts, int count) {
  ts_clear(ts);
  void *cc = &probeinfo.eb[candidate].cachelines[si];
  cacheline_t cl = NULL;
  for (int i = ps_size(ps); i--; ) {
    int n = ps_get(ps, i);
    probeinfo.eb[n].cachelines[si].cl_links[link] = cl;
    cl = &probeinfo.eb[n].cachelines[si];
  }
  for (int i = 0; i < count; i++) {
    probe_access(cc);
    for (int j = 0; j < EVICT_COUNT; j++)
      walk(cl, link);
    ts_add(ts, probe_time(cc));
  }
}

int probe_evictMeasure(pageset_t evict, int measure, int offset, ts_t ts, int count) {
  evictmeasureloop(evict, measure, offset, 1, ts, count);
  return ts_median(ts);
}

static void findmap(pageset_t eb, int candidate, char *map, pageset_t *pss, int si, ts_t ts) {
  pageset_t t = ps_dup(eb);
  pageset_t quick[MAX_SLICES];
  for (int i = 0; i < MAX_SLICES; i++)
    quick[i] = NULL;
  int psid = -1;
  int new = 0;
  for (int i = 0; i < ps_size(eb); i++)  {
    int r = ps_get(eb, i);
    ps_remove(t, r);
    if (probe_evictMeasure(t, candidate, si, ts, 32) < L3THRESHOLD) {
      if (map[r] == -1) {
	if (psid == -1) {
	  for (int j = 0; j < MAX_SLICES; j++) 
	    if (pss[j] == NULL) {
	      pss[j] = ps_new();
	      psid = j;
	      break;
	    }
	  new = 1;
	}
	if (debug) {
	  if (!new)
	    fprintf(stderr, "Unmapped eb %d added to set %d\n", r, psid);
	  else
	    fprintf(stderr, "(eb) %d ==> %d\n", r, psid);
	}
	map[r] = psid;
	ps_push(pss[psid], r);
      }
      if (psid == -1) 
	psid = map[r];
      if (psid != map[r])
	fprintf(stderr, "Double conflict %d, %d (on eb %d)\n", psid, map[r], r);
      if (map[candidate] == -1)  {
	//fprintf(stderr, "(ca) %d ==> %d\n", candidate, psid);
	map[candidate] = psid;
	ps_push(pss[psid], candidate);
      }
    }
    ps_push(t, r);
  }
}
  

static int findquick(pageset_t *quick, int candidate, pageset_t *pss,  char *map, int si, ts_t ts, int count) {
  for (int i = 0; i < MAX_SLICES; i++) {
    if (quick[i] != NULL && probe_evictMeasure(quick[i], candidate, si, ts, count) >= L3THRESHOLD) {
      int index = ps_get(quick[i], 0);
      map[candidate] = map[index];
      ps_push(pss[map[index]], candidate);
      return 1;
    }
  }
  return 0;
}


pageset_t *split(int si) {
  pageset_t candidates = ebpageset();
  pageset_t eb = ps_new();
  cacheline_t ebcl = NULL;
  pageset_t *rv = calloc(MAX_SLICES, sizeof(pageset_t));
  pageset_t quick[MAX_SLICES];
  for (int i = 0; i < MAX_SLICES; i++)
    quick[i] = NULL;
  char *map = malloc(probeinfo.ebsetindices);
  for (int i = 0; i < probeinfo.ebsetindices; i++)
    map[i] = -1;

  ts_t ts = ts_alloc();
  int i = 0;
  int fq = 0;
  while (ps_size(candidates)) {
    int candidate = ps_pop(candidates);
    if (findquick(quick, candidate, rv, map, si, ts, 32)) {
      fq++;
      continue;
    }
    int time = probe_evictMeasure(eb, candidate, si, ts, 32);
    //printf("%3d: %3d %d\n", i++, candidate, time);
    if (time < L3THRESHOLD) 
      ps_push(eb, candidate);
    else {
      findmap(eb, candidate, map, rv, si, ts);
      if (map[candidate] != -1 && ps_size(rv[map[candidate]]) ==25) {
	for (int i = 0; i < MAX_SLICES; i++)
	  if (quick[i] == NULL) {
	    quick[i] = ps_dup(rv[map[candidate]]);
	    break;
	  }
      }
    }
  }
  for (int i = 0; i < MAX_SLICES; i++) {
    if (quick[i] != NULL)
      ps_delete(quick[i]);
  }
  free(map);
  return rv;
}

int acctime(ts_t ts, pageset_t ps, int si, int link, int count) {
  ts_clear(ts);
  pageset_t tps = ps_new();
  for (int i = 0; i < 10; i++)
    ps_push(tps, ps_get(ps, i));
  int cand = ps_get(ps, 10);
  evictmeasureloop(tps, cand, si, link, ts, count);
  int rv = ts_median(ts);
  ps_delete(tps);
  return rv;
}

static int timeplot = 0;

char *probe_map1(int setindex) {
  char name[1000];
  sprintf(name, "Map/Index-%03x.plot", setindex);
  FILE *f = fopen(name, "w");
  if (f) 
    fprintf(f, "set term pdfcairo size 11.7,8.27\nset xrange [0:%d]\nset style fill solid noborder\nset yrange [0:50000]\nset multiplot layout %d,%d title 'Set index 0x%03x'\n", L3THRESHOLD, NCORES, NCORES, setindex);
  char *rv = malloc(probeinfo.ebsetindices);
  for (int i = 0; i < probeinfo.ebsetindices; i++)
    rv[i] = -1;
  ts_t ts = ts_alloc();
  fprintf(stderr, "Set 0x%03x Times: ", setindex);
  pageset_t *map = split(setindex);
  char cores[NCORES];
  for (int i = 0; i < NCORES;i++)
    cores[i] = 0;
  for (int slice = 0; slice < NCORES; slice++) {
    if (map[slice] != NULL) {
      ps_sort(map[slice]);
      int mincore = -1;
      int mincoretime = 100000;
      for (int core = 0; core < NCORES; core++) {
	cpu_set_t cs;
	CPU_ZERO(&cs);
	CPU_SET(COREID(core), &cs);
	if (sched_setaffinity(0, sizeof(cs), &cs) < 0) {
	  perror("migrate");
	  exit(1);
	}
	acctime(ts, map[slice], setindex, 1, 100000);
	if (f != NULL) {
	  fprintf(f, "set title 'Slice %d, Core %d'\nunset key\nplot '-' using 1:2 with boxes notitle\n", slice, core);
	  for (int i = 1; i < L3THRESHOLD; i++)
	    fprintf(f, "%d %d\n", i, ts_get(ts, i));
	  fprintf(f, "e\n");
	}
	int mean = ts_mean(ts, 10);
	if (mean < mincoretime) {
	  mincoretime = mean;
	  mincore = core;
	}
	fprintf(stderr, "%2d.%01d ", mean/10, mean %10);

	/*if (ts_get(ts, 39) > 45000 && ts_get(ts,46) > 40000 && ts_get(ts, 39) + ts_get(ts,46) > 95000) {
	  cores[slice] |= 1<<core;
	} */
      }
      fprintf(stderr, "/ ");
      cores[slice] = 1 << mincore;
    }
  }
  fprintf(stderr, "\nBefore cleaning set 0x%03x:", setindex);
  for (int i = 0; i < NCORES;i++)
    fprintf(stderr, " 0x%02x", cores[i]);
  fprintf(stderr, "\n");
  char c1[NCORES];
  for (int i = 0; i < NCORES;i++)
    c1[i] = -1;
  int mod;
  do {
    mod = 0;
    for (int i = 0; i < NCORES; i++) {
      if (cores[i] == 0  || (cores[i] & (cores[i] - 1)))
	continue;
      for (int j = 0; j < NCORES; j++) {
	if (j == i)
	  continue;
	if (cores[j] != cores[i] && ((cores[j] & cores[i]) != 0 )) {
	  cores[j] &= ~cores[i];
	  mod =1;
	}
      }
    }
  } while (mod);
  fprintf(stderr, "After cleaning set 0x%03x:", setindex);
  for (int i = 0; i < NCORES;i++)
    fprintf(stderr, " 0x%02x", cores[i]);
  fprintf(stderr, "\n");
  for (int i = 0; i < NCORES; i++) {
    mod = -1;
    for (int j = 0; j < NCORES; j++) {
      if (cores[j] == 1<<i) {
	c1[j] = i;
	if (mod != -1)
	  fprintf(stderr, "Error set 0x%03x: Slices %d and %d map to core %d\n", setindex, j, mod, i);
	mod = j;
      }
    }
    if (mod == -1)
      fprintf(stderr, "Error set 0x%03x: No slice maps to core %d\n", setindex, i);
  }

  for (int slice = 0; slice < NCORES; slice++) {
    if (map[slice] != NULL)  {
      for (int i = 0; i < ps_size(map[slice]); i++)
	rv[ps_get(map[slice], i)] = c1[slice];
      ps_delete(map[slice]);
    } else {
      fprintf(stderr, "Error set 0x%03x: Null slice\n", setindex);
    }
  }
  if (f)
    fclose(f);
  ts_free(ts);
  return rv;
}
  
char **probe_map() {
  char **rv = calloc(SETINDEX_LINES, sizeof(char *));
  for (int i = 0; i < SETINDEX_LINES; i++) {
    rv[i] = probe_map1(i);
  }
  return rv;
}


void probe_init(uint64_t ebsetindices) {

  probeinfo.ebsetindices = ebsetindices  / SETINDEX_SIZE;
  probeinfo.eb = (setindex_t) mmap64(NULL, probeinfo.ebsetindices * SETINDEX_SIZE, PROT_READ|PROT_WRITE, 
      						MAP_LARGEPAGES|MAP_ANON|MAP_PRIVATE, -1, 0);
  if (probeinfo.eb == MAP_FAILED) {
    perror("probe_init: eb: mmap");
    exit(1);
  }

  int f = open("/proc/self/pagemap", O_RDONLY);
  uint64_t buf;
  uint64_t adrs = (uint64_t)probeinfo.eb;
  for (uint64_t p = 0; p < ebsetindices; p += HUGEPAGESIZE) {
    *((int *)(adrs + p)) = 1;
    uint64_t seek = (adrs + p) /4096 * 8;
    lseek64(f, seek, SEEK_SET);
    int r = read(f, &buf, sizeof(buf));
    printf("%d 0x%016llx\n", p/HUGEPAGESIZE, buf * 0x1000);
  }
  close(f);
    


  pageset_t ps = ebpageset();
  int prev = 0;
  while (ps_size(ps)) {
    int next = ps_pop(ps);
    for (int i = 0; i < SETINDEX_LINES; i++) 
      probeinfo.eb[prev].cachelines[i].next = &probeinfo.eb[next].cachelines[i];
    prev = next;
  }
  for (int i = 0; i < SETINDEX_LINES; i++) 
    probeinfo.eb[prev].cachelines[i].next = NULL;
  ps_delete(ps);

  char ** data = probe_map();
  for (int i = 0; i < SETINDEX_LINES; i++) {
    for (int j = 0; j < probeinfo.ebsetindices; j++)
      putchar('0' + data[i][j]);
    putchar('\n');
  }
}


