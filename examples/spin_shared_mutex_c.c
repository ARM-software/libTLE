/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <tle/mutex.h>

/* shared variable */
int g_i = 0;

/* protects g_i */
libtle_spin_shared_mutex_t g_i_mutex = LIBTLE_SPIN_SHARED_MUTEX_INIT;
_Thread_local libtle_spin_shared_mutex_handle_t  g_i_lock;

void* __attribute__((noinline)) safe_increment(void *data)
{
    libtle_mutex_lock(&g_i_mutex, &g_i_lock);
    ++g_i;
    libtle_mutex_unlock(&g_i_mutex, &g_i_lock);
    return NULL;
}

int __attribute__((noinline)) safe_read()
{
    int tmp;
    libtle_mutex_lock_shared(&g_i_mutex, &g_i_lock);
    tmp = g_i;
    libtle_mutex_unlock_shared(&g_i_mutex, &g_i_lock);
    return tmp;
}

pthread_t thr[2];

int main()
{
    /*libtle_mutex_init(&g_i_mutex);*/

    printf("%s: %d\n", __func__, (int) safe_read());

    if (pthread_create(&thr[0], NULL, safe_increment, NULL)) {
        fprintf(stderr, "Could not start thread #1\n");
        exit(1);
    }
    if (pthread_create(&thr[1], NULL, safe_increment, NULL)) {
        fprintf(stderr, "Could not start thread #2\n");
        exit(1);
    }

    printf("%s: %d\n", __func__, (int) safe_read());

    pthread_join(thr[0], NULL);
    pthread_join(thr[1], NULL);

    printf("%s: %d\n", __func__, (int) safe_read());

    return 0;
}
