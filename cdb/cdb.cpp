/* vim: set ts=4 sw=4 tw=0: */
#include <climits>
#include <fstream>
#include <sstream>
#include <set>
#include <sys/ipc.h>

#include "tfc_base_config_file.h"
#include "tfc_base_str.h"
#include "cdb.h"
#include "cdb_error.h"

#define MYSQL_SERVER // define MYSQL_SERVER since we need opt_cdb_* variables
#include "../sql/mysql_priv.h" // must be included after system headers such as pthread.h, otherwise conflict with mysys

using namespace std;
using namespace cdb;
using namespace tfc::base;

#define MB (1024*1024)

int cdb_errno = CDB_FINE;
int cdb_2nd_errno = 0;

///////////////////////////////////////////////////////////////////////////////

string cdb_mysqld_data_path;

/* list all cdb shm conf below, id must start from 1.
 */
CDBShmConf cdb_shm_conf_array[] = {
    //name, id, size, node_total, bucket_size, n_chunks, chunk_size
    {"cdb_ins_dml_1",        1, 1*MB,  4096, 4096, 4096, 128},
    {"cdb_ins_dml_2",        2, 1*MB,  4096, 4096, 4096, 128},
    {"cdb_ins_conn_1",       3, 1*MB,  4096, 4096, 4096, 128},
    {"cdb_ins_conn_2",       4, 1*MB,  4096, 4096, 4096, 128},
    {"cdb_ins_client_dml_1", 5, 1*MB,  4096, 4096, 4096, 128},
    {"cdb_ins_client_dml_2", 6, 1*MB,  4096, 4096, 4096, 128},
    //{"cdb_tab_dml_1",        7, 64*MB, 100000, 100000, 100000, 512},
    //{"cdb_tab_dml_2",        8, 64*MB, 100000, 100000, 100000, 512}
    {"cdb_tab_dml_1",        7, 1*MB, 1024, 1024, 1024, 512},
    {"cdb_tab_dml_2",        8, 1*MB, 1024, 1024, 1024, 512}
};
int cdb_shm_conf_size = sizeof(cdb_shm_conf_array)/sizeof(cdb_shm_conf_array[0]);

CDBShmPairConf cdb_shm_pair_conf_array[] = {
    // conf_index must match the offset of item with name = shm_name1 in cdb_shm_conf_array
    //name, shm_name1, shm_name2, conf_index, map_file
    {"cdb_ins_dml", "cdb_ins_dml_1", "cdb_ins_dml_2", 0, "cdb_ins_dml_map.txt"},
    {"cdb_ins_conn", "cdb_ins_conn_1", "cdb_ins_conn_2", 2, "cdb_ins_conn_map.txt"},
    {"cdb_ins_client_dml", "cdb_ins_client_dml_1", "cdb_ins_client_dml_2", 4, "cdb_ins_client_dml_map.txt"},
    {"cdb_tab_dml", "cdb_tab_dml_1", "cdb_tab_dml_2", 6, "cdb_tab_dml_map.txt"}
};
int cdb_shm_pair_conf_size = sizeof(cdb_shm_pair_conf_array)/sizeof(cdb_shm_pair_conf_array[0]);

set<string> cdb_shm_names;
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
            ret = pair.flush_pair_info(cdb_mysqld_data_path);
            if (ret != 0) {
                // TOFIX: Fatal Error, then?
                sql_print_error("CDB: flush_pair_info %s failed %d", it->first.c_str(), ret);
            }
        }
    }
}

