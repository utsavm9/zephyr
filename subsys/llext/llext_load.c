/*
 * Copyright (c) 2023 Intel Corporation
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util.h>
#include <zephyr/llext/elf.h>
#include <zephyr/llext/loader.h>
#include <zephyr/llext/llext.h>
#include <zephyr/llext/llext_internal.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(llext, CONFIG_LLEXT_LOG_LEVEL);

#include <string.h>

#include "llext_priv.h"

/*
 * NOTICE: Functions in this file do not clean up allocations in their error
 * paths; instead, this is performed once and for all when leaving the parent
 * `do_llext_load()` function. This approach consolidates memory management
 * in a single place, simplifying error handling and reducing the risk of
 * memory leaks.
 *
 * The following rationale applies:
 *
 * - The input `struct llext` and fields in `struct loader` are zero-filled
 *   at the beginning of the do_llext_load function, so that every pointer is
 *   set to NULL and every bool is false.
 * - If some function called by do_llext_load allocates memory, it does so by
 *   immediately writing the pointer in the `ext` and `ldr` structures.
 * - do_llext_load() will clean up the memory allocated by the functions it
 *   calls, taking into account if the load process was successful or not.
 */

static const char ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};

const void *llext_loaded_sect_ptr(struct llext_loader *ldr, struct llext *ext, unsigned int sh_ndx)
{
	enum llext_mem mem_idx = ldr->sect_map[sh_ndx].mem_idx;

	if (mem_idx == LLEXT_MEM_COUNT) {
		return NULL;
	}

	return (const uint8_t *)ext->mem[mem_idx] + ldr->sect_map[sh_ndx].offset;
}

/*
 * Load basic ELF file data
 */

static int llext_load_elf_data(struct llext_loader *ldr, struct llext *ext)
{
	int ret;

	/* read ELF header */

	ret = llext_seek(ldr, 0);
	if (ret != 0) {
		LOG_ERR("Failed to seek for ELF header");
		return ret;
	}

	ret = llext_read(ldr, &ldr->hdr, sizeof(ldr->hdr));
	if (ret != 0) {
		LOG_ERR("Failed to read ELF header");
		return ret;
	}

	/* check whether this is a valid ELF file */
	if (memcmp(ldr->hdr.e_ident, ELF_MAGIC, sizeof(ELF_MAGIC)) != 0) {
		LOG_HEXDUMP_ERR(ldr->hdr.e_ident, 16, "Invalid ELF, magic does not match");
		return -ENOEXEC;
	}

	switch (ldr->hdr.e_type) {
	case ET_REL:
		LOG_DBG("Loading relocatable ELF");
		break;

	case ET_DYN:
		LOG_DBG("Loading shared ELF");
		break;

	default:
		LOG_ERR("Unsupported ELF file type %x", ldr->hdr.e_type);
		return -ENOEXEC;
	}

	/*
	 * Read all ELF section headers and initialize maps.  Buffers allocated
	 * below are freed when leaving do_llext_load(), so don't count them in
	 * alloc_size.
	 */

	if (ldr->hdr.e_shentsize != sizeof(elf_shdr_t)) {
		LOG_ERR("Invalid section header size %d", ldr->hdr.e_shentsize);
		return -ENOEXEC;
	}

	ext->sect_cnt = ldr->hdr.e_shnum;

	size_t sect_map_sz = ext->sect_cnt * sizeof(ldr->sect_map[0]);

	ldr->sect_map = llext_alloc(sect_map_sz);
	if (!ldr->sect_map) {
		LOG_ERR("Failed to allocate section map, size %zu", sect_map_sz);
		return -ENOMEM;
	}
	ext->alloc_size += sect_map_sz;
	for (int i = 0; i < ext->sect_cnt; i++) {
		ldr->sect_map[i].mem_idx = LLEXT_MEM_COUNT;
		ldr->sect_map[i].offset = 0;
	}

	ext->sect_hdrs = (elf_shdr_t *)llext_peek(ldr, ldr->hdr.e_shoff);
	if (ext->sect_hdrs) {
		ext->sect_hdrs_on_heap = false;
	} else {
		size_t sect_hdrs_sz = ext->sect_cnt * sizeof(ext->sect_hdrs[0]);

		ext->sect_hdrs_on_heap = true;
		ext->sect_hdrs = llext_alloc(sect_hdrs_sz);
		if (!ext->sect_hdrs) {
			LOG_ERR("Failed to allocate section headers, size %zu", sect_hdrs_sz);
			return -ENOMEM;
		}

		ret = llext_seek(ldr, ldr->hdr.e_shoff);
		if (ret != 0) {
			LOG_ERR("Failed to seek for section headers");
			return ret;
		}

		ret = llext_read(ldr, ext->sect_hdrs, sect_hdrs_sz);
		if (ret != 0) {
			LOG_ERR("Failed to read section headers");
			return ret;
		}
	}

	return 0;
}

