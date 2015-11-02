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

#ifndef __SYSINFO_H__
#define __SYSINFO_H__ 1

#define KB	1024
#define MB 	(1024 * KB)
#define GB	(1024 * MB)

#define NCORES 6
#define COREID(c) (c * 2)
#define NWAYS 20
#define HUGEPAGESIZE GB

#define L3THRESHOLD 100
#define EVICT_COUNT 3



#define EBSIZE (1ULL * GB)



#endif // __SYSINFO_H__
