#include <cstdio>
#include <QtCore/QFile>
#include <QtCore/QString>
#ifdef _MSC_VER
#include <iso646.h>
#endif

static const char *template_typed = R"raw(
#ifdef TYPED_METHOD_BIND
template<class T $ifret ,class R$ $ifargs ,$ $arg, class P@$>
class MethodBind$argc$$ifret R$$ifconst C$ : public MethodBind {
public:

    $ifret R$ $ifnoret void$ (T::*method)($arg, P@$) $ifconst const$;
#ifdef DEBUG_METHODS_ENABLED
    virtual Variant::Type _gen_argument_type(int p_arg) const { return _get_argument_type(p_arg); }
    virtual GodotTypeInfo::Metadata get_argument_meta(int p_arg) const {
        $ifret if (p_arg==-1) return GetTypeInfo<R>::METADATA;$
        $arg if (p_arg==(@-1)) return GetTypeInfo<P@>::METADATA;
        $
        return GodotTypeInfo::METADATA_NONE;
    }
    Variant::Type _get_argument_type(int p_argument) const {
        $ifret if (p_argument==-1) return (Variant::Type)GetTypeInfo<R>::VARIANT_TYPE;$
        $arg if (p_argument==(@-1)) return (Variant::Type)GetTypeInfo<P@>::VARIANT_TYPE;
        $
        return Variant::NIL;
    }
    virtual PropertyInfo _gen_argument_type_info(int p_argument) const {
        $ifret if (p_argument==-1) return GetTypeInfo<R>::get_class_info();$
        $arg if (p_argument==(@-1)) return GetTypeInfo<P@>::get_class_info();
        $
        return PropertyInfo();
    }
#endif
    virtual String get_instance_class() const {
        return T::get_class_static();
    }

    virtual Variant call(Object* p_object,const Variant** p_args,int p_arg_count, Variant::CallError& r_error) {

        T *instance=Object::cast_to<T>(p_object);
        r_error.error=Variant::CallError::CALL_OK;
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_V(!instance,Variant());
        if (p_arg_count>get_argument_count()) {
            r_error.error=Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
            r_error.argument=get_argument_count();
            return Variant();

        }
        if (p_arg_count<(get_argument_count()-get_default_argument_count())) {

            r_error.error=Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
            r_error.argument=get_argument_count()-get_default_argument_count();
            return Variant();
        }
        $arg CHECK_ARG(@);
        $
#endif
        $ifret Variant ret = $(instance->*method)($arg, _VC(@)$);
        $ifret return Variant(ret);$
        $ifnoret return Variant();$
    }

#ifdef PTRCALL_ENABLED
    virtual void ptrcall(Object*p_object,const void** p_args,void *r_ret) {

        T *instance=Object::cast_to<T>(p_object);
        $ifret PtrToArg<R>::encode( $ (instance->*method)($arg, PtrToArg<P@>::convert(p_args[@-1])$) $ifret ,r_ret)$ ;
    }
#endif
    MethodBind$argc$$ifret R$$ifconst C$ () {
#ifdef DEBUG_METHODS_ENABLED
        _set_const($ifconst true$$ifnoconst false$);
        _generate_argument_types($argc$);
#else
        set_argument_count($argc$);
#endif

        $ifret _set_returns(true); $
    }
};

template<class T $ifret ,class R$ $ifargs ,$ $arg, class P@$>
MethodBind* create_method_bind($ifret R$ $ifnoret void$ (T::*p_method)($arg, P@$) $ifconst const$ ) {

    MethodBind$argc$$ifret R$$ifconst C$<T $ifret ,R$ $ifargs ,$ $arg, P@$> * a = memnew( (MethodBind$argc$$ifret R$$ifconst C$<T $ifret ,R$ $ifargs ,$ $arg, P@$>) );
    a->method=p_method;
    return a;
}
#endif
)raw";

static const char *template_untyped = R"raw(
#ifndef TYPED_METHOD_BIND
$iftempl template<$ $ifret class R$ $ifretargs ,$ $arg, class P@$ $iftempl >$
class MethodBind$argc$$ifret R$$ifconst C$ : public MethodBind {

public:

