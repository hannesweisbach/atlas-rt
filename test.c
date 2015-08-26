#include <unistd.h>
#include <stdio.h>

#include "dispatch/dispatch.h"

static void func(void *ctx) {
  int *i = (int *)ctx;
  printf("%s %p %d\n", __func__, ctx, (*i)++);
}

int main() {
  int __block i = 0;
  
  dispatch_queue_t queue = dispatch_queue_create("test", NULL);
  dispatch_async_f(queue, &i, func);
  printf("after: %d\n", i);
  dispatch_sync_f(queue, &i, func);
  printf("after: %d\n", i);
  dispatch_async(queue, ^{
    printf("block async %d\n", i++);
  });
  printf("after: %d\n", i);
  dispatch_sync(queue, ^{
    printf("block sync %d\n", i);
  });
  printf("after: %d\n", i);
  dispatch_queue_release(queue);
}
