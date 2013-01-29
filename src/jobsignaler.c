#include "jobsignaler.h"

// ****************************************************************************
_application_h* jobsignaler_registration() {

  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[registration] started\n");
  #endif
  int application_id = (int) getpid();

  // In the environment, get JOBSIGNALER_DIR, which is the directory
  // there the files about the running applications are stored; if not
  // present, the call terminates the application to avoid sementation
  // faults
  if (getenv("JOBSIGNALER_DIR") == NULL) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[registration] failed, JOBSIGNALER_DIR undefined\n");
    #endif
    exit(EXIT_FAILURE_UNDEFINEDAUTOSIGNALER);
  }

  // Creating file to signal the application presence
  char filename[_H_MAX_FILENAMELENGHT];
  sprintf(filename, "%s/%d", getenv("JOBSIGNALER_DIR"), application_id);
  FILE* fd = fopen(filename, "w");
  fclose(fd);

  // Creating shared memory segment
  key_t key = ftok(filename, application_id);
  int shmid = shmget(key, sizeof(_application_h), 0666 | IPC_CREAT);
  _application_h* a = (_application_h*) shmat(shmid, (void*) 0, 0); 
  if (a == (_application_h*) -1) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[registration] failed, shared memory failure\n");
    #endif
    exit(EXIT_FAILURE_SHAREDMEMORY);
  }
  a->application_id = application_id;
  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[registration] ended\n");
  #endif
  return a;

}

// ****************************************************************************
int jobsignaler_set(_application_h* a, uint types, uint64_t* ert) {

  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[set] started\n");
  #endif
  int retvalue = EXIT_NORMAL;
  char filename[_H_MAX_FILENAMELENGHT];
  sprintf(filename, "%s/%d", getenv("JOBSIGNALER_DIR"), a->application_id);
  key_t key = ftok(filename, a->application_id);
  int shmid = shmget(key, sizeof(_application_h), 0666);
  a->shared_memory_segment = shmid;

  // Application dependant initialization
  if (types > _H_MAX_JOBS) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[set] jobs number exceeding maximum; limited\n");
    #endif
    types = _H_MAX_JOBS; 
  }
  a->jobs = types;
  int c_act;
  for (c_act = 0; c_act<types; ++c_act)
    a->expected_response_times[c_act] = ert[c_act];
  
  // Application independent initialization
  a->total_jobs = 0;
  a->progress_jobs = 0;
  a->completed_jobs = 0;
  #ifdef _JOBSIGNALER_MULTITHREADED
    retvalue = pthread_mutex_init(&a->mutex, NULL);
    if (retvalue != 0) {
      #ifdef _JOBSIGNALER_ERROR
        fprintf(stderr, "[set] mutex error\n");
      #endif
    }
  #endif
  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[set] ended\n");
  #endif
  return retvalue;

}

// ****************************************************************************
int jobsignaler_terminate(_application_h* a) {

  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[terminate] started\n");
  #endif
  char filename[_H_MAX_FILENAMELENGHT];
  sprintf(filename, "%s/%d", getenv("JOBSIGNALER_DIR"), a->application_id); 
  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[terminate] removing %s\n", filename);
  #endif
  remove(filename);
  int shmid = a->shared_memory_segment;
  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[terminate] shared memory: %d\n", shmid);
  #endif
  shmctl(shmid, IPC_RMID, NULL); // Deleting shared memory segment
  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[terminate] ended\n");
  #endif
  return EXIT_NORMAL;

}

// ****************************************************************************
int jobsignaler_signalstart(_application_h* a, uint type) {

  // Get actual time
  struct timespec time_info;
  int64_t time;
  clock_gettime(CLOCK_REALTIME, &time_info);
  time = (int64_t) time_info.tv_sec*1000000000 + (int64_t) time_info.tv_nsec;
  uint job_id = a->total_jobs;

  #ifdef _JOBSIGNALER_MULTITHREADED
    pthread_mutex_lock(&a->mutex);
  #endif

  // The number of jobs in progress should never exceed the max number
  // Otherwise jobs will be overwritten and will never finish
  uint index_in_progress = a->progress_jobs % _H_MAX_RECORDS;
  a->jprogress[index_in_progress].id = a->total_jobs;
  a->jprogress[index_in_progress].type = type;
  a->jprogress[index_in_progress].start_timestamp = time;
  a->jprogress[index_in_progress].end_timestamp = time;
  a->total_jobs++;
  a->progress_jobs++;

  #ifdef _JOBSIGNALER_DEBUG
    fprintf(stdout, "[start] added job %d\n", index_in_progress);
  #endif

  #ifdef _JOBSIGNALER_MULTITHREADED
    pthread_mutex_unlock(&a->mutex);
  #endif
  return job_id;

}

// ****************************************************************************
int jobsignaler_signalend(_application_h* a, uint id) {

  // Get actual time
  struct timespec time_info;
  int64_t time;
  clock_gettime(CLOCK_REALTIME, &time_info);
  time = (int64_t) time_info.tv_sec*1000000000 + (int64_t) time_info.tv_nsec;
  int retvalue = EXIT_FAILURE_JOBNOTFOUND;

  #ifdef _JOBSIGNALER_MULTITHREADED
    pthread_mutex_lock(&a->mutex);
  #endif

  // Looking for the job to be terminated
  int c_act;
  for (c_act=0; c_act<(a->progress_jobs % _H_MAX_RECORDS); ++c_act) {
    if (a->jprogress[c_act].id == id) {
      
      // Writing it
      uint index_completed = a->completed_jobs % _H_MAX_RECORDS;
      int type = a->jprogress[c_act].type;
      int64_t start = a->jprogress[c_act].start_timestamp;
      a->jcompleted[index_completed].id = a->total_jobs;
      a->jcompleted[index_completed].type = type;
      a->jcompleted[index_completed].start_timestamp = start;
      a->jcompleted[index_completed].end_timestamp = time;
      a->completed_jobs++;

      // Clearing up the progress log
      a->jprogress[c_act].id = 0;
      a->jprogress[c_act].type = 0;
      a->jprogress[c_act].start_timestamp = 0;
      a->jprogress[c_act].end_timestamp = 0;
      a->progress_jobs--;

      // Done
      #ifdef _JOBSIGNALER_DEBUG
        fprintf(stdout, "[stop] removed job %d into %d\n", c_act, index_completed);
      #endif
      retvalue = EXIT_NORMAL;
    }
  }

  #ifdef _JOBSIGNALER_MULTITHREADED
    pthread_mutex_unlock(&a->mutex);
  #endif
  return retvalue;

}

