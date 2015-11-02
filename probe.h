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

#ifndef __PROBE_H__
#define __PROBE_H__ 1

void probe_clflush(void *p);
void probe_access(void *p);
int probe_setindex(void *p);
void probe_evict(int si);
int probe_time(void *p);
void probe_init(uint64_t ebsize);

// Hardware and config info
int probe_npages();
int probe_noffsets();
int probe_nways();
int probe_ncores();
int probe_pagesize();


#endif // __PROBE_H__
