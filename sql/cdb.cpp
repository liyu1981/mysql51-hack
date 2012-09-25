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

/* list all cdb shm conf below, id must start from 1.
 */
CDBShmConf cdb_shm_conf_array[] = {
    //name, id, size, node_total, bucket_size, n_chunks, chunk_size
    {"cdb_ins_dml", 1, 8*MB, 1024, 1024, 1024, 512}
};
int cdb_shm_conf_size = 1;

bool
init_cdb_shm_mgr(const char* mysqld_data_path)
{
    int ret;
    char shmid_file_path[1024] = {0};
    sprintf(shmid_file_path, "%s/cdb_shm_id.txt", mysqld_data_path);
    ofstream shmid_of;
    shmid_of.open(shmid_file_path, std::ios::out | std::ios::trunc);
    if (!shmid_of.is_open()) {
        cdb_errno = CDB_SHM_INIT_SHMID_FILE_FAILED;
        return false;
    }

    CDBShmMgr& sm = CDBShmMgr::getInstance();

    cdb_errno = sm.init(mysqld_data_path, cdb_shm_conf_size, shmid_of);
    if (cdb_errno != CDB_FINE)
        return false;
    CDBShm& shm_locks = sm.get(CDB_SHM_LOCKS_NAME);

    for (int i=0; i<cdb_shm_conf_size; ++i) {
        CDBShmConf& c = cdb_shm_conf_array[i];
        key_t k = ftok(mysqld_data_path, c._id);
        if (!sm.reg(c._name, k, c._size)) {
            cdb_errno = CDB_SHM_INIT_REG_ERROR;
            return false;
        }

        CDBShm& s = sm.get(c._name);
        s._ca = new CacheAccess();
        ret = s._ca->open((char*)s._addr, s._size, s._new, c._node_total, c._bucket_size, c._n_chunks, c._chunk_size);
        if (ret != 0) {
            cdb_errno = CDB_SHM_INIT_CA_OPEN_ERROR;
            cdb_2nd_errno = ret;
            return false;
        }

        s._lock = (spinlock_t*)shm_locks._addr+i;
        spin_lock_init(s._lock);

        shmid_of << s._name << " " << s._key << " (lock " << i << ")" << endl;
    }

    shmid_of.close();

    return true;
}

bool
shutdown_cdb_shm_mgr()
{
    CDBShmMgr& sm = CDBShmMgr::getInstance();
    CDBShm& shm_locks = sm.get(CDB_SHM_LOCKS_NAME);
    if (shm_locks._addr == 0)
        return false;
    else
        return shmctl(shm_locks._id, IPC_RMID, NULL) == 0;
}

bool
attach_cdb_shm_mgr(const char* mysqld_data_path)
{
    int ret;

    CDBShmMgr& sm = CDBShmMgr::getInstance();

    cdb_errno = sm.attach(mysqld_data_path, cdb_shm_conf_size);
    if (cdb_errno != CDB_FINE)
        return false;
    CDBShm& shm_locks = sm.get(CDB_SHM_LOCKS_NAME);

    for (int i=0; i<cdb_shm_conf_size; ++i) {
        CDBShmConf& c = cdb_shm_conf_array[i];
        key_t k = ftok(mysqld_data_path, c._id);
        if (!sm.reg(c._name, k, c._size)) {
            cdb_errno = CDB_SHM_INIT_REG_ERROR;
            return false;
        }

        CDBShm& s = sm.get(c._name);
        s._ca = new CacheAccess();
        ret = s._ca->open((char*)s._addr, s._size, s._new, c._node_total, c._bucket_size, c._n_chunks, c._chunk_size);
        if (ret != 0) {
            cdb_errno = CDB_SHM_INIT_CA_OPEN_ERROR;
            cdb_2nd_errno = ret;
            return false;
        }

        s._lock = (spinlock_t*)shm_locks._addr+i;
    }

    return true;
}

double
cdb_calc_time_diff(CDBInsDmlOpJunk& op_junk)
{
    double b = op_junk._begin_tval.tv_sec*1.0+op_junk._begin_tval.tv_usec*0.000001;
    double e = op_junk._end_tval.tv_sec*1.0+op_junk._end_tval.tv_usec*0.000001;
    return e-b;
}

void
cdb_ins_dml_begin(CDBInsDmlOp& op, CDBInsDmlOpJunk& op_junk)
{
    gettimeofday(&op_junk._begin_tval, NULL);
}

void
cdb_ins_dml_end(CDBInsDmlOp& op, CDBInsDmlOpJunk& op_junk)
{
    gettimeofday(&op_junk._end_tval, NULL);
    double op_diff = cdb_calc_time_diff(op_junk);

    CDBShm& s = CDBShmMgr::getInstance().get("cdb_ins_dml");
    TfcShmMap<CDBInsDmlOpKey, CDBInsDmlOp> m;
    m._ca = s._ca;

    spin_lock(s._lock);

    int rv = m.find(op._key);
    if (rv > 0) {
        // not found
        op._total = 1;
        op._time_sum = op_diff;
        op._time_min = op_diff;
        op._time_max = op_diff;
        m.insert(op._key, op);
    }
    else if (rv == 0) {
        // found, so update
        CDBInsDmlOp entry;
        m.get(op._key, entry);
        entry._total += 1;
        entry._time_sum += op_diff;
        if (op_diff < entry._time_min)
            entry._time_min = op_diff;
        if (op_diff > entry._time_max)
            entry._time_max = op_diff;
        m.insert(entry._key, entry);
    }
    else {
        // TOFIX: sth wrong, may be shm map corrupted?
    }

    spin_unlock(s._lock);
}

