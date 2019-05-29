#  <center> lab 3 document </center>
<center> 姓名：程浩 学号：515080910012 </center>

## PART A
- EXE 1
  在page_init()之前为envs分配内存空间,在页表中设置它的映射关系。
```
envs  = (struct Env  * ) boot_alloc(NENV   * sizeof (struct Env));
boot_map_region(kern_pgdir,UENVS , PTSIZE, PADDR(envs), PTE_U);
```
- EXE 2
  - env_init() 初始化在envs数组中的env结构体，并把它们加入到env_free_list。
```
	for (int i = NENV - 1; i >= 0; i--)
	{
		envs[i].env_link = env_free_list;
   		env_free_list = &envs[i];
	}
```

  - env_setup_vm() 初始化新的用户环境的页目录表。
```
    e->env_pgdir = (pde_t *)page2kva(p);
	p->pp_ref++;
	
	for (i = 0; i < PDX(UTOP); i++)
	{
		e->env_pgdir[i] = 0;
	}
	for (i = PDX(UTOP); i < NPDENTRIES; i++)
	{
		e->env_pgdir[i] = kern_pgdir[i];
	}
```
  - region_alloc()为用户环境分配物理空间（起始地址与中止地址页对齐）。
```
    uintptr_t va_start = (uintptr_t)ROUNDDOWN(va, PGSIZE);
    uintptr_t va_end   = (uintptr_t)ROUNDUP  (va + len, PGSIZE);
    uintptr_t i ;
    for ( i = va_start ; i < va_end ; i += PGSIZE) 
    {
        struct PageInfo * pg = page_alloc(0); // Does not zero or otherwise initialize the mapped pages in any way
        if (!pg)
        panic("region_alloc failed!");
        page_insert(e->env_pgdir, pg, (void *)i , PTE_W | PTE_U);
    }
```
  - load_icode()为每一个user设置它的初始代码区、标识位等。
```
	struct Elf *head = (struct Elf *)binary;
	struct Proghdr *ph, *eph;

	if (head->e_magic != ELF_MAGIC)
	{
		panic("load_icode failed: it's a invalid ELF!!");
	}
	
	e->env_tf.tf_eip = head->e_entry;
	lcr3(PADDR(e->env_pgdir));
	ph = (struct Proghdr *)((uint8_t *) head + head->e_phoff);
	eph = ph + head->e_phnum;
	// 申请PGSIZE并初始化
	region_alloc(e, (void *) (USTACKTOP - PGSIZE), PGSIZE);
	e->env_heap_bottom = (uintptr_t)ROUNDDOWN(USTACKTOP - PGSIZE,PGSIZE);
	for (; ph < eph; ph++)
	{
		if (ph->p_type == ELF_PROG_LOAD)
		{
			if (ph->p_memsz - ph->p_filesz < 0 )
			{
				panic("load_icode failed: p_memsz < p_filesz !!");
			}
			region_alloc(e, (void *)ph->p_va, ph->p_memsz);
			memset((void *)ph->p_va, 0, ph->p_memsz);
			memmove((void *)ph->p_va, binary + ph->p_offset, ph->p_filesz);
			
		}
	}
	lcr3(PADDR(kern_pgdir));
	region_alloc(e, (void *)(USTACKTOP - PGSIZE), PGSIZE);
```
  - env_create()加载ELF文件到用户环境中。
```
    struct Env* env;
	int ret = env_alloc(&env, 0);
	if (ret != 0)
	{
		panic("env_create failed: env alloc failed!!");
	}
	else if (ret == -E_NO_FREE_ENV)
	{
		panic("env_create failed: no free env!!");
	}
	else if (ret == -E_NO_MEM)
	{
		panic("env_create failed: no mem!!");
	}
	load_icode(env , binary);
	env->env_type = type;
```
  - env_run()开始运行一个用户环境。
```
	if (curenv && curenv->env_status == ENV_RUNNING)
	{
		curenv->env_status = ENV_RUNNABLE;
	}
	curenv = e;
	curenv->env_status = ENV_RUNNING;
	curenv->env_runs++;
	lcr3(PADDR(e->env_pgdir));
	
	env_pop_tf(&curenv->env_tf);

	panic("env_run not yet implemented");
```

- EXE 4
  - trapentry.S 汇编以及定义函数 
  利用宏TRAPHANDLER_NOEC()定义入口函数同时按照文档要求完成_allltraps
