/*
 *  Copyright (c) 2011-2012, Los Alamos National Security, LLC.
 *  All rights Reserved.
 *
 *  Copyright 2011-2012. Los Alamos National Security, LLC. This software was produced 
 *  under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National 
 *  Laboratory (LANL), which is operated by Los Alamos National Security, LLC 
 *  for the U.S. Department of Energy. The U.S. Government has rights to use, 
 *  reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS 
 *  ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR 
 *  ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is modified
 *  to produce derivative works, such modified software should be clearly marked,
 *  so as not to confuse it with the version available from LANL.
 *
 *  Additionally, redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Los Alamos National Security, LLC, Los Alamos 
 *       National Laboratory, LANL, the U.S. Government, nor the names of its 
 *       contributors may be used to endorse or promote products derived from 
 *       this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE LOS ALAMOS NATIONAL SECURITY, LLC AND 
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT 
 *  NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL
 *  SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *  
 *  CLAMR -- LA-CC-11-094
 *  This research code is being developed as part of the 
 *  2011 X Division Summer Workshop for the express purpose
 *  of a collaborative code for development of ideas in
 *  the implementation of AMR codes for Exascale platforms
 *  
 *  AMR implementation of the Wave code previously developed
 *  as a demonstration code for regular grids on Exascale platforms
 *  as part of the Supercomputing Challenge and Los Alamos 
 *  National Laboratory
 *  
 *  Authors: Bob Robey       XCP-2   brobey@lanl.gov
 *           Neal Davis              davis68@lanl.gov, davis68@illinois.edu
 *           David Nicholaeff        dnic@lanl.gov, mtrxknight@aol.com
 *           Dennis Trujillo         dptrujillo@lanl.gov, dptru10@gmail.com
 * 
 */
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "timer.h"

void cpu_timer_start(struct timeval *tstart_cpu){
   gettimeofday(tstart_cpu, NULL);
}

double cpu_timer_stop(struct timeval tstart_cpu){
   double result;
   struct timeval tstop_cpu, tresult;

   gettimeofday(&tstop_cpu, NULL);
   tresult.tv_sec = tstop_cpu.tv_sec - tstart_cpu.tv_sec;
   tresult.tv_usec = tstop_cpu.tv_usec - tstart_cpu.tv_usec;
   result = (double)tresult.tv_sec + (double)tresult.tv_usec*1.0e-6;
   return(result);
}

pid_t pid;
FILE *stat_fp, *meminfo_fp;

long long timer_memused(){
   char proc_stat_file[50];
   char str[140];
   char *p;
   int i, err;
   int memdebug = 1;
   long long mem_current;
   long long page_size = 1; //4096

   if (!stat_fp){
      pid = getpid();
      sprintf(proc_stat_file, "/proc/%d/stat", pid);
      stat_fp = fopen(proc_stat_file, "r");
      if (!stat_fp){
         printf("fopen %s failed: \n", proc_stat_file);
         return(-1);
      }
   }

   err = fflush(stat_fp);
   if (err) {
      printf("fflush %s failed: %s\n", proc_stat_file, strerror(err));
      return(-1);
   }
   err = fseek(stat_fp, 0L, 0);
   if (err) {
      printf("fseek %s failed: %s\n", proc_stat_file, strerror(err));
      return(-1);
   }

   fgets(str, 132, stat_fp);
   if (memdebug) {
      printf("str: %s\n",str);
   }
   if (ferror(stat_fp)) {
      printf("fgets %s failed: %s\n", proc_stat_file, strerror(err));
      return(-1);
   }

   p = strtok(str," ");
   for (i=0; i<21; ++i){
      p = strtok('\0'," ");
      if (memdebug) {
         printf("p: %d %s\n",i,p);
      }
   }

/* Getting 23rd field which is rss size in pages */
   p = strtok('\0'," ");
   if (memdebug) {
      printf("rss size token: %s\n",p);
   }

   mem_current = atoll(p)*page_size;
   if (memdebug) {
      printf("STAT rss: %lld \n",mem_current);
   }

   return(mem_current);
}

#define TIMER_ONEK 1024
long long timer_memfree(){
   int err, found;
   long long freemem, cachedmem;
   int memdebug = 0;
   char buf[260];
   char *p;

   freemem = -1;
   cachedmem = 0;

   if (!meminfo_fp){
      meminfo_fp = fopen("/proc/meminfo", "r");
      if (!meminfo_fp){
         printf("fopen failed: \n");
         return(-1);
      }
   }

   err = fflush(meminfo_fp);
   if (err) {
      printf("fflush failed: %s\n", strerror(err));
      return(-1);
   }
   err = fseek(meminfo_fp, 0L, 0);
   if (err) {
      printf("fseek failed: %s\n", strerror(err));
      return(-1);
   }

   found = 0;
   while (!found && !feof(meminfo_fp)) {
      if (fgets(buf, 255, meminfo_fp)) { /* read header */
         p = strtok(buf, " ");
         if (memdebug){
            printf("p: %s\n",p);
         }
         if (!strcmp(p, "MemFree:")) found = 1;
      } else {
         break;
      }
   }

   if (found){
      p = strtok('\0', " "); /* should now point to free memory string */
      freemem = atoll(p)*TIMER_ONEK;

      if (memdebug) printf("MEMINFO: freemem %lld \n",freemem);
   }

   found = 0;
   while (!found && !feof(meminfo_fp)) {
      if (fgets(buf, 255, meminfo_fp)) { /* read header */
         p = strtok(buf, " ");
         if (memdebug){
            printf("p: %s\n",p);
         }
         if (!strcmp(p, "Cached:")) found = 1;
      } else {
         break;
      }
   }

   if (found){
      p = strtok('\0', " "); /* should now point to cached memory string */
      cachedmem = atoll(p)*TIMER_ONEK;

      if (memdebug) printf("MEMINFO: cachedmem %lld \n",cachedmem);
   }

   //return(freemem+cachedmem);
   return(freemem);
}

long long timer_memtotal(){
   int err, found;
   long long totalmem;
   int memdebug = 0;
   char buf[260];
   char *p;

   totalmem = -1;

   if (!meminfo_fp){
      meminfo_fp = fopen("/proc/meminfo", "r");
      if (!meminfo_fp){
         printf("fopen failed: \n");
         return(-1);
      }
   }

   err = fflush(meminfo_fp);
   if (err) {
      printf("fflush failed: %s\n", strerror(err));
      return(-1);
   }
   err = fseek(meminfo_fp, 0L, 0);
   if (err) {
      printf("fseek failed: %s\n", strerror(err));
      return(-1);
   }

   found = 0;
   while (!found && !feof(meminfo_fp)) {
      if (fgets(buf, 255, meminfo_fp)) { /* read header */
         p = strtok(buf, " ");
         if (memdebug){
            printf("p: %s\n",p);
         }
         if (!strcmp(p, "MemTotal:")) found = 1;
      } else {
         break;
      }
   }

   if (found){
      p = strtok('\0', " "); /* should now point to total memory string */
      totalmem = atoll(p)*TIMER_ONEK;

      if (memdebug) printf("MEMINFO: totalmem %lld \n",totalmem);
   }

   return(totalmem);
}

