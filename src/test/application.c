#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <jobsignaler.h>

#define TOTAL_JOBS 20
#define ARCH_NUM_OP 300
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
  int64_t time_milliseconds = cpu + correction;
  if (time_milliseconds < 1) time_milliseconds = 1; // sanity check

  for (j=0; j<time_milliseconds; j++) {
    // work for one millisecond
    int num_times = ARCH_NUM_OP; // profiling the architecture
    double c = 0.0;
    int i;
    for(i=0; i<num_times; i++) { 
      double a = generate_random_0_1();
      double b = generate_random_0_1();
      c += a*b;
    }
    res_discarded += (int)(c*10);
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
    memcpy(mem_buffer_snd, mem_buffer_rec, total_size); // also memmove could works
    free(mem_buffer_snd);
    free(mem_buffer_rec);
  }
  return res_discarded;
}

int main(int argc, char* argv[]) {

  float service_level;
  float a_cpu, b_cpu;
  float a_mem, b_mem;
  double epsilon;
  if (argc != 7) {
  // Side note - Launch with:
  // ./application 1.0 845.0 0.0 0.0 0.0 1.0
  // to have performance close to zero without adaptation on lennon
    #ifdef ERROR_APPLICATION
      fprintf(stderr, "[application] usage:\n");
      fprintf(stderr, "<application> initial_sl a_cpu b_cpu a_mem b_mem epsilon\n");
      exit(EXIT_APPLICATIONFAILURE);
    #endif
  }
  service_level = atof(argv[1]);
  a_cpu = atof(argv[2]);
  b_cpu = atof(argv[3]);
  a_mem = atof(argv[4]);
  b_mem = atof(argv[5]);
  epsilon = atof(argv[6]);

  _application_h* myself = jobsignaler_registration();
  uint64_t ert[1] = {1000000000};
  jobsignaler_set(myself, 1, ert);

  int jobs;
  double performance;
  for(jobs=0; jobs<TOTAL_JOBS; ++jobs) {
    int type = 0;
    uint id = jobsignaler_signalstart(myself, type);
    performance = get_performance_number(myself, type);

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
      name = sprintf(name, "%u.log", myself->application_id);
      FILE* logfile = fopen(name, "a+");
      fprintf(logfile, "%lld, %f, %f, %lld, %lld, %lld\n", 
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