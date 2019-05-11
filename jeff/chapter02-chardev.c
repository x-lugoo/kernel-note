<demo_chr_dev.c>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>

static struct cdev chr_dev;

  struct cdev {
	struct kobject kobj;
       /* 用于驱动模型 */
	struct module *owner;
	/* 当前字符设备所属模块 */
	const struct file_operations *ops;
	struct list_head list;
	/* 用来将系统中的字符设备形成链表。*/
	dev_t dev;
	/* 字符设备的设备号，由主设备和次设备构成 */
	unsigned int count;
	/* 棣属于同一主设备号的次设备号的个数，用于表示当前设备驱动程序控制的设备数量 */
};
/*
 * struct cdev仅仅作为一个内嵌的数据结构内嵌于实际的字符设备驱动结构中
 */

static dev_t ndev;

static int chr_open(struct *inode nd, struct file *filp)
{
	int major = MAJOR(nd->i_rdev);
	int minor = MINOR(nd->i_rdev);
	printk("chr_open,major = %d, minor = %d", major, minor);
	return 0;
}

static ssize_t char_read(struct file *f, char __user *u, size_t sz, loff_t *off)
{
	printk("In the chr_read() function\n");
	return 0;
}

struct file_operation chr_ops = 
{
	.owner = THIS_MODULE,
	.open = chr_open,
	.read = chr_read,
};


static int demo_init(void)
{
	int ret;
	cdev_init(&chr_dev, &chr_ops);
	ret = alloc_chrdev_region(&ndev, 0 , 1，"chr_dev");
	if (ret < 0)
		return ret;
	ret = cdev_add(&chr_dev, ndev, 1);
	if (ret < 0)
		return ret;
	return 0;
}


statict void demo_exit(void)
{
	cdev_del(&chr_dev);
	unregister_chrdev_region(ndev, 1);
}



/*
* struct file_operations 中有个 struct module *owner;
* owner指向第一章中提到的.gnu.linkonce.this_module段的struct module结构，
* 作用就是file_operations中的函数被调用时不要卸载当前模块。
* 如果设备驱动编译进内核不编译成模块，THIS_MODULE就是空指针，可以
* 从内核代码中看到这一段。
*/
#ifdef MODULE
extern struct module __this_module;
#define THIS_MODULE (&__this_module)
#else
#define THIS_MODULE ((struct module *)0)
#endif



static struct char_device_struct {
	struct char_device_struct *next;
	/* 这个next是用来连接主设备号哈希值相同的情况，比如MKDEV(2,0)和MKDEV(257,0) */
	unsigned int major;
	unsigned int baseminor;
	次设备号
	int minorct;
	次设备号个数
	char name[64];
	struct cdev *cdev;		/* will die */
	/* 这里的will die在linux5.0版本中都还在，有点搞笑 */
} *chrdevs[CHRDEV_MAJOR_HASH_SIZE];
#define CHRDEV_MAJOR_HASH_SIZE 255

/* 这个结构跟踪每一个字符设备主设备号；*/

int cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
	int error;

	p->dev = dev;
	p->count = count;

	return kobj_map(cdev_map, dev, count, NULL,
			 exact_match, exact_lock, p);
}
/* 参数p为要加入系统的字符设备对象的指针，dev为设备号，count表示
从次设备号开始连续的设备数量。 */

static struct kobj_map *cdev_map;
struct kobj_map {
	struct probe {
		struct probe *next;
	/* 散列冲突链表的下一个元素 */
		dev_t dev;
	/* 设备号范围的初始设备号（主、次设备号） */
		unsigned long range;
	 /* 设备号范围的大小 */
		struct module *owner;
	/* 指向实现设备驱动程序模块的指针(编译进内核就是空指针) */
		kobj_probe_t *get;
	 /* 探测谁拥有这个设备号范围  */
		int (*lock)(dev_t, void *);
	/* 增加设备号范围内拥有者的引用计数器  */
		void *data;
	/* 用来指向 struct cdev */
	} *probes[255];
	struct mutex *lock;
};
/*
* 用来跟踪struct cdev,与struct char_device_struct跟踪设备号是一个原理.
* inode->i_fop = &def_chr_fops,
* file->f_op = &dev_chr_fops;
* file->f_op->open(inode, file);
*/

/*
 * Called every time a character special file is opened
 */
static int chrdev_open(struct inode *inode, struct file *filp)
{
	struct cdev *p;
	struct cdev *new = NULL;
	int ret = 0;

	spin_lock(&cdev_lock);
	p = inode->i_cdev;
	if (!p) {
		struct kobject *kobj;
		int idx;
		spin_unlock(&cdev_lock);
		kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);
		if (!kobj)
			return -ENXIO;
		new = container_of(kobj, struct cdev, kobj);
		spin_lock(&cdev_lock);
		/* Check i_cdev again in case somebody beat us to it while
		   we dropped the lock. */
		p = inode->i_cdev;
		if (!p) {
			inode->i_cdev = p = new;
			list_add(&inode->i_devices, &p->list);
			new = NULL;
		} else if (!cdev_get(p))
			ret = -ENXIO;
	} else if (!cdev_get(p))
		ret = -ENXIO;
	spin_unlock(&cdev_lock);
	cdev_put(new);
	if (ret)
		return ret;

	ret = -ENXIO;
	filp->f_op = fops_get(p->ops);
	if (!filp->f_op)
		goto out_cdev_put;

	if (filp->f_op->open) {
		ret = filp->f_op->open(inode, filp);
		if (ret)
			goto out_cdev_put;
	}

	return 0;

 out_cdev_put:
	cdev_put(p);
	return ret;
}


/*
 * 主要的作用是通过cdev_map找到对应的struct cdev，然后把cdev中的ops
 * 赋值给打开文件的f_op,然后调用cdev中的open,打完收工。
 */