bool
cdb_init_config_file(const string& config_file)
{
    CFileConfig cfc;

    try{
        cfc.Init(config_file);
    }
    catch (conf_load_error ex) {
        sql_print_error("[cdb_init_config_file] CFileConfig::Init failed: %s, Use default config.", ex.what());
        /*return true, use default config*/
        return true;
    }

    int shm_num = from_str<int>(cfc["root\\shm\\shm_num"]);
    if(cdb_shm_conf_size != shm_num) {
        sql_print_error("[cdb_init_config_file] shm_num is invalid.");
        return false;
    }

    int pair_num = from_str<int>(cfc["root\\shm_pair\\pair_num"]);
    if(cdb_shm_pair_conf_size != pair_num) {
        sql_print_error("[cdb_init_config_file] pair_num is invalid.");
        return false;
    }

    for(int i = 0; i < cdb_shm_conf_size; ++i) {
        ostringstream ss;
        ss << "root\\shm\\shm" << (i+1) << "\\";
        string prefix = ss.str();
        CDBShmConf& c = cdb_shm_conf_array[i];
        c._name = cfc[prefix + "name"];
        c._id = from_str<int>(cfc[prefix + "id"]);
        c._size = from_str<unsigned>(cfc[prefix + "size"]);
        c._node_total = from_str<int>(cfc[prefix + "node_total"]);
        c._bucket_size = from_str<int>(cfc[prefix + "bucket_size"]);
        c._n_chunks = from_str<int>(cfc[prefix + "n_chunks"]);
        c._chunk_size = from_str<int>(cfc[prefix + "chunk_size"]);
        cdb_shm_names.insert(c._name);
    }

    for(int i = 0; i < cdb_shm_pair_conf_size; ++i) {
        ostringstream ss;
        ss << "root\\shm_pair\\pair" << (i+1) << "\\";
        string prefix = ss.str();
        CDBShmPairConf& c = cdb_shm_pair_conf_array[i];
        c._name = cfc[prefix + "name"];
        c._shm_name1 = cfc[prefix + "shm_name1"];
        c._shm_name2 = cfc[prefix + "shm_name2"];

        if((cdb_shm_names.find(c._shm_name1) == cdb_shm_names.end()) ||
           (cdb_shm_names.find(c._shm_name2) == cdb_shm_names.end()) ) {
            sql_print_error("shm_name: %s or %s is not exsit.", c._shm_name1.c_str(), c._shm_name2.c_str());
            return false;
        }

        c._conf_index = from_str<int>(cfc[prefix + "conf_index"]);
        c._map_file = cfc[prefix + "map_file"];
    }

    return true;
}

