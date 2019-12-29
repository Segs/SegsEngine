#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <cstdio>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QDebug>
#ifdef _MSC_VER
#include <iso646.h>
#endif

QString stripComments(QString src)
{
    bool single_line=false;
    bool multi_line = false;
    QString result;
    src.replace("\r\n","\n");
    result.reserve(src.size());
    for(int i=0; i<src.size()-1; ++i)
    {
        if(single_line && src[i]=='\n') {
            i++;
            single_line = false;
        }
        else if (multi_line && src[i]=='*' && src[i+1]=='/') {
            multi_line=false;
            ++i;
        }
        else if(single_line || multi_line)
        {
            continue; // skipped
        }
        else if(src[i]=='/' && src[i+1]=='/') {
            single_line = true;
            ++i;
        }
        else if(src[i]=='/' && src[i+1]=='*') {
            multi_line=true;
            ++i;
        }
        else
            result+=src[i];

    }
    if(!single_line && !multi_line)
        result+=src.back();

    return result;
}
int findClosingBraceIdx(const QStringRef &src,int idx,int depth,char br_open='{',char br_close='}')
{
    bool can_exit = depth!=0;
    for(;idx<src.size();++idx)
    {
        if(src[idx]==br_open) {
            can_exit = true; // first brace encountered, can actually get the whole thing.
            depth++;
        }
        else if(src[idx]==br_close)
            depth--;
        if(depth==0 && can_exit)
            break;
    }
    return idx;
}
int findOpeningBraceIdx(const QStringRef &src,int idx,int depth,char br_open='{',char br_close='}')
{
    bool can_exit = depth!=0;
    for(;idx>=0;--idx)
    {
        if(src[idx]==br_open) {
            depth--;
        }
        else if(src[idx]==br_close) {
            can_exit = true; // first brace encountered, can actually get the whole thing.
            depth++;
        }
        if(depth==0 && can_exit)
            break;
    }
    return idx;
}
int nextSemicolon(const QStringRef &src,int idx)
{
    return src.indexOf(';',idx);
}
struct GD_TypeDecl {
    QString name;
};
struct ArgDecl {
    GD_TypeDecl type;
    QString name;
    QString def_val;
};
ArgDecl processArg(const QStringRef &d)
{
    ArgDecl res;

    int start_of_name = d.size()-1;
    int def_val_idx = d.lastIndexOf('=');
    if(def_val_idx!=-1)
    {
        start_of_name = def_val_idx-1;
        res.def_val = d.mid(def_val_idx+1).trimmed().toString();
        while(d[start_of_name].isSpace())
            start_of_name--;
    }
    while(start_of_name>=0) {
        if(d[start_of_name].isLetter()||d[start_of_name].isNumber()||d[start_of_name]=='_')
            start_of_name--;
        else {
            break;
        }
    }
    if(def_val_idx!=-1)
        res.name = d.mid(start_of_name+1,def_val_idx-start_of_name-2).toString();
    else
        res.name = d.mid(start_of_name+1).toString();
    res.type = GD_TypeDecl {d.mid(0,start_of_name+1).toString()};
    return res;
}
QVector<ArgDecl> splitArgs(const QStringRef &src)
{
    //TODO: doesn't handle argument names embedded in parens
    QVector<ArgDecl> res;
    int nesting=0;
    int start_idx=0;
    int idx;
    for(idx=0; idx<src.size(); ++idx)
    {
        if(src[idx]=='<'||src[idx]=='(') {
            nesting++;
        }
        else if(src[idx]=='>'||src[idx]==')') {
            nesting++;
        }
        else if(nesting==0 && src[idx]==',') {
            res<< processArg(src.mid(start_idx,idx-start_idx));
            start_idx = idx+1;
        }
    }
    if(start_idx!=idx)
        res<< processArg(src.mid(start_idx+1,idx-(start_idx+1)));
    return res;
}
struct GD_Invocable
{
    GD_TypeDecl return_type;
    QString name;
    QVector<ArgDecl> args;
};
void processInvocable(GD_Invocable &tgt,const QStringRef &src)
{
    int start_of_args = findOpeningBraceIdx(src,src.size()-1,0,'(',')');
    int start_of_name = start_of_args-1;
    QStringRef args = src.mid(start_of_args+1,src.size()-(start_of_args+2));
    while(start_of_name!=0) {
        if(src[start_of_name].isLetter()||src[start_of_name].isNumber()||src[start_of_name]=='_')
            start_of_name--;
        else {
            break;
        }
    }
    tgt.name = src.mid(start_of_name+1,start_of_args-start_of_name-1).toString();
    tgt.args = splitArgs(args);
    tgt.return_type = GD_TypeDecl {src.mid(0,start_of_name).toString()};
}
struct GD_Class {
    QString name;
    QString parent_name;
    bool requested_binds=false;
    QVector<GD_Invocable> bindable_definitions;
};
int processClass(const QStringRef &src,int idx,GD_Class &cl)
{
    int closing=findClosingBraceIdx(src,idx,1);
    QStringRef sub_area = src.mid(idx,closing);
    cl.requested_binds = sub_area.contains("HAS_BINDS");
    int internal_idx=0;
    while((internal_idx=sub_area.indexOf("INVOCABLE",internal_idx))!=-1)
    {
        //TODO: will barf on complex return types-> void (*)(z) getTheZ();
        int fin_idx = findClosingBraceIdx(sub_area,internal_idx,0,'(',')');
        if(fin_idx!=-1) {
            GD_Invocable inv;
            processInvocable(inv,sub_area.mid(internal_idx+9,fin_idx-(internal_idx+9)+1).trimmed());
            cl.bindable_definitions << inv;
            internal_idx = fin_idx+1;
        }
        else {
            qCritical() << "INVOCABLE was not terminated correctly ?";
        }
    }

    return closing;
}
QVector<GD_Class> collectClasses(const QStringRef &src)
{
    QVector<GD_Class> res;
    int idx = 0;
    while((idx=src.indexOf("GDCLASS(",idx))!=-1)
    {
        int closing_paren_idx = src.indexOf(')',idx);
        QStringRef v = src.mid(idx+8,closing_paren_idx - (idx+8));
        auto pr = v.split(',');
        GD_Class entry {pr[0].trimmed().toString(),pr[1].trimmed().toString(),false,{}};
        idx = closing_paren_idx+1;
        idx = processClass(src,idx,entry);
        res << entry;
    }
    return res;
}
QString genDesc(const GD_Invocable &inv)
{
    QString res="D_METHOD(\""+inv.name + '"';
    if(!inv.args.isEmpty()) {
        res+=",{";
        bool first=true;
        for(const ArgDecl &arg : inv.args) {
            if(!first)
                res+=',';
            first = false;
            res+='"'+arg.name+'"';
        }
        res+="}";
    }
    res+=')';
    return res;
}
QString genDefVals(const GD_Invocable &inv)
{
    QString res;
    QStringList defs;
    for(auto arg : inv.args) {
        if(!arg.def_val.isEmpty())
            defs+= QString("DEFVAL(%1)").arg(arg.def_val);
    }
    if(defs.isEmpty())
        return "";
    return "{" + defs.join(',') + "}";
}
struct BindDefInfo {
    QString include;
    QString methods;
};

