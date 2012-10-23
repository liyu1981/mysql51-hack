/* vim: set ts=4 sw=4 tw=0: */
#ifndef _CDB_ERROR_H
#define _CDB_ERROR_H

#define CDB_FINE (int)0 // Fine Is Not Error

// shm mgr related errors
#define CDB_SHM_INIT_SHMID_FILE_FAILED                                 (int)7001
#define CDB_SHM_INIT_REG_ERROR                                         (int)7002
#define CDB_SHM_INIT_CA_OPEN_ERROR                                     (int)7003
#define CDB_SHM_INIT_SHMLOCKS_ERROR                                    (int)7004
#define CDB_SHM_INIT_SHMLOCKS_STALE                                    (int)7005
#define CDB_SHM_PAIR_MAP_OPEN_FAILED                                   (int)7006
#define CDB_SHM_INIT_SWITCH_THREAD_FAIL                                (int)7007

#endif

