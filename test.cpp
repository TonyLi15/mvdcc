
#include <linux/kernel.h>
#include <linux/module.h>

#include <cstdint>
#include <iostream>

uint64_t read_pmc(uint32_t ecx) {
  uint32_t a, d;
  __asm __volatile("rdpmc" : "=a"(a), "=d"(d) : "c"(ecx));
  return ((uint64_t)a) | (((uint64_t)d) << 32);
}

// uint64_t read_cr4() {
//   uint64_t a = 0;
//   // asm volatile("movq %%cr4, %%rax;" : "=a"(a));
//   __asm __volatile("movq %%cr4, %0\n" : "=r"(a));
//   // std::cout << "a: " << a << std::endl;
//   return a;
// }
// asm volatile ("mov %%cr4, %0" : "=r" (cr4));
// asm volatile("movq %%cr4, %%rax;" : "=a" (vmsa->cr4));

void cpuid() {
  uint32_t a = 10, b, c, d;
  __asm __volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(a));
  std::cout << "a: " << a << std::endl;
  std::cout << "b: " << b << std::endl;
  std::cout << "c: " << c << std::endl;
  std::cout << "d: " << d << std::endl;
}

// __asm(アセンブリコード : 出力オペランド : 入力オペランド :
// 上書きされるレジスタ); より正確には，次のような構文だ．
// __asm("アセンブリコード" : "制約"(出力オペランド) : "制約"(入力オペランド) :
// "上書きされるレジスタ");

/*
Reads the contents of the performance monitoring counter (PMC) specified in ECX
register into registers EDX:EAX. (On processors that support the Intel 64
architecture, the high-order 32 bits of RCX are ignored.) The EDX register is
loaded with the high-order 32 bits of the PMC and the EAX register is loaded
with the low-order 32 bits. (On processors that support the Intel 64
architecture, the high-order 32 bits of each of RAX and RDX are cleared.) If
fewer than 64 bits are implemented in the PMC being read, unimplemented bits
returned to EDX:EAX will have value zero.
*/
int main() {
  // read_pmc(0);
  // cpuid();
  read_cr4();
}