bool
cdb_check_shm_conf(CDBShmConf& c)
{
    long need_shm_size = CHashMap::get_total_pool_size(c._node_total, c._bucket_size, c._n_chunks, c._chunk_size);
    double ratio = ((double)need_shm_size)/c._size;
    if (ratio > 1.0) {
        sql_print_error("CDB: shm named %s is configured smaller size than needed(%d bytes, %d %%), check your shm conf!",
                        c._name.c_str(), (int)need_shm_size, (int)(ratio*100));
        return false;
    }
    else if (ratio < 0.8) {
        sql_print_warning("CDB: shm named %s is configured bigger size than needed(%d bytes, %d %%)!",
                          c._name.c_str(), (int)need_shm_size, (int)(ratio*100));
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool
cdb_init_shm_mgr(const char* mysqld_data_path)
{
    int ret;

    cdb_mysqld_data_path = mysqld_data_path;

    ret = cdb_init_config_file(cdb_mysqld_data_path + "/cdb.conf");
    if(ret == false)
        return false;

    // first check all shm confs
    for (int i=0; i<cdb_shm_conf_size; ++i) {
        CDBShmConf& c = cdb_shm_conf_array[i];
        if (!cdb_check_shm_conf(c)) {
            cdb_errno = CDB_SHM_INIT_CONF_ERROR;
            return false;
        }
    }

    // init shm mgr
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

    // init all shm sections
    for (int i=0; i<cdb_shm_conf_size; ++i) {
        CDBShmConf& c = cdb_shm_conf_array[i];

        key_t k = ftok(mysqld_data_path, c._id);
        if (!sm.reg(c._name.c_str(), k, c._size)) {
            cdb_errno = CDB_SHM_INIT_REG_ERROR;
            return false;
        }

        CDBShm& s = sm.get(c._name);
        s._ca = new CacheAccess();
        ret = s._ca->open((char*)s._data_addr, s._data_size, s._new, c._node_total, c._bucket_size, c._n_chunks, c._chunk_size);
        if (ret != 0) {
            cdb_errno = CDB_SHM_INIT_CA_OPEN_ERROR;
            cdb_2nd_errno = ret;
            return false;
        }

        s._lock = sm.get_lock(i);

        shmid_of << s._name << " " << s._key << " (lock " << i << ")" << endl;
        fprintf(stderr, "=== shm_name: %s, shm_key: %#x ===\n", s._name.c_str(), s._key);
        fflush(stderr);
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

        ret = nsp->flush_pair_info(cdb_mysqld_data_path);
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
cdb_shutdown_shm_mgr()
{
    CDBShmMgr& sm = CDBShmMgr::getInstance();
    sm.destroy_all_lock();

    // no need to free mem of cdb_shm_pair_map, left it to OS

    // need to wait for the switch thread finish?

    return true;
}

bool
cdb_attach_shm_mgr(const char* mysqld_data_path)
{
    int ret;

    CDBShmMgr& sm = CDBShmMgr::getInstance();

    cdb_errno = sm.attach(mysqld_data_path, cdb_shm_conf_size);
    if (cdb_errno != CDB_FINE)
        return false;

    for (int i=0; i<cdb_shm_conf_size; ++i) {
        CDBShmConf& c = cdb_shm_conf_array[i];
        key_t k = ftok(mysqld_data_path, c._id);
        if (!sm.reg(c._name.c_str(), k, c._size)) {
            cdb_errno = CDB_SHM_INIT_REG_ERROR;
            return false;
        }

        CDBShm& s = sm.get(c._name);
        s._ca = new CacheAccess();
        ret = s._ca->open((char*)s._data_addr, s._data_size, s._new, c._node_total, c._bucket_size, c._n_chunks, c._chunk_size);
        if (ret != 0) {
            cdb_errno = CDB_SHM_INIT_CA_OPEN_ERROR;
            cdb_2nd_errno = ret;
            return false;
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void
cdb_comm_stat_add(CDBCommStat& cs, double v, bool init)
{
    const double time_bucket_threshold[CDB_TIME_BUCKET_SIZE+1] = {
        0.02, 0.04, 0.06, 0.08, 0.1, 0.5, 1, 2, 10,
        DBL_MAX // placeholder
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
        if (v>time_bucket_threshold[i] && v<time_bucket_threshold[i+1]) {
            cs._time_bucket[i] += 1;
            break;
        }
    }
}

void
cdb_ins_dml_add(CDBInsDml& op, unsigned long long int begin_time, unsigned long long int end_time)
{
    if (opt_cdb_stat_ins_dml == 0) {
        sql_print_warning("CDB: opt_cdb_stat_ins_dml is off.");
        return;
    }

    double op_diff = (end_time - begin_time) * 0.000001;

    CDBShm& s = cdb_shm_pair_map["cdb_ins_dml"]->get_current();
    TfcShmMap<CDBInsDmlKey, CDBInsDml> m;
    m._ca = s._ca;

    pthread_mutex_lock(s._lock);

    int rv = m.find(op._key);
    if (rv > 0) {
        // not found
        cdb_comm_stat_add(op._comm_stat, op_diff, true);
        int r = m.insert(op._key, op);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_dml_add insert new item failed %d", r);
        }
    }
    else if (rv == 0) {
        // found, so update
        CDBInsDml entry;
        int r = m.get(op._key, entry);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_dml_add get item failed %d", r);
        }
        cdb_comm_stat_add(entry._comm_stat, op_diff);
        r = m.insert(entry._key, entry);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_dml_add update item failed %d", r);
        }
    }
    else {
        // TOFIX: sth wrong, may be shm map corrupted?
        sql_print_error("CDB: cdb_ins_dml_add find key failed %d", rv);
    }

    pthread_mutex_unlock(s._lock);
}

///////////////////////////////////////////////////////////////////////////////

void
cdb_ins_conn_add(CDBInsConn& conn, unsigned long long int begin_time, unsigned long long end_time)
{
    if (opt_cdb_stat_ins_conn == 0)
        return;

    double time_diff = (end_time - begin_time) * 0.000001;

    CDBShm& s = cdb_shm_pair_map["cdb_ins_conn"]->get_current();
    TfcShmMap<CDBInsConnKey, CDBInsConn> m;
    m._ca = s._ca;

    pthread_mutex_lock(s._lock);

    int rv = m.find(conn._key);
    if (rv > 0) {
        // not found
        cdb_comm_stat_add(conn._comm_stat, time_diff, true);
        int r = m.insert(conn._key, conn);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_conn_add insert new item failed %d", r);
        }
    }
    else if (rv == 0) {
        // found, so update
        CDBInsConn entry;
        int r = m.get(conn._key, entry);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_conn_add get item failed %d", r);
        }
        cdb_comm_stat_add(entry._comm_stat, time_diff);
        r = m.insert(entry._key, entry);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_conn_add update item failed %d", r);
        }
    }
    else {
        // TOFIX: sth wrong, may be shm map corrupted?
        sql_print_error("CDB: cdb_ins_conn_add find key failed %d", rv);
    }

    pthread_mutex_unlock(s._lock);
}

///////////////////////////////////////////////////////////////////////////////

void
cdb_ins_client_dml_add(CDBInsClientDml& op, unsigned long long int begin_time, unsigned long long int end_time)
{
    if (opt_cdb_stat_client_dml == 0)
        return;

    double op_diff = (end_time - begin_time) * 0.000001;

    CDBShm& s = cdb_shm_pair_map["cdb_ins_client_dml"]->get_current();
    TfcShmMap<CDBInsClientDmlKey, CDBInsClientDml> m;
    m._ca = s._ca;

    pthread_mutex_lock(s._lock);

    int rv = m.find(op._key);
    if (rv > 0) {
        // not found
        cdb_comm_stat_add(op._comm_stat, op_diff, true);
        int r = m.insert(op._key, op);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_client_dml_add insert new item failed %d", r);
        }
    }
    else if (rv == 0) {
        // found, so update
        CDBInsClientDml entry;
        int r = m.get(op._key, entry);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_client_dml_add get item failed %d", r);
        }
        cdb_comm_stat_add(entry._comm_stat, op_diff);
        r = m.insert(entry._key, entry);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_ins_client_dml_add update item failed %d", r);
        }
    }
    else {
        // TOFIX: sth wrong, may be shm map corrupted?
        sql_print_error("CDB: cdb_ins_client_dml_add find key failed %d", rv);
    }

    pthread_mutex_unlock(s._lock);
}

