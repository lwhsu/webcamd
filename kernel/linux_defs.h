/*-
 * Copyright (c) 2009 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINUX_DEFS_H_
#define	_LINUX_DEFS_H_

/*
 * Stripped down version of the Linux defines
 */

#define	__nop do {} while (0)
#define	__user
#define	__kernel
#define	__safe
#define	__force
#define	__nocast
#define	__iomem
#define	__must_check
#define	__chk_user_ptr(x) __nop
#define	__chk_io_ptr(x) __nop
#define	__builtin_warning(x, ...) (1)
#define	__acquires(x)
#define	__releases(x)
#define	__acquire(x) __nop
#define	__release(x) __nop
#define	__cond_lock(x,c) (c)
#define	__pgprot(x)     ((pgprot_t){(x)})
#define	SetPageReserved(...)   __nop
#define	ClearPageReserved(...) __nop
#define	_PAGE_PRESENT   0
#define	_PAGE_RW        0
#define	_PAGE_USER      0
#define	_PAGE_ACCESSED  0
#define	PAGE_SHARED 0
#define	PAGE_ALIGN(addr)        (((addr)+PAGE_SIZE-1)&(~(PAGE_SIZE - 1)))
#define	PAGE_OFFSET	0
#define	__pa(x)                 ((unsigned long)(x)-PAGE_OFFSET)
#define	S_IRUGO		0
#define	offsetof(t, m) ((uint8_t *)&((t *)0)->m - (uint8_t *)0)
#define	container_of(ptr, t, m) ((t *)((uint8_t *)(ptr) - offsetof(t, m)))
#define	__devinitdata
#define	__init
#define	__exit
#define	__stringify(x)	#x
#define	ERESTARTSYS     512
#define	ENOIOCTLCMD     513
#define	EMEDIUMTYPE	514
#define	ENODATA		515
#define	EXPORT_SYMBOL(...)
#define	EXPORT_SYMBOL_GPL(...)
#define	MODULE_AUTHOR(...)
#define	MODULE_DESCRIPTION(...)
#define	MODULE_LICENSE(...)
#define	MODULE_DEVICE_TABLE(...)
#define	MODULE_PARM_DESC(...)
#define	MODULE_VERSION(...)
#define	MODULE_ALIAS(...)
#define	THIS_MODULE (NULL)
#define	module_param(...)
#define	info(...) __nop
#define	printk(...) __nop
#define	pr_err(...) __nop
#define	dev_dbg(...) __nop
#define	dev_err(...) __nop
#define	warn printk
#define	dbg printk
#define	err printk
#define	kmalloc(s,opt) malloc(s)
#define	kzalloc(s,opt) calloc(1, s)
#define	dma_alloc_coherent(d,s,h,g) calloc(1, s)
#define	dma_free_coherent(d,s,v,h) free(v)
#define	vmalloc_32_user(s) malloc(s)
#define	vmalloc_user(s) malloc(s)
#define	vmalloc_32(s) malloc(s)
#define	vmalloc_to_page(x) ((struct page *)(x))	/* HACK */
#define	vfree(ptr) free(ptr)
#define	kfree(ptr) free(ptr)
#define	udelay(d) usleep(d)
#define	msleep(d) usleep((d) * 1000)
#define	msleep_interruptible(d) usleep((d) * 1000)
#define	mdelay(d) usleep((d) * 1000)
#define	__GFP_WAIT 0
#define	__GFP_HIGH 0
#define	__GFP_IO 0
#define	__GFP_FS 0
#define	__GFP_COLD 0
#define	__GFP_NOWARN 0
#define	__GFP_REPEAT 0
#define	__GFP_NOFAIL 0
#define	__GFP_NORETRY 0
#define	__GFP_NO_GROW 0
#define	__GFP_COMP 0
#define	__GFP_ZERO 0
#define	__GFP_NOMEMALLOC 0
#define	__GFP_HARDWALL 0
#define	GFP_NOWAIT 0
#define	GFP_ATOMIC 0
#define	GFP_NOIO 0
#define	GFP_NOFS 0
#define	GFP_KERNEL 0
#define	GFP_USER 0
#define	GFP_HIGHUSER 0
#define	TASK_RUNNING            0
#define	TASK_INTERRUPTIBLE      1
#define	TASK_UNINTERRUPTIBLE    2
#define	TASK_STOPPED            4
#define	TASK_TRACED             8
#define	EXIT_ZOMBIE             16
#define	EXIT_DEAD               32
#define	TASK_NONINTERACTIVE     64
#define	no_llseek	NULL
#define	BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG))
#define	BIT_WORD(nr) ((nr) / BITS_PER_LONG)
#define	BITS_PER_BYTE 8
#define	BITS_PER_LONG (sizeof(long) * BITS_PER_BYTE)
#define	BIT(n) (1UL << (n))
#define	KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define	LINUX_VERSION_CODE KERNEL_VERSION(2, 6, 29)
#define	BUS_ID_SIZE 32
#define	DECLARE_BITMAP(n, max) unsigned long n[((max)+BITS_PER_LONG-1)/BITS_PER_LONG]
#define	MKDEV(maj,min) (min)
#define	dev_set_name(d, ...) snprintf((d)->name, sizeof((d)->name), __VA_ARGS__)
#define	DEFAULT_POLLMASK POLLNVAL
#define	_IOC_TYPE(cmd) IOCGROUP(cmd)
#define	_IOC_SIZE(cmd) IOCPARM_LEN(cmd)
#define	_IOC_NR(cmd) ((cmd) & 0xFF)
#define	_IOC_DIR(cmd) ((cmd) & IOC_DIRMASK)
#define	_IOC_NONE IOC_VOID
#define	_IOC_READ IOC_OUT
#define	_IOC_WRITE IOC_IN
#define	__OLD_VIDIOC_
#define	PAGE_SIZE 4096
#define	PAGE_SHIFT 12
#define	down_read(...) __nop
#define	up_read(...) __nop
#define	VM_WRITE 0x0001
#define	VM_READ 0x0002
#define	VM_SHARED 0x0004
#define	VM_DONTEXPAND 0x0008
#define	VM_FAULT_OOM 0x0010
#define	VM_RESERVED 0x0020
#define	VM_IO 0x0040
#define	DMA_FROM_DEVICE 0x01
#define	DMA_TO_DEVICE 0x02
#define	module_param_named(...)
#define	ARRAY_SIZE(ptr) (sizeof(ptr) / sizeof((ptr)[0]))
#define	__KERNEL__
#define	capable(...) 1
#define	uninitialized_var(...) __VA_ARGS__
#define	false 0
#define	true 1
#define	HZ 1000
#define	jiffies get_jiffies_64()
#define	msecs_to_jiffies(x) (x)
#define	jiffies_to_msecs(x) (x)
#define	unlikely(...) __VA_ARGS__
#define	DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define	min(a,b) (((a) < (b)) ? (a) : (b))
#define	max(a,b) (((a) > (b)) ? (a) : (b))
#define	prefetch(x) (void)x
#define	BUG(...) __nop
#define	BUG_ON(...) __nop
#define	WARN_ON(...) __nop
#define	spin_lock_init(lock) __nop
#define	spin_lock_irqsave(l,f)  do { (f) = 1; atomic_lock(); } while (0)
#define	spin_unlock_irqrestore(l,f) do { if (f) { (f) = 0; atomic_unlock(); } } while (0)
#define	spin_lock(...)  atomic_lock()
#define	spin_unlock(...) atomic_unlock()
#define	spin_lock_irq(...)  atomic_lock()
#define	spin_unlock_irq(...) atomic_unlock()
#define	atomic_inc_return atomic_inc
#define	atomic_dec_return atomic_dec
#define	IS_ERR_VALUE(x) ((x) >= (unsigned long)-(1<<14))
#define	find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#define	find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)
#define	signal_pending(...) 0
#define	down_write(...) __nop
#define	down_read(...) __nop
#define	up_write(...) __nop
#define	up_read(...) __nop
#define	DECLARE_RWSEM(x) int x
#define	crc32(s, d, l) crc32_le(s, (unsigned char const *)d, l)
#define	CRCPOLY_LE 0xedb88320
#define	CRCPOLY_BE 0x04c11db7
#define	mb() __asm volatile("":::"memory")
#define	fops_get(x) (x)
#define	fops_put(x) __nop
#define	current NULL
#define	dma_sync_single_for_cpu(...) __nop
#define	pgprot_noncached(x) (x)
#define	set_current_state(...) __nop
#define	time_after(a,b) (((long)(b) - (long)(a)) < 0)
#define	time_before(a,b) time_after(b,a)
#define	__attribute_const__
#define	noinline

typedef unsigned short umode_t;
typedef signed char __s8;
typedef unsigned char __u8;
typedef signed short __s16;
typedef unsigned short __u16;
typedef signed int __s32;
typedef unsigned int __u32;
typedef signed long long __s64;
typedef unsigned long long __u64;
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef u64 dma_addr_t;
typedef u64 dma64_addr_t;
typedef u64 sector_t;
typedef unsigned short __le16;
typedef unsigned int __le32;
typedef unsigned short __be16;
typedef unsigned int __be32;
typedef unsigned long kernel_ulong_t;
typedef unsigned int uint;
typedef long long loff_t;
typedef unsigned int gfp_t;
typedef uint32_t dev_t;
typedef uint8_t bool;

#endif					/* _LINUX_DEFS_H_ */