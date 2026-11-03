#include <prism.h>
#include <stdio.h>

int main(void) {
  PrismContext *ctx = prism_init(NULL);
  if (ctx == NULL) {
    fprintf(stderr, "Failed to initialize Prism\n");
    return 1;
  }
  PrismBackend *backend = prism_registry_create_best(ctx);
  if (backend == NULL) {
    fprintf(stderr, "No speech backend is available\n");
    prism_shutdown(ctx);
    return 1;
  }
  PrismError err = prism_backend_speak(backend, "Hello from Prism.", true);
  if (err != PRISM_OK) {
    fprintf(stderr, "Speech failed: %s\n", prism_error_string(err));
  }
  printf("Using %s. Press Enter to exit.\n", prism_backend_name(backend));
  getchar();
  prism_backend_free(backend);
  prism_shutdown(ctx);
  return err == PRISM_OK ? 0 : 1;
}
