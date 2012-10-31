#ifndef _TFC_SHM_MAP_WRAP_H_
#define _TFC_SHM_MAP_WRAP_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "tfc_cache_access.h"
#include "tfc_md5.h"
#include "tfc_spin_lock.h"

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

#define KEY_SIZE 16

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
struct TfcShmMapKey
{
	static char* get_md5key(const T& key)
	{
        char* s =  md5_buf((unsigned char*)&key, sizeof(T));
        return s;
	}
};

template<>
struct TfcShmMapKey<string>
{
	static char* get_md5key(const string& key)
	{
		const char* rawkey = key.c_str();
        char* s =  md5_buf((unsigned char*)&rawkey, key.size());
        return s;
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

struct TfcShmMapFreeLater
{
    TfcShmMapFreeLater(char* p): _p(p) {}
    ~TfcShmMapFreeLater() { free(_p); }
    char* _p;
};

template<class KT, class VT, class LockPolicy = TfcShmMapNoLock>
class TfcShmMap
	: public TfcShmMapBase<VT, LockPolicy>
{
public:
	int find(const KT key)
    {
        char* md5 = TfcShmMapKey<KT>::get_md5key(key);
        TfcShmMapFreeLater __fl(md5);
        return TfcShmMapBase<VT, LockPolicy>::base_find(md5);
    }

    VT get(const KT key)
    {
        char* md5 = TfcShmMapKey<KT>::get_md5key(key);
        TfcShmMapFreeLater __fl(md5);
        return TfcShmMapBase<VT, LockPolicy>::base_get(md5);
    }

    int get(const KT key, VT& value)
    {
        char* md5 = TfcShmMapKey<KT>::get_md5key(key);
        TfcShmMapFreeLater __fl(md5);
        return TfcShmMapBase<VT, LockPolicy>::base_get(md5, value);
    }

	int insert(const KT key, const VT value )
    {
        char* md5 = TfcShmMapKey<KT>::get_md5key(key);
        TfcShmMapFreeLater __fl(md5);
        return base_insert(md5, value);
    }

	int erase(const KT key)
    {
        char* md5 = TfcShmMapKey<KT>::get_md5key(key);
        TfcShmMapFreeLater __fl(md5);
        return TfcShmMapBase<VT, LockPolicy>::base_erase(md5);
    }

	typedef TfcShmMapIterator<VT> iterator;
    iterator begin() { _begin.init_begin(TfcShmMapBase<VT, LockPolicy>::_ca); return _begin; }
    iterator end() { _end._ca = NULL; return _end; }
	iterator _begin;
	iterator _end;
};

} // end of namespace tfc

#endif