/*
 * Find all relevant string and symbol tables
 */
static int llext_find_tables(struct llext_loader *ldr, struct llext *ext)
{
	int table_cnt, i;
	int shstrtab_ndx = ldr->hdr.e_shstrndx;
	int strtab_ndx = -1;

	memset(ldr->sects, 0, sizeof(ldr->sects));

	/* Find symbol and string tables */
	for (i = 0, table_cnt = 0; i < ext->sect_cnt && table_cnt < 3; ++i) {
		elf_shdr_t *shdr = ext->sect_hdrs + i;

		LOG_DBG("section %d at 0x%zx: name %d, type %d, flags 0x%zx, "
			"addr 0x%zx, size %zd, link %d, info %d",
			i,
			(size_t)shdr->sh_offset,
			shdr->sh_name,
			shdr->sh_type,
			(size_t)shdr->sh_flags,
			(size_t)shdr->sh_addr,
			(size_t)shdr->sh_size,
			shdr->sh_link,
			shdr->sh_info);

		if (shdr->sh_type == SHT_SYMTAB && ldr->hdr.e_type == ET_REL) {
			LOG_DBG("symtab at %d", i);
			ldr->sects[LLEXT_MEM_SYMTAB] = *shdr;
			ldr->sect_map[i].mem_idx = LLEXT_MEM_SYMTAB;
			strtab_ndx = shdr->sh_link;
			table_cnt++;
		} else if (shdr->sh_type == SHT_DYNSYM && ldr->hdr.e_type == ET_DYN) {
			LOG_DBG("dynsym at %d", i);
			ldr->sects[LLEXT_MEM_SYMTAB] = *shdr;
			ldr->sect_map[i].mem_idx = LLEXT_MEM_SYMTAB;
			strtab_ndx = shdr->sh_link;
			table_cnt++;
		} else if (shdr->sh_type == SHT_STRTAB && i == shstrtab_ndx) {
			LOG_DBG("shstrtab at %d", i);
			ldr->sects[LLEXT_MEM_SHSTRTAB] = *shdr;
			ldr->sect_map[i].mem_idx = LLEXT_MEM_SHSTRTAB;
			table_cnt++;
		} else if (shdr->sh_type == SHT_STRTAB && i == strtab_ndx) {
			LOG_DBG("strtab at %d", i);
			ldr->sects[LLEXT_MEM_STRTAB] = *shdr;
			ldr->sect_map[i].mem_idx = LLEXT_MEM_STRTAB;
			table_cnt++;
		}
	}

	if (!ldr->sects[LLEXT_MEM_SHSTRTAB].sh_type ||
	    !ldr->sects[LLEXT_MEM_STRTAB].sh_type ||
	    !ldr->sects[LLEXT_MEM_SYMTAB].sh_type) {
		LOG_ERR("Some sections are missing or present multiple times!");
		return -ENOEXEC;
	}

	return 0;
}

/*
 * Maps the ELF sections into regions according to their usage flags,
 * calculating ldr->sects and ldr->sect_map.
 */
