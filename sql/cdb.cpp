#include "cdb.h"

#include "cdb_error.h"
#include "cdb_shm_mgr.h"

#include <fstream>
#include <sys/ipc.h>

using namespace std;
using namespace cdb;

#define MB (1024*1024)

int cdb_errno = CDB_FINE;
int cdb_2nd_errno = 0;

CDBShmConf cdb_shm_conf_array[] = {
    //name, id, size, node_total, bucket_size, n_chunks, chunk_size
    {"cdb_ins_dml", 1, 8*MB, 1024, 1024, 1024, 512}
};
int cdb_shm_conf_size = 1;

bool
init_cdb_shm_mgr(const char* mysqld_data_path)
{
    char shmid_file_path[1024] = {0};
    sprintf(shmid_file_path, "%s/cdb_shm_id.txt", mysqld_data_path);
    ofstream shmid_of;
    shmid_of.open(shmid_file_path, std::ios::out | std::ios::trunc);
    if (!shmid_of.is_open()) {
        cdb_errno = CDB_SHM_INIT_SHMID_FILE_FAILED;
        return false;
    }

    CDBShmMgr& sm = CDBShmMgr::getInstance();

    for (int i=0; i<cdb_shm_conf_size; ++i) {
        CDBShmConf& c = cdb_shm_conf_array[i];
        key_t k = ftok(mysqld_data_path, c._id);
        if (!sm.reg(c._name, k, c._size)) {
            cdb_errno = CDB_SHM_INIT_REG_ERROR;
            return false;
        }

        CDBShm& s = sm.get(c._name);
        s._ca = new CacheAccess();
        int ret = s._ca->open((char*)s._addr, s._size, s._new, c._node_total, c._bucket_size, c._n_chunks, c._chunk_size);
        if (ret != 0) {
            cdb_errno = CDB_SHM_INIT_CA_OPEN_ERROR;
            cdb_2nd_errno = ret;
            return false;
        }

        shmid_of << c._name << " " << k << endl;
    }

    shmid_of.close();

    return true;
}

