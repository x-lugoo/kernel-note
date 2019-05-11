# 第一章:内核模块


```
	<include/linux/module.h>

#define __EXPORT_SYMBOL(sym, sec)                               \
        extern typeof(sym) sym;                                 \
        __CRC_SYMBOL(sym, sec)                                  \
        static const char __kstrtab_##sym[]                     \
        __attribute__((section("__ksymtab_strings"), aligned(1))) \
        = MODULE_SYMBOL_PREFIX #sym;                            \
        static const struct kernel_symbol __ksymtab_##sym       \
        __used                                                  \
        __attribute__((section("__ksymtab" sec), unused))       \
        = { (unsigned long)&sym, __kstrtab_##sym }

#define EXPORT_SYMBOL(sym)                                      \
        __EXPORT_SYMBOL(sym, "")

#define EXPORT_SYMBOL_GPL(sym)                                  \
        __EXPORT_SYMBOL(sym, "_gpl")

#define EXPORT_SYMBOL_GPL_FUTURE(sym)                           \
        __EXPORT_SYMBOL(sym, "_gpl_future")
static const char * __kstrtab_my_exp_function = "my_exp_funxtion"
static const struct kernel_symbol __kstrtab_my_exp_function 
= { (unsigned long)&my_exp_function, __kstrtab_my_exp_function }\

struct kernel_symbol   
{
	unsigned long value;  /* my_exp_function  放在 __ksymtab 段  */   
	const char *name;     /* __kstrtab_my_exp_function 放在 __ksymtab_strings */
};
```

**找段名称字符串表的基地址:**
通过hdr中的e_shstrndx 对应到entry数组的下标直接索引到字符串表的entry,再通过entry中的 sh_offset找到字符串表的基地址，基地址使用secstrings变量保存。

**找符号名称字符串表基地址:**
遍历数组entry中的各个entry，找到entry数组结构中sh_type类型为SHT_SYMTAB的entry，通过这个entry结构中的sh_link对应到entry数组结构下标的entry，从这个entry的sh_offset找到对应的section的基地址,即是符号名称字符串表基地址，使用strtab表示。

获取某一段的名称
假设该段在Section header table中的索引为i,那么secstrings + entry[i].sh.name即是该段的名字。
获取符号名称的基地址：
遍历整个符号名称字符串表，因为这个段是struct Elf_Sym数组构成，遍历数组中买一个struct Elf_Sym结构，匹配到与查找名称相同的Struct Elf_Sym.再从Struct Elf_Sym取出符号名称的基地址。

符号表中的结构体定义：
struct Elf_Sym {
	Elf32_Word st_name;  /* 符号名在符号名称字符串表中的索引 */
	Elf32_Addr st_value;   /* 符号所在的内存地址 */
	Elf32_Word st_size;
	unsigned char st_info;
	unsigned char st_other;
	Elf32_Half  st_shndx;  /* 符号所在的段在section header table中的索引值 */
};
内核中未定义的符号 st_shndx会设置成SHN_UNDEF.

内核中对用EXPORT_SYSBOL导出的符号，模块在编译的时候会为这个ELF文件生成一个段 .rel__ksymtab,它专门用于对__ksymtab段的重定位，称为relocation section.
这个段里面都是由 struct elf32_rel数组构成。
typedef struct elf32_rel {
ELf32_Addr r_offset;
Elf32_Word r_info;
}Elf32_Rel
系统根据r_offset得到需要修改的导出符号struct kernel_symble中的value所在的内存地址。
r_info获取得到符号表中的偏移量，即可得到struct Elf_Sym 结构，从结构中获取基地址，把这个基地址给上面的value,最终导致导出的符号地址被修改。

```
__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = { 
        .name = KBUILD_MODNAME,
        .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
        .exit = cleanup_module,
#endif
        .arch = MODULE_ARCH_INIT,
};
```
用.gnu.linkonce.this_module段表示struct module结构，在内核中
有这样定义：
extern struct module __this_module;
#define THIS_MODULE (&__this_module)
所以THIS_MODULE就指向了.gnu.linkonce.this_module段中的struct module 结构。


