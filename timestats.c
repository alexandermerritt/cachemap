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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

#include "timestats.h"

struct ts {
  uint32_t data[TIME_MAX];
};

ts_t ts_alloc() {
  ts_t rv = (ts_t)malloc(sizeof(struct ts));
  ts_clear(rv);
  return rv;
}

void ts_free(ts_t ts) {
  free(ts);
}

void ts_clear(ts_t ts) {
  bzero(ts, sizeof(struct ts));
}

void ts_add(ts_t ts, int tm) {
  assert(tm > 0);
  if (tm < TIME_MAX)
    ts->data[tm]++;
  else
    ts->data[0]++;
}

uint32_t ts_get(ts_t ts, int tm) {
  assert(tm > 0);
  return tm < TIME_MAX ? ts->data[tm] : 0;
}

uint32_t ts_outliers(ts_t ts) {
  return ts->data[0];
}


int ts_median(ts_t ts) {
  int c = 0;
  for (int i = 0; i < TIME_MAX; i++)
    c += ts->data[i];
  c = (c + 1) / 2;
  for (int i = 1; i < TIME_MAX; i++)
    if ((c -= ts->data[i]) < 0)
      return i;
  return 0;
}

int ts_mean(ts_t ts, int scale) {
  uint64_t sum = 0;
  int count = 0;
  for (int i = 0; i < TIME_MAX; i++) {
    count += ts->data[i];
    sum += i* (uint64_t)ts->data[i];
  }
  return (int)((sum * scale)/count);
}
