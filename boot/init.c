#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
  // Open /dev/kmsg to write logs directly to the Kernel Log
  int kmsg = open("/dev/kmsg", O_WRONLY);
  if (kmsg == -1) {
    return 1;
  }

  // Print some message
  dprintf(kmsg, "Hello World from \033[32mLinux Userspace\033[0m!\n");

  while (true) {
    sleep(1);
  }

  // Close the file handle
  close(kmsg);

  return 0;
}