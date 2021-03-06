/***************************************************************
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <map>
#include <string>
#include <set>

#include "Job.h"

typedef std::map<std::string, Job> JobCollectionType;

extern JobCollectionType g_jobs;

typedef std::set<std::string> JobSetType;

extern JobSetType g_dirty_jobs;
extern JobSetType g_delete_jobs;

#endif /* _GLOBALS_H */
