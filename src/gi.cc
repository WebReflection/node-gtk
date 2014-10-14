/*
 * Copyright (C) 2014 Endless Mobile
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include <node.h>
#include <girepository.h>

#include "value.h"
#include "function.h"
#include "gobject.h"

using namespace v8;

static void DefineEnumeration(Handle<Object> module_obj, GIBaseInfo *info) {
    Handle<Object> enum_obj = Object::New ();

    int n = g_enum_info_get_n_values(info);
    for (int i = 0; i < n; i++) {
        GIValueInfo *value_info = g_enum_info_get_value (info, i);

        const char *value_name = g_base_info_get_name ((GIBaseInfo *) value_info);
        gint64 value_val = g_value_info_get_value (value_info);

        char *upcase_name = g_ascii_strup (value_name, -1);
        enum_obj->Set (String::NewSymbol (upcase_name), Number::New (value_val));
        g_free (upcase_name);
    }

    const char *enum_name = g_base_info_get_name (info);
    module_obj->Set (String::NewSymbol (enum_name), enum_obj);
}

static void DefineConstant(Handle<Object> module_obj, GIBaseInfo *info) {
    GITypeInfo *type_info = g_constant_info_get_type ((GIConstantInfo *) info);
    GIArgument garg;
    g_constant_info_get_value ((GIConstantInfo *) info, &garg);
    Handle<Value> value = GNodeJS::GIArgumentToV8 (type_info, &garg);
    g_base_info_unref ((GIBaseInfo *) type_info);

    const char *constant_name = g_base_info_get_name ((GIBaseInfo *) info);
    module_obj->Set (String::NewSymbol (constant_name), value);
}

static void DefineFunction(Handle<Object> module_obj, GIBaseInfo *info) {
    const char *function_name = g_base_info_get_name ((GIBaseInfo *) info);
    module_obj->Set (String::NewSymbol (function_name), GNodeJS::MakeFunction (info));
}

static void DefineObject(Handle<Object> module_obj, GIBaseInfo *info) {
    const char *class_name = g_base_info_get_name ((GIBaseInfo *) info);
    module_obj->Set (String::NewSymbol (class_name), GNodeJS::MakeClass (info));
}

static void DefineInfo(Handle<Object> module_obj, GIBaseInfo *info) {
    GIInfoType type = g_base_info_get_type (info);

    switch (type) {
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
        DefineEnumeration (module_obj, info);
        break;
    case GI_INFO_TYPE_CONSTANT:
        DefineConstant (module_obj, info);
        break;
    case GI_INFO_TYPE_FUNCTION:
        DefineFunction (module_obj, info);
        break;
    case GI_INFO_TYPE_OBJECT:
        DefineObject (module_obj, info);
        break;
    default:
        break;
    }
}

static Handle<Value> ImportNS(const Arguments& args) {
    GIRepository *repo = g_irepository_get_default ();
    HandleScope scope;

    if (args.Length() < 1) {
        ThrowException (Exception::TypeError (String::New ("Wrong number of arguments")));
        return scope.Close (Undefined ());
    }

    String::Utf8Value ns_str (args[0]->ToString());
    const char *ns = *ns_str;

    const char *version = NULL;
    if (args[1]->IsString ()) {
        String::Utf8Value version_str (args[1]->ToString ());
        version = *version_str;
    }

    GError *error = NULL;
    g_irepository_require (repo, ns, version, (GIRepositoryLoadFlags) 0, &error);
    if (error) {
        ThrowException (Exception::TypeError (String::New (error->message)));
        return scope.Close (Undefined ());
    }

    Handle<Object> module_obj = Object::New ();

    int n = g_irepository_get_n_infos (repo, ns);
    for (int i = 0; i < n; i++) {
        GIBaseInfo *info = g_irepository_get_info (repo, ns, i);
        DefineInfo (module_obj, info);
        g_base_info_unref (info);
    }

    return scope.Close (module_obj);
}

void InitModule(Handle<Object> exports) {
    exports->Set(String::NewSymbol("importNS"), FunctionTemplate::New(ImportNS)->GetFunction());
}

NODE_MODULE(gi, InitModule)
