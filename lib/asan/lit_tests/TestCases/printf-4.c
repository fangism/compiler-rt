// RUN: %clang_asan -O2 %s -o %t
// RUN: ASAN_OPTIONS=check_printf=1 not %t 2>&1 | FileCheck --check-prefix=CHECK-ON %s
// RUN: ASAN_OPTIONS=check_printf=0 %t 2>&1 | FileCheck --check-prefix=CHECK-OFF %s
// RUN: %t 2>&1 | FileCheck --check-prefix=CHECK-OFF %s

#include <stdio.h>
int main() {
  volatile char c = '0';
  volatile int x = 12;
  volatile float f = 1.239;
  volatile char s[] = "34";
  volatile char buf[2];
  sprintf((char *)buf, "%c %d %.3f %s\n", c, x, f, s);
  puts((const char *)buf);
  return 0;
  // Check that size of output buffer is sanitized.
  // CHECK-ON: stack-buffer-overflow
  // CHECK-ON-NOT: 0 12 1.239 34
  // CHECK-OFF: 0 12 1.239 34
}
