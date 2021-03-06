#include "chrono.h"

#include "debug.h"
#include "macros.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "native/thread.h"
#include <pthread.h>


bool chrono_thread_init = false;

static void *chrono_thread(void *args) {
    LOG_INFO("Chono", "Thread starting");

    CHRONO_INFO *info = args;
    chrono_thread_init = true;
    while (info->ptr != info->target) {
        info->ptr += info->step;
        yieldcpu(info->interval_ms);
    }
    chrono_thread_init = false;

    if (info->callback) {
        info->callback(info->cb_data);
    }

    LOG_INFO("Chrono", "Thread exited cleanly");

    pthread_exit(0);
}

bool chrono_start(CHRONO_INFO *info) {
    if (!info) {
        LOG_ERR("Chrono", "Chrono info structure is null.");
        return false;
    }

    thread(chrono_thread, info);

    return true;
}

bool chrono_end(CHRONO_INFO *info) {
    if (!info) {
        LOG_ERR("Chrono", "Chrono info is null");
        return false;
    }

    (*info).finished = true;

    while (chrono_thread_init) { //wait for thread to die
        yieldcpu(1);
    }

    return true;
}

void chrono_callback(uint32_t ms, void func(void *), void *funcargs) {
    yieldcpu(ms);
    func(funcargs);
}
