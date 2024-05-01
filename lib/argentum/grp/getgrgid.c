#include <grp.h>
#include <stdio.h>

// TODO: replace with real data
static struct group root_group = {
  .gr_gid    = 0,
  .gr_mem    = { "root" },
  .gr_name   = "root",
  .gr_passwd = "",
};

struct group *
getgrgid(gid_t gid)
{
  if (gid == 0)
    return &root_group;

  fprintf(stderr, "TODO: getgrgid(%d)\n", gid);
  return -1;
}