    StringName type_name;
    $ifret R$ $ifnoret void$ (__UnexistingClass::*method)($arg, P@$) $ifconst const$;

#ifdef DEBUG_METHODS_ENABLED
    virtual Variant::Type _gen_argument_type(int p_arg) const { return _get_argument_type(p_arg); }
    virtual GodotTypeInfo::Metadata get_argument_meta(int p_arg) const {
        $ifret if (p_arg==-1) return GetTypeInfo<R>::METADATA;$
        $arg if (p_arg==(@-1)) return GetTypeInfo<P@>::METADATA;
        $
        return GodotTypeInfo::METADATA_NONE;
    }

    Variant::Type _get_argument_type(int p_argument) const {
        $ifret if (p_argument==-1) return (Variant::Type)GetTypeInfo<R>::VARIANT_TYPE;$
        $arg if (p_argument==(@-1)) return (Variant::Type)GetTypeInfo<P@>::VARIANT_TYPE;
        $
        return Variant::NIL;
    }

    virtual PropertyInfo _gen_argument_type_info(int p_argument) const {
        $ifret if (p_argument==-1) return GetTypeInfo<R>::get_class_info();$
        $arg if (p_argument==(@-1)) return GetTypeInfo<P@>::get_class_info();
        $
        return PropertyInfo();
    }

#endif
    virtual String get_instance_class() const {
        return type_name;
    }

    virtual Variant call(Object* p_object,const Variant** p_args,int p_arg_count, Variant::CallError& r_error) {

        __UnexistingClass *instance = (__UnexistingClass*)p_object;

        r_error.error=Variant::CallError::CALL_OK;
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_V(!instance,Variant());
        if (p_arg_count>get_argument_count()) {
            r_error.error=Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
            r_error.argument=get_argument_count();
            return Variant();
        }

        if (p_arg_count<(get_argument_count()-get_default_argument_count())) {

            r_error.error=Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
            r_error.argument=get_argument_count()-get_default_argument_count();
            return Variant();
        }

        $arg CHECK_ARG(@);
        $
#endif
        $ifret Variant ret = $(instance->*method)($arg, _VC(@)$);
        $ifret return Variant(ret);$
        $ifnoret return Variant();$
    }
#ifdef PTRCALL_ENABLED
    virtual void ptrcall(Object*p_object,const void** p_args,void *r_ret) {
        __UnexistingClass *instance = (__UnexistingClass*)p_object;
        $ifret PtrToArg<R>::encode( $ (instance->*method)($arg, PtrToArg<P@>::convert(p_args[@-1])$) $ifret ,r_ret) $ ;
    }
#endif
    MethodBind$argc$$ifret R$$ifconst C$ () {
#ifdef DEBUG_METHODS_ENABLED
        _set_const($ifconst true$$ifnoconst false$);
        _generate_argument_types($argc$);
#else
        set_argument_count($argc$);
#endif
        $ifret _set_returns(true); $


    }
};

template<class T $ifret ,class R$ $ifargs ,$ $arg, class P@$>
MethodBind* create_method_bind($ifret R$ $ifnoret void$ (T::*p_method)($arg, P@$) $ifconst const$ ) {

    MethodBind$argc$$ifret R$$ifconst C$ $iftempl <$  $ifret R$ $ifretargs ,$ $arg, P@$ $iftempl >$ * a = memnew( (MethodBind$argc$$ifret R$$ifconst C$ $iftempl <$ $ifret R$ $ifretargs ,$ $arg, P@$ $iftempl >$) );
    union {

        $ifret R$ $ifnoret void$ (T::*sm)($arg, P@$) $ifconst const$;
        $ifret R$ $ifnoret void$ (__UnexistingClass::*dm)($arg, P@$) $ifconst const$;
    } u;
    u.sm=p_method;
    a->method=u.dm;
    a->type_name=T::get_class_static();
    return a;
}
#endif
)raw";

static const char *template_typed_free_func =
R"raw(
#ifdef TYPED_METHOD_BIND
template<class T $ifret ,class R$ $ifargs ,$ $arg, class P@$>
class FunctionBind$argc$$ifret R$$ifconst C$ : public MethodBind {
public:

