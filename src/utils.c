#include "utils.h"

char* settings_path;
char* public_dir;

void initialize_paths(char* argv) {
  settings_path = (char*)malloc(PATH_MAX);
  public_dir = (char*)malloc(PATH_MAX);
  strcpy(settings_path, dirname(argv));
  strcpy(public_dir, settings_path);  
  strcat(settings_path, "/settings.json");
  strcat(public_dir, "/public/");
}

void free_paths() {
  free(settings_path);
  free(public_dir);
}