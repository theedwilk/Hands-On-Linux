#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xe6616ea7, "module_layout" },
	{ 0xbb0dd01, "usb_deregister" },
	{ 0x32f6bfa8, "usb_register_driver" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0x3854774b, "kstrtoll" },
	{ 0x8fa06637, "kmem_cache_alloc_trace" },
	{ 0xaadfd013, "kmalloc_caches" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x93c7edeb, "usb_find_common_endpoints" },
	{ 0x70ae2d10, "sysfs_create_group" },
	{ 0xdb6d7e69, "kobject_create_and_add" },
	{ 0xac8dd6c9, "kernel_kobj" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0xb742fd7, "simple_strtol" },
	{ 0x5a921311, "strncmp" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0xa916b694, "strnlen" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x81ffdb90, "usb_bulk_msg" },
	{ 0x754d539c, "strlen" },
	{ 0xe914e41e, "strcpy" },
	{ 0x656e4a6e, "snprintf" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x37a0cba, "kfree" },
	{ 0xadfa5a31, "kobject_put" },
	{ 0x92997ed8, "_printk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v10C4pEA60d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "674658F3BE3531F73A721D7");
