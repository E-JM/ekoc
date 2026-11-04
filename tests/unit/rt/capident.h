#ifndef EK_TESTS_RT_CAPIDENT_H
#define EK_TESTS_RT_CAPIDENT_H

#include "ekort.h"

#include <stdlib.h>

/* The shared constructor identities, defined the way an emitted unit
   defines them, under the same external linkage: the runtime's bundle
   closures build values over these typeinfos, and generated pattern
   matches test this exact pointer. The drops follow the layout
   contract of rt/internal.h -- the header, then the payload slots -- so
   a runtime-built value is reclaimed exactly like a unit-built one. A
   suite includes this once, and one suite is one unit. */

static void capident_drop0(ek_obj *o)
{
    (void)o;
}

static void capident_drop1(ek_obj *o)
{
    ek_release(((ek_val *)o)[1]);
}

static void capident_drop3(ek_obj *o)
{
    ek_release(((ek_val *)o)[1]);
    ek_release(((ek_val *)o)[2]);
    ek_release(((ek_val *)o)[3]);
}

const ek_typeinfo ekT_std$Result$Ok = { EK_KIND_STRUCT,
                                        capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Result$Err = { EK_KIND_STRUCT,
                                         capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Option$Some = { EK_KIND_STRUCT,
                                          capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Option$None = { EK_KIND_STRUCT,
                                          capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$IoError$NotFound = { EK_KIND_STRUCT,
                                               capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$IoError$Permission = { EK_KIND_STRUCT,
                                                 capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$IoError$Exists = { EK_KIND_STRUCT,
                                             capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$IoError$NotFile = { EK_KIND_STRUCT,
                                              capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$IoError$Closed = { EK_KIND_STRUCT,
                                             capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$IoError$InvalidData = { EK_KIND_STRUCT,
                                                  capident_drop1, NULL, NULL };
const ek_typeinfo ekT_sys$IoError$Other = { EK_KIND_STRUCT,
                                            capident_drop1, NULL, NULL };
const ek_typeinfo ekT_sys$OpenMode$Read = { EK_KIND_STRUCT,
                                            capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$OpenMode$Write = { EK_KIND_STRUCT,
                                             capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$OpenMode$Append = { EK_KIND_STRUCT,
                                              capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$OpenMode$New = { EK_KIND_STRUCT,
                                           capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$OpenMode$Update = { EK_KIND_STRUCT,
                                              capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys$FileInfo = { EK_KIND_STRUCT,
                                       capident_drop3, NULL, NULL };
const ek_typeinfo ekT_time$Time$Time = { EK_KIND_STRUCT,
                                         capident_drop1, NULL, NULL };
const ek_typeinfo ekT_time$Mono$Mono = { EK_KIND_STRUCT,
                                         capident_drop1, NULL, NULL };
const ek_typeinfo ekT_time$Duration$Duration = { EK_KIND_STRUCT,
                                                 capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$ParseError$Empty = { EK_KIND_STRUCT,
                                               capident_drop0, NULL, NULL };
const ek_typeinfo ekT_std$ParseError$Invalid = { EK_KIND_STRUCT,
                                                 capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$ParseError$OutOfRange = { EK_KIND_STRUCT,
                                                    capident_drop0, NULL, NULL };
const ek_typeinfo ekT_std$Json$Null = { EK_KIND_STRUCT, capident_drop0, NULL, NULL };
const ek_typeinfo ekT_std$Json$Bool = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Json$Int = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Json$Float = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Json$Str = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Json$Arr = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std$Json$Obj = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
#endif