    $ifret R$ $ifnoret void$ (*method) ($ifconst const$ T *$ifargs , $$arg, P@$);
#ifdef DEBUG_METHODS_ENABLED
    virtual Variant::Type _gen_argument_type(int p_arg) const { return _get_argument_type(p_arg); }
    virtual GodotTypeInfo::Metadata get_argument_meta(int p_arg) const {
        $ifret if (p_arg==-1) return GetTypeInfo<R>::METADATA;$
        $arg if (p_arg==(@-1)) return GetTypeInfo<P@>::METADATA;
        $
        return GodotTypeInfo::METADATA_NONE;
    }
    Variant::Type _get_argument_type(int p_argument) const {
        $ifret if (p_argument==-1) return (Variant::Type)GetTypeInfo<R>::VARIANT_TYPE;$
        $arg if (p_argument==(@-1)) return (Variant::Type)GetTypeInfo<P@>::VARIANT_TYPE;
        $
        return Variant::NIL;
    }
    virtual PropertyInfo _gen_argument_type_info(int p_argument) const {
        $ifret if (p_argument==-1) return GetTypeInfo<R>::get_class_info();$
        $arg if (p_argument==(@-1)) return GetTypeInfo<P@>::get_class_info();
        $
        return PropertyInfo();
    }
#endif
    virtual String get_instance_class() const {
        return T::get_class_static();
    }

    virtual Variant call(Object* p_object,const Variant** p_args,int p_arg_count, Variant::CallError& r_error) {

        T *instance=Object::cast_to<T>(p_object);
        r_error.error=Variant::CallError::CALL_OK;
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_V(!instance,Variant());
        if (p_arg_count>get_argument_count()) {
            r_error.error=Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
            r_error.argument=get_argument_count();
            return Variant();

        }
        if (p_arg_count<(get_argument_count()-get_default_argument_count())) {

            r_error.error=Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
            r_error.argument=get_argument_count()-get_default_argument_count();
            return Variant();
        }
        $arg CHECK_ARG(@);
        $
#endif
        $ifret Variant ret = $(method)(instance$ifargs , $$arg, _VC(@)$);
        $ifret return Variant(ret);$
        $ifnoret return Variant();$
    }

#ifdef PTRCALL_ENABLED
    virtual void ptrcall(Object*p_object,const void** p_args,void *r_ret) {

        T *instance=Object::cast_to<T>(p_object);
        $ifret PtrToArg<R>::encode( $ (method)(instance$ifargs , $$arg, PtrToArg<P@>::convert(p_args[@-1])$) $ifret ,r_ret)$ ;
    }
#endif
    FunctionBind$argc$$ifret R$$ifconst C$ () {
#ifdef DEBUG_METHODS_ENABLED
        _set_const($ifconst true$$ifnoconst false$);
        _generate_argument_types($argc$);
#else
        set_argument_count($argc$);
#endif

        $ifret _set_returns(true); $
    }
};

