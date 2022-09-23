#include "utils.h"

char* settings_path;

void initialize_paths(char* argv) {
  settings_path = (char*)malloc(PATH_MAX);
  strcpy(settings_path, dirname(argv));
  strcat(settings_path, "/settings.json");
}

void free_paths() {
  free(settings_path);
}