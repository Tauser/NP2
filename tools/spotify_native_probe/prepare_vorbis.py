"""Private Tremor copy with codec-only bounded allocation hooks."""
import argparse
from pathlib import Path


def generate(source, output):
    output.mkdir(parents=True, exist_ok=True)
    for path in source.iterdir():
        if path.is_file() and path.suffix in (".c", ".h"):
            (output/path.name).write_bytes(path.read_bytes())
    path=output/"os_types.h"
    text=path.read_text()
    old="#define _ogg_malloc  malloc\n#define _ogg_calloc  calloc\n#define _ogg_realloc realloc\n#define _ogg_free    free"
    if text.count(old)!=1:
        raise ValueError("Pinned Tremor allocator anchor changed")
    text=text.replace(old,'''#include "spotify_ogg_memory.h"
#define _ogg_malloc spotify_ogg_malloc
#define _ogg_calloc spotify_ogg_calloc
#define _ogg_realloc spotify_ogg_realloc
#define _ogg_free spotify_ogg_free''')
    start=text.index("#ifdef _WIN32")
    text=text[:start]+'''/* Fixed-width types for both the P4 and host regression compiler. */
#include <stdint.h>
typedef int64_t ogg_int64_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
#endif /* _OS_TYPES_H */
'''
    path.write_text(text)
    (output/"config_types.h").write_text('''#ifndef _OS_CVTYPES_H
#define _OS_CVTYPES_H
#include "os_types.h"
#endif
''')


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source",type=Path);parser.add_argument("output",type=Path)
    args=parser.parse_args();generate(args.source,args.output)
