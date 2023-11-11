#ifndef __AG_INCLUDE_ARCH_KERNEL_SMP_H__
#define __AG_INCLUDE_ARCH_KERNEL_SMP_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

/** At most four CPUs on Cortex-A9 MPCore */
#define ARCH_SMP_CPU_MAX   4

#ifndef __ASSEMBLER__

#ifdef __cplusplus
extern "C" {
#endif

struct Cpu;

void        arch_smp_init(void);
struct Cpu *arch_smp_get_cpu(unsigned);
unsigned    arch_smp_id(void);

#ifdef __cplusplus
};
#endif

#endif // !__ASSEMBLER__

#endif  // !__AG_INCLUDE_ARCH_KERNEL_SMP_H__
