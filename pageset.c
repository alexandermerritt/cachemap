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

#include <stdlib.h>
#include <string.h>
#include "pageset.h"

#define PS_INITSIZE	64



pageset_t ps_new() {
  pageset_t rv = malloc(sizeof(struct pageset));

  rv->datasize = PS_INITSIZE;
  rv->data = malloc(sizeof(int) * rv->datasize);
  rv->npages = 0;
  return rv;
}


pageset_t ps_dup(pageset_t ps) {
  pageset_t rv = malloc(sizeof(struct pageset));

  rv->data = malloc(sizeof(int) * ps->datasize);
  memcpy(rv->data, ps->data, sizeof(int) * ps->npages);
  rv->datasize = ps->datasize;
  rv->npages = ps->npages;
  return rv;
}

void ps_move(pageset_t from, pageset_t to) {
  free(to->data);
  to->data = from->data;
  to->datasize = from->datasize;
  to->npages = from->npages;
  free(from);
}


void ps_delete(pageset_t ps) {
  if (ps) {
    if (ps->data)
      free(ps->data);
    free(ps);
  }
}

void ps_push(pageset_t ps, int page) {
  if (ps->npages == ps->datasize) {
    ps->datasize *=2;
    ps->data = realloc(ps->data, sizeof(int) * ps->datasize);
  }
  ps->data[ps->npages++] = page;
}

int ps_pop(pageset_t ps) {
  if (ps->npages == 0)
    return -1;
  return ps->data[--ps->npages];
}

int ps_size(pageset_t ps) {
  return ps->npages;
}

int ps_get(pageset_t ps, int i) {
  if (i >= ps->npages)
    return -1;
  return ps->data[i];
}

void ps_set(pageset_t ps, int i, int page) {
  if (i >= ps->npages)
    return;
  ps->data[i] = page;
}

void ps_replace(pageset_t ps, int from , int to) {
  for (int i = 0; i < ps->npages; i++)
    if (ps->data[i] == from)
      ps->data[i] = to;
}

void ps_remove(pageset_t ps, int page) {
  if (ps->npages == 0)
    return;
  int l = ps_pop(ps);
  if (l == page)
    return;
  for (int i = 0; i < ps->npages; i++)
    if (ps->data[i] == page) {
      ps->data[i] = l;
      return;
    }
  ps_push(ps, l);
}

void ps_removeset(pageset_t ps, pageset_t set) {
  for (int i = ps_size(set); i--; )
    ps_remove(ps, ps_get(set, i));
}


void ps_randomise(pageset_t ps) {
  for (int i = 2; i < ps->npages; i++) {
    int j = random() % i;
    int t = ps->data[i];
    ps->data[i] = ps->data[j];
    ps->data[j] = t;
  }
}

void ps_clear(pageset_t ps) {
  ps->npages = 0;
}

static int intcmp(const void *v1, const void *v2) {
  return *(int*)v1 - *(int*)v2;
}

void ps_sort(pageset_t ps) {
  qsort(ps->data, ps->npages, sizeof(ps->data[0]), intcmp);
}