static int llext_map_sections(struct llext_loader *ldr, struct llext *ext,
			      const struct llext_load_param *ldr_parm)
{
	int i, j;
	const char *name;

	for (i = 0; i < ext->sect_cnt; ++i) {
		elf_shdr_t *shdr = ext->sect_hdrs + i;

		name = llext_string(ldr, ext, LLEXT_MEM_SHSTRTAB, shdr->sh_name);

		if (ldr->sect_map[i].mem_idx != LLEXT_MEM_COUNT) {
			LOG_DBG("section %d name %s already mapped to region %d",
				i, name, ldr->sect_map[i].mem_idx);
			continue;
		}

		/* Identify the section type by its flags */
		enum llext_mem mem_idx;

		switch (shdr->sh_type) {
		case SHT_NOBITS:
			mem_idx = LLEXT_MEM_BSS;
			break;
		case SHT_PROGBITS:
			if (shdr->sh_flags & SHF_EXECINSTR) {
				mem_idx = LLEXT_MEM_TEXT;
			} else if (shdr->sh_flags & SHF_WRITE) {
				mem_idx = LLEXT_MEM_DATA;
			} else {
				mem_idx = LLEXT_MEM_RODATA;
			}
			break;
		case SHT_PREINIT_ARRAY:
			mem_idx = LLEXT_MEM_PREINIT;
			break;
		case SHT_INIT_ARRAY:
			mem_idx = LLEXT_MEM_INIT;
			break;
		case SHT_FINI_ARRAY:
			mem_idx = LLEXT_MEM_FINI;
			break;
		default:
			mem_idx = LLEXT_MEM_COUNT;
			break;
		}

		/* Special exception for .exported_sym */
		if (strcmp(name, ".exported_sym") == 0) {
			mem_idx = LLEXT_MEM_EXPORT;
		}

		if (mem_idx == LLEXT_MEM_COUNT ||
		    !(shdr->sh_flags & SHF_ALLOC) ||
		    shdr->sh_size == 0) {
			LOG_DBG("section %d name %s skipped", i, name);
			continue;
		}

		switch (mem_idx) {
		case LLEXT_MEM_PREINIT:
		case LLEXT_MEM_INIT:
		case LLEXT_MEM_FINI:
			if (shdr->sh_entsize != sizeof(void *) ||
			    shdr->sh_size % shdr->sh_entsize != 0) {
				LOG_ERR("Invalid %s array in section %d", name, i);
				return -ENOEXEC;
			}
		default:
			break;
		}

		LOG_DBG("section %d name %s maps to region %d", i, name, mem_idx);

		ldr->sect_map[i].mem_idx = mem_idx;
		elf_shdr_t *region = ldr->sects + mem_idx;

		/*
		 * ELF objects can have sections for memory regions, detached from
		 * other sections of the same type. E.g. executable sections that will be
		 * placed in slower memory. Don't merge such sections into main regions
		 */
		if (ldr_parm->section_detached && ldr_parm->section_detached(shdr)) {
			continue;
		}

		if (region->sh_type == SHT_NULL) {
			/* First section of this type, copy all info to the
			 * region descriptor.
			 */
			memcpy(region, shdr, sizeof(*region));
		} else {
			/* Make sure this section is compatible with the region */
			if ((shdr->sh_flags & SHF_BASIC_TYPE_MASK) !=
			    (region->sh_flags & SHF_BASIC_TYPE_MASK)) {
				LOG_ERR("Unsupported section flags %#x / %#x for %s (region %d)",
					(uint32_t)shdr->sh_flags, (uint32_t)region->sh_flags,
					name, mem_idx);
				return -ENOEXEC;
			}

			/* Check if this region type is extendable */
			switch (mem_idx) {
			case LLEXT_MEM_BSS:
				/* SHT_NOBITS sections cannot be merged properly:
				 * as they use no space in the file, the logic
				 * below does not work; they must be treated as
				 * independent entities.
				 */
				LOG_ERR("Multiple SHT_NOBITS sections are not supported");
				return -ENOTSUP;
			case LLEXT_MEM_PREINIT:
			case LLEXT_MEM_INIT:
			case LLEXT_MEM_FINI:
				/* These regions are not extendable and must be
				 * referenced at most once in the ELF file.
				 */
				LOG_ERR("Region %d redefined", mem_idx);
				return -ENOEXEC;
			default:
				break;
			}

			if (ldr->hdr.e_type == ET_DYN) {
				/* In shared objects, sh_addr is the VMA.
				 * Before merging this section in the region,
				 * make sure the delta in VMAs matches that of
				 * file offsets.
				 */
				if (shdr->sh_addr - region->sh_addr !=
				    shdr->sh_offset - region->sh_offset) {
					LOG_ERR("Incompatible section addresses "
						"for %s (region %d)", name, mem_idx);
					return -ENOEXEC;
				}
			}

			/*
			 * Extend the current region to include the new section
			 * (overlaps are detected later)
			 */
			size_t address = MIN(region->sh_addr, shdr->sh_addr);
			size_t bot_ofs = MIN(region->sh_offset, shdr->sh_offset);
			size_t top_ofs = MAX(region->sh_offset + region->sh_size,
					     shdr->sh_offset + shdr->sh_size);

			region->sh_addr = address;
			region->sh_offset = bot_ofs;
			region->sh_size = top_ofs - bot_ofs;
		}
	}

	/*
	 * Test that no computed region overlaps. This can happen if sections of
	 * different llext_mem type are interleaved in the ELF file or in VMAs.
	 */
	for (i = 0; i < LLEXT_MEM_COUNT; i++) {
		for (j = i+1; j < LLEXT_MEM_COUNT; j++) {
			elf_shdr_t *x = ldr->sects + i;
			elf_shdr_t *y = ldr->sects + j;

			if (x->sh_type == SHT_NULL || x->sh_size == 0 ||
			    y->sh_type == SHT_NULL || y->sh_size == 0) {
				/* Skip empty regions */
				continue;
			}

			/*
			 * The export symbol table may be surrounded by
			 * other data sections. Ignore overlaps in that
			 * case.
			 */
			if ((i == LLEXT_MEM_DATA || i == LLEXT_MEM_RODATA) &&
			    j == LLEXT_MEM_EXPORT) {
				continue;
			}

			/*
			 * Exported symbols region can also overlap
			 * with rodata.
			 */
			if (i == LLEXT_MEM_EXPORT || j == LLEXT_MEM_EXPORT) {
				continue;
			}

			if ((ldr->hdr.e_type == ET_DYN) &&
			    (x->sh_flags & SHF_ALLOC) && (y->sh_flags & SHF_ALLOC)) {
				/*
				 * Test regions that have VMA ranges for overlaps
				 */
				if ((x->sh_addr <= y->sh_addr &&
				     x->sh_addr + x->sh_size > y->sh_addr) ||
				    (y->sh_addr <= x->sh_addr &&
				     y->sh_addr + y->sh_size > x->sh_addr)) {
					LOG_ERR("Region %d VMA range (0x%zx +%zd) "
						"overlaps with %d (0x%zx +%zd)",
						i, (size_t)x->sh_addr, (size_t)x->sh_size,
						j, (size_t)y->sh_addr, (size_t)y->sh_size);
					return -ENOEXEC;
				}
			}

			/*
			 * Test file offsets. BSS sections store no
			 * data in the file and must not be included
			 * in checks to avoid false positives.
			 */
			if (i == LLEXT_MEM_BSS || j == LLEXT_MEM_BSS) {
				continue;
			}

			if ((x->sh_offset <= y->sh_offset &&
			     x->sh_offset + x->sh_size > y->sh_offset) ||
			    (y->sh_offset <= x->sh_offset &&
			     y->sh_offset + y->sh_size > x->sh_offset)) {
				LOG_ERR("Region %d ELF file range (0x%zx +%zd) "
					"overlaps with %d (0x%zx +%zd)",
					i, (size_t)x->sh_offset, (size_t)x->sh_size,
					j, (size_t)y->sh_offset, (size_t)y->sh_size);
				return -ENOEXEC;
			}
		}
	}

	/*
	 * Calculate each ELF section's offset inside its memory region. This
	 * is done as a separate pass so the final regions are already defined.
	 */
	for (i = 0; i < ext->sect_cnt; ++i) {
		elf_shdr_t *shdr = ext->sect_hdrs + i;
		enum llext_mem mem_idx = ldr->sect_map[i].mem_idx;

		if (mem_idx != LLEXT_MEM_COUNT) {
			ldr->sect_map[i].offset = shdr->sh_offset - ldr->sects[mem_idx].sh_offset;
		}
	}

	return 0;
}

