#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc fs.h>
#include <asm/uaccess.h>
#define BUFFER SIZE 128
#define PROC NAME "hello"
ssize t proc read(struct file *file, char user *usr buf,
size t count, loff t *pos);
static struct file operations proc ops = {
.owner = THIS MODULE,
.read = proc read,
};
/* This function is called when the module is loaded. */
int proc init(void)
{
/* creates the /proc/hello entry */
proc create(PROC NAME, 0666, NULL, &proc ops);
return 0;
}
/* This function is called when the module is removed. */
void proc exit(void)
{
/* removes the /proc/hello entry */
remove proc entry(PROC NAME, NULL);
}
/* This function is called each time /proc/hello is read */
ssize t proc read(struct file *file, char user *usr buf,
size t count, loff t *pos)
{
int rv = 0;
char buffer[BUFFER SIZE];
static int completed = 0;
if (completed) {
completed = 0;
return 0;
}
completed = 1;
rv = sprintf(buffer, "Hello World A1115545∖n");
/* copies kernel space buffer to user space usr buf */
copy to user(usr buf, buffer, rv);
return rv;
}
module init(proc init);
module exit(proc exit);
MODULE LICENSE("GPL");
MODULE DESCRIPTION("Hello Module");
MODULE AUTHOR("SGG");