// ****************************************************************************
int get_applications(int* application_ids) {

  if(getenv("JOBSIGNALER_DIR") == NULL) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[get applications] JOBSIGNALER_DIR undefined\n");
    #endif
    return EXIT_FAILURE_UNDEFINEDAUTOSIGNALER;
  }
  // Assumption, in the directory there are only memory mapped files
  // Which means that the application programmer should choose an
  // empty directory, keep that in mind when you set JOBSIGNALER_DIR
  DIR* jobdir;
  struct dirent *fildir;
  jobdir = opendir(getenv("JOBSIGNALER_DIR"));
  if (jobdir == NULL) {
    return -1; // an error occurred
  }
  else {
    int filecount = 0;
    while (fildir = readdir(jobdir)) {
      if ((strcmp(fildir->d_name, ".") != 0) &&  
            (strcmp(fildir->d_name, "..") != 0)) {
        application_ids[filecount] = atoi(fildir->d_name);
        ++filecount;
      }
    }
    closedir(jobdir);
    return filecount;
  }

}

// ****************************************************************************
_application_h* monitor_application_init(int application_id) {

  char filename[_H_MAX_FILENAMELENGHT];
  sprintf(filename, "%s/%d", getenv("JOBSIGNALER_DIR"), application_id);
  key_t key = ftok(filename, application_id);
  int shmid = shmget(key, sizeof(_application_h), 0666);
  _application_h* a = (_application_h*) shmat(shmid, (void*) 0, 0); 
  if (a == (_application_h*)-1) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[monitor init] error in shared attach\n");
    #endif
    return NULL;
  }
  return a;

}

// ****************************************************************************
int monitor_application_stop(_application_h* a) {

  // Detaching shared memory
  if (shmdt(a) == -1) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[monitor stop] error in shared detach\n");
    #endif
  }
  return EXIT_NORMAL;

}

// ****************************************************************************
int set_weight(int application_id, double weight) {

  if(getenv("JOBSIGNALER_DIR") == NULL) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[weight] JOBSIGNALER_DIR undefined\n");
    #endif
    return EXIT_FAILURE_UNDEFINEDAUTOSIGNALER;
  }
  char filename[_H_MAX_FILENAMELENGHT];
  sprintf(filename, "%s/%d", getenv("JOBSIGNALER_DIR"), application_id);
  key_t key = ftok(filename, application_id);
  int shmid = shmget(key, sizeof(_application_h), 0666);
  _application_h* a = (_application_h*) shmat(shmid, (void*) 0, 0); 
  if (a == (_application_h*) -1) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[weight] error in shared attach\n");
    #endif
    return EXIT_FAILURE_SHAREDMEMORY;
  }
  a->weight = weight; // set
  if (shmdt(a) == -1) {
    #ifdef _JOBSIGNALER_ERROR
      fprintf(stderr, "[weight] error in shared detach\n");
    #endif
    return EXIT_FAILURE_SHAREDMEMORY;
  }
  return EXIT_NORMAL;

}

// ****************************************************************************
double get_weight(_application_h* a) { 

  return a->weight; 

}

// ****************************************************************************
void set_performance_multiplier(_application_h* a, double multiplier) {

  a->performance_multiplier = multiplier;

}

// ****************************************************************************
double get_performance_multiplier(_application_h* a) {

  return a->performance_multiplier;

}

// ****************************************************************************
double get_performance_number(_application_h* a, int job_type) {

  // Averaging the value of last jobs of specified type: the function
  // uses all jobs of the given type that are in the latest
  // records. If empty it will return zero. If called with -1 it will
  // return the average over all type of jobs.
  double sum_performances = 0.0;
  double num_performances = 0.0;
  int c_act;
  int upvalue = a->completed_jobs;
  if (a->completed_jobs > _H_MAX_RECORDS)
    upvalue = _H_MAX_RECORDS;

  for (c_act=0; c_act<upvalue; ++c_act) {
    if ((a->jcompleted[c_act].type == job_type || 
        job_type == -1 || job_type>=a->jobs) &&
        // this last condition in the if is used to avoid to report
        // job of type 0 that are not real jobs but initial values of
        // the vector: real jobs have a start timestamp
        a->jcompleted[c_act].start_timestamp != 0) {
      int64_t deadline = a->expected_response_times[a->jcompleted[c_act].type];
      int64_t response_time = a->jcompleted[c_act].end_timestamp
        - a->jcompleted[c_act].start_timestamp;
      double this_perf = ((double) deadline / (double) response_time) - 1.0;

      if (this_perf<-1.0) this_perf = -1.0; // thresholds
      if (this_perf>+1.0) this_perf = +1.0;
      sum_performances += this_perf;
      num_performances += 1.0;
    } 
  }
  if (num_performances == 0)
    return -1.0;
  else
    // averaging
    return sum_performances/num_performances;

}