static int llext_count_export_syms(struct llext_loader *ldr, struct llext *ext)
{
	size_t ent_size = ldr->sects[LLEXT_MEM_SYMTAB].sh_entsize;
	size_t syms_size = ldr->sects[LLEXT_MEM_SYMTAB].sh_size;
	int sym_cnt = syms_size / sizeof(elf_sym_t);
	const char *name;
	elf_sym_t sym;
	int i, ret;
	size_t pos;

	LOG_DBG("symbol count %u", sym_cnt);

	ext->sym_tab.sym_cnt = 0;
	for (i = 0, pos = ldr->sects[LLEXT_MEM_SYMTAB].sh_offset;
	     i < sym_cnt;
	     i++, pos += ent_size) {
		if (!i) {
			/* A dummy entry */
			continue;
		}

		ret = llext_seek(ldr, pos);
		if (ret != 0) {
			return ret;
		}

		ret = llext_read(ldr, &sym, ent_size);
		if (ret != 0) {
			return ret;
		}

		uint32_t stt = ELF_ST_TYPE(sym.st_info);
		uint32_t stb = ELF_ST_BIND(sym.st_info);
		uint32_t sect = sym.st_shndx;

		name = llext_string(ldr, ext, LLEXT_MEM_STRTAB, sym.st_name);

		if ((stt == STT_FUNC || stt == STT_OBJECT) && stb == STB_GLOBAL) {
			LOG_DBG("function symbol %d, name %s, type tag %d, bind %d, sect %d",
				i, name, stt, stb, sect);
			ext->sym_tab.sym_cnt++;
		} else {
			LOG_DBG("unhandled symbol %d, name %s, type tag %d, bind %d, sect %d",
				i, name, stt, stb, sect);
		}
	}

	return 0;
}

