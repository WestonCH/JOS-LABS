// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	// {"time", "Displat the program's running time", mon_time},
	// { "c", "GDB-style instruction continue.", mon_c },
	// { "si", "GDB-style instruction stepi.", mon_si },
	// { "x", "GDB-style instruction examine.", mon_x },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
    char *pret_addr;

	// Your code here.
    pret_addr = (char *)read_pretaddr();
	uint32_t new_addr = (uint32_t)do_overflow;

	/*
	|args   |		|old ret|
	|old ret|  -> 	|new ret|
	|-------|		|-------|
	*/
	//back up old ret addr first
	for (int i = 0; i < 4; i++){
		cprintf("%*s%n\n", pret_addr[i] & 0xFF, " ", pret_addr + 4 + i);
	}
    //overwrite new ret_addr
	for (int i = 0; i < 4; i++){
		cprintf("%*s%n\n", (new_addr >> (8*i)) & 0xFF, " ", pret_addr + i);
	}

}

void
overflow_me(void)
{
        start_overflow();
}


int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.

	uint32_t ebp = read_ebp();// indicate the base pointer into the stack used by that function
	uint32_t esp = read_esp();// the instruction address to which control will return when the function return

	cprintf("Stack backtrace:\n");
	while(ebp!=0)
	{	
		/*
			when call a func, push args from right to left
			push ret addr
			enter func
			mov esp, ebp
			push ebp
		*/
		//get args esp from stack
		esp = *((uint32_t *)ebp + 1);
		uint32_t arg1 = *((uint32_t *)ebp + 2);
		uint32_t arg2 = *((uint32_t *)ebp + 3);
		uint32_t arg3 = *((uint32_t *)ebp + 4);
		uint32_t arg4 = *((uint32_t *)ebp + 5);
		uint32_t arg5 = *((uint32_t *)ebp + 6);

		cprintf("  eip %08x ebp %08x args %08x %08x %08x %08x %08x\n",esp,ebp,arg1,arg2,arg3,arg4,arg5);
		//output the line no
		struct Eipdebuginfo info;
		debuginfo_eip((uintptr_t)esp, &info);
		//get func name
		char func_name[info.eip_fn_namelen+1];
		for(int i=0;i<info.eip_fn_namelen;i++){
			func_name[i]=info.eip_fn_name[i];
		}
		func_name[info.eip_fn_namelen]='\0';
		uint32_t lineno = esp - (uint32_t)info.eip_fn_addr;
		cprintf("         %s:%u %s+%u\n",info.eip_file, info.eip_line, func_name, lineno);
		ebp = *((uint32_t*)ebp);
	}
	overflow_me();
    cprintf("Backtrace success\n");
	return 0;
}

// int mon_time(int argc, char **argv, struct Trapframe *tf)
// {
// 	if(argc==1)
// 	{
// 		cprintf("Usage: time [command]\n");
// 		return 0;
// 	}
// 	int i=0;
// 	while(i<sizeof(commands)/sizeof(commands[0]))
// 	{
// 		//first find the command
// 		if(strcmp(argv[1],commands[i].name)==0)
// 		{
// 			unsigned long long time1=read_tsc();
// 			commands[i].func(argc-1,argv+1,tf);
// 			unsigned long long time2=read_tsc();
// 			cprintf("%s cycles: %llu\n",argv[1],time2-time1);
// 			return 0;
// 		}
// 		i++;
// 	}
// 	cprintf("Unknown command\n",argv[1]);
// 	return 0;
// }

// //continue
// int
// mon_c(int argc, char **argv, struct Trapframe *tf)
// {
//   	if(tf)//GDB-mode
//     	return -1;
//   	cprintf("not support continue in non-gdb mode\n");
//   	return 0;
// }

// //stepi
// int
// mon_si(int argc, char **argv, struct Trapframe *tf)
// {
//   	if(tf)
// 	{//GDB-mode
//     	tf->tf_eflags |= FL_TF;
//     	struct Eipdebuginfo info;
//     	debuginfo_eip((uintptr_t)tf->tf_eip, &info);
//     	cprintf("tf_eip=%08x\n%s:%u %.*s+%u\n",
//     			tf->tf_eip,info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, tf->tf_eip - (uint32_t)info.eip_fn_addr);
//     	return -1;
//   	}
//   	cprintf("not support stepi in non-gdb mode\n");
//   	return 0;
// }

// //examine
// int
// mon_x(int argc, char **argv, struct Trapframe *tf)
// {
//   	if (tf)
// 	{//GDB-mode
//     	if (argc != 2) 
// 		{
//     		cprintf("Please enter the address");
//       		return 0;
//     	}
//     	uintptr_t examine_address = (uintptr_t)strtol(argv[1], NULL, 16);
//     	uint32_t examine_value;
//     	__asm __volatile("movl (%0), %0" : "=r" (examine_value) : "r" (examine_address));
//     	cprintf("%d\n", examine_value);
//     	return 0;
//   	}
//   	cprintf("not support stepi in non-gdb mode\n");
//   	return 0;
// }

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

