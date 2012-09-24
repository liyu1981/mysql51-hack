#include "cdb_shm_mgr.h"
#include "cdb_error.h"

namespace cdb {

static const int C_DEFAULT_OPEN_FLAG = 0600;

CDBShmMgr* CDBShmMgr::_instance = NULL;

CDBShm wrong_shm = {"", 0, 0, 0, 0, false, 0};

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
CDBShmMgr::reg(const CDBShm& s)
{
    char* p = NULL;

    CDBShm* ns = new CDBShm();
    ns->_name = s._name;
    ns->_key = s._key;
    ns->_size = s._size;
    ns->_id = -1;
    ns->_addr = 0;
    ns->_new = false;

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

    if (ns->_new) {
        memset(ns->_addr, 0, ns->_size);
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
        shmdt(it->second->_addr);
    }
}

} // end of namespace cdb