static int llext_allocate_symtab(struct llext_loader *ldr, struct llext *ext)
{
	struct llext_symtable *sym_tab = &ext->sym_tab;
	size_t syms_size = sym_tab->sym_cnt * sizeof(struct llext_symbol);

	sym_tab->syms = llext_alloc(syms_size);
	if (!sym_tab->syms) {
		return -ENOMEM;
	}
	memset(sym_tab->syms, 0, syms_size);
	ext->alloc_size += syms_size;

	return 0;
}

static int llext_export_symbols(struct llext_loader *ldr, struct llext *ext,
				const struct llext_load_param *ldr_parm)
{
	struct llext_symtable *exp_tab = &ext->exp_tab;
	struct llext_symbol *sym;
	unsigned int i;

	if (IS_ENABLED(CONFIG_LLEXT_IMPORT_ALL_GLOBALS)) {
		/* Use already discovered global symbols */
		exp_tab->sym_cnt = ext->sym_tab.sym_cnt;
		sym = ext->sym_tab.syms;
	} else {
		/* Only use symbols in the .exported_sym section */
		exp_tab->sym_cnt = ldr->sects[LLEXT_MEM_EXPORT].sh_size
				   / sizeof(struct llext_symbol);
		sym = ext->mem[LLEXT_MEM_EXPORT];
	}

	if (!exp_tab->sym_cnt) {
		/* No symbols exported */
		return 0;
	}

	exp_tab->syms = llext_alloc(exp_tab->sym_cnt * sizeof(struct llext_symbol));
	if (!exp_tab->syms) {
		return -ENOMEM;
	}

	for (i = 0; i < exp_tab->sym_cnt; i++, sym++) {
		/*
		 * Offsets in objects, built for pre-defined addresses have to
		 * be translated to memory locations for symbol name access
		 * during dependency resolution.
		 */
		const char *name = NULL;

		if (ldr_parm && ldr_parm->pre_located) {
			ssize_t name_offset = llext_file_offset(ldr, (uintptr_t)sym->name);

			if (name_offset > 0) {
				name = llext_peek(ldr, name_offset);
			}
		}
		if (!name) {
			name = sym->name;
		}

		exp_tab->syms[i].name = name;
		exp_tab->syms[i].addr = sym->addr;
		LOG_DBG("sym %p name %s", sym->addr, sym->name);
	}

	return 0;
}

