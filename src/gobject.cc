
#include "gobject.h"

#include "function.h"
#include "value.h"
#include "closure.h"

using namespace v8;

namespace GNodeJS {

static bool InitGParameterFromProperty(GParameter    *parameter,
                                       void          *klass,
                                       Local<String> name,
                                       Local<Value>  value) {
    String::Utf8Value name_str (name);
    GParamSpec *pspec = g_object_class_find_property (G_OBJECT_CLASS (klass), *name_str);
    if (pspec == NULL)
        return false;

    parameter->name = pspec->name;
    g_value_init (&parameter->value, G_PARAM_SPEC_VALUE_TYPE (pspec));
    V8ToGValue (&parameter->value, value);
    return true;
}

static bool InitGParametersFromProperty(GParameter    **parameters_p,
                                        int            *n_parameters_p,
                                        void           *klass,
                                        Local<Object>  property_hash) {
    Local<Array> properties = property_hash->GetOwnPropertyNames ();
    int n_parameters = properties->Length ();
    GParameter *parameters = g_new0 (GParameter, n_parameters);

    for (int i = 0; i < n_parameters; i++) {
        Local<Value> name = properties->Get (i);
        Local<Value> value = property_hash->Get (name);

        if (!InitGParameterFromProperty (&parameters[i], klass, name->ToString (), value))
            return false;
    }

    *parameters_p = parameters;
    *n_parameters_p = n_parameters;
    return true;
}

static void ToggleNotify(gpointer user_data, GObject *gobject, gboolean toggle_down);

static G_DEFINE_QUARK(gnode_js_object, gnode_js_object);

static void AssociateGObject(Isolate *isolate, Local<Object> object, GObject *gobject) {
    object->SetAlignedPointerInInternalField (0, gobject);

    g_object_ref_sink (gobject);
    g_object_add_toggle_ref (gobject, ToggleNotify, NULL);

    Persistent<Object> *persistent = new Persistent<Object>(isolate, object);
    g_object_set_qdata (gobject, gnode_js_object_quark (), persistent);
}

static void GObjectConstructor(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate ();

    /* The flow of this function is a bit twisty.

     * There's two cases for when this code is called:
     * user code doing `new Gtk.Widget({ ... })`, and
     * internal code as part of WrapperFromGObject, where
     * the constructor is called with one external. */

    if (!args.IsConstructCall ()) {
        isolate->ThrowException (Exception::TypeError (String::NewFromUtf8 (isolate, "Not a construct call.")));
        return;
    }

    Local<Object> self = args.This ();

    if (args[0]->IsExternal ()) {
        /* The External case. This is how WrapperFromGObject is called. */

        void *data = External::Cast (*args[0])->Value ();
        GObject *gobject = G_OBJECT (data);

        AssociateGObject (isolate, self, gobject);
    } else {
        /* User code calling `new Gtk.Widget({ ... })` */

        GObject *gobject;
        GIBaseInfo *info = (GIBaseInfo *) External::Cast (*args.Data ())->Value ();
        GType gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) info);
        void *klass = g_type_class_ref (gtype);

        GParameter *parameters = NULL;
        int n_parameters = 0;

        if (args[0]->IsObject ()) {
            Local<Object> property_hash = args[0]->ToObject ();

            if (!InitGParametersFromProperty (&parameters, &n_parameters, klass, property_hash)) {
                isolate->ThrowException (Exception::TypeError (String::NewFromUtf8 (isolate, "Unable to make GParameters.")));
                goto out;
            }
        }

        gobject = (GObject *) g_object_newv (gtype, n_parameters, parameters);
        AssociateGObject (isolate, self, gobject);

    out:
        g_free (parameters);
        g_type_class_unref (klass);
    }
}

static G_DEFINE_QUARK(gnode_js_template, gnode_js_template);

static void SignalConnectInternal(const FunctionCallbackInfo<Value> &args, bool after) {
    Isolate *isolate = args.GetIsolate ();
    GObject *gobject = GObjectFromWrapper (args.This ());

    String::Utf8Value signal_name (args[0]->ToString ());
    Local<Function> callback = Local<Function>::Cast (args[1]->ToObject ());
    GClosure *gclosure = MakeClosure (isolate, callback);

    ulong handler_id = g_signal_connect_closure (gobject, *signal_name, gclosure, after);
    args.GetReturnValue ().Set(Integer::NewFromUnsigned (isolate, handler_id));
}

static void SignalConnect(const FunctionCallbackInfo<Value> &args) {
    SignalConnectInternal (args, false);
}

static Local<FunctionTemplate> GetBaseClassTemplate(Isolate *isolate) {
    Local<FunctionTemplate> tpl = FunctionTemplate::New (isolate);
    Local<ObjectTemplate> proto = tpl->PrototypeTemplate ();
    proto->Set (String::NewFromUtf8 (isolate, "connect"), FunctionTemplate::New (isolate, SignalConnect)->GetFunction ());
    return tpl;
}

static Local<FunctionTemplate> GetClassTemplateFromGI(Isolate *isolate, GIBaseInfo *info);

