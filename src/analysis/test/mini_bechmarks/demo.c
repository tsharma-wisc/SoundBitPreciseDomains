#include <stdio.h>
#include "include/reach.h"

int main () {
  int x, y, z, w, a, b, c;
  x=y=z=w=0;
  while (rand()) {
    if (y+1) {
      x++; y = y+2;
    }
    else {
      if (z+1) {
        if (x >= 4) {
          x++; y = y+3;
        }
      }
    }

    // Handle case when y overflows
    if (x > y) {
      x = 0; y = 0; 
    }
  }

  if (2*x <= y)
    UNREACHABLE();

  return 0;
}
