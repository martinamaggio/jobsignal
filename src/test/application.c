#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Using the library:
#include <jobsignaler.h>

#define TOTAL_JOBS 100
#define NOISE_PERCENTAGE 0.0
#define MINIMUM_SERVICE_LEVEL 0.0001

#define ERROR_APPLICATION 0
#define LOGGING_APPLICATION 0
#define EXIT_APPLICATIONFAILURE -1

double generate_random_0_1() {
  time_t now = time(0);
  unsigned char *c = (unsigned char*)&now;
  unsigned seed = 0;
  size_t iterator;
  for(iterator=0; iterator < sizeof now; ++iterator)
    seed = seed * (UCHAR_MAX + 2U) + c[iterator];
  srand(seed);
  return (rand() * ( 1.0 / (RAND_MAX+1.0)));
}

int do_work(int64_t cpu, int64_t mem, double noise) {
  int j;
  int res_discarded;

  double val = generate_random_0_1() - 0.5; // random between -0.5 and 0.5
  int64_t correction = val * noise * cpu; // noise correction
  int64_t operations = cpu + correction;
  if (operations < 1) operations = 1; // sanity check

  // Do mathematical operations
  for (j=0; j<operations; j++) { 
    double a = generate_random_0_1();
    double b = generate_random_0_1();
    res_discarded += (int)(a*b*10);
  }

  int num_chars = mem / 2;
  if (num_chars > 0) {
    // use memory
    char *mem_buffer_snd;
    char *mem_buffer_rec;
    char *p;
    int total_size = num_chars * sizeof(char);
    mem_buffer_snd = (char *)malloc(total_size);
    mem_buffer_rec = (char *)malloc(total_size);
    for (p = mem_buffer_snd; p < mem_buffer_snd + num_chars; p++)
      *p = (char)((uintptr_t)p & 0xff);
    memcpy(mem_buffer_snd, mem_buffer_rec, total_size); 
    // also memmove could have worked, instead of memcpy
    free(mem_buffer_snd);
    free(mem_buffer_rec);
  }
  return res_discarded;
}

int main(int argc, char* argv[]) {

  float service_level;
  float a_cpu, b_cpu;
  float a_mem, b_mem;
  double epsilon, weight;
  double deadline_seconds;
  if (argc != 9) {
    #ifdef ERROR_APPLICATION
      fprintf(stderr, "[application] usage:\n");
      fprintf(stderr, "<a> sl a_cpu b_cpu a_mem b_mem eps weight deadline\n");
      exit(EXIT_APPLICATIONFAILURE);
    #endif
  }
  
  // Parameter parsing
  service_level = atof(argv[1]);
  a_cpu = atof(argv[2]);
  b_cpu = atof(argv[3]);
  a_mem = atof(argv[4]);
  b_mem = atof(argv[5]);
  epsilon = atof(argv[6]);
  weight = atof(argv[7]);
  deadline_seconds = atof(argv[8]);

  _application_h* myself = jobsignaler_registration();
  uint64_t deadline = (unsigned int) ((double)1000000000 * deadline_seconds);
  uint64_t ert[1] = {deadline};
  jobsignaler_set(myself, 1, ert);
  set_weight(myself->application_id, weight);

  int jobs;
  double performance;
  for(jobs=0; jobs<TOTAL_JOBS; ++jobs) {
    int type = 0;
    uint id = jobsignaler_signalstart(myself, type);
    performance = get_performance_number(myself, type);

    // I want to adapt only if needed
    if (performance != 0.0) {
      // Service level adaptation
      service_level += epsilon * (performance * service_level);
      if (service_level < MINIMUM_SERVICE_LEVEL)
        service_level = MINIMUM_SERVICE_LEVEL;
      if (service_level != service_level) // avoid nans
        service_level = 1.0;
    }

    int64_t cpu_requirement = a_cpu * service_level + b_cpu;
    int64_t mem_requirement = a_mem * service_level + b_mem;

    #ifdef LOGGING_APPLICATION
      struct timespec time_info;
      int64_t current_time;
      clock_gettime(CLOCK_REALTIME, &time_info);
      current_time = (int64_t) time_info.tv_sec*1000000000 
        + (int64_t) time_info.tv_nsec;
      char name[10];
      sprintf(name, "%u.log", myself->application_id);
      FILE* logfile = fopen(name, "a+");
      fprintf(logfile, "%lld, %f, %f, %lld, %lld, %u\n", 
        current_time, performance, service_level, 
        cpu_requirement, mem_requirement, id);
      fclose(logfile);
    #endif

    // Do the required work  
    do_work(cpu_requirement, mem_requirement, NOISE_PERCENTAGE);
    jobsignaler_signalend(myself, id);
  }
  jobsignaler_terminate(myself);

}
