.section .init
.global _init
_init:
  mov     ip, sp
  stmdb   sp!, {fp, ip, lr, pc}
  sub     fp, ip, #4

.section .fini
.global _fini
_fini:
  mov     ip, sp
  stmdb   sp!, {fp, ip, lr, pc}
  sub     fp, ip, #4
