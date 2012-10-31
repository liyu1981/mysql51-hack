#ifndef _MYSQL_HEADERS_H
#define _MYSQL_HEADERS_H

#define MYSQL_SERVER // define MYSQL_SERVER since we need opt_cdb_* variables

#ifdef MY_PTHREAD_FASTMUTEX
#undef MY_PTHREAD_FASTMUTEX // undef this to avoid the pthread conflict
#endif

#include "../sql/mysql_priv.h" // must be included after system headers such as pthread.h, otherwise conflict with mysys

#endif