BindDefInfo genClassBinders(const QString header_path,QVector<GD_Class> &src)
{
    BindDefInfo res;
    QTextStream ts(&res.methods);
    res.include = QString("#include \"%1\"\n").arg(header_path);
    const QString indent(QStringLiteral("    "));
    for(const GD_Class & cl : src)
    {
        if(!cl.bindable_definitions.isEmpty())
        {
            ts << QString("void %1::_bind_methods() {\n").arg(cl.name);
            for(const auto &bn : cl.bindable_definitions) {
                //MethodBinder::bind_method(D_METHOD("load_interactive", {"path", "type_hint"}), &_ResourceLoader::load_interactive, {DEFVAL("")});

                ts << indent+QString("MethodBinder::bind_method(%1,").arg(genDesc(bn));
                ts << QString("&%1::%2").arg(cl.name,bn.name);
                QString def_vals=genDefVals(bn);
                if(!def_vals.isEmpty())
                    ts<<","<<def_vals;
                ts<<")\n";

            }
            ts << QString("}\n");
        }
    }
    return res;
}
int main(int argc, char **argv)
{
    QString text = "";
    if(argc<3)
        return -1;
    QString to_scan(argv[1]);
    if(!QFile::exists(to_scan)) {
        qCritical() << "Source file "<<to_scan<<"missing";
        return -1;
    }
    QFileInfo fi(to_scan);
    if(!fi.isDir())
    {
        qCritical() << "Provided path"<<to_scan<<"is not a directory file";
        return -1;
    }

    QFile tgt1(argv[2]);
    if(!tgt1.open(QFile::WriteOnly))
    {
        qCritical() << "Provided output file cannot be opened";
        //tgt1.write(stripped.toLocal8Bit());
    }

    QDirIterator di(to_scan,{"*.h"},QDir::Files,QDirIterator::Subdirectories);
    QVector<BindDefInfo> entries;
    while(di.hasNext())
    {
        QString path=di.next();
        QFile fl(path);
        if(!fl.open(QFile::ReadOnly|QFile::Text))
        {
            qCritical() << "Opening"<<to_scan<<"failed";
            return -1;
        }

        QByteArray filedata = fl.readAll();
        QString stripped = stripComments(QString::fromUtf8(filedata));
        auto v = collectClasses(&stripped);
        entries<<genClassBinders(path,v);
    }
    QTextStream result_file(&tgt1);

    for(const BindDefInfo &bi : entries)
    {
        result_file<<bi.include;
    }
    for(const BindDefInfo &bi : entries)
    {
        result_file<< "// methods for "<<bi.include;
        result_file<<bi.methods;
    }
    tgt1.close();
    return 0;
}
