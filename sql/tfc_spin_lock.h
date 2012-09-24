#ifndef _TFC_SPIN_LOCK_H_
#define _TFC_SPIN_LOCK_H_

#define LOCK "lock ; "

#define __raw_spin_lock_string \
        "\n1:\t" \
        LOCK \
        "decb %0\n\t" \
        "jns 3f\n" \
        "2:\t" \
        "rep;nop\n\t" \
        "cmpb $0,%0\n\t" \
        "jle 2b\n\t" \
        "jmp 1b\n" \
        "3:\n\t"

typedef struct {
        volatile unsigned int slock;
} raw_spinlock_t;

typedef struct {
        raw_spinlock_t raw_lock;
} spinlock_t;

static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
        __asm__ __volatile__(
                __raw_spin_lock_string
                :"+m" (lock->slock) : : "memory");
}


#define __raw_spin_unlock_string \
        "movb $1,%0" \
                :"+m" (lock->slock) : : "memory"

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
        __asm__ __volatile__(
                __raw_spin_unlock_string
        );
}

#define spin_lock_init(lock)    do {(lock)->raw_lock.slock = 1; } while (0)
#define spin_lock(lock) __raw_spin_lock(&(lock)->raw_lock)
#define spin_unlock(lock)  __raw_spin_unlock(&(lock)->raw_lock)

#endif

