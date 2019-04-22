/*
   Copyright 2015 Bloomberg Finance L.P.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

/**
 * We keep an in-memory structure of the pending blocksql transactions
 * We avoid repeating blocksql requests and wait for the first transaction to
 * commit before sending back an answer
 */

#include <stdio.h>
#include <pthread.h>
#include <plhash.h>
#include <poll.h>
#include "comdb2.h"
#include "osqlblkseq.h"
#include "logmsg.h"

int gbl_block_blkseq_poll = 10; /* 10 msec */

static hash_t *hiqs = NULL;
static pthread_rwlock_t hlock = PTHREAD_RWLOCK_INITIALIZER;

/*
 * Init this module
 *
 */
int osql_blkseq_init(void)
{
    int rc = 0;

    Pthread_rwlock_wrlock(&hlock);

    hiqs = hash_init_o(offsetof(struct ireq, seq), sizeof(fstblkseq_t));
    if (!hiqs) {
        logmsg(LOGMSG_FATAL, "UNABLE TO init hash\n");
        abort();
    }

    Pthread_rwlock_unlock(&hlock);

    return rc;
}

/**
 * Main function
 * - check to see if the seq exists
 * - if this is a replay, return OSQL_BLOCKSEQ_REPLAY
 * - if this is NOT a replay, insert the seq and return OSQL_BLOCKSEQ_FIRST
 *
 */
int osql_blkseq_register(struct ireq *iq)
{
    struct ireq *iq_src = NULL;
    int rc = 0;

    assert(hiqs != NULL);

    Pthread_rwlock_wrlock(&hlock);
    iq_src = hash_find(hiqs, (const void *)&iq->seq);
    if (!iq_src) { /* not there, we add it */
        hash_add(hiqs, iq);
        rc = OSQL_BLOCKSEQ_FIRST;
    }
    Pthread_rwlock_unlock(&hlock);
   
    /* rc == 0 means we need to wait for it to go away */
    while (rc == 0) {
        poll(NULL, 0, gbl_block_blkseq_poll);

        /* rdlock will suffice */
        Pthread_rwlock_rdlock(&hlock);
        iq_src = hash_find_readonly(hiqs, (const void *)&iq->seq);
        Pthread_rwlock_unlock(&hlock);

        if (!iq_src) {
            /* done waiting */
            rc = OSQL_BLOCKSEQ_REPLAY;
        }
        /* keep searching */
    }

    return rc;
}

/**
 * Remove a blkseq from memory hash so that the next blocksql
 * repeated transactions can proceed ahead
 *
 */
int osql_blkseq_unregister(struct ireq *iq)
{
    /* Fast return if have_blkseq is false.
       It not only saves quite a few instructions,
       but also avoids a race condition with osql_open() */
    if (!iq->have_blkseq)
        return 0;

    assert(hiqs != NULL);

    Pthread_rwlock_wrlock(&hlock);
    hash_del(hiqs, iq);
    Pthread_rwlock_unlock(&hlock);
    return 0;
}