static void ClassDestroyed(const WeakCallbackData<FunctionTemplate, GIBaseInfo> &data) {
    GIBaseInfo *info = data.GetParameter ();
    GType gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) info);

    void *type_data = g_type_get_qdata (gtype, gnode_js_template_quark ());
    assert (type_data != NULL);
    Persistent<FunctionTemplate> *persistent = (Persistent<FunctionTemplate> *) type_data;
    delete persistent;

    g_type_set_qdata (gtype, gnode_js_template_quark (), NULL);
    g_base_info_unref (info);
}

static Local<FunctionTemplate> GetClassTemplate(Isolate *isolate, GIBaseInfo *info, GType gtype) {
    void *data = g_type_get_qdata (gtype, gnode_js_template_quark ());

    if (data) {
        Persistent<FunctionTemplate> *persistent = (Persistent<FunctionTemplate> *) data;
        Local<FunctionTemplate> tpl = Local<FunctionTemplate>::New (isolate, *persistent);
        return tpl;
    } else {
        Local<FunctionTemplate> tpl = FunctionTemplate::New (isolate, GObjectConstructor, External::New (isolate, info));

        Persistent<FunctionTemplate> *persistent = new Persistent<FunctionTemplate>(isolate, tpl);
        persistent->SetWeak (g_base_info_ref (info), ClassDestroyed);
        g_type_set_qdata (gtype, gnode_js_template_quark (), persistent);

        const char *class_name = g_base_info_get_name (info);
        tpl->SetClassName (String::NewFromUtf8 (isolate, class_name));

        tpl->InstanceTemplate ()->SetInternalFieldCount (1);

        GIObjectInfo *parent_info = g_object_info_get_parent (info);
        if (parent_info) {
            Local<FunctionTemplate> parent_tpl = GetClassTemplateFromGI (isolate, (GIBaseInfo *) parent_info);
            tpl->Inherit (parent_tpl);
        } else {
            tpl->Inherit (GetBaseClassTemplate (isolate));
        }

        return tpl;
    }
}

static Local<FunctionTemplate> GetClassTemplateFromGI(Isolate *isolate, GIBaseInfo *info) {
    GType gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) info);
    return GetClassTemplate (isolate, info, gtype);
}

static Local<FunctionTemplate> GetClassTemplateFromGType(Isolate *isolate, GType gtype) {
    GIRepository *repo = g_irepository_get_default ();
    GIBaseInfo *info = g_irepository_find_by_gtype (repo, gtype);
    return GetClassTemplate (isolate, info, gtype);
}

Local<Function> MakeClass(Isolate *isolate, GIBaseInfo *info) {
    Local<FunctionTemplate> tpl = GetClassTemplateFromGI (isolate, info);
    return tpl->GetFunction ();
}

static void ObjectDestroyed(const WeakCallbackData<Object, GObject> &data) {
    GObject *gobject = data.GetParameter ();

    void *type_data = g_object_get_qdata (gobject, gnode_js_object_quark ());
    assert (type_data != NULL);
    Persistent<Object> *persistent = (Persistent<Object> *) type_data;
    delete persistent;

    /* We're destroying the wrapper object, so make sure to clear out
     * the qdata that points back to us. */
    g_object_set_qdata (gobject, gnode_js_object_quark (), NULL);

    g_object_unref (gobject);
}

static void ToggleNotify(gpointer user_data, GObject *gobject, gboolean toggle_down) {
    void *data = g_object_get_qdata (gobject, gnode_js_object_quark ());
    assert (data != NULL);

    Persistent<Object> *persistent = (Persistent<Object> *) data;

    if (toggle_down) {
        /* We're dropping from 2 refs to 1 ref. We are the last holder. Make
         * sure that that our weak ref is installed. */
        persistent->SetWeak (gobject, ObjectDestroyed);
    } else {
        /* We're going from 1 ref to 2 refs. We can't let our wrapper be
         * collected, so make sure that our reference is persistent */
        persistent->ClearWeak ();
    }
}

Local<Value> WrapperFromGObject(Isolate *isolate, GObject *gobject) {
    void *data = g_object_get_qdata (gobject, gnode_js_object_quark ());

    if (data) {
        /* Easy case: we already have an object. */
        Persistent<Object> *persistent = (Persistent<Object> *) data;
        Local<Object> obj = Local<Object>::New (isolate, *persistent);
        return obj;
    } else {
        GType gtype = G_OBJECT_TYPE (gobject);

        Local<FunctionTemplate> tpl = GetClassTemplateFromGType (isolate, gtype);
        Local<Function> constructor = tpl->GetFunction ();

        Local<Value> gobject_external = External::New (isolate, gobject);
        Local<Value> args[] = { gobject_external };
        Local<Object> obj = constructor->NewInstance (1, args);
        return obj;
    }
}

GObject * GObjectFromWrapper(Local<Value> value) {
    Local<Object> object = value->ToObject ();
    void *data = object->GetAlignedPointerFromInternalField (0);
    GObject *gobject = G_OBJECT (data);
    return gobject;
}

};
