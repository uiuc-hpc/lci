namespace util {
// TODO: Simplify the thread spawning and pinning
void pin_thread_to_cpu(size_t cpu_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_id, &cpuset);
  pthread_t current_thread = pthread_self();
  int rc = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
    fprintf(stderr, "Error setting CPU affinity for thread %zu: %d\n", cpu_id, rc);
  }
}
} // namespace util