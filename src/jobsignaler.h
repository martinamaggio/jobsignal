#ifndef __JOBSIGNALER_H
#define __JOBSIGNALER_H

  // System includes
  #include <sys/ipc.h>
  #include <sys/shm.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  // Library includes
  #include <dirent.h>
  #include <fcntl.h>
  #include <inttypes.h>
  #include <pthread.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <time.h>
  #include <unistd.h>

  #undef _JOBSIGNALER_DEBUG
  #define _JOBSIGNALER_ERROR 1
  #define _JOBSIGNALER_MULTITHREADED 1

  #define _H_MAX_JOBS 10 // Number of different job types
  #define _H_MAX_RECORDS 10 // Number of maximum records 
  #define _H_MAX_FILENAMELENGHT 1000

  // Exit codes
  #define EXIT_NORMAL 0
  #define EXIT_FAILURE_UNDEFINEDAUTOSIGNALER -1
  #define EXIT_FAILURE_SHAREDMEMORY -2
  #define EXIT_FAILURE_JOBNOTFOUND -3

  typedef struct {
    uint id;
    uint type;
    int64_t start_timestamp;
    int64_t end_timestamp;
  } _job_h;

  typedef struct {
    unsigned int application_id;
    int shared_memory_segment;
    uint jobs; // Number of possible job types
    double weight;
    double performance_multiplier;
    uint total_jobs;
    uint progress_jobs;
    uint completed_jobs;
    uint64_t expected_response_times[_H_MAX_JOBS];
    _job_h jprogress[_H_MAX_RECORDS];
    _job_h jcompleted[_H_MAX_RECORDS];
    pthread_mutex_t mutex;
  } _application_h;

  // Functions for application management
  _application_h* jobsignaler_registration();
  int jobsignaler_set(_application_h* a, uint types, uint64_t* ert);
  int jobsignaler_terminate(_application_h* a);
  int jobsignaler_signalstart(_application_h* a, uint type);
  int jobsignaler_signalend(_application_h* a, uint id);

  // Functions for monitor management
  _application_h* monitor_application_init(int application_id);
  int get_applications(int* application_ids);
  int monitor_application_stop(_application_h* a);
  int set_weight(int application_id, double weight);
  double get_weight(_application_h* a);

  // Functions for the resource manager or to retrieve performance
  void set_performance_multiplier(_application_h* a, double multiplier);
  double get_performance_multiplier(_application_h* a);
  double get_performance_number(_application_h* a, int job_type);

#endif
