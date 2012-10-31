#ifndef _TFC_SHM_MAP_WRAP_H_
#define _TFC_SHM_MAP_WRAP_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "tfc_cache_access.h"
#include "tfc_md5.h"
#include "tfc_spin_lock.h"
#include "MurmurHash3.h"

#define KEY_SIZE 16
#define MURMURHASH_SEED 17

namespace tfc {

using namespace std;
using namespace tfc::cache;
using namespace tfc::md5;

struct TfcShmMapLock
{
    static void lock(spinlock_t* l) { spin_lock(l); }
    static void unlock(spinlock_t* l) { spin_unlock(l); }
};

struct TfcShmMapNoLock
{
    static void lock(spinlock_t* l) {}
    static void unlock(spinlock_t* l) {}
};

template<class VT, class LockPolicy>
class TfcShmMapBase
{
public:
	int base_find(char* md5key)
	{
        LockPolicy::lock(_lock);
		unsigned int dl; bool df; long ts;
		int rv = _ca->get_key(md5key, dl, df, ts);
        LockPolicy::unlock(_lock);
		return rv;
    }

    VT base_get(char* md5key)
	{
        LockPolicy::lock(_lock);
		VT value;
		unsigned int dl; bool df; long ts;
		_ca->get(md5key, (char*)&value, sizeof(VT), dl, df, ts);
        LockPolicy::unlock(_lock);
		return value;
	}

	int base_get(char* md5key, VT& value)
	{
        LockPolicy::lock(_lock);
		unsigned int dl; bool df; long ts;
		int rv = _ca->get(md5key, (char*)&value, sizeof(VT), dl, df, ts);
        LockPolicy::unlock(_lock);
		return rv;
	}

	int base_insert(char* md5key, const VT value)
	{
        LockPolicy::lock(_lock);
		int rv = _ca->set(md5key, (char*)&value, sizeof(VT));
        LockPolicy::unlock(_lock);
        return rv;
	}

	int base_erase(char* md5key)
	{
        LockPolicy::lock(_lock);
		int rv = _ca->del(md5key);
        LockPolicy::unlock(_lock);
		return rv;
	}

    size_t size()
    {
        if (_ca == NULL)
            return 0;
        unsigned hu, ht, cu, ct;
        _ca->get_node_num(hu, ht, cu, ct);
        return hu;
    }

	CacheAccess* _ca;
	spinlock_t* _lock;
};

template<class T>
struct TfcShmMapHashMd5
{
	static void get_hash_key(const T& key, char* hash_key)
	{
        md5_buf_safe((unsigned char*)&key, sizeof(T), hash_key);
	}
};

template<class T>
struct TfcShmMapHashMurmur
{
    static void get_hash_key(const T& key, char* hash_key)
    {
        MurmurHash3_x64_128((const void*)&key, sizeof(T), MURMURHASH_SEED, (void*)hash_key);
    }
};

template<class VT>
struct TfcShmMapIterator
{
	TfcShmMapIterator()
	{
		_ca = NULL;
		memset(first_key, 0, KEY_SIZE);
		memset(cur_key, 0, KEY_SIZE);
		cur_data_flag = -1;
		memset(&cur_data, 0, sizeof(VT));
	}

	~TfcShmMapIterator() {}
	TfcShmMapIterator(const TfcShmMapIterator& other) { *this = other; }

	TfcShmMapIterator& operator=(const TfcShmMapIterator& other)
	{
		this->_ca = other._ca;
		this->cur_data_flag = other.cur_data_flag;
		memcpy(first_key, other.first_key, KEY_SIZE);
		memcpy(cur_key, other.cur_key, KEY_SIZE);
		this->cur_data = other.cur_data;
		return *this;
	}

	bool operator==(const TfcShmMapIterator& other)
	{
		if (this->_ca != other._ca)
			return false;

		if (this->_ca == NULL && other._ca == NULL)
			return true;

		int rv2 = memcmp(cur_key, other.cur_key, KEY_SIZE);
		return rv2 == 0;
	}

	bool operator!=(const TfcShmMapIterator& other) { return !(*this == other); }

	void init_begin(CacheAccess* ca)
	{
		_ca = ca;
		unsigned dl; bool df; long ts;
		if (_ca != NULL)
		{
			cur_data_flag = _ca->oldest_key(first_key, dl, df, ts);
			if (cur_data_flag != 0)
			{
				_ca = NULL;
			}
			else
			{
				memcpy(cur_key, first_key, KEY_SIZE);
				cur_data_flag = _ca->get(cur_key, (char*)&cur_data, sizeof(VT), dl, df, ts);
			}
		}
	}

	TfcShmMapIterator& operator++(int i)
	{
		unsigned dl; bool df; long ts;
		if (_ca != NULL)
		{
			cur_data_flag = _ca->oldest_key(cur_key, dl, df, ts);
			if (cur_data_flag == 0)
			{
				if (memcmp(first_key, cur_key, KEY_SIZE) == 0)
				{
					_ca = NULL;
				}
				else
				{
					cur_data_flag = _ca->get(cur_key, (char*)&cur_data, sizeof(VT), dl, df, ts);
				}
			}
			else
			{
				_ca = NULL;
			}
		}
		return *this;
	}

	int extract(VT& dest)
	{
		if (cur_data_flag == 0)
		{
			dest = cur_data;
		}
		return cur_data_flag;
	}

	CacheAccess* _ca;
	char first_key[KEY_SIZE];
	char cur_key[KEY_SIZE];
	VT cur_data;
	int cur_data_flag;
};

template<class PT>
struct TfcShmMapFreeLater
{
    TfcShmMapFreeLater(PT* p): _p(p) {}
    ~TfcShmMapFreeLater() { free(_p); }
    PT* _p;
};

template<class KT, class VT,
         class LockPolicy = TfcShmMapNoLock,
         class HashPolicy = TfcShmMapHashMurmur<KT> >
class TfcShmMap
	: public TfcShmMapBase<VT, LockPolicy>
{
public:
    void cache_key(const KT key)
    {
        HashPolicy::get_hash_key(key, _hash_key);
    }

    void set_cache_key(char* hash_key)
    {
        memcpy(_hash_key, hash_key, KEY_SIZE);
    }

    int find()
    {
        return TfcShmMapBase<VT, LockPolicy>::base_find(_hash_key);
    }

	int find(const KT key)
    {
        cache_key(key);
        return find();
    }

    VT get()
    {
        return TfcShmMapBase<VT, LockPolicy>::base_get(_hash_key);
    }

    VT get(const KT key)
    {
        cache_key(key);
        return get();
    }

    int get(VT& value)
    {
        return TfcShmMapBase<VT, LockPolicy>::base_get(_hash_key, value);
    }

    int get(const KT key, VT& value)
    {
        cache_key(key);
        return get(value);
    }

    int insert(const VT value)
    {
        return base_insert(_hash_key, value);
    }

	int insert(const KT key, const VT value )
    {
        cache_key(key);
        return insert(value);
    }

    int erase()
    {
        return TfcShmMapBase<VT, LockPolicy>::base_erase(_hash_key);
    }

	int erase(const KT key)
    {
        cache_key(key);
        return erase();
    }

    char _hash_key[KEY_SIZE];

	typedef TfcShmMapIterator<VT> iterator;
    iterator begin() { _begin.init_begin(TfcShmMapBase<VT, LockPolicy>::_ca); return _begin; }
    iterator end() { _end._ca = NULL; return _end; }
	iterator _begin;
	iterator _end;
};

} // end of namespace tfc

#endif

