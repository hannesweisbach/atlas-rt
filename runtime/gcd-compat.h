#pragma once

#ifdef __cplusplus
#include <ctime>
#else
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


struct dispatch_queue;
struct dispatch_queue_attr;
typedef struct dispatch_queue *dispatch_queue_t;
typedef struct dispatch_queue_attr *dispatch_queue_attr_t;

extern dispatch_queue_attr_t DISPATCH_QUEUE_SERIAL;

#ifdef __BLOCKS__
typedef void (^dispatch_block_t)(void);
#endif
typedef void (*dispatch_function_t)(void *);

dispatch_queue_t dispatch_queue_create(const char *label,
                                       dispatch_queue_attr_t attr);
void dispatch_queue_release(dispatch_queue_t);

#ifdef __BLOCKS__
void dispatch_async(dispatch_queue_t queue, dispatch_block_t);
void dispatch_sync(dispatch_queue_t queue, dispatch_block_t);
#endif

void dispatch_async_f(dispatch_queue_t queue, void *context,
                      dispatch_function_t);
void dispatch_sync_f(dispatch_queue_t queue, void *context,
                     dispatch_function_t);

#ifdef __BLOCKS__
void dispatch_async_atlas(dispatch_queue_t queue,
                          const struct timespec *deadline,
                          const double *metrics, const size_t metrics_count,
                          dispatch_block_t);
void dispatch_sync_atlas(dispatch_queue_t queue,
                         const struct timespec *deadline, const double *metrics,
                         const size_t metrics_count, dispatch_block_t);
#endif

void dispatch_async_atlas_f(dispatch_queue_t queue,
                            const struct timespec *deadline,
                            const double *metrics, const size_t metrics_count,
                            void *context, dispatch_function_t);
void dispatch_sync_atlas_f(dispatch_queue_t queue,
                           const struct timespec *deadline,
                           const double *metrics, const size_t metrics_count,
                           void *context, dispatch_function_t);

typedef long dispatch_once_t;
#ifdef __BLOCKS__
void dispatch_once(dispatch_once_t * predicate, dispatch_block_t);
#endif
void dispatch_once_f(dispatch_once_t *predicate, void *context,
                     dispatch_function_t function);

void dispatch_release(dispatch_queue_t);
dispatch_queue_t dispatch_get_main_queue();
void dispatch_main(void);
struct timespec atlas_now(void);

#ifdef __cplusplus
}
#endif

