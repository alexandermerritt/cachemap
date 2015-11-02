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
#include <sys/mman.h>
#include <sched.h>


#include "probe.h"
#include "timestats.h"
#include "sysinfo.h"

int debug = 0;

void init() {
  probe_init(EBSIZE);
}

void *allocpage() {
  void *rv = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
  if (rv == MAP_FAILED) {
    perror("allocpage: mmap");
    exit(1);
  }
  return rv;
}



int main(int c, char **v) {
  //srandom(time(NULL));
  cpu_set_t cs;
  CPU_ZERO(&cs);
  CPU_SET(0, &cs);
  if (sched_setaffinity(0, sizeof(cs), &cs) < 0) {
    perror("migrate0");
    exit(1);
  }

  setbuf(stdout, NULL);
  init();
  exit(0);
  if (debug) fprintf(stderr, "init\n");

  ts_t flush = ts_alloc();
  void *p = allocpage();
  if (debug) fprintf(stderr, "allocpage\n");
  for (int i = 0; i < 1000000; i++)  {
    probe_clflush(p);
    ts_add(flush, probe_time(p));
  }
  if (debug)fprintf(stderr, "flush\n");

  int si = probe_setindex(p);
  ts_t evict = ts_alloc();
  for (int i = 0; i < 1000000; i++)  {
    probe_evict(si);
    probe_evict(si);
    probe_evict(si);
    ts_add(evict, probe_time(p));
    /*if (i % 1000 == 999)
      fputc('.', stderr); */
    
  }
  if (debug)fprintf(stderr, "evict\n");

  printf("#time flush evict\n");
  for (int i = 1; i < TIME_MAX; i++) 
    printf("%d %d %d\n", i, ts_get(flush, i), ts_get(evict, i));

}


