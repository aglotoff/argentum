#include <kernel/ipc.h>

void
endpoint_init(struct Endpoint *endpoint)
{
  k_mailbox_create(&endpoint->mbox,
                   sizeof(void *),
                   &endpoint->mbox_buf,
                   sizeof endpoint->mbox_buf);
}

int
endpoint_receive(struct Endpoint *endpoint, struct Request **req_store)
{
  return k_mailbox_receive(&endpoint->mbox, req_store, K_SLEEP_UNWAKEABLE);
}
