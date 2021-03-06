#include <inc/lib.h>

// parent
int share = 1;

void umain(int argc, char **argv)
{
    int ch = sfork();
    
    if (ch != 0) 
    {
        cprintf ("I'm parent with share num = %d\n", share);
        // child
        share = 2;
    } 
    else 
    {
        sys_yield();
        cprintf ("I'm child with share num = %d\n", share);
    }
}