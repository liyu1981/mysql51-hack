#include "cdb.h"

#include "cdb_error.h"
#include "cdb_shm_mgr.h"

#include <climits>
#include <fstream>
#include <sys/ipc.h>
#include <pthread.h>

using namespace std;
using namespace cdb;

#define MB (1024*1024)

int cdb_errno = CDB_FINE;
int cdb_2nd_errno = 0;

///////////////////////////////////////////////////////////////////////////////

string cdb_mysqld_data_path;

/* list all cdb shm conf below, id must start from 1.
 */
CDBShmConf cdb_shm_conf_array[] = {
    //name, id, size, node_total, bucket_size, n_chunks, chunk_size
    {"cdb_ins_dml_1", 1, 8*MB, 1024, 1024, 1024, 512},
    {"cdb_ins_dml_2", 2, 8*MB, 1024, 1024, 1024, 512}
};
int cdb_shm_conf_size = sizeof(cdb_shm_conf_array)/sizeof(cdb_shm_conf_array[0]);

CDBShmPairConf cdb_shm_pair_conf_array[] = {
    //name, shm_name1, shm_name2, conf_index, map_file
    {"cdb_ins_dml", "cdb_ins_dml_1", "cdb_ins_dml_2", 0, "cdb_ins_dml_map.txt"}
};
int cdb_shm_pair_conf_size = sizeof(cdb_shm_pair_conf_array)/sizeof(cdb_shm_pair_conf_array[0]);

map<string, CDBShmPair*> cdb_shm_pair_map;
pthread_t cdb_shm_pair_switch_thread;
const int cdb_shm_pair_switch_period = 60;

void*
cdb_shm_pair_switch_function(void* p)
{
    int ret = 0;
    while (true) {
        sleep(cdb_shm_pair_switch_period);

        for (map<string, CDBShmPair*>::iterator it = cdb_shm_pair_map.begin(); it != cdb_shm_pair_map.end(); ++it) {
            CDBShmPair& pair = *it->second;
            pair.switch_shm();
            ret = pair.flush_map_file(cdb_mysqld_data_path);
            if (ret != 0) {
                // TOFIX: Fatal Error, then?
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

bool
init_cdb_shm_mgr(const char* mysqld_data_path)
{
    int ret;

    cdb_mysqld_data_path = mysqld_data_path;

    // init shm mgr & lock section
    string shmid_file_path = cdb_mysqld_data_path + string("/cdb_shm_id.txt");
    ofstream shmid_of;
    shmid_of.open(shmid_file_path.c_str(), std::ios::out | std::ios::trunc);
    if (!shmid_of.is_open()) {
        cdb_errno = CDB_SHM_INIT_SHMID_FILE_FAILED;
        return false;
    }

    CDBShmMgr& sm = CDBShmMgr::getInstance();

    cdb_errno = sm.init(mysqld_data_path, cdb_shm_conf_size, shmid_of);
    if (cdb_errno != CDB_FINE)
        return false;
    CDBShm& shm_locks = sm.get(CDB_SHM_LOCKS_NAME);

    // init all shm sections
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

    // init shm pair map
    for (int i=0; i<cdb_shm_pair_conf_size; ++i) {
        CDBShmPairConf& c = cdb_shm_pair_conf_array[i];
        CDBShmPair* nsp = new CDBShmPair();
        nsp->_shm_conf = &cdb_shm_conf_array[c._conf_index];
        nsp->_pair_conf = &c;
        gettimeofday(&nsp->_last_switch_time, NULL);
        nsp->_current = &sm.get(c._shm_name1);
        nsp->_standby = &sm.get(c._shm_name2);
        cdb_shm_pair_map[c._name] = nsp;

        ret = nsp->flush_map_file(cdb_mysqld_data_path);
        if (ret != 0) {
            cdb_errno = ret;
            return false;
        }
    }

    // now start the switcher thread
    ret = pthread_create(&cdb_shm_pair_switch_thread, NULL, cdb_shm_pair_switch_function, NULL);
    if (ret !=0) {
        cdb_errno = CDB_SHM_INIT_SWITCH_THREAD_FAIL;
        cdb_2nd_errno = ret;
        return false;
    }

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

    // no need to free mem of cdb_shm_pair_map, left it to OS

    // need to wait for the switch thread finish?
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
        if(shm_locks._new)
            spin_lock_init(s._lock);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void
cdb_comm_stat_add(CDBCommStat& cs, double v, bool init)
{
    const double DBL_MAX =(double)LLONG_MAX;
    const double time_bucket_threshold[CDB_TIME_BUCKET_SIZE] = {
        0.02, 0.04, 0.06, 0.08, 0.1, 0.5, 1, 2, 10
    };

    if (init) {
        cs._total = 0;
        cs._time_sum = 0;
        cs._time_min = DBL_MAX;
        cs._time_max = 0;
        for (int i=0; i<CDB_TIME_BUCKET_SIZE; ++i)
            cs._time_bucket[i] = 0;
    }

    cs._total += 1;
    cs._time_sum += v;
    if (v < cs._time_min)
        cs._time_min = v;
    if (v > cs._time_max)
        cs._time_max = v;
    for (int i=0; i<CDB_TIME_BUCKET_SIZE; ++i) {
        if (v>time_bucket_threshold[i])
            cs._time_bucket[i] += 1;
    }
}

void cdb_ins_dml_op_add(CDBInsDmlOp& op, unsigned long long int begin_time, unsigned long long int end_time)
{
    double op_diff = (end_time - begin_time) * 0.000001;

    CDBShm& s = cdb_shm_pair_map["cdb_ins_dml"]->get_current();
    TfcShmMap<CDBInsDmlOpKey, CDBInsDmlOp> m;
    m._ca = s._ca;

    spin_lock(s._lock);

    int rv = m.find(op._key);
    if (rv > 0) {
        // not found
        cdb_comm_stat_add(op._comm_stat, op_diff, true);
        if (m.insert(op._key, op)>0) {
            // TOFIX: insert failed, then?
        }
    }
    else if (rv == 0) {
        // found, so update
        CDBInsDmlOp entry;
        m.get(op._key, entry);
        cdb_comm_stat_add(entry._comm_stat, op_diff);
        if (m.insert(entry._key, entry)>0) {
            // TOFIX: insert failed, then?
        }
    }
    else {
        // TOFIX: sth wrong, may be shm map corrupted?
    }

    spin_unlock(s._lock);
}