///////////////////////////////////////////////////////////////////////////////

/* CDBTabDml binary layout

   -----------------------------------------------------------------------------
   | _comm_stats | _result | name_count <name> <name> <name> ....
   -----------------------------------------------------------------------------
   ^             ^
   |             |
   buf start     key start here

   name is db string + table string. string is len(1byte) + data.
 */

int
CDBTabDml::marshal_size()
{
    int ret = sizeof(_comm_stats);
    ret += sizeof(_key._result);
    ret += sizeof(unsigned char); // one byte for name_count, so max 256 names
    for (size_t i=0; i<_key._names.size(); ++i) {
        ret += _key._names[i]._db.size()+1; // each string has 1byte for len.
        ret += _key._names[i]._table.size()+1;
    }
    return ret;
}

void
CDBTabDml::marshal_key(char* buf, int buf_size, char*& key_md5)
{
    char* p = buf + sizeof(_comm_stats);
    char* key_buf = p;
    int key_size = buf_size - sizeof(_comm_stats);

    *(int*)p = _key._result; p = p+sizeof(_key._result);

    *(unsigned char*)p = (unsigned char)_key._names.size(); p = p+1;
    for (size_t i=0; i<_key._names.size(); ++i) {
        string& s = _key._names[i]._db;
        int ss = s.size();
        *p = (char)ss; p = p+sizeof(char);
        memcpy(p, s.c_str(), ss); p = p+ss;

        s = _key._names[i]._table;
        ss = s.size();
        *p = (char)ss; p = p+sizeof(char);
        memcpy(p, s.c_str(), ss); p = p+ss;
    }

    key_md5 = md5_buf((unsigned char*)key_buf, key_size);
}

