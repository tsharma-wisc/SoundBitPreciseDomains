// Picked from http://www.cfdvs.iitb.ac.in/~bhargav/dagger/fig2.c
// Automatically refining abstract interpretations TR-07-23
/* Example from Figure 2 */
#include <stdio.h>
#include <stdlib.h>

int main () {
  int x, y, z, w;
  x=y=z=w=0;
  while (rand() ) {
    if (rand()) {
      x++; y = y+2;
    }
    else {
      if (rand()) {
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

  // This assertion should be true even in cases when 
  assert (3*x >= y);
  return 0;
}
