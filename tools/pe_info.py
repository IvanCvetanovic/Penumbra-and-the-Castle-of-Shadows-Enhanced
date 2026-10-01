"""Print a Windows PE file's header facts: machine, section table, timestamp and imports.

Used while decoding the original's executables (docs/spec); nothing in the build uses it.

    python tools/pe_info.py <file.exe or file.dll>...
"""

import struct, datetime

def parse_pe(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] != b'MZ':
        print(path, 'not MZ'); return
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    if data[e_lfanew:e_lfanew+4] != b'PE\x00\x00':
        print(path, 'not PE'); return
    coff = e_lfanew + 4
    machine, nsec, ts = struct.unpack_from('<HHI', data, coff)
    opt_off = coff + 20
    magic = struct.unpack_from('<H', data, opt_off)[0]
    pe32 = (magic == 0x10b)
    ptr_size = '<I' if pe32 else '<Q'
    size_size = 4 if pe32 else 8
    ep = struct.unpack_from('<I', data, opt_off+16)[0]
    base = struct.unpack_from(ptr_size, data, opt_off + (28 if pe32 else 24))[0]
    subsystem = struct.unpack_from('<H', data, opt_off+68)[0]
    dllchars = struct.unpack_from('<H', data, opt_off+70)[0]
    dirs_off = opt_off + (96 if pe32 else 112)
    import_rva, import_size = struct.unpack_from('<II', data, dirs_off+8)
    sec_off = opt_off + struct.unpack_from('<H', data, coff+16)[0]
    secs = []
    for i in range(nsec):
        so = sec_off + i*40
        name = data[so:so+8].rstrip(b'\x00').decode('ascii', 'replace')
        vsz, va, rawsz, rawptr = struct.unpack_from('<IIII', data, so+8)
        secs.append((name, va, vsz, rawptr, rawsz))

    def rva2off(rva):
        for name, va, vsz, rawptr, rawsz in secs:
            if va <= rva < va + max(vsz, rawsz):
                return rawptr + (rva - va)
        return None

    imports = []
    if import_rva:
        io = rva2off(import_rva)
        if io is not None:
            idx = 0
            while True:
                ent = data[io+idx*20: io+idx*20+20]
                if len(ent) < 20: break
                oft, tsts, fwd, name_rva, first_thunk = struct.unpack('<IIIII', ent)
                if name_rva == 0 and oft == 0 and first_thunk == 0: break
                no = rva2off(name_rva)
                name = b''
                if no is not None:
                    end = data.index(b'\x00', no)
                    name = data[no:end]
                th_rva = first_thunk if first_thunk else oft
                names = []
                toff = rva2off(th_rva) if th_rva else None
                if toff is not None:
                    j = 0
                    while len(names) < 500:
                        try:
                            ent2 = struct.unpack_from(ptr_size, data, toff + j*size_size)[0]
                        except struct.error:
                            break
                        if ent2 == 0: break
                        hint_flag = 1 << (63 if not pe32 else 31)
                        if not (ent2 & hint_flag):
                            ho = rva2off(ent2 & 0x7FFFFFFF)
                            if ho is not None:
                                hn = data[ho+2:data.index(b'\x00', ho+2)]
                                names.append(hn.decode('ascii', 'replace'))
                        j += 1
                imports.append((name.decode('ascii', 'replace'), names))
                idx += 1

    mach_names = {0x14c: 'i386', 0x8664: 'amd64', 0x1c0: 'ARM', 0x1c4: 'ARMNT', 0xaa64: 'ARM64'}
    subsys = {1: 'native', 2: 'windows GUI', 3: 'windows CUI'}
    dt = datetime.datetime.utcfromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S UTC') if ts else 'n/a'
    print('=' * 90)
    print(path)
    print(f'  {"32-bit PE32" if pe32 else "64-bit PE32+"}  machine={mach_names.get(machine, hex(machine))}  sections={nsec}  timestamp={ts} ({dt})')
    print(f'  entrypoint RVA=0x{ep:x}  imagebase=0x{base:x}  subsystem={subsys.get(subsystem, subsystem)}  dllchars=0x{dllchars:04x}')
    print('  sections:', ', '.join(f'{n}(va={va:#x},vsz={vsz:#x},raw={rawsz:#x})' for n, va, vsz, rawptr, rawsz in secs))
    print(f'  imports ({len(imports)} DLLs):')
    for dll, names in imports:
        print(f'    {dll} ({len(names)} fn): ' + ', '.join(names[:15]) + ('...' if len(names) > 15 else ''))

if __name__ == '__main__':
    import sys
    for p in sys.argv[1:]:
        parse_pe(p)