template<class T $ifret ,class R$ $ifargs ,$ $arg, class P@$>
MethodBind* create_method_bind($ifret R$ $ifnoret void$ (*p_method)($ifconst const$ T *$ifargs , $$arg, P@$) ) {

    FunctionBind$argc$$ifret R$$ifconst C$<T $ifret ,R$ $ifargs ,$ $arg, P@$> * a = memnew( (FunctionBind$argc$$ifret R$$ifconst C$<T $ifret ,R$ $ifargs ,$ $arg, P@$>) );
    a->method=p_method;
    return a;
}
#endif

)raw";
QString make_version(const char *template_name, int nargs, int argmax, bool const_val,bool ret)
{
    QString intext = template_name;
    int from_pos = 0;
    QString outtext = "";

    while(true)
    {
        auto to_pos = intext.indexOf("$", from_pos);
        if (to_pos == -1)
        {
			outtext += intext.midRef(from_pos);
            break;
        }
        else
			outtext += intext.midRef(from_pos,to_pos-from_pos);
        auto end = intext.indexOf("$", to_pos + 1);
        if (end == -1)
            break;  //# ignore
        QString macro = intext.mid(to_pos + 1,end-(to_pos + 1));
        QString cmd = "";
        QString data = "";

        if (macro.indexOf(" ") != -1)
        {
            cmd = macro.mid(0,macro.indexOf(" "));
            data = macro.mid(macro.indexOf(" ") + 1);
        }
        else
            cmd = macro;

        if (cmd == "argc")
            outtext += QString::number(nargs);
        if (cmd == "ifret" and ret)
            outtext += data;
        if (cmd == "ifargs" and nargs)
            outtext += data;
        if (cmd == "ifretargs" and nargs and ret)
            outtext += data;
        if (cmd == "ifconst" and const_val)
            outtext += data;
        else if (cmd == "ifnoconst" and not const_val)
            outtext += data;
        else if (cmd == "ifnoret" and not ret)
            outtext += data;
        else if (cmd == "iftempl" and (nargs > 0 or ret))
            outtext += data;
        else if (cmd == "arg,")
            for(int i=1; i <=nargs; ++i)
            {
                if (i > 1)
                    outtext += ", ";
                outtext += QString(data).replace("@", QString::number(i));
            }
        else if (cmd == "arg")
        {
            for(int i=1; i <=nargs; ++i)
                outtext += QString(data).replace("@", QString::number(i));
        }
        else if (cmd == "noarg")
        {
            for(int i=nargs + 1; i <= argmax; ++i)
                outtext += QString(data).replace("@", QString::number(i));
        }
        from_pos = end + 1;
    }
    return outtext;
}

int main(int argc, char **argv)
{
    int versions = 13;
    int versions_ext = 6;
    QString text = "";
    if(argc<3)
        return -1;

    int gen_ext = atoi(argv[1]);
    switch(gen_ext) {
    case 0:
    {
        for(int i=0; i<versions_ext; ++i)
        {
            QString t = "";
            t += make_version(template_untyped, i, versions, false, false);
            t += make_version(template_typed, i, versions, false, false);
            t += make_version(template_untyped, i, versions, false, true);
            t += make_version(template_typed, i, versions, false, true);
            t += make_version(template_untyped, i, versions, true, false);
            t += make_version(template_typed, i, versions, true, false);
            t += make_version(template_untyped, i, versions, true, true);
            t += make_version(template_typed, i, versions, true, true);
            text += t;
        }
        break;
    }
    case 1: {
        for(int i=versions_ext; i<versions + 1; ++i)
        {
            QString t = "";
            t += make_version(template_untyped, i, versions, false, false);
            t += make_version(template_typed, i, versions, false, false);
            t += make_version(template_untyped, i, versions, false, true);
            t += make_version(template_typed, i, versions, false, true);
            t += make_version(template_untyped, i, versions, true, false);
            t += make_version(template_typed, i, versions, true, false);
            t += make_version(template_untyped, i, versions, true, true);
            t += make_version(template_typed, i, versions, true, true);
            text += t;
        }
        QFile tgt2(argv[2]);
        if(tgt2.open(QFile::WriteOnly))
        {
            tgt2.write(text.toLocal8Bit());
        }
        tgt2.close();
        break;
    }
    case 2:
    {
        text = "#ifndef METHOD_BIND_FREE_FUNC_H\n#define METHOD_BIND_FREE_FUNC_H\n";

        text += "\n//including this header file allows method binding to use free functions\n";
        text += "//note that the free function must have a pointer to an instance of the class as its first parameter\n";

        for(int i=0; i<versions + 1; ++i) {
            text += make_version(template_typed_free_func, i, versions, false, false);
            text += make_version(template_typed_free_func, i, versions, false, true);
            text += make_version(template_typed_free_func, i, versions, true, false);
            text += make_version(template_typed_free_func, i, versions, true, true);
        }
        text += "#endif";
        break;
    }
    default:
        return -1;
    }
    QFile tgt1(argv[2]);
    if(tgt1.open(QFile::WriteOnly))
    {
        tgt1.write(text.toLocal8Bit());
    }
    tgt1.close();

    return 0;
}
