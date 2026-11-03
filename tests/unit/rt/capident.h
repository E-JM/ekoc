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

const ek_typeinfo ekT_std_Result_Ok = { EK_KIND_STRUCT,
                                        capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Result_Err = { EK_KIND_STRUCT,
                                         capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Option_Some = { EK_KIND_STRUCT,
                                          capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Option_None = { EK_KIND_STRUCT,
                                          capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_IoError_NotFound = { EK_KIND_STRUCT,
                                               capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_IoError_Permission = { EK_KIND_STRUCT,
                                                 capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_IoError_Exists = { EK_KIND_STRUCT,
                                             capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_IoError_NotFile = { EK_KIND_STRUCT,
                                              capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_IoError_Closed = { EK_KIND_STRUCT,
                                             capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_IoError_InvalidData = { EK_KIND_STRUCT,
                                                  capident_drop1, NULL, NULL };
const ek_typeinfo ekT_sys_IoError_Other = { EK_KIND_STRUCT,
                                            capident_drop1, NULL, NULL };
const ek_typeinfo ekT_sys_OpenMode_Read = { EK_KIND_STRUCT,
                                            capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_OpenMode_Write = { EK_KIND_STRUCT,
                                             capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_OpenMode_Append = { EK_KIND_STRUCT,
                                              capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_OpenMode_New = { EK_KIND_STRUCT,
                                           capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_OpenMode_Update = { EK_KIND_STRUCT,
                                              capident_drop0, NULL, NULL };
const ek_typeinfo ekT_sys_FileInfo = { EK_KIND_STRUCT,
                                       capident_drop3, NULL, NULL };
const ek_typeinfo ekT_time_Time_Time = { EK_KIND_STRUCT,
                                         capident_drop1, NULL, NULL };
const ek_typeinfo ekT_time_Mono_Mono = { EK_KIND_STRUCT,
                                         capident_drop1, NULL, NULL };
const ek_typeinfo ekT_time_Duration_Duration = { EK_KIND_STRUCT,
                                                 capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_ParseError_Empty = { EK_KIND_STRUCT,
                                               capident_drop0, NULL, NULL };
const ek_typeinfo ekT_std_ParseError_Invalid = { EK_KIND_STRUCT,
                                                 capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_ParseError_OutOfRange = { EK_KIND_STRUCT,
                                                    capident_drop0, NULL, NULL };
const ek_typeinfo ekT_std_Json_Null = { EK_KIND_STRUCT, capident_drop0, NULL, NULL };
const ek_typeinfo ekT_std_Json_Bool = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Json_Int = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Json_Float = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Json_Str = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Json_Arr = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
const ek_typeinfo ekT_std_Json_Obj = { EK_KIND_STRUCT, capident_drop1, NULL, NULL };
#endif
