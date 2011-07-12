#include "infraredray.h"

void main(void) {
  irInit();

  while (1) {
    if (irTest()) {
      P1 = irGetDataCode();
    }
  }

}
