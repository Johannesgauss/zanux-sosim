#include "arch/vmm.h"
#include "pmm.h"

// TODO: a better allocation function!

static virt_t s_page_base = (virt_t) 0xFFFF851000000000;
static virt_t s_page_offset = (virt_t) 4096;
//static virt_t s_heap_top = (virt_t) 0xFFFF851000004096; // s_page_base + s_page_offset
static virt_t s_usr_page_base = (virt_t) 0x0000080000000000;
static virt_t s_usr_page_offset = (virt_t) 4096;

virt_t kmalloc(size_t size)
{
	if (size == 0)
		size = 8;
	size = (size + 7ULL) & ~7ULL;

	if (size < PAGE_SIZE) {
		virt_t alloc_offset = 0;
		if ((uintptr_t)(s_page_offset + size) <= PAGE_SIZE) {
			alloc_offset = s_page_offset;
			s_page_offset += size;
		} else {
			s_page_base += PAGE_SIZE;
			vmm_map(s_page_base);
			s_page_offset = (virt_t)size;
		}
		return ((uintptr_t)s_page_base + alloc_offset);
	} else {
		virt_t base_alloc = s_page_base + PAGE_SIZE;
		virt_t i; for (i = 0; i < (virt_t)size; i += PAGE_SIZE) {
			vmm_map((uintptr_t)base_alloc + i);
		}
		s_page_base += (uintptr_t)i;
		if (size % PAGE_SIZE == 0)
			s_page_offset = (virt_t)PAGE_SIZE;
		else
			s_page_offset = (virt_t)(size % PAGE_SIZE);
		return base_alloc;
	}
}

void kfree(virt_t addr)
{
	return;	
}

virt_t vmalloc(size_t size)
{
	return kmalloc(size);
}

void vfree(virt_t addr)
{
	return;
}


virt_t usr_kmalloc(size_t size)
{
	if (size == 0)
		size = 8;
	size = (size + 7ULL) & ~7ULL;

	if (size < PAGE_SIZE) {
		virt_t alloc_offset = 0;
		if ((uintptr_t)(s_usr_page_offset + size) < PAGE_SIZE) {
			alloc_offset = s_usr_page_offset;
			s_usr_page_offset += size;
		} else if ((uintptr_t)(s_usr_page_offset + size) >= PAGE_SIZE) {
			s_usr_page_base += PAGE_SIZE;
			vmm_map(s_usr_page_base);
			s_usr_page_offset = (virt_t)size;
		}
		return ((uintptr_t)s_usr_page_base + alloc_offset);
	} else {

		virt_t base_alloc = s_usr_page_base + PAGE_SIZE;
		virt_t i; for (i = 0; i < (virt_t)size; i += PAGE_SIZE) {
			vmm_map((uintptr_t)base_alloc + i);	
		}
		s_usr_page_base += (uintptr_t)i;
		if (size % PAGE_SIZE == 0)
			s_usr_page_base += PAGE_SIZE;
		s_usr_page_offset = (virt_t) (size % PAGE_SIZE);
		return base_alloc;
	}
}

virt_t usr_vmalloc(size_t size)
{
	return usr_kmalloc(size);
}

void usr_kfree(virt_t addr)
{
	return;
}
