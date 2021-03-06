// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (
			!((err & FEC_WR) && 
			(uvpt[PGNUM(addr)] & PTE_COW) && 
			(uvpd[PDX(addr)] & PTE_P) && 
			(uvpt[PGNUM(addr)] & PTE_P))
		)
	{
		panic("pgfault: not the copy-on-write page!!! \n");
	}
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	r = sys_page_alloc(0, (void *)PFTEMP, PTE_P | PTE_W | PTE_U);
	if (r < 0)
	{
		panic("pgfault: sys_page_alloc fail!!! %e \n", r);
	}
	memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);

	r = sys_page_map(0, (void *)PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P | PTE_W | PTE_U);
	if (r < 0)
	{
		panic("pgfault: sys_page_map fail!!! %e \n", r);
	}

	r = sys_page_unmap(0, (void *)PFTEMP);
	if (r < 0)
	{
		panic("pgfault: sys_page_unmap fail!!! %e \n", r);
	}
	return ;

	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void* addr = (void *) (pn * PGSIZE);
	if((uvpt[pn] & PTE_COW) || (uvpt[pn] & PTE_W))
	{
		if((r = sys_page_map(0, addr, envid, addr, PTE_COW | PTE_U | PTE_P)) < 0)
			panic("duppage: sys_page_map fail!!! %e \n", r);

		if((r = sys_page_map(0, addr, 0, addr, PTE_COW | PTE_U | PTE_P)) < 0)
			panic("duppage: sys_page_map fail!!! %e \n", r);
	}
	else
	{
		if((r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P)) < 0)
			panic("duppage: sys_page_map fail!!! %e \n", r);
	}

	// panic("duppage not implemented");
	return 0;
}

static int 
sduppage(envid_t envid, unsigned pn, int cow_enabled)
{
	int res;
	void *addr = (void *)(pn * PGSIZE);
	int perm = PGOFF(uvpt[pn]) & PTE_SYSCALL;

	if (cow_enabled && (perm & PTE_W)) 
	{
		perm |= PTE_COW;
		perm &= ~PTE_W;

		if ((res = sys_page_map(0, addr, envid, addr, perm)) < 0)
			panic("sduppage: sys_page_map fail!!! %e\n", res);

		if ((res = sys_page_map(0, addr, 0, addr, perm)) < 0)
			panic("sduppage: sys_page_map fail!!! %e\n", res);
	} 
	else 
	{
		if ((res = sys_page_map(0, addr, envid, addr, perm)) < 0)
			panic("sduppage: sys_page_map fail!!! %e\n", res);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int res;
	set_pgfault_handler(pgfault);
	// child environment.
	envid_t envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) 
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// parent
	uint32_t addr;
	for (addr = 0; addr < USTACKTOP; addr += PGSIZE)
	{
		if((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U))
			duppage(envid, PGNUM(addr));
	}

	if((res = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P)) < 0)
		panic("fork: sys_page_alloc: %e", res);

	extern void _pgfault_upcall();
	if ((res = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("fork: sys_env_set_pgfault_upcall: %e \n", res);

	// let child env run
	if ((res = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("fork: sys_env_set_status: %e", res);

	return envid;

	// panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	int r;
	set_pgfault_handler(pgfault);
	
	// child environment.
	envid_t envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) 
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// parent
	bool stackarea = true;
	uint32_t addr;
	for ( addr = USTACKTOP - PGSIZE; addr >= UTEXT; addr -= PGSIZE) 
	{
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P))
			sduppage(envid, PGNUM(addr), stackarea);
		else
			stackarea = false;
	}

	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P)) < 0)
		panic("sfork: sys_page_alloc: %e \n", r);

	extern void _pgfault_upcall(void);
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("sfork: sys_env_set_pgfault_upcall: %e \n", r);

	// let child env run
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sfork: sys_env_set_status : %e \n", r);

	return envid;

	// panic("sfork not implemented");
	// return -E_INVAL;
}
