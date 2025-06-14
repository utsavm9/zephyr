#!/usr/bin/env python3
#
# Copyright (c) 2017 Intel Corporation
# Copyright (c) 2018 Foundries.io
# Copyright (c) 2023 Nordic Semiconductor NA
#
# SPDX-License-Identifier: Apache-2.0
#

import struct

class gen_isr_parser:
    source_header = """
/* AUTO-GENERATED by gen_isr_tables.py, do not edit! */

#include <zephyr/toolchain.h>
#include <zephyr/linker/sections.h>
#include <zephyr/sw_isr_table.h>
#include <zephyr/arch/cpu.h>

typedef void (* ISR)(const void *);
"""

    source_assembly_header = """
#ifndef ARCH_IRQ_VECTOR_JUMP_CODE
#error "ARCH_IRQ_VECTOR_JUMP_CODE not defined"
#endif
"""

    def __init__(self, intlist_data, config, log):
        """Initialize the parser.

        The function prepares parser to work.
        Parameters:
        - intlist_data: The binnary data from intlist section
        - config: The configuration object
        - log: The logging object, has to have error and debug methods
        """
        self.__config = config
        self.__log = log
        intlist = self.__read_intlist(intlist_data)
        self.__vt, self.__swt, self.__nv = self.__parse_intlist(intlist)

    def __read_intlist(self, intlist_data):
        """read a binary file containing the contents of the kernel's .intList
        section. This is an instance of a header created by
        include/zephyr/linker/intlist.ld:

         struct {
           uint32_t num_vectors;       <- typically CONFIG_NUM_IRQS
           struct _isr_list isrs[]; <- Usually of smaller size than num_vectors
        }

        Followed by instances of struct _isr_list created by IRQ_CONNECT()
        calls:

        struct _isr_list {
            /** IRQ line number */
            int32_t irq;
            /** Flags for this IRQ, see ISR_FLAG_* definitions */
            int32_t flags;
            /** ISR to call */
            void *func;
            /** Parameter for non-direct IRQs */
            const void *param;
        };
        """
        intlist = {}
        prefix = self.__config.endian_prefix()

        # Extract header and the rest of the data
        intlist_header_fmt = prefix + "II"
        header_sz = struct.calcsize(intlist_header_fmt)
        header_raw = struct.unpack_from(intlist_header_fmt, intlist_data, 0)
        self.__log.debug(str(header_raw))

        intlist["num_vectors"]    = header_raw[0]
        intlist["offset"]         = header_raw[1]
        intdata = intlist_data[header_sz:]

        # Extract information about interrupts
        if self.__config.check_64b():
            intlist_entry_fmt = prefix + "iiQQ"
        else:
            intlist_entry_fmt = prefix + "iiII"

        intlist["interrupts"] = [i for i in
                struct.iter_unpack(intlist_entry_fmt, intdata)]

        self.__log.debug("Configured interrupt routing")
        self.__log.debug("handler    irq flags param")
        self.__log.debug("--------------------------")

        for irq in intlist["interrupts"]:
            self.__log.debug("{0:<10} {1:<3} {2:<3}   {3}".format(
                hex(irq[2]), irq[0], irq[1], hex(irq[3])))

        return intlist

    def __parse_intlist(self, intlist):
        """All the intlist data are parsed into swt and vt arrays.

        The vt array is prepared for hardware interrupt table.
        Every entry in the selected position would contain None or the name of the function pointer
        (address or string).

        The swt is a little more complex. At every position it would contain an array of parameter and
        function pointer pairs. If CONFIG_SHARED_INTERRUPTS is enabled there may be more than 1 entry.
        If empty array is placed on selected position - it means that the application does not implement
        this interrupt.

        Parameters:
        - intlist: The preprocessed list of intlist section content (see read_intlist)

        Return:
        vt, swt - parsed vt and swt arrays (see function description above)
        """
        nvec = intlist["num_vectors"]
        offset = intlist["offset"]

        if nvec > pow(2, 15):
            raise ValueError('nvec is too large, check endianness.')

        self.__log.debug('offset is ' + str(offset))
        self.__log.debug('num_vectors is ' + str(nvec))

        # Set default entries in both tables
        if not(self.__config.args.sw_isr_table or self.__config.args.vector_table):
            self.__log.error("one or both of -s or -V needs to be specified on command line")
        if self.__config.args.vector_table:
            vt = [None for i in range(nvec)]
        else:
            vt = None
        if self.__config.args.sw_isr_table:
            swt = [[] for i in range(nvec)]
        else:
            swt = None

        # Process intlist and write to the tables created
        for irq, flags, func, param in intlist["interrupts"]:
            if self.__config.test_isr_direct(flags):
                if not vt:
                    self.__log.error("Direct Interrupt %d declared with parameter 0x%x "
                                     "but no vector table in use"
                                      % (irq, param))
                if param != 0:
                    self.__log.error("Direct irq %d declared, but has non-NULL parameter"
                                     % irq)
                if not 0 <= irq - offset < len(vt):
                    self.__log.error("IRQ %d (offset=%d) exceeds the maximum of %d"
                                     % (irq - offset, offset, len(vt) - 1))
                vt[irq - offset] = func
            else:
                # Regular interrupt
                if not swt:
                    self.__log.error("Regular Interrupt %d declared with parameter 0x%x "
                                     "but no SW ISR_TABLE in use"
                                     % (irq, param))

                table_index = self.__config.get_swt_table_index(offset, irq)

                if not 0 <= table_index < len(swt):
                    self.__log.error("IRQ %d (offset=%d) exceeds the maximum of %d" %
                                     (table_index, offset, len(swt) - 1))
                if self.__config.check_shared_interrupts():
                    lst = swt[table_index]
                    if (param, func) in lst:
                        self.__log.error("Attempting to register the same ISR/arg pair twice.")
                    if len(lst) >= self.__config.get_sym("CONFIG_SHARED_IRQ_MAX_NUM_CLIENTS"):
                        self.__log.error(f"Reached shared interrupt client limit. Maybe increase"
                                         + f" CONFIG_SHARED_IRQ_MAX_NUM_CLIENTS?")
                else:
                    if len(swt[table_index]) > 0:
                        self.__log.error(f"multiple registrations at table_index {table_index} for irq {irq} (0x{irq:x})"
                                         + f"\nExisting handler 0x{swt[table_index][0][1]:x}, new handler 0x{func:x}"
                                         + "\nHas IRQ_CONNECT or IRQ_DIRECT_CONNECT accidentally been invoked on the same irq multiple times?"
                        )
                swt[table_index].append((param, func))

        return vt, swt, nvec

    def __write_code_irq_vector_table(self, fp):
        fp.write(self.source_assembly_header)

        fp.write("void __irq_vector_table __attribute__((naked)) _irq_vector_table(void) {\n")
        for i in range(self.__nv):
            func = self.__vt[i]

            if func is None:
                func = self.__config.vt_default_handler

            if isinstance(func, int):
                func_as_string = self.__config.get_sym_from_addr(func)
            else:
                func_as_string = func

            fp.write("\t__asm(ARCH_IRQ_VECTOR_JUMP_CODE({}));\n".format(func_as_string))
        fp.write("}\n")

    def __write_address_irq_vector_table(self, fp):
        fp.write("const uintptr_t __irq_vector_table _irq_vector_table[%d] = {\n" % self.__nv)
        for i in range(self.__nv):
            func = self.__vt[i]

            if func is None:
                func = self.__config.vt_default_handler

            if isinstance(func, int):
                fp.write("\t{},\n".format(func))
            else:
                fp.write("\t((uintptr_t)&{}),\n".format(func))

        fp.write("};\n")

    def __write_shared_table(self, fp):
        if not self.__config.check_sym("CONFIG_DYNAMIC_INTERRUPTS"):
            fp.write("const ")
        fp.write("struct z_shared_isr_table_entry __shared_sw_isr_table"
                " z_shared_sw_isr_table[%d] = {\n" % self.__nv)

        for i in range(self.__nv):
            if self.__swt[i] is None:
                client_num = 0
                client_list = None
            else:
                client_num = len(self.__swt[i])
                client_list = self.__swt[i]

            if client_num <= 1:
                fp.write("\t{ },\n")
            else:
                fp.write(f"\t{{ .client_num = {client_num}, .clients = {{ ")
                for j in range(0, client_num):
                    routine = client_list[j][1]
                    arg = client_list[j][0]

                    fp.write(f"{{ .isr = (ISR){ hex(routine) if isinstance(routine, int) else routine }, "
                            f".arg = (const void *){hex(arg)} }},")

                fp.write(" },\n},\n")

        fp.write("};\n")

    def write_source(self, fp):
        fp.write(self.source_header)

        if self.__config.check_shared_interrupts():
            self.__write_shared_table(fp)

        if self.__vt:
            if self.__config.check_sym("CONFIG_IRQ_VECTOR_TABLE_JUMP_BY_ADDRESS"):
                self.__write_address_irq_vector_table(fp)
            elif self.__config.check_sym("CONFIG_IRQ_VECTOR_TABLE_JUMP_BY_CODE"):
                self.__write_code_irq_vector_table(fp)
            else:
                self.__log.error("CONFIG_IRQ_VECTOR_TABLE_JUMP_BY_{ADDRESS,CODE} not set")

        if not self.__swt:
            return

        if not self.__config.check_sym("CONFIG_DYNAMIC_INTERRUPTS"):
            fp.write("const ")
        fp.write("struct _isr_table_entry __sw_isr_table _sw_isr_table[%d] = {\n"
                % self.__nv)

        level2_offset = self.__config.get_irq_baseoffset(2)
        level3_offset = self.__config.get_irq_baseoffset(3)

        for i in range(self.__nv):
            if len(self.__swt[i]) == 0:
                # Not used interrupt
                param = "0x0"
                func = self.__config.swt_spurious_handler
            elif len(self.__swt[i]) == 1:
                # Single interrupt
                param = "{0:#x}".format(self.__swt[i][0][0])
                func = self.__swt[i][0][1]
            else:
                # Shared interrupt
                param = "&z_shared_sw_isr_table[{0}]".format(i)
                func = self.__config.swt_shared_handler

            if isinstance(func, int):
                func_as_string = "{0:#x}".format(func)
            else:
                func_as_string = func

            if level2_offset is not None and i == level2_offset:
                fp.write("\t/* Level 2 interrupts start here (offset: {}) */\n".
                         format(level2_offset))
            if level3_offset is not None and i == level3_offset:
                fp.write("\t/* Level 3 interrupts start here (offset: {}) */\n".
                         format(level3_offset))

            fp.write("\t{{(const void *){0}, (ISR){1}}}, /* {2} */\n".format(param, func_as_string, i))
        fp.write("};\n")
