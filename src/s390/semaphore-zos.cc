// Copyright 2012 the V8 project authors. All rights reserved.
//
// Copyright IBM Corp. 2016. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "semaphore-zos.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/modes.h>


void assignSemInitializeError() {
  switch (errno) {
  case EACCES:
    errno = EPERM;
    break;
  case EINVAL:
    break;
  case EPERM:
    break;
  case ERANGE:
    errno = EINVAL;
    break;
  }
}


void assignSemDestroyError() {
  switch (errno) {
  case EACCES:
    errno = EINVAL;
    break;
  case EINVAL:
    break;
  case EPERM:
    errno = EINVAL;
    break;
  case ERANGE:
    break;
  }
}


void assignSemgetError() {
  switch (errno) {
  case EACCES:
    errno = EPERM;
    break;
  case EINVAL:
    break;
  case ENOENT:
    errno = EINVAL;
    break;
  case ENOSPC:
    break;
  default:
    break;
  }
}


void assignSemopErrorCode() {
  switch (errno) {
  case EACCES:
    errno = EINVAL;
    break;
  case EINVAL:
    break;
  case EFAULT:
    errno = EINVAL;
    break;
  case EFBIG || EIDRM:
    errno = EINVAL;
    break;
  case ERANGE:
    break;
  case ENOSPC:
    break;
  case EINTR:
    break;
  case EAGAIN:
    break;
  default:
    errno = EINVAL;
    break;
  }
}


// On success returns 0. On error returns -1 and errno is set.
int sem_init(int *semid, int pshared, unsigned int value) {
  if ((*semid = semget(IPC_PRIVATE, 1, S_IRUSR | S_IWUSR)) == -1) {
    assignSemgetError();
    return -1;
  }
  // Assign value to the semaphore.
  struct sembuf buf;
  buf.sem_num = 0;
  buf.sem_op = value;
  buf.sem_flg = 0;
  if (semop(*semid, &buf, 1) == -1) {
    assignSemInitializeError();
    return -1;
  }
  return 0;
}


/* sem_destroy -- destroys the semaphore using semctl() */
int sem_destroy(int *semid) {
  int ret = semctl(*semid, 0, IPC_RMID);
  if (ret == -1) {
    assignSemDestroyError();  /* assign err code for semctl*/
  }
  return ret;
}

/* sem_wait -- it gets a lock on semaphore and implemented using semop() */
int sem_wait(int *semid) {
  struct sembuf sb;
  sb.sem_num = 0;
  sb.sem_op = -1;
  sb.sem_flg = 0;
  if (semop(*semid, &sb, 1) == -1) {
    assignSemopErrorCode();
    return -1;
  }
  return 0;
}


/* sem_timedwait -- it waits for a specific time-period to get a lock on
 * semaphore. Implemented using __semop_timed() */
int sem_timedwait(int *semid, struct timespec *ts) {
  int ret;
  struct sembuf sb;
  sb.sem_num = 0;
  sb.sem_op = -1;
  sb.sem_flg = 0;

  ret = __semop_timed(*semid, &sb, 1, ts);
  if (ret != 0) {
    assignSemopErrorCode();
  }
  return ret;
}


/* sem_post -- it releases lock on semaphore using semop */
int sem_post(int *semid) {
  struct sembuf sb;
  sb.sem_num = 0;
  sb.sem_op = 1;
  sb.sem_flg = 0;
  if (semop(*semid, &sb, 1) == -1) {
    assignSemopErrorCode();
    return -1;
  }
  return 0;
}