```
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(divide_handler, T_DIVIDE)
TRAPHANDLER_NOEC(debug_handler, T_DEBUG)
TRAPHANDLER_NOEC(nmi_handler, T_NMI)
TRAPHANDLER_NOEC(brkpt_handler, T_BRKPT)
TRAPHANDLER_NOEC(oflow_handler, T_OFLOW)
TRAPHANDLER_NOEC(bound_handler, T_BOUND)
TRAPHANDLER_NOEC(illop_handler, T_ILLOP)
TRAPHANDLER_NOEC(device_handler, T_DEVICE)
TRAPHANDLER(dblflt_handler, T_DBLFLT)
TRAPHANDLER(tss_handler, T_TSS)
TRAPHANDLER(segnp_handler, T_SEGNP)
TRAPHANDLER(stack_handler, T_STACK)
TRAPHANDLER(gpflt_handler, T_GPFLT)
TRAPHANDLER(pgflt_handler, T_PGFLT)
TRAPHANDLER_NOEC(fperr_handler, T_FPERR)
TRAPHANDLER_NOEC(align_handler, T_ALIGN)
TRAPHANDLER_NOEC(mchk_handler, T_MCHK)
TRAPHANDLER_NOEC(simderr_handler, T_SIMDERR)

TRAPHANDLER_NOEC(entry_syscall, T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
  	pushl %ds
	pushl %es
	pushal 

	movl $GD_KD, %eax
	movw %ax, %ds
	movw %ax, %es

	push %esp
	call trap

```

  - trap.c中声明与初始化

```
void divide_handler();
void debug_handler();
void nmi_handler();
void brkpt_handler();
void oflow_handler();
void bound_handler();
void illop_handler();
void device_handler();
void dblflt_handler();
void tss_handler();
void segnp_handler();
void stack_handler();
void gpflt_handler();
void pgflt_handler();
void fperr_handler();
void align_handler();
void mchk_handler();
void simderr_handler();

void sysenter_handler();
void entry_syscall();
```
```
	SETGATE(idt[T_DIVIDE], 0, GD_KT, divide_handler, 0);
	SETGATE(idt[T_DEBUG], 0, GD_KT, debug_handler, 0);
	SETGATE(idt[T_NMI], 0, GD_KT, nmi_handler, 0);
	SETGATE(idt[T_BRKPT], 0, GD_KT, brkpt_handler, 3);
	SETGATE(idt[T_OFLOW], 0, GD_KT, oflow_handler, 3);
	SETGATE(idt[T_BOUND], 0, GD_KT, bound_handler, 3);
	SETGATE(idt[T_ILLOP], 0, GD_KT, illop_handler, 0);
	SETGATE(idt[T_DEVICE], 0, GD_KT, device_handler, 0);
	SETGATE(idt[T_DBLFLT], 0, GD_KT, dblflt_handler, 0);
	SETGATE(idt[T_TSS], 0, GD_KT, tss_handler, 0);
	SETGATE(idt[T_SEGNP], 0, GD_KT, segnp_handler, 0);
	SETGATE(idt[T_STACK], 0, GD_KT, stack_handler, 0);
	SETGATE(idt[T_GPFLT], 0, GD_KT, gpflt_handler, 0);
	SETGATE(idt[T_PGFLT], 0, GD_KT, pgflt_handler, 0);
	SETGATE(idt[T_FPERR], 0, GD_KT, fperr_handler, 0);
	SETGATE(idt[T_ALIGN], 0, GD_KT, align_handler, 0);
	SETGATE(idt[T_MCHK], 0, GD_KT, mchk_handler, 0);
	SETGATE(idt[T_SIMDERR], 0, GD_KT, simderr_handler, 0);
	SETGATE(idt[T_SYSCALL], 0, GD_KT,entry_syscall, 3);

	wrmsr(0x174, GD_KT, 0); // SYSENTER_CS_MSR
	wrmsr(0x175, KSTACKTOP, 0); // SYSENTER_ESP_MSR
	wrmsr(0x176, sysenter_handler, 0); // SYSENTER_EIP_MSR
```

## PART B
- EXE 5 EXE 6 
	这里包括了对后面dispatch的实现，包括将缺页异常引导到page_fault_handler()、断点异常处理等。
```
	switch(tf->tf_trapno) 
	{
		case T_PGFLT:
			page_fault_handler(tf);
			return;
		case T_DEBUG:
		case T_BRKPT:
			monitor(tf);
			return;
		case T_SYSCALL:
			// print_trapframe(tf);
			ret_code = syscall
			(
				tf->tf_regs.reg_eax,
				tf->tf_regs.reg_edx,
				tf->tf_regs.reg_ecx,
				tf->tf_regs.reg_ebx,
				tf->tf_regs.reg_edi,
				tf->tf_regs.reg_esi
			);
			tf->tf_regs.reg_eax = ret_code;
			break;
		default:
			// Unexpected trap: The user process or the kernel has a bug.
			print_trapframe(tf);
			if (tf->tf_cs == GD_KT)
				panic("unhandled trap in kernel");
			else 
			{
				env_destroy(curenv);
				return;
			}
	}

```