int
CDBTabDml::de_marshal_key(char* buf, int buf_size)
{
    char* p = buf + sizeof(_comm_stats);
    //int key_size = buf_size - sizeof(_comm_stats);

    _key._result = *(int*)p; p = p+sizeof(_key._result);

    _key._names.clear();
    int name_count = *(unsigned char*)p; p = p+1;
    for (int i=0; i<name_count; ++i) {
        CDBTabName tn;
        int ss = *(char*)p; p = p+sizeof(char);
        tn._db.assign(p, ss); p = p+ss;
        ss = *(char*)p; p = p+sizeof(char);
        tn._table.assign(p, ss); p = p+ss;
        _key._names.push_back(tn);
    }

    return CDB_FINE;
}

void
CDBTabDml::marshal_stats(char* buf, int buf_size)
{
    memcpy(buf, &_comm_stats[0], sizeof(_comm_stats));
}

int
CDBTabDml::de_marshal_stats(char* buf, int buf_size)
{
    memcpy(&_comm_stats[0], buf, sizeof(_comm_stats));
    return CDB_FINE;
}

int
CDBTabDml::de_marshal(char* buf, int buf_size)
{
    int rv = de_marshal_stats(buf, buf_size);
    if (rv != CDB_FINE)
        return rv;
    return de_marshal_key(buf, buf_size);
}

void
CDBTabDml::reset_stats()
{
    memset(&_comm_stats[0], 0, sizeof(_comm_stats));
}

void
cdb_tab_dml_add(CDBTabDml& op, int type, unsigned long long int begin_time, unsigned long long int end_time)
{
    if (opt_cdb_stat_table_dml == 0)
        return;

    if (type <0 || type >= CDB_OP_TYPE_SIZE) {
        sql_print_error("CDB: cdb_tab_dml_add wrong type %d, skip", type);
        return;
    }

    double op_diff = (end_time - begin_time) * 0.000001;

    CDBShm& s = cdb_shm_pair_map["cdb_tab_dml"]->get_current();

    char* md5 = NULL;
    int buf_size = op.marshal_size();
    char* buf = (char*)malloc(buf_size);

    pthread_mutex_lock(s._lock);

    // NOTICE: have to do this after lock since the md5 calc function inside marshal_key is not thread safe.
    op.marshal_key(buf, buf_size, md5);

    unsigned int dl; bool df; long ts;
    int rv = s._ca->get_key(md5, dl, df, ts);
    if (rv>0) {
        // not found
        op.reset_stats();
        cdb_comm_stat_add(op._comm_stats[type], op_diff, true);
        op.marshal_stats(buf, buf_size);
        int r = s._ca->set(md5, buf, buf_size);
        if (r>0) {
            // TOFIX:
            sql_print_error("CDB: cdb_tab_dml_add insert new item failed %d", r);
        }
    }
    else if (rv==0) {
        // found, so update
        int r = s._ca->get(md5, buf, buf_size, dl, df, ts);
        if (r>0) {
            // TOFIX:
            sql_print_error("CDB: cdb_tab_dml_add get item failed %d", r);
        }
        op.de_marshal_stats(buf, buf_size);
        cdb_comm_stat_add(op._comm_stats[type], op_diff);
        op.marshal_stats(buf, buf_size);
        r = s._ca->set(md5, buf, buf_size);
        if (r>0) {
            // TOFIX: insert failed, then?
            sql_print_error("CDB: cdb_tab_dml_add update item failed %d", r);
        }
    }
    else {
        // TOFIX: sth wrong, may be shm map corrupted?
        sql_print_error("CDB: cdb_tab_dml_add find key failed %d", rv);
    }

    pthread_mutex_unlock(s._lock);
    free(buf);
}

