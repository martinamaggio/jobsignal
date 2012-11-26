#include <stdio.h>
#include <sys/types.h>
#include <jobsignaler.h>

#define TOTAL_JOBS 20
#define DEBUG_APPLICATION 0

int main(int argc, char* argv[]) {
  _application_h* myself = jobsignaler_registration();
  uint64_t ert[1] = {1000000000};
  jobsignaler_set(myself, 1, ert);

  int jobs;
  double performance;
  for(jobs=0; jobs<TOTAL_JOBS; ++jobs) {
    int type = 0;
    uint id = jobsignaler_signalstart(myself, type);
    performance = get_performance_number(myself, type);
    #ifdef DEBUG_APPLICATION
      fprintf(stdout, "[application] Performance: %f\n", performance);
    #endif
    sleep(1);
    jobsignaler_signalend(myself, id);
  }
  jobsignaler_terminate(myself);
}