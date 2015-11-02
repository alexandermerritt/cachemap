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

#ifndef __PAGESET_H__
#define __PAGESET_H__

/*pageset stack, record page numbers in a stack*/
struct pageset {
    int *data;       /*page number stack*/
    int npages;      /*num of pages in the satck*/
    int datasize;    /*stack size*/
};



typedef struct pageset *pageset_t;

pageset_t ps_new();
pageset_t ps_dup(pageset_t ps);
void ps_delete(pageset_t ps);
void ps_move(pageset_t from, pageset_t to);

void ps_clear(pageset_t ps);
void ps_push(pageset_t ps, int page);
int ps_pop(pageset_t ps);
int ps_size(pageset_t ps);
int ps_get(pageset_t ps, int i);
void ps_set(pageset_t ps, int i, int page);
void ps_replace(pageset_t ps, int from, int to);
void ps_remove(pageset_t ps, int page);
void ps_removeset(pageset_t ps, pageset_t set);
void ps_randomise(pageset_t ps);
void ps_sort(pageset_t ps);



#endif //__PAGESET_H__