- EXE 7 EXE8(syscall's dispatch in trapentry.S & trap.c已经在前面的联系中完成)
  - kern/syscall() 根据syscallno判断具体调用哪一个system call
```
  switch(syscallno)
  {
    case SYS_cputs:
      sys_cputs((char *)a1,(size_t)a2);
      return 0;
    case SYS_cgetc:
      return sys_cgetc();
    case SYS_getenvid:
      return sys_getenvid();
    case SYS_env_destroy:
      return sys_env_destroy((envid_t) a1);
    case SYS_map_kernel_page:
      return sys_map_kernel_page((void*) a1, (void*) a2);
    case SYS_sbrk:
      return sys_sbrk((uint32_t)a1);
    case NSYSCALLS:
    default:
      return -E_INVAL;
  }
```
  - trapentry.S
```
.globl sysenter_handler;
.type sysenter_handler, @function;
.align 2;
sysenter_handler:
  pushl %edi
  pushl %ebx
  pushl %ecx
  pushl %edx
  pushl %eax
  call syscall
  movl %ebp, %ecx
  movl %esi, %edx
  sysexit
```

  - lib/syscall()
```
asm volatile(
		"pushl %%ecx\n\t"
		"pushl %%edx\n\t"
		"pushl %%ebx\n\t"
		"pushl %%esp\n\t"
		"pushl %%ebp\n\t"
		"pushl %%esi\n\t"
		"pushl %%edi\n\t"
		"pushl %%esp\n\t"
		"popl %%ebp\n\t"
		
		"leal after_sysenter_label%=, %%esi\n\t" 
		"sysenter\n\t"
		"after_sysenter_label%=:\n\t"

		"popl %%edi\n\t"
		"popl %%esi\n\t"
		"popl %%ebp\n\t"
		"popl %%esp\n\t"
		"popl %%ebx\n\t"
		"popl %%edx\n\t"
		"popl %%ecx\n\t"

		: "=a" (ret)
		: "a" (num),
		  "d" (a1),
		  "c" (a2),
		  "b" (a3),
		  "D" (a4),
		  "S" (a5)
		: "cc", "memory");
```

- EXE 9

  - lib/libmain.c libmain()
	使用sys_getenvid()得到当前env id
```
thisenv = &envs[ENVX(sys_getenvid())];
```

- EXE 10
  - 申请PGSIZE并初始化在前面的load_icode()中已写上
  - 添加一个env属性用来记录heap的底部
  - sys_sbrk()
```
// lab3 add 
uintptr_t env_heap_tail;
--------------------------------------------------
sys_sbrk()

  region_alloc(curenv, (void *) (curenv->env_heap_tail - inc), inc);
  return curenv->env_heap_tail = (uintptr_t)ROUNDDOWN(curenv-> env_heap_tail- inc,PGSIZE);

```

- EXE 11
  - kern/trap.c page_fault_handler()
  - kern/pmap.c user_mem_check()
  - kdebug.c 
	检查是否是内核态；
	最后加上usd，stabs，stabstr的地址检测
```
if ((tf->tf_cs & 0x3) == 0)
	panic("kernel page fault");
--------------------------------------------------
	perm = PGOFF(perm);
	uintptr_t vanext;
	uintptr_t offset = 0;

	while (offset < len) 
	{
		// Address to check.
		vanext = (uintptr_t)(va + offset);
		// Check whether the address is in valid range.
		if (vanext >= ULIM) 
		{
			user_mem_check_addr = vanext;
			return -E_FAULT;
		}
		// Check whether the page is present and its permissions.
		pte_t *pte = pgdir_walk(env->env_pgdir, (void *)vanext, 0);
		if (!pte || (*pte & perm) != perm) 
		{
			user_mem_check_addr = vanext;
			return -E_FAULT;
		}
		// Next page.
		if (offset)
			offset += PGSIZE;
		else
			offset += PGSIZE - (uintptr_t)va % PGSIZE;
	}
	return 0;
--------------------------------------------------
if (user_mem_check(curenv, usd, sizeof(struct UserStabData), 0) < 0)
	return -1;

if (user_mem_check(curenv, stabs  , stab_end   -stabs  , 0) < 0)
   	return -1;
if (user_mem_check(curenv, stabstr, stabstr_end-stabstr, 0) < 0)
   	return -1;

```

- EXE 13
  - user/evilhello2.c ring0_call()
```
gdt = (struct Segdesc *)(ROUNDUP((void *)vaddr - PGSIZE, PGSIZE) + PGOFF(r_gdt.pd_base));
entry = gdt + (GD_TSS0 >> 3);
old = *entry;
ring0_func = fun_ptr;

SETCALLGATE(*((struct Gatedesc*)entry), GD_KT, call_fun_ptr, 3);
asm volatile ("lcall %0, $0" : : "i"(GD_TSS0));

```
