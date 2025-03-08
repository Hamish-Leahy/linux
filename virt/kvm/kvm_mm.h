/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __KVM_MM_H__
#define __KVM_MM_H__ 1

/*
 * KVM Memory Management Header
 *
 * This header file defines the memory management structures and functions
 * used by the KVM (Kernel-based Virtual Machine) subsystem. It provides
 * an interface for managing memory slots and page frame numbers (PFNs).
 */

/* Locking mechanism for MMU operations */
#ifdef KVM_HAVE_MMU_RWLOCK
#define KVM_MMU_LOCK_INIT(kvm)		rwlock_init(&(kvm)->mmu_lock)
#define KVM_MMU_LOCK(kvm)		write_lock(&(kvm)->mmu_lock)
#define KVM_MMU_UNLOCK(kvm)		write_unlock(&(kvm)->mmu_lock)
#else
#define KVM_MMU_LOCK_INIT(kvm)		spin_lock_init(&(kvm)->mmu_lock)
#define KVM_MMU_LOCK(kvm)		spin_lock(&(kvm)->mmu_lock)
#define KVM_MMU_UNLOCK(kvm)		spin_unlock(&(kvm)->mmu_lock)
#endif /* KVM_HAVE_MMU_RWLOCK */

/* Structure for following page frame numbers */
struct kvm_follow_pfn {
	const struct kvm_memory_slot *slot; /* Memory slot associated with the page */
	const gfn_t gfn;                    /* Guest Frame Number */
	unsigned long hva;                  /* Host Virtual Address */

	/* Flags modifying lookup behavior, e.g., FOLL_WRITE */
	unsigned int flags;

	/*
	 * Pin the page (effectively FOLL_PIN, which is an mm/ internal flag).
	 * The page *must* be pinned if KVM will write to the page via a kernel
	 * mapping, e.g., via kmap(), mremap(), etc.
	 */
	bool pin;

	/*
	 * If non-NULL, try to get a writable mapping even for a read fault.
	 * Set to true if a writable mapping was obtained.
	 */
	bool *map_writable;

	/*
	 * Optional output. Set to a valid "struct page" if the returned pfn
	 * is for a refcounted or pinned struct page, NULL if the returned pfn
	 * has no struct page or if the struct page is not being refcounted
	 * (e.g., tail pages of non-compound higher order allocations from
	 * IO/PFNMAP mappings).
	 */
	struct page **refcounted_page;
};

/* Function to convert host virtual address to page frame number */
kvm_pfn_t hva_to_pfn(struct kvm_follow_pfn *kfp);

#ifdef CONFIG_HAVE_KVM_PFNCACHE
void gfn_to_pfn_cache_invalidate_start(struct kvm *kvm,
				       unsigned long start,
				       unsigned long end);
#else
static inline void gfn_to_pfn_cache_invalidate_start(struct kvm *kvm,
						     unsigned long start,
						     unsigned long end)
{
	/* No operation if PFN cache is not available */
}
#endif /* CONFIG_HAVE_KVM_PFNCACHE */

#ifdef CONFIG_KVM_PRIVATE_MEM
void kvm_gmem_init(struct module *module);
int kvm_gmem_create(struct kvm *kvm, struct kvm_create_guest_memfd *args);
int kvm_gmem_bind(struct kvm *kvm, struct kvm_memory_slot *slot,
		  unsigned int fd, loff_t offset);
void kvm_gmem_unbind(struct kvm_memory_slot *slot);
#else
static inline void kvm_gmem_init(struct module *module)
{
	/* No operation if private memory is not enabled */
}

static inline int kvm_gmem_bind(struct kvm *kvm,
					 struct kvm_memory_slot *slot,
					 unsigned int fd, loff_t offset)
{
	WARN_ON_ONCE(1); /* Warn if this function is called unexpectedly */
	return -EIO; /* Input/output error */
}

static inline void kvm_gmem_unbind(struct kvm_memory_slot *slot)
{
	WARN_ON_ONCE(1); /* Warn if this function is called unexpectedly */
}
#endif /* CONFIG_KVM_PRIVATE_MEM */

#endif /* __KVM_MM_H__ */