static int llext_copy_symbols(struct llext_loader *ldr, struct llext *ext,
			      const struct llext_load_param *ldr_parm)
{
	size_t ent_size = ldr->sects[LLEXT_MEM_SYMTAB].sh_entsize;
	size_t syms_size = ldr->sects[LLEXT_MEM_SYMTAB].sh_size;
	int sym_cnt = syms_size / sizeof(elf_sym_t);
	struct llext_symtable *sym_tab = &ext->sym_tab;
	elf_sym_t sym;
	int i, j, ret;
	size_t pos;

	for (i = 0, pos = ldr->sects[LLEXT_MEM_SYMTAB].sh_offset, j = 0;
	     i < sym_cnt;
	     i++, pos += ent_size) {
		if (!i) {
			/* A dummy entry */
			continue;
		}

		ret = llext_seek(ldr, pos);
		if (ret != 0) {
			return ret;
		}

		ret = llext_read(ldr, &sym, ent_size);
		if (ret != 0) {
			return ret;
		}

		uint32_t stt = ELF_ST_TYPE(sym.st_info);
		uint32_t stb = ELF_ST_BIND(sym.st_info);
		unsigned int shndx = sym.st_shndx;

		if ((stt == STT_FUNC || stt == STT_OBJECT) &&
		    stb == STB_GLOBAL && shndx != SHN_UNDEF) {
			const char *name = llext_string(ldr, ext, LLEXT_MEM_STRTAB, sym.st_name);

			__ASSERT(j <= sym_tab->sym_cnt, "Miscalculated symbol number %u\n", j);

			sym_tab->syms[j].name = name;

			elf_shdr_t *shdr = ext->sect_hdrs + shndx;
			uintptr_t section_addr = shdr->sh_addr;

			if (ldr_parm->pre_located &&
			    (!ldr_parm->section_detached || !ldr_parm->section_detached(shdr))) {
				sym_tab->syms[j].addr = (uint8_t *)sym.st_value +
					(ldr->hdr.e_type == ET_REL ? section_addr : 0);
			} else {
				const void *base;

				base = llext_loaded_sect_ptr(ldr, ext, shndx);
				if (!base) {
					/* If the section is not mapped, try to peek.
					 * Be noisy about it, since this is addressing
					 * data that was missed by llext_map_sections.
					 */
					base = llext_peek(ldr, shdr->sh_offset);
					if (base) {
						LOG_DBG("section %d peeked at %p", shndx, base);
					} else {
						LOG_ERR("No data for section %d", shndx);
						return -ENOTSUP;
					}
				}

				sym_tab->syms[j].addr = (uint8_t *)base + sym.st_value -
					(ldr->hdr.e_type == ET_REL ? 0 : section_addr);
			}

			LOG_DBG("function symbol %d name %s addr %p",
				j, name, sym_tab->syms[j].addr);
			j++;
		}
	}

	return 0;
}

/*
 * Load a valid ELF as an extension
 */
