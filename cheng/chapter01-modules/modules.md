# 第一章 内核模块

#### ELF介绍
![ELF](https://images0.cnblogs.com/blog/417887/201309/10163329-3b81c167e3e34681b4d30f19953ccc81.jpg)


#### 1. 模块的加载过程

*  insmod 读取模块文件数据到用户空间，然后执行系统调用sys_init_module处理模块加载

*  sys_init_module调用load_module， 将内核空间利用vmalloc分配一块大小等于模块的地址空间， 将用户空间的数据复制到内核空间

* load_module通过如下计算就可以获得section名称字符串表的基地址secstrings和符号名称字符串表的基地址strtab. 
计算section名称的方法
    >    char *secstrings = (char *)hdr + entry[hdr->e_shstrndx].sh_offset.
    >计算符号表名称字符串表的基地址的方法
    >
    >  遍历section header table中的所有entry, 找一个entry[i].sh_type = SHT_SYMTAB的entry, SHT_SYMTAB 表明这个entry所对应的section是一个符号表。
    >entry[i].sh_link是符号名称字符串表在section在section header table中的索引， 所以基地址就为 char *strtab = (char*)hdr+entry[entry[i].sh_link].sh_offset
*  开始遍历section header table中所有的entry, 将entry的sh_addr指向HDR中的实际地址。

* layout_sections函数中， 内核遍历HDR视图中的每一个section, 对每一个标记有HF_ALLOC的section，将其划分到两大类的section当中:CORE和INIT.

* 对“未解决的引用”符号的处理
simplify_symbols函数的代码实现， 函数首先通过一个for循环遍历符号表中的所有符号， 根据Elf_sym的st_shndx查找到SHN_UNDEF, 这就是未解决的引用， 调用solve_symbol来处理， 后者会调用find_symbol来查找符号， 找到了就将它在内存中的实际值赋值给st_value, 这样未解决的引用就有了正确的内存地址。
* 重定位
重定位主要用来解决静态链接时的符号引用与动态加载时实际符号地址不一致的问题.
如果模块有用EXPORT_SYMBOL导出符号， 那么这个模块的elf文件会生成一个独立的section:".rel_ksymtab", 它专门用于对"__ksymtab" section的重定位， 称为relocation section. 
* 模块成功加载到系统之后， 表示模块的struct module变量mod加入到modules中(一个全局的链表变量，记录系统中的模块)

#### 2. 模块如何引用内核或者其他模块中的函数和变量


通过find_symbol函数查找，第一部分在内核导出的符号表中查找对应的符号， 找到就返回对应的符号信息， 否则，再进行第二部分查找，在系统已加载的模块中查找.


#### 3. 模块的参数传递机制
模块必需使用module_param宏声明模块可以接收的参数， 如module_param(dolphin, int, 0), 内核模块加载器对参数的构造过程发生在模块初始化函数之前， 所以在模块构造函数中就可以得到命令行传过来的实际参数。
命令行的参数值复制到模块的参数过程中， module_param宏定义的"__param"section起到桥梁的作用， 通过"__param"section， 内核找到模块中定义参数所在内存地址， 然后用命令行中参数值修改之。

#### 4. 模块间的依赖关系
struct list_head source_list和struct list_head target_list 用来构建有信赖关系模块的链表， 对链表的使用要结合数据结构struct module_use:

如果有依赖关系， 在solve_symbol函数中将会导出这"未解决的引用"符号模块记录在变量struct module *owner中， 调用ref_module(mod, owner)在模块mod和owner之间建立信赖关系。 ref_module做过检查后调用add_module_usage(mod, owner)在mod和owner模块间建立信赖关系. 

#### 5. 模块的版本控制机制
 linux系统对此的解决方案是使用接口的校验和，也叫接口crc校验码， 为了确保这种机制能够正常运行, 内核必须启用CONFIG_MODVERSIONS宏， 模块编译时也需启用此宏， 否则加载不成功。
 
 __kcrctab_my_exp_function用来保存__crc_my_exp_function变量地址并将其放在一个名为__kcrctab的section中. 可见如果内核启用了CONFIG_MODVERSIONS宏， 每一个导出
的符号都会生成一个crc校验码check_version用一个for循环在__versions section中进行遍历， 对每一个struct modversion_info元素和找到的符号名symname进行匹配， 如果匹配成功就进行接口的校验码比较。

独立编译的内核模块，如果引用到了内核导出一符号， 在模块编译的过程中， 工具链会到内核源码所在目录下查找Module.symvers文件， 将得到的printk的crc校验码记录到模块的__versions section中。
    

EXPORT_SYMBOL的宏定义
```c
#ifdef CONFIG_MODVERSIONS
/* Mark the CRC weak since genksyms apparently decides not to
 * generate a checksums for some symbols */
#define __CRC_SYMBOL(sym, sec)					\
	extern void *__crc_##sym __attribute__((weak));		\
	static const unsigned long __kcrctab_##sym		\
	__used							\
	__attribute__((section("__kcrctab" sec), unused))	\
	= (unsigned long) &__crc_##sym;
#else
#define __CRC_SYMBOL(sym, sec)
#endif

/* For every exported symbol, place a struct in the __ksymtab section */
#define __EXPORT_SYMBOL(sym, sec)				\
	extern typeof(sym) sym;					\
	__CRC_SYMBOL(sym, sec)					\
	static const char __kstrtab_##sym[]			\
	__attribute__((section("__ksymtab_strings"), aligned(1))) \
	= MODULE_SYMBOL_PREFIX #sym;                    	\
	static const struct kernel_symbol __ksymtab_##sym	\
	__used							\
	__attribute__((section("__ksymtab" sec), unused))	\
	= { (unsigned long)&sym, __kstrtab_##sym }

#define EXPORT_SYMBOL(sym)					\
	__EXPORT_SYMBOL(sym, "")

#define EXPORT_SYMBOL_GPL(sym)					\
	__EXPORT_SYMBOL(sym, "_gpl")

#define EXPORT_SYMBOL_GPL_FUTURE(sym)				\
	__EXPORT_SYMBOL(sym, "_gpl_future")
```
字符串表是elf文件中的一个section, 用来保存elf文件中各个section的名称或符号表名.
    计算section名称的方法
    char *secstrings = (char *)hdr + entry[hdr->e_shstrndx].sh_offset.
    计算符号表名称字符串表的基地址的方法
    
    .遍历section header table中的所有entry, 找一个entry[i].sh_type = SHT_SYMTAB的entry, SHT_SYM
    TAB    表明这个entry所对应的section是一个符号表。
    
    .entry[i].sh_link是符号名称字符串表在section在section header table中的索引， 所以基地址就为
    char *strtab = (char*)hdr+entry[entry[i].sh_link].sh_offset

    至此load_module通过以上计算就可以获得section名称字符串表的基地址secstrings和符号名称字符串表
    的基地址strtab. 

### 参考文档
ELF规范:    [Executable and Linking Format Specification V1.2](
http://www.sco.com/developers/gabi/latest/contents.html)  
参考文档 - [ELF-64 Object File Format](https://www.uclibc.org/docs/elf-64-gen.pdf)  
[参考blog]  
(https://www.cnblogs.com/feng9exe/p/6899351.html)  
(https://blog.csdn.net/dyron/article/details/9022629)
