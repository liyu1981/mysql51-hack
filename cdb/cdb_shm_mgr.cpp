#include "cdb_shm_mgr.h"
#include "cdb_error.h"
#include "tfc_spin_lock.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

namespace cdb {

static const int C_DEFAULT_OPEN_FLAG = 0600;

CDBShmMgr* CDBShmMgr::_instance = NULL;

CDBShm wrong_shm = {"", 0, 0, 0, 0, false, 0, 0, 0, 0, 0};

CDBShmMgr::CDBShmMgr()
{
}

CDBShmMgr&
CDBShmMgr::getInstance()
{
    if (NULL == _instance) {
        _instance = new CDBShmMgr();
    }
    return *_instance;
}

int
CDBShmMgr::init(const char* mysqld_data_path, int shm_conf_size, ofstream& shmid_of)
{
    size_t size = sizeof(spinlock_t)*shm_conf_size;
    key_t key = ftok(mysqld_data_path, 0);
    if (!reg(CDB_SHM_LOCKS_NAME, key, size, false)) {
        return CDB_SHM_INIT_SHMLOCKS_ERROR;
    }
    CDBShm& sm = get(CDB_SHM_LOCKS_NAME);
    if (!sm._new) {
        return CDB_SHM_INIT_SHMLOCKS_STALE;
    }
    shmid_of << CDB_SHM_LOCKS_NAME << " " << key << endl;
    return CDB_FINE;
}

int
CDBShmMgr::attach(const char* mysqld_data_path, int shm_conf_size)
{
    size_t size = sizeof(spinlock_t)*shm_conf_size;
    key_t key = ftok(mysqld_data_path, 0);
    if (!reg(CDB_SHM_LOCKS_NAME, key, size, false)) {
        return CDB_SHM_INIT_SHMLOCKS_ERROR;
    }
    return CDB_FINE;
}

CDBShm&
CDBShmMgr::get(const char* c)
{
    string s(c);
    map<string, CDBShm*>::iterator it = _shm_map.find(s);
    if (it == _shm_map.end())
        return wrong_shm;
    return *it->second;
}

bool
CDBShmMgr::reg(const CDBShm& s, bool has_meta)
{
    char* p = NULL;

    CDBShm* ns = new CDBShm();
    ns->_name = s._name;
    ns->_key = s._key;
    ns->_size = s._size;
    ns->_id = -1;
    ns->_addr = 0;
    ns->_new = false;
    ns->_ca = NULL;
    ns->_lock = NULL;
    ns->_data_addr = NULL;
    ns->_data_size = ns->_size;
    ns->_meta_addr = NULL;

    if (has_meta) {
        ns->_size += CDB_SHM_META_MAX;
    }

    int id = shmget(ns->_key, ns->_size, C_DEFAULT_OPEN_FLAG);
    if (id < 0) {
        id = shmget(ns->_key, ns->_size, C_DEFAULT_OPEN_FLAG | IPC_CREAT | IPC_EXCL);
        ns->_new = true;
        if (id < 0)
            goto error_return;
    }

    ns->_id = id;
    p = (char*) shmat(ns->_id, NULL, 0);
    if (ssize_t(p) == -1)
        goto error_return;

    ns->_addr = (void*)p;
    if (ns->_new)
        memset(ns->_addr, 0, ns->_size);

    if (has_meta) {
        ns->_meta_addr = (void*)ns->_addr;
        ns->_data_addr = (void*)((char*)ns->_addr+CDB_SHM_META_MAX);
    }
    else {
        ns->_meta_addr = NULL;
        ns->_data_addr = ns->_addr;
    }

    _shm_map[ns->_name] = ns;

    return true;

error_return:
    delete ns;
    return false;
}

void
CDBShmMgr::detach_all()
{
    for (map<string, CDBShm*>::iterator it = _shm_map.begin(); it != _shm_map.end(); ++it) {
        if (it->second->_addr != 0)
            shmdt(it->second->_addr);
    }
}

void
CDBShmPair::switch_shm()
{
    // 1st: reset standby. standby is readonly, no need to lock for reset
    memset(_standby->_data_addr, 0, _standby->_data_size);
    delete _standby->_ca;
    _standby->_ca = new CacheAccess();
    int ret = _standby->_ca->open((char*)_standby->_data_addr, _standby->_data_size, true,
                                  _shm_conf->_node_total, _shm_conf->_bucket_size, _shm_conf->_n_chunks, _shm_conf->_chunk_size);
    if (ret != 0) {
        // TOFIX: Fatal error, must exit!
    }

    // 2nd: lock _current and _standby, wait for outstand write
    spin_lock(_current->_lock);
    spin_lock(_standby->_lock);

    // 3rd: switch pointers
    CDBShm* tmp = _current;
    _current = _standby;
    _standby = tmp;

    // 4th: release locks
    spin_unlock(_standby->_lock);
    spin_unlock(_current->_lock);
}

CDBShm&
CDBShmPair::get_current()
{
    return *_current;
}

int
CDBShmPair::flush_pair_info(const string& dir_path)
{
    int ret;
    ret = flush_pair_meta_data(dir_path);
    if (ret != CDB_FINE)
        return ret;
    ret = flush_pair_map_file(dir_path);
    if (ret != CDB_FINE)
        return ret;
    return CDB_FINE;
}

int
CDBShmPair::flush_pair_meta_data(const string& dir_path)
{
    //TOFIX: current this method does not check boundary
    //       make sure that size of all data will not exceed CDB_SHM_META_MAX bytes

    char* cp = (char*)_current->_meta_addr;
    char* sp = (char*)_standby->_meta_addr;

    // 1: last switch time
    memcpy(cp, &_last_switch_time.tv_sec, sizeof(_last_switch_time.tv_sec));
    cp = cp + sizeof(_last_switch_time.tv_sec);
    memcpy(cp, &_last_switch_time.tv_usec, sizeof(_last_switch_time.tv_usec));
    cp = cp + sizeof(_last_switch_time.tv_usec);
    memcpy(sp, &_last_switch_time.tv_sec, sizeof(_last_switch_time.tv_sec));
    sp = sp + sizeof(_last_switch_time.tv_sec);
    memcpy(sp, &_last_switch_time.tv_usec, sizeof(_last_switch_time.tv_usec));
    sp = sp + sizeof(_last_switch_time.tv_usec);

    // 2: is current flag
    *cp = (char)1;
    cp = cp + 1;
    *sp = (char)0;
    sp = sp + 1;

    // 3: shm name
    *(size_t*)cp = _current->_name.size();
    cp = cp + sizeof(size_t);
    memcpy(cp, _current->_name.c_str(), _current->_name.size());
    cp = cp + _current->_name.size();
    *(size_t*)sp = _standby->_name.size();
    sp = sp + sizeof(size_t);
    memcpy(sp, _standby->_name.c_str(), _standby->_name.size());
    sp = sp + _standby->_name.size();

    // 4: dir path
    *(size_t*)cp = dir_path.size();
    cp = cp + sizeof(size_t);
    memcpy(cp, dir_path.c_str(), dir_path.size());
    cp = cp + dir_path.size();
    *(size_t*)sp = dir_path.size();
    sp = sp + sizeof(size_t);
    memcpy(sp, dir_path.c_str(), dir_path.size());
    sp = sp + dir_path.size();

    return CDB_FINE;
}

int
CDBShmPair::flush_pair_map_file(const string& dir_path)
{
    string pair_map_file_path = dir_path + string("/") + string(_pair_conf->_map_file);
    ofstream of;
    of.open(pair_map_file_path.c_str(), std::ios::out | std::ios::trunc);
    if (!of.is_open()) {
        return CDB_SHM_PAIR_MAP_OPEN_FAILED;
    }

    of << "last_switch_time: " << _last_switch_time.tv_sec << " " << _last_switch_time.tv_usec << " "
       << "current: " << _current->_name << " " << _current->_key << " "
       << "standby: " << _standby->_name << " " << _standby->_key << " "
       << endl;

    of.close();
    return CDB_FINE;
}

} // end of namespace cdb


