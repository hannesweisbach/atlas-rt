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

dispatch_queue_t dispatch_queue_create(const char *label,
                                       dispatch_queue_attr_t attr);
void dispatch_queue_release(dispatch_queue_t);

#if defined(__clang__) && defined(__block)
void dispatch_async(dispatch_queue_t queue, void (^block)(void));
void dispatch_sync(dispatch_queue_t queue, void (^block)(void));
#endif

void dispatch_async_f(dispatch_queue_t queue, void *context,
                      void (*function)(void *));
void dispatch_sync_f(dispatch_queue_t queue, void *context,
                     void (*function)(void *));

#if defined(__clang__) && defined(__block)
void dispatch_async_atlas(dispatch_queue_t queue,
                          const struct timespec *deadline,
                          const double *metrics, const size_t metrics_count,
                          void (^block)(void));
void dispatch_sync_atlas(dispatch_queue_t queue,
                         const struct timespec *deadline, const double *metrics,
                         const size_t metrics_count, void (^block)(void));
#endif

void dispatch_async_atlas_f(dispatch_queue_t queue,
                            const struct timespec *deadline,
                            const double *metrics, const size_t metrics_count,
                            void *context, void (*function)(void *));
void dispatch_sync_atlas_f(dispatch_queue_t queue,
                           const struct timespec *deadline,
                           const double *metrics, const size_t metrics_count,
                           void *context, void (*function)(void *));

#ifdef __cplusplus
}
#endif

