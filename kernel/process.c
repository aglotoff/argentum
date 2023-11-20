#include <errno.h>
#include <string.h>

#include <kernel/elf.h>
#include <kernel/object.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

static struct ObjectCache *process_cache;

static pid_t process_next_id = 1;

#define PROCESS_ID_HASH_SIZE 64

static struct ListLink process_id_hash[PROCESS_ID_HASH_SIZE];

void
process_init(void)
{
  int i;
  
  for (i = 0; i < PROCESS_ID_HASH_SIZE; i++)
    list_init(&process_id_hash[i]);

  process_cache = object_cache_create("process", sizeof(struct Process), 0,
                                     NULL, NULL);
  if (process_cache == NULL)
    panic("cannot allocate process_cache");
}

static struct Process *
process_alloc(void)
{
  struct Process *process;

  if ((process = (struct Process *) object_alloc(process_cache)) == NULL)
    return NULL;

  if ((process->vm = arch_vm_create()) == NULL) {
    object_free(process_cache, process);
    return NULL;
  }

  list_init(&process->threads);
  process->active_threads = 0;

  return process;
}

void
process_free(struct Process *process)
{
  arch_vm_destroy(process->vm);
  object_free(process_cache, process);
}

pid_t
process_create(const void *binary)
{
  Elf32_Ehdr *elf;
  Elf32_Phdr *ph, *eph;
  struct Process *process;
  struct Thread *main_thread;
  int perm, err = 0;

  elf = (Elf32_Ehdr *) binary;
  if (memcmp(elf->ident, "\x7f""ELF", 4) != 0) {
    err = -EINVAL;
    goto fail1;
  }

  if ((process = process_alloc()) == NULL) {
    err = -ENOMEM;
    goto fail1;
  }

  ph = (Elf32_Phdr *) ((uint8_t *) elf + elf->phoff);
  eph = ph + elf->phnum;

  for ( ; ph < eph; ph++) {
    if (ph->type != PT_LOAD)
      continue;

    if (ph->filesz > ph->memsz) {
      err = -EINVAL;
      goto fail2;
    }

    perm = VM_READ | VM_USER;
    if (perm & PTF_WRITE) perm |= VM_WRITE;
    if (perm & PTF_EXECINSTR) perm |= VM_EXEC;

    if ((err = vm_range_alloc(process->vm, ph->vaddr, ph->memsz, perm)) < 0)
      goto fail2;

    if ((err = vm_copy_out(process->vm, ph->vaddr, (uint8_t *) elf + ph->offset,
                           ph->filesz)) < 0)
      goto fail2;
  }

  if ((main_thread = thread_create_user(process, elf->entry)) == NULL) {
    err = -ENOMEM;
    goto fail2;
  }
  
  process->id = process_next_id++;
  return process->id;

fail2:
  process_free(process);
fail1:
  return err;
}
