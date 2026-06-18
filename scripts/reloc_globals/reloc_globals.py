import configparser
import os
import lief
import sys
import logging
import argparse
import runtime


def init_debug_logger(debug):
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG, format='%(message)s')
    logger = logging.getLogger()
    logger.disabled = not debug

    return logger


def process_device(elffile, rt_info, logger):
    logger.debug('Processing device file %s' % elffile.name)

    dev_globals = []
    # get source file name from input object file symbol table
    dev_src_file = [
        sym.name for sym in elffile.symbols if sym.type == lief.ELF.SYMBOL_TYPES.FILE
    ][0]
    for dev_sym in elffile.symbols:
        # skip xxx.bstart or xxx.bstop symbols
        if dev_sym.name.endswith('.bstart') or dev_sym.name.endswith('.bstop'):
            continue
        host_sym = rt_info.get_symbol_addr(dev_sym.name, dev_src_file)
        if host_sym is not None:
            logger.debug('Processing device symbol:\n %s' % dev_sym)
            logger.debug('Host global symbol info:\n %s' % host_sym)
            logger.debug(
                'Replace device variable address from 0x%x to 0x%x\n'
                % (dev_sym.value, host_sym)
            )
            # device symbol address point to host global symbol
            dev_sym.value = host_sym
            dev_globals.append(dev_sym)
    changed = False
    for reloc in elffile.relocations:
        if reloc.symbol in dev_globals:
            content_list = reloc.section.content.tolist()
            if '.text.body' in reloc.section.name:
                assert reloc.type == 2, "unexpected relocation type."
                changed = apply_relocation(reloc, 0, logger)
            elif reloc.section.name == '.data' or reloc.section.name.startswith(
                '.rodata'
            ):
                # relocation type 2 equals R_RISCV_64
                assert reloc.type == 2, "unexpected relocation type of %d." % reloc.type
                changed = apply_relocation(reloc, 0, logger)

    return changed


def apply_relocation(reloc, offset, logger):
    content_list = reloc.section.content.tolist()
    oringin_val = ''.join(
        map(
            lambda x: hex(x)[2:].zfill(2),
            reversed(content_list[reloc.address + offset : reloc.address + offset + 8]),
        )
    )
    # assert (hex2sint(
    #     oringin_val,
    #     64) == reloc.addend), 'unexpected addend value of global symbol.'
    reloc_value = hex(reloc.symbol.value + reloc.addend)[2:].zfill(16)
    logger.debug(
        'Apply global variable %s relocation from %s to %s'
        % (reloc.symbol.name, oringin_val, reloc_value)
    )

    for idx in range(8):
        content_list[reloc.address + offset + 7 - idx] = int(
            reloc_value[idx * 2 : (idx + 1) * 2 :], 16
        )

    # relocation type 0 means R_RISCV_NONE
    reloc.type = 0
    reloc.symbol = None
    setattr(reloc.section, 'content', content_list)
    return True


def hex2sint(hexstr, bits):
    value = int(hexstr, 16)
    if value & (1 << (bits - 1)):
        value -= 1 << bits
    return value


def collect_undef_symbols(undef_syms_set, def_syms_set, dev_elf):
    for sym in dev_elf.symbols:
        if (
            lief.ELF.SYMBOL_SECTION_INDEX(sym.shndx)
            == lief.ELF.SYMBOL_SECTION_INDEX.UNDEF
        ):
            undef_syms_set.add(sym.name)
        else:
            def_syms_set.add(sym.name)
    # all undefined symbols will have a weak symbol definition later,
    # so we can directly add a main symbol for input objects.
    undef_syms_set.add('main')


def get_undef_symbols_white_list(logger):
    conf_file = "reloc_globals.ini"
    if not os.path.exists(conf_file):
        base_path = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
        conf_file = os.path.join(base_path, conf_file)
    if not os.path.exists(conf_file):
        logger.warning("conf_file : %s does not exist! while list is empty!" % conf_file)
        return set()

    logger.debug('Porcessing white list: %s' % conf_file)
    conf = configparser.ConfigParser(allow_no_value=True)
    conf.read(conf_file, encoding='utf-8')
    white_list = set(conf['UndefSymbolWhiteList'].keys())
    return white_list


def generate_undef_symbols(undef_syms, def_syms, runtime_info, out_file, logger):
    binary = lief.parse(out_file)
    undef_syms = undef_syms - def_syms
    # exclude undefined symbol used in kernel function
    undef_syms = set(
        sym for sym in undef_syms if runtime_info.get_symbol_info(sym) is None
    )

    # undef symbols may be functions or global objects
    # block isa header is 16byte aligned and symbols should also align to 16 byte.
    stub_section = lief.ELF.Section()
    stub_section.name = ".undefined"
    stub_section.type = lief.ELF.SECTION_TYPES.PROGBITS
    stub_section.entry_size = 16
    stub_section.alignment = 16
    stub_section.content = [0] * 16 * len(undef_syms)
    binary.add(stub_section, False)

    white_list = get_undef_symbols_white_list(logger)
    for idx, sym in enumerate(undef_syms):
        if any(sym.endswith(white_list_sym) for white_list_sym in white_list):
            continue
        symbol = lief.ELF.Symbol()
        symbol.name = sym
        symbol.type = lief.ELF.SYMBOL_TYPES.OBJECT
        symbol.value = idx * 16
        symbol.binding = lief.ELF.SYMBOL_BINDINGS.WEAK
        symbol.size = 16
        symbol.shndx = len(binary.sections) - 1
        logger.debug("generated stub code for symbol %s." % sym)
        symbol = binary.add_static_symbol(symbol)

    binary.write(out_file)


def main():

    parser = argparse.ArgumentParser(add_help=False, prog=sys.argv[0])

    parser.add_argument(
        '-d',
        '--device',
        action='store',
        nargs='+',
        dest='device_binary',
        help='Input device binary file',
    )

    parser.add_argument(
        '-H', '--help', action='help', dest='help', help='Display this information'
    )

    parser.add_argument(
        '-D',
        '--debug',
        action='store_true',
        default=True,
        dest='debug_enable',
        help='Dump debug information',
    )

    parser.add_argument(
        '-r',
        '--runtime',
        action='store',
        dest='runtime_info',
        help='Input runtime info json',
    )

    args = parser.parse_args()

    runtime_info = runtime.parse(args.runtime_info)
    logger = init_debug_logger(args.debug_enable)

    dev_file = args.device_binary
    undef_symbol_set = set()
    def_symbol_set = set()
    for filename in dev_file:
        dev_elf = lief.parse(filename)
        collect_undef_symbols(undef_symbol_set, def_symbol_set, dev_elf)
        changed = process_device(dev_elf, runtime_info, logger)
        if changed:
            dev_elf.write(filename)

    # generate stub code for undefined symbols and write to last file
    if len(undef_symbol_set) > 0:
        generate_undef_symbols(
            undef_symbol_set, def_symbol_set, runtime_info, dev_file[-1], logger
        )


if __name__ == "__main__":
    main()
