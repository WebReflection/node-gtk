
#include "boxed.h"
#include "function.h"

using namespace v8;

namespace GNodeJS {

struct Boxed {
    GIBaseInfo *info;
};

static G_DEFINE_QUARK(gnode_js_template, gnode_js_template);

static void BoxedClassDestroyed(const WeakCallbackData<FunctionTemplate, GIBaseInfo> &data) {
    GIBaseInfo *info = data.GetParameter ();
    GType gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) info);

    void *type_data = g_type_get_qdata (gtype, gnode_js_template_quark ());
    assert (type_data != NULL);
    Persistent<FunctionTemplate> *persistent = (Persistent<FunctionTemplate> *) type_data;
    delete persistent;

    g_type_set_qdata (gtype, gnode_js_template_quark (), NULL);
    g_base_info_unref (info);
}

static void BoxedConstructor(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate ();

    /* See gobject.cc for how this works */

    if (!args.IsConstructCall ()) {
        isolate->ThrowException (Exception::TypeError (String::NewFromUtf8 (isolate, "Not a construct call.")));
        return;
    }

    Local<Object> self = args.This ();

    if (args[0]->IsExternal ()) {
        /* The External case. This is how WrapperFromBoxed is called. */

        /* XXX: We might want to copy the boxed? */
        void *boxed = External::Cast (*args[0])->Value ();

        self->SetAlignedPointerInInternalField (0, boxed);
    } else {
        /* TODO: Boxed construction not supported yet. */
        g_assert_not_reached ();
    }
}

static Local<FunctionTemplate> GetBoxedTemplate(Isolate *isolate, GIBaseInfo *info, GType gtype) {
    void *data;

    if (gtype == G_TYPE_NONE)
        data = NULL;
    else
        data = g_type_get_qdata (gtype, gnode_js_template_quark ());

    if (data) {
        Persistent<FunctionTemplate> *persistent = (Persistent<FunctionTemplate> *) data;
        Local<FunctionTemplate> tpl = Local<FunctionTemplate>::New (isolate, *persistent);
        return tpl;
    } else {
        Local<FunctionTemplate> tpl = FunctionTemplate::New (isolate, BoxedConstructor, External::New (isolate, info));

        Persistent<FunctionTemplate> *persistent = new Persistent<FunctionTemplate>(isolate, tpl);
        persistent->SetWeak (g_base_info_ref (info), BoxedClassDestroyed);

        const char *class_name = g_base_info_get_name (info);
        tpl->SetClassName (String::NewFromUtf8 (isolate, class_name));

        tpl->InstanceTemplate ()->SetInternalFieldCount (1);

        if (gtype != G_TYPE_NONE)
            g_type_set_qdata (gtype, gnode_js_template_quark (), persistent);

        return tpl;
    }
}

static Local<FunctionTemplate> GetBoxedTemplateFromGI(Isolate *isolate, GIBaseInfo *info) {
    GType gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *) info);
    return GetBoxedTemplate (isolate, info, gtype);
}

Local<Function> MakeBoxed(Isolate *isolate, GIBaseInfo *info) {
    Local<FunctionTemplate> tpl = GetBoxedTemplateFromGI (isolate, info);
    return tpl->GetFunction ();
}

Local<Value> WrapperFromBoxed(Isolate *isolate, GIBaseInfo *info, void *data) {
    Local<Function> constructor = MakeBoxed (isolate, info);

    Local<Value> boxed_external = External::New (isolate, data);
    Local<Value> args[] = { boxed_external };
    Local<Object> obj = constructor->NewInstance (1, args);
    return obj;
}

void * BoxedFromWrapper(Local<Value> value) {
    Local<Object> object = value->ToObject ();
    void *data = object->GetAlignedPointerFromInternalField (0);
    return data;
}

};