int do_llext_load(struct llext_loader *ldr, struct llext *ext,
		  const struct llext_load_param *ldr_parm)
{
	const struct llext_load_param default_ldr_parm = LLEXT_LOAD_PARAM_DEFAULT;
	int ret;

	if (!ldr_parm) {
		ldr_parm = &default_ldr_parm;
	}

	/* Zero all memory that is affected by the loading process
	 * (see the NOTICE at the top of this file).
	 */
	memset(ext, 0, sizeof(*ext));
	ldr->sect_map = NULL;

	LOG_DBG("Loading ELF data...");
	ret = llext_prepare(ldr);
	if (ret != 0) {
		LOG_ERR("Failed to prepare the loader, ret %d", ret);
		goto out;
	}

	ret = llext_load_elf_data(ldr, ext);
	if (ret != 0) {
		LOG_ERR("Failed to load basic ELF data, ret %d", ret);
		goto out;
	}

#ifdef CONFIG_USERSPACE
	ret = k_mem_domain_init(&ext->mem_domain, 0, NULL);
	if (ret != 0) {
		LOG_ERR("Failed to initialize extenion memory domain %d", ret);
		goto out;
	}
#endif

	LOG_DBG("Finding ELF tables...");
	ret = llext_find_tables(ldr, ext);
	if (ret != 0) {
		LOG_ERR("Failed to find important ELF tables, ret %d", ret);
		goto out;
	}

	LOG_DBG("Allocate and copy strings...");
	ret = llext_copy_strings(ldr, ext);
	if (ret != 0) {
		LOG_ERR("Failed to copy ELF string sections, ret %d", ret);
		goto out;
	}

	LOG_DBG("Mapping ELF sections...");
	ret = llext_map_sections(ldr, ext, ldr_parm);
	if (ret != 0) {
		LOG_ERR("Failed to map ELF sections, ret %d", ret);
		goto out;
	}

	LOG_DBG("Allocate and copy regions...");
	ret = llext_copy_regions(ldr, ext, ldr_parm);
	if (ret != 0) {
		LOG_ERR("Failed to copy regions, ret %d", ret);
		goto out;
	}

	LOG_DBG("Counting exported symbols...");
	ret = llext_count_export_syms(ldr, ext);
	if (ret != 0) {
		LOG_ERR("Failed to count exported ELF symbols, ret %d", ret);
		goto out;
	}

	LOG_DBG("Allocating memory for symbol table...");
	ret = llext_allocate_symtab(ldr, ext);
	if (ret != 0) {
		LOG_ERR("Failed to allocate extension symbol table, ret %d", ret);
		goto out;
	}

	LOG_DBG("Copying symbols...");
	ret = llext_copy_symbols(ldr, ext, ldr_parm);
	if (ret != 0) {
		LOG_ERR("Failed to copy symbols, ret %d", ret);
		goto out;
	}

	if (ldr_parm->relocate_local) {
		LOG_DBG("Linking ELF...");
		ret = llext_link(ldr, ext, ldr_parm);
		if (ret != 0) {
			LOG_ERR("Failed to link, ret %d", ret);
			goto out;
		}
	}

	ret = llext_export_symbols(ldr, ext, ldr_parm);
	if (ret != 0) {
		LOG_ERR("Failed to export, ret %d", ret);
		goto out;
	}

	if (!ldr_parm->pre_located) {
		llext_adjust_mmu_permissions(ext);
	}

out:
	/*
	 * Free resources only used during loading, unless explicitly requested.
	 * Note that this exploits the fact that freeing a NULL pointer has no effect.
	 */

	if (ret != 0 || !ldr_parm || !ldr_parm->keep_section_info) {
		llext_free_inspection_data(ldr, ext);
	}

	/* Until proper inter-llext linking is implemented, the symbol table is
	 * not useful outside of the loading process; keep it only if debugging
	 * is enabled and no error is detected.
	 */
	if (!(IS_ENABLED(CONFIG_LLEXT_LOG_LEVEL_DBG) && ret == 0)) {
		llext_free(ext->sym_tab.syms);
		ext->sym_tab.sym_cnt = 0;
		ext->sym_tab.syms = NULL;
	}

	if (ret != 0) {
		LOG_DBG("Failed to load extension: %d", ret);

		/* Since the loading process failed, free the resources that
		 * were allocated for the lifetime of the extension as well,
		 * such as regions and exported symbols.
		 */
		llext_free_regions(ext);
		llext_free(ext->exp_tab.syms);
		ext->exp_tab.sym_cnt = 0;
		ext->exp_tab.syms = NULL;
	} else {
		LOG_DBG("loaded module, .text at %p, .rodata at %p", ext->mem[LLEXT_MEM_TEXT],
			ext->mem[LLEXT_MEM_RODATA]);
	}

	llext_finalize(ldr);

	return ret;
}

int llext_free_inspection_data(struct llext_loader *ldr, struct llext *ext)
{
	if (ldr->sect_map) {
		ext->alloc_size -= ext->sect_cnt * sizeof(ldr->sect_map[0]);
		llext_free(ldr->sect_map);
		ldr->sect_map = NULL;
	}

	return 0;
}
