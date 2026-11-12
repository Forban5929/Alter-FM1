/* SPDX-License-Identifier: GPL-3.0-only
 * cc -O2 -o build/alter/alter_dvm_test tools/alter/alter_dvm_test.c && build/alter/alter_dvm_test */
#include <stdint.h>
#include <stdio.h>
#include "../../firmware/src/eng_dvm.h"   /* v0.75: every quotient PLATE, CORDE, RING, RESO ask, against C's / */
int main(void){ uint64_t bad=0, n=0;
  for (uint32_t d=4096; d<=14256; d++){ uint32_t m=dvm_make(d,42); for (int32_t z=-32767; z<=32767; z+=1){ int32_t x=z<<12; n++; if (dvm_div(x,m,42)!=x/(int32_t)d) bad++; } }
  for (uint32_t d=4096; d<=9176; d++){ uint32_t m=dvm_make(d,42); for (int32_t z=-65534; z<=65534; z+=3){ int32_t x=z<<12; n++; if (dvm_div(x,m,42)!=x/(int32_t)d) bad++; } }
  for (uint32_t d=10; d<=3510; d++){ uint32_t m=dvm_make(d,32); for (int32_t e=0; e<=65536; e++){ n++; if (dvm_div(e,m,32)!=e/(int32_t)d) bad++; } }
  printf("%llu checks, %llu wrong\n",(unsigned long long)n,(unsigned long long)bad); return bad!=0; }
