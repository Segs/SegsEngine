#include "reflection_walker.h"

#include "json_visitor.h"
#include "cpp_visitor.h"
#include "type_system.h"

#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>

namespace {

enum class BlockType {
    Class=0,
    Struct=1,
    Namespace=2
};

struct TranslationUnit {
    QString filename;
    QString contents;
    QVector<TS_TypeLike *> nesting_stack;
    QMap<int,TS_Type *> field_collectors;
    QVector<int> brace_nesting_stack;
    QVector<int> open_brace_indices;
    // namespace/class/struct nesting, used to verify proper nesting of registered types
    struct BlockName {
        QStringView name;
        QStringView full_def;
        int level;
        BlockType type;
    };

    QVector<BlockName> name_stack;
};

struct ParseHead {
    TranslationUnit &tu;
    int start_offset;
    int end_offset;
    int offset;
    int bracket_nesting_level = -1;
    bool collecting_signals=false;
    QString error;
    explicit ParseHead(TranslationUnit &p, int start = -1, int end = -1) : tu(p) {
        start = std::max(0, start);
        start = std::min(tu.contents.size(), start);
        if(end==-1)
            end = tu.contents.size();
        end = std::max(start, end);
        end = std::min(end, tu.contents.size());
        start_offset = start;
        end_offset = end;
        offset = 0;
    }
    ParseHead(const ParseHead &) = delete;
    explicit ParseHead(ParseHead &from, int start, int len = -1) : tu(from.tu) {
        start_offset = start;
        offset = 0;
        end_offset = start + len;
    }
    [[nodiscard]] const QChar *begin() const { return tu.contents.constData() + start_offset; }
    [[nodiscard]] const QChar *end() const { return tu.contents.constData() + end_offset; }
    [[nodiscard]] QStringView slice() const { return tu.contents.midRef(start_offset, end_offset - start_offset); }

    [[nodiscard]] QChar peek(int idx=0) const { return slice()[offset+idx]; }
    [[nodiscard]] QChar take() { return slice()[offset++]; }
    [[nodiscard]] bool valid_offset(int idx=0) const {
        return start_offset+offset+idx < end_offset;
    }
    [[nodiscard]] QStringView peek_slice(int cnt) const { return slice().mid(offset, cnt); }
    void consume(int cnt = 1) {
        offset += cnt;
        offset = std::min(offset, end_offset - start_offset);
    }
    bool atEnd() const { return end_offset == start_offset || offset == (end_offset - start_offset); }

    [[nodiscard]] int searchForward(QChar c) const { return slice().indexOf(c, offset); }
    [[nodiscard]] int searchForward(const QVector<QChar> &chars) const {
        int min_pos = slice().size()+1;
        for(QChar c : chars) {
            int pos=searchForward(c);
            if(pos==-1)
                continue;
            min_pos = std::min(pos,min_pos);
        }
        return min_pos==(slice().size()+1) ? -1 : min_pos;
    }
    int searchBackward(QChar c) const { return slice().lastIndexOf(c, offset); }
    void skipWS() {
        while(!atEnd() && peek().isSpace())
            consume();
    }
    QStringView getIdent() {
        int start_idx = offset;
        skipWS();
        while(!atEnd()) {
            QChar c = take();
            if(!c.isLetterOrNumber() && c!='_') {
                offset--;
                break;
            }
        }
        return slice().mid(start_idx,offset-start_idx);
    }
};

bool verifyNesting(ParseHead &pu, const char *name,QStringView var_name) {
    if(pu.tu.nesting_stack.empty()) {
        pu.error = QString("Incorrect block nesting detected when adding %1 named: %2").arg(name).arg(var_name);
        qCritical() << pu.error;
        return false; // empty
    }

    return true;
}
struct ReflectionData {
    ModuleConfig config;
    QVector<TS_Namespace*> namespaces;
    QHash<QString, TS_Base*> created_types;
};
ReflectionData g_rd;

bool save_to_file(const ReflectionData& data, QIODevice* io_device) {
    QJsonObject root;
    root["module_name"] = data.config.module_name;
    root["api_version"] = data.config.api_version;
    root["api_hash"] = data.config.api_hash;
    root["version"] = data.config.version;

    QJsonArray dependencies;
    for (const auto& v : data.config.imports) {
        QJsonObject dependency;
        dependency["name"] = v.module_name;
        dependency["api_version"] = v.api_version;
        dependencies.push_back(dependency);
    }
    root["dependencies"] = dependencies;

    QJsonArray j_namespaces;
    for (const auto* v : data.namespaces) {
        VisitorInterface* visitor = createJsonVisitor();
        v->accept(visitor);
        j_namespaces.push_back(takeRootFromJsonVisitor(visitor));
        delete visitor;
    }
    root["namespaces"] = j_namespaces;

    QString content = QJsonDocument(root).toJson();
    io_device->write(content.toUtf8());
    return true;
}

QString currentTypePath(const TranslationUnit& pu, QStringView name) {
    QString type_path;
    if (!pu.nesting_stack.empty()) {
        type_path = pu.nesting_stack.back()->relative_path();
        type_path += "::";
    }
    if (!name.isEmpty()) {
        type_path += name;
    }

    return type_path;
}

void endBlock(ParseHead& pu) {
    assert(!pu.tu.nesting_stack.empty());
    if(pu.tu.field_collectors.contains(pu.bracket_nesting_level)) {
        pu.tu.field_collectors.remove(pu.bracket_nesting_level);
    }
    int match_bracing = pu.tu.brace_nesting_stack.back();
    assert(match_bracing == -1 || match_bracing == pu.bracket_nesting_level);
    pu.tu.nesting_stack.pop_back();
    pu.tu.brace_nesting_stack.pop_back();
}

QString getNestedBlockPath(const ParseHead& pu) {
    QStringList parts;
    for (auto v : pu.tu.name_stack) {
        parts.push_back(v.name.toString());
    }
    return parts.join("::");
}

void startNamespace(ParseHead& pu, QStringView name) {
    bool skip_verify = false;
    bool in_ns_block = !pu.tu.name_stack.empty() && pu.tu.name_stack.back().type == BlockType::Namespace;
    if (pu.tu.nesting_stack.empty() && !in_ns_block) {
        skip_verify = true;
        pu.tu.name_stack.push_front({ name,name,-1,BlockType::Namespace });
    }
    const auto& entry(pu.tu.name_stack.back());

    QString type_path = currentTypePath(pu.tu, name);
    QString nested_path = getNestedBlockPath(pu);
    if (!skip_verify) {
        if (entry.type != BlockType::Namespace) {
            pu.error = QString("Macro SE_NAMESPACE was placed in non-namespace block (%1)").arg(nested_path);
            qCritical() << pu.error;
            return;
        }
        if (entry.name != name) {
            pu.error = QString("Macro SE_NAMESPACE name does not match enclosing namespace block '%1'!='%2'")
                .arg(entry.name)
                .arg(name);
            qCritical() << pu.error;
            return;
        }
        if (type_path != nested_path) {
            pu.error = QString("Macro SE_NAMESPACE nested in unregistered namespace '%1'!='%2'")
                .arg(nested_path)
                .arg(type_path);
            qCritical() << pu.error;
            return;
        }
    }
    TS_Namespace* ns;
    if (!g_rd.created_types.contains(type_path)) {
        ns = new TS_Namespace(name.toString());
        g_rd.created_types[type_path] = ns;
        if (pu.tu.nesting_stack.empty()) {
            g_rd.namespaces.push_back(ns);
        }
    }
    else {
        TS_Base* entry = g_rd.created_types[type_path];
        assert(entry->kind() == TS_Base::NAMESPACE);
        ns = (TS_Namespace*)entry;
    }

    if (!pu.tu.nesting_stack.empty()) {
        pu.tu.nesting_stack.back()->add_child(ns);
    }
    pu.tu.nesting_stack.push_back(ns);
    pu.tu.brace_nesting_stack.push_back(pu.bracket_nesting_level);
}

/*
    Const processing:
    NAME = value[,\s]
*/
void addConstant(ParseHead& pu, QStringView name) {

    if (!verifyNesting(pu, "constant", name)) {
        return;
    }

    QString type_path = currentTypePath(pu.tu, name);
    assert(!g_rd.created_types.contains(type_path));
    TS_TypeLike* tl = pu.tu.nesting_stack.back();

    QString re_text(QString(R"(^\s*%1\s*=\s*([^,\r\n]+)[,\r\n]?)").arg(name));
    QString regexp(re_text);
    QRegularExpression constexpr_re(regexp, QRegularExpression::MultilineOption);
    auto res = constexpr_re.match(pu.slice());
    QString value = res.captured(1);

    TS_Constant* cn = new TS_Constant(name.toString(), value);
    if (tl->kind() == TS_Base::ENUM) {
        // TODO: Verify constant type ( simple int expression )
    }
    else if (!value.startsWith('"')) {
        cn->const_type.name = "int32_t";
    }
    tl->add_child(cn);
    g_rd.created_types[type_path] = cn;
}

QByteArray removeComments(QByteArray dat) {
    bool in_block_comment = false;
    int idx = 0;
    QByteArray res;

    while (idx < dat.size()) {
        if (dat.mid(idx, 2) == "/*") {
            int next_eol = dat.indexOf("*/", idx + 2);
            if (next_eol == -1)
                break;
            idx = next_eol + 2;
            continue;
        }
        if (dat.mid(idx, 2) == "//") {
            int next_eol = dat.indexOf('\n', idx + 2);
            if (next_eol != -1)
                idx = next_eol - 1;
            idx++;
            continue;
        }
        if (!in_block_comment) {
            res.push_back(dat[idx]);
        }
        idx++;
    }
    return res.trimmed();
}
QByteArray joinLineContinuations(QByteArray src) {
    int idx = 0;
    QByteArray res;
    // normalize line endings;
    src.replace("\r\n","\n");
    src.replace("\r","\n");
    res.reserve(src.size());
    while (idx < src.size()) {
        char c = src[idx++];
        if(c=='\\') {
            if(idx<src.size()) {
                char next_c = src[idx];
                if(next_c=='\n') {
                    idx++; // skip \\\n thus joining with next line.
                    continue;
                }
            }
        }
        res.push_back(c);

    }
    return res;
}

QPair<int, int> extractDelimitedBlock(QStringView dat, QChar lbrack, QChar rbrack) {
    // PRE_CONDITION, dat does not contain comments. ( block and line comments removed )
    int nest_level = 0;
    int idx = 0;
    QPair<int, int> res{ -1, -1 };

    while (idx < dat.size()) {
        QChar c = dat[idx];
        if (c == lbrack) {
            nest_level++;
        }
        else if (c == rbrack) {
            nest_level--;
            if (nest_level < 1)
                break;
        }
        else {
            if (nest_level > 0 && res.first == -1 && !c.isSpace())
                res.first = idx;
        }
        idx++;
    }
    res.second = idx;
    return res;
}

enum TokenType
{
    DontCare,
    String,
    Ident,
    WS,
    EOL,
    END
};
struct Token
{
    QStringView data;
    TokenType token_type;

    bool operator==(QChar c) const
    {
        return data.size()==1 && data.front()==c;
    }

    bool operator!=(QChar c) const
    {
        return data.size()!=1 || data.front()!=c;
    }
    bool operator==(QStringView with) const
    {
        return data==with;
    }
    bool operator==(QLatin1String with) const
    {
        return data==with;
    }
};
bool isWS(QChar c)
{
    return c == ' ' || c == '\t';
}
bool isEOL(QChar c)
{
    return c == '\n' || c == '\r';
}
static QVector<Token> token_cache;
Token nextTokenInternal(ParseHead &pu)
{
    if(!token_cache.empty()) {
        return token_cache.takeFirst();
    }
    if(pu.atEnd()) {
        return Token {QStringView(),TokenType::END };
    }
    QChar c = pu.peek();
    int offset_start= pu.offset;
    if(isWS(c))
    {
        int idx=1;
        for (; pu.valid_offset(idx); ++idx)
        {
            if(!isWS(pu.peek(idx)))
            {
                break;
            }
        }
        pu.consume(idx);
        return Token {pu.slice().mid(offset_start,idx),TokenType::WS };
    }
    if (isEOL(c))
    {
        int idx = 1;
        for (; pu.valid_offset(idx); ++idx)
        {
            if (!isEOL(pu.peek(idx)))
            {
                break;
            }
        }
        pu.consume(idx);
        return Token{ pu.slice().mid(offset_start,idx),TokenType::EOL };
    }

    if(c=='"') // poor man's string extractor
    {
        bool in_escape=false;
        int idx = 1;
        for( ;pu.valid_offset(idx); ++idx)
        {
            QChar current = pu.peek(idx);
            if(in_escape)
            {
                in_escape = false;
                continue;
            }
            if (current=='\\')
            {
                in_escape = true;
                continue;
            }
            if(current=='"')
            {
                ++idx;
                break;
            }
        }
        pu.consume(idx);
        return Token{ pu.slice().mid(offset_start,idx),TokenType::String };
    }
    if(c.isLetter() || c=='_')
    {
        int idx = 1;
        for (; pu.valid_offset(idx); ++idx)
        {
            QChar current_char = pu.peek(idx);
            if (!(current_char.isLetterOrNumber()||current_char=='_'))
            {
                break;
            }
        }
        pu.consume(idx);
        return Token{ pu.slice().mid(offset_start,idx),TokenType::Ident };
    }

    pu.consume(1);
    return Token{ pu.slice().mid(offset_start,1),TokenType::DontCare };
}
Token nextToken(ParseHead &pu) {
    Token t = nextTokenInternal(pu);
   // qDebug() << t.token_type << t.data;
    return t;
}
Token nextNonWsNonEol(ParseHead &pu) {
    Token res = nextToken(pu);
    while(res.token_type==TokenType::WS || res.token_type==TokenType::EOL) {
        res = nextToken(pu);
    }
    return res;
}
void ungetToken(ParseHead &pu,Token t) {
    token_cache.push_back(t);
}

struct ArgTypeMod {
    bool is_const=false;
    bool is_volatile=false;
    bool is_restrict=false;
    bool is_signed=false;
    bool is_unsigned=false;
};
struct ArgTypeDecl {
    QStringView arg_name;
    QStringView type_name;
    QStringView template_params; // if template

    ArgTypeMod mod;
    bool is_pointer = false;
    bool is_reference = false;
    bool is_move = false;
    QStringView default_value;
    TypePassBy pass_by;
    void calc_pass_by() {
        /*
            Value = 0, // T
            Reference, // T &
            ConstReference, // const T &
            RefValue, // Ref<T>
            ConstRefReference, // const Ref<T> &
            Move, // T &&
            Pointer,
        */
        if(is_pointer) {
            pass_by=TypePassBy::Pointer;
            if(mod.is_const) {
                pass_by = TypePassBy::ConstPointer;
            }
            if(mod.is_volatile||mod.is_restrict) {
                qDebug() << "Values passed by pointers do not carry their modifiers.";
            }
            return;
        }
        if(is_reference) {
            if(mod.is_const) {
                pass_by = TypePassBy::ConstReference;
            }
            else {
                pass_by = TypePassBy::Reference;
            }
            return;
        }
        if(is_move) {
            pass_by = TypePassBy::Move;
            return;
        }
        pass_by = TypePassBy::Value;
    }
};

struct FieldDecl {

    QVector<QStringView> storage_specifiers;
    bool is_static=false;
    bool is_constexpr=false;
    ArgTypeDecl type;
    QVector<int> array_dims;
};

struct MethodDecl {
    QVector<QStringView> storage_specifiers;
    QStringView name;
    bool is_virtual=false;
    bool is_static=false;
    bool is_constexpr=false;
    ArgTypeDecl return_type;
    QVector<ArgTypeDecl> args;
};

TypeReference convertToTref(const ArgTypeDecl &from) {
    TypeReference res(from.type_name.toString());

    res.pass_by = from.pass_by;
    res.name = from.type_name.toString();
    res.template_argument = from.template_params.toString();
    // Hard Coded type reference converters.
    if(res.name=="RES") {
        res.name = "Ref";
        res.template_argument = "Resource";
    }
    return res;
}

// This is a brute-force method that replaces a few known type aliases with their expanded values.
QStringView processTypeAliases(QStringView val) {
    return val;
}
/////////////////////////////////////////////////////
} // end of anonymous namespace
/////////////////////////////////////////////////////

//TODO: enum scans backward, but does not take block nesting into account
void addEnum(ParseHead &pu, QStringView name) {
    if(!verifyNesting(pu,"enum",name)) {
        return;
    }

    assert(!pu.tu.nesting_stack.empty());
    QString type_path = currentTypePath(pu.tu, name);
    assert(!g_rd.created_types.contains(type_path));
    TS_TypeLike *tl = pu.tu.nesting_stack.back();

    QString regexp(QString(R"(enum\s+(class)?\s*%1\s*:?\s*([\w_]+)?)").arg(name));
    QRegularExpression constexpr_re(regexp, QRegularExpression::MultilineOption);
    auto res = constexpr_re.match(pu.slice());
    if (!res.hasMatch()) {
        assert(!"Cannot find start of enum definition in this file");
    }
    int num_captchures = res.lastCapturedIndex();

    int start_idx = res.capturedEnd();
    QStringView nextfew = pu.slice().mid(start_idx);
    QPair<int, int> enum_def = extractDelimitedBlock(nextfew, '{', '}');
    if (enum_def.first == enum_def.second || enum_def.second == -1) {
        qCritical() << "Enum definition is empty!";
    }
    QString type = "int32_t";
    //QStringList allcaps = res.capturedTexts();
    bool is_strict=false;
    if (num_captchures == 1) {
        // only class marker
        is_strict = res.captured(1).trimmed()=="class";
    } else if (num_captchures == 2) {
        is_strict = res.captured(1).trimmed()=="class";
        type = processTypeAliases(res.captured(2).trimmed()).toString();
    }
    TS_Enum *en = new TS_Enum(name.toString());
    en->underlying_val_type = { type };
    en->is_strict = is_strict;
    g_rd.created_types[type_path] = en;

    pu.tu.nesting_stack.push_back(en);

    int idx = 0;
    QStringView enum_def_block(pu.slice().mid(enum_def.first + start_idx, enum_def.second - enum_def.first));
    for (QStringView v : enum_def_block.split('\n')) {
        v = v.trimmed();
        if (v.isEmpty())
            continue;
        if (v.endsWith(',')) {
            v = v.mid(0, v.size() - 1);
        }
        QString number;
        auto parts = v.split('=');
        if (parts.size() == 1) {
            assert(idx >= 0);
            number = QString::number(idx++);
            parts.push_back(number);
        } else {
            bool isok = false;
            number=parts[1].toString();
            int new_idx = parts[1].toInt(&isok);
            if (!isok)
                idx = -1; // can't calculate non-'=' entries after that
            else
                idx = new_idx + 1; // next entry will have this
        }
        QString const_name=parts[0].trimmed().toString();
        TS_Constant *constant = new TS_Constant(const_name,parts[1].toString());
        en->add_child(constant);
        g_rd.created_types[type_path+"::"+const_name] = constant;
    }

    tl->add_child(en);

    pu.tu.nesting_stack.pop_back();
}
struct ClassDecl {
    QString name;
    QString base;
};

static ClassDecl extractClassName(QStringView decl) {
    ClassDecl res;
    decl = decl.trimmed();

    QRegularExpression splitter(R"((\w+)\s*(:\s*.*)?)");
    auto result = splitter.match(decl);
    assert(result.hasMatch());

    res.name = result.captured(1);
    if (result.lastCapturedIndex() == 2)
        res.base = result.captured(2).mid(1).trimmed(); // skip :
    return res;
}

void processSEClass(ParseHead &pu, QStringView params) {
    assert(!pu.tu.nesting_stack.empty());
    if(pu.tu.name_stack.empty()) {
        pu.error = "SE_CLASS macro placed outside of a block";
        return;
    }
    const auto &namestack_entry(pu.tu.name_stack.back());
    auto blocktype =namestack_entry.type;
    if(blocktype!=BlockType::Class && blocktype!=BlockType::Struct) {
        pu.error = "SE_CLASS macro must be placed inside class or struct block";
        return;
    }

    QStringView class_decl = namestack_entry.full_def.trimmed();

    // Step 2. parse the decl.
    ClassDecl parsed_decl = extractClassName(class_decl);

    auto opts=params.split(',');
    const bool is_singleton=opts.contains(QString("singleton"));
    const bool is_struct=opts.contains(QString("struct"));
    // handle opaque types with enums/constants.
    const bool is_opaque=opts.contains(QString("opaque"));

    // TODO: find actual base-class based on parsed bases
    if (!parsed_decl.base.isEmpty()) {
        QStringList bases=parsed_decl.base.split(',');
        QString first_base=bases.first();
        auto cc = first_base.split(' ');
        if(cc.size()==1) // no access specifier
            parsed_decl.base = first_base;
        else {
            parsed_decl.base = cc.mid(1).join(' '); // skip the access specifier and join the rest;
        }
    }

    QString type_path = currentTypePath(pu.tu, parsed_decl.name);
    QString nested_path = getNestedBlockPath(pu);
    if(type_path.size()!=nested_path.size()) {
        pu.error = "SE_CLASS macro placed in nested class that has no SE_CLASS macro, this is unsupported";
        return;
    }

    assert(!g_rd.created_types.contains(type_path));
    TS_TypeLike *tl = pu.tu.nesting_stack.back();

    TS_Type *tp = new TS_Type(parsed_decl.name);
    tp->required_header = QString(pu.tu.filename).replace(".cpp", ".h");
    tp->is_singleton = is_singleton;
    tp->is_opaque = is_opaque;

    if (!parsed_decl.base.isEmpty()) {
        tp->base_type = { parsed_decl.base };
    }
    if(is_struct) {
        pu.tu.field_collectors[pu.bracket_nesting_level] = tp;
    }

    pu.tu.nesting_stack.push_back(tp);
    pu.tu.brace_nesting_stack.push_back(pu.bracket_nesting_level);

    tl->add_child(tp);
}

static void processOpaqueType(ParseHead &pu, QStringView params) {
    assert(!pu.tu.nesting_stack.empty());
    if(pu.tu.name_stack.empty()) {
        pu.error = "SE_OPAQUE_TYPE must have enclosing type/namespace";
        return;
    }

    bool valid_name=true;
    for(QChar c : params) {
        if(!(c.isLetterOrNumber() || c=='_')) {
            valid_name=false;
            break;
        }
    }
    if(!valid_name) {
        pu.error = QString("SE_OPAQUE_TYPE provided with invalid type name:")+params.toString();
        return;
    }

    QString type_path = currentTypePath(pu.tu, params);

    assert(!g_rd.created_types.contains(type_path));
    TS_TypeLike *tl = pu.tu.nesting_stack.back();

    TS_Type *tp = new TS_Type(params.toString());
    tp->required_header = QString(pu.tu.filename).replace(".cpp", ".h");
    tp->is_opaque = true;

    tl->add_child(tp);
}

void processSEProperty(ParseHead &pu, QStringView params) {
    assert(!pu.tu.nesting_stack.empty());
    TS_TypeLike *tl_val = pu.tu.nesting_stack.back();
    if(tl_val->kind()!=TS_Base::CLASS) {
        pu.error = "Properties are currently only supported in classes";
        return;
    }
    TS_Type *tl = static_cast<TS_Type *>(tl_val);

    using Elem = QPair<QStringView,QStringView>;
    int read_idx = params.indexOf(QStringLiteral(" READ"));
    if(read_idx==-1) {
        pu.error = "Property must have READ option";
        return;
    }
    QStringView type_and_name = params.mid(0,read_idx).trimmed();
    int last_idx=type_and_name.size()-1;
    while(last_idx>0) { // if it reaches 0 we've failed to find type
        QChar c = type_and_name[last_idx];
        if(!(c.isLetterOrNumber() || c=='_')) {
            break;
        }
        --last_idx;
    }
    QStringView type_name = processTypeAliases(type_and_name.mid(0,last_idx).trimmed());
    QStringView property_name = type_and_name.mid(last_idx).trimmed();

    params = params.mid(read_idx).trimmed();
    auto elem_source = params.split(' ');
    int num_elems = elem_source.size();
    if((num_elems%2)!=0) {
        pu.error = "SE_PROPERTY needs an even number of parameters";
        return;
    }
    if(num_elems<2) {
        pu.error = "SE_PROPERTY needs at least type,name and read function";
        return;
    }

    QVector<Elem> options;
    for(int idx=0; idx<num_elems; idx+=2) {
        options.push_back({elem_source[idx],elem_source[idx+1]});
    }

    TS_Property *prop=nullptr;
    bool should_add = true;
    QStringView group_name;
    for(const Elem &e:options) {
        if(e.first==QLatin1String("GROUP")) {
            // we have a grouped property, see if we can find it in our type.
            group_name = e.second;

            tl->visit_kind(TS_Base::PROPERTY,[&](const TS_Base *child) {
                assert(prop==nullptr && "If this assert fails, it means there are multiple properties with the same name");
                TS_Property *child_prop = (TS_Property *)child;
                if(child_prop->name==group_name) {
                    prop = child_prop;
                }
            });
            if(!prop) { // not available yet.
                prop = new TS_Property(group_name.toString());
                prop->max_property_index = -2; // grouped
            }
            else {
                should_add = false;
            }
        }
    }

    if(!prop) {
        prop = new TS_Property(property_name.toString());
    }

    TS_Property::ResolvedPropertyEntry entry;
    Elem read_def = options.takeFirst();
    entry.entry_type.push_back({type_name.toString()});
    entry.getter = read_def.second.toString();

    // we're in a group, property names are stored in entries.
    if(!group_name.empty()) {
        entry.subfield_name = property_name.toString();
    }

    while(!options.empty()) {
        Elem opt = options.takeFirst();
        if(opt.first==QLatin1String("WRITE")) {
            entry.setter = opt.second.toString();
        } else if(opt.first==QLatin1String("RESET")) {
            qDebug() << "Unhandled resetFunc";
        } else if(opt.first==QLatin1String("NOTIFY")) {
            qDebug() << "Unhandled notify";
        } else if(opt.first==QLatin1String("USAGE")) {
            for(const auto &v : opt.second.split('|')) {
                prop->usage_flags.push_back(v.trimmed().toString());
            }
        } else if(opt.first==QLatin1String("META_FUNC")) {
            qDebug() << "Unhandled metaFunc";
        } else if(opt.first==QLatin1String("GROUP")) {
            // handled above.
        } else {
            qDebug() << "Unhandled SE_PROPERTY option"<<opt.first<<opt.second;
        }

    }
    prop->indexed_entries.push_back(entry);
    if(should_add) {
        tl->add_child(prop);
    }
}

static bool ensureNS(ParseHead &pu) {
    // TODO: this needs a global setting on the commandline ??
    if (pu.tu.nesting_stack.empty()) {
        // Global namespace is always put at brace nesting level == -1;
        int current_level = pu.bracket_nesting_level;
        pu.bracket_nesting_level = -1;
        // this might be done after a block was already visited ->
        // class Foo {
        //   SE_CLASS()
        // will visit the block-open and record Foo, and after seeing SE_CLASS it will ensureNS
        startNamespace(pu, g_rd.config.default_ns);
        pu.bracket_nesting_level = current_level;
        return true;
    }

    return false;
}

static bool parseArgTypeMod(ParseHead &pu,ArgTypeMod &tgt) {
    // const,volatile,restrict,signed,unsigned  [ long double unhandled ]

    // mods* type_spec mods*
    Token t = nextNonWsNonEol(pu);

    bool mod_found=true;
    if(t.token_type!=TokenType::Ident) {
        return false;
    }

    while(mod_found && t.token_type==TokenType::Ident) {
        QStringView ident=t.data;
        if(ident==QLatin1String("const")) {
            tgt.is_const=true;
        }
        else if(ident==QLatin1String("volatile")) {
            tgt.is_volatile=true;
        }
        else if(ident==QLatin1String("restrict")) {
            tgt.is_restrict=true;
        }
        else if(ident==QLatin1String("signed")) {
            tgt.is_signed=true;
        }
        else if(ident==QLatin1String("unsigned")) {
            tgt.is_unsigned=true;
        } else {
            mod_found = false;
            ungetToken(pu,t);
            break;
        }

        t = nextNonWsNonEol(pu);
    }
    return true;
}

// Handles only very simple types : TypeName | TemplateName<TypeName>
static bool parseTypeSpec(ParseHead &pu,ArgTypeDecl &tgt) {
    // get typename
    Token t = nextNonWsNonEol(pu);
    if(t.token_type!=TokenType::Ident) {
        return false;
    }

    tgt.type_name = processTypeAliases(t.data);

    assert(tgt.type_name.size()>1);
    t = nextNonWsNonEol(pu);

    // Find internal text of <.....>
    if(t.token_type==TokenType::DontCare && t=='<') {
        int start_ab = pu.offset;
        int nesting_depth=1;
        while(!pu.atEnd()) {
            QChar c = pu.peek();
            pu.consume();
            if(c=='<') {
                nesting_depth++;
            }
            else if(c=='>') {
                nesting_depth--;
                if(nesting_depth==0)
                    break;
            }
        }
        tgt.template_params = pu.slice().mid(start_ab,pu.offset-start_ab-1);
    } else {
        ungetToken(pu,t);
    }
    return true;
}
//NOTE: this only handles west-const style definitions.
static bool parseArgTypeDecl(ParseHead &pu,ArgTypeDecl &tgt) {

    // mods* type_spec [*|&]?
    if(!parseArgTypeMod(pu,tgt.mod)) {
        return false;
    }
    if(!parseTypeSpec(pu,tgt)) {
        return false;
    }

    Token t = nextNonWsNonEol(pu);
    if(t.token_type==END) {
        tgt.calc_pass_by();
        return true;
    }
    if(t.token_type!=DontCare) {
        // what follows is not a '*'/'&' etc., bail
        tgt.calc_pass_by();
        ungetToken(pu,t);
        return true;
    }

    QChar m = t.data.front();
    Token following = nextNonWsNonEol(pu);
    // disallow ** *& &*
    if( (m=='*' && following=='*') || (m=='*' && following=='&') || (m=='&' && following=='*') ) {
        qCritical() << "Unhandled function return/argment type";
        return false;
    }
    if(m=='*' ) {
        tgt.is_pointer=true;
        ungetToken(pu,following);
    }
    else if(m=='&') {
        if(following=='&') {

            tgt.is_move=true;
            return true;
        }
        ungetToken(pu,following);
        tgt.is_reference=true;
    } else {
        ungetToken(pu,t);
        ungetToken(pu,following);
    }
    tgt.calc_pass_by();
    return true;
}
static void parseArgumentDefault(ParseHead &pu,ArgTypeDecl &tgt) {
    static QString braced_default("{}");
    int bracket_nest_level=0;
    int paren_nest_level=0;
    pu.skipWS();
    int start_offset=pu.offset;
    Token t = nextNonWsNonEol(pu);

    while(t.token_type!=END) {

        if(bracket_nest_level==0 && paren_nest_level==0 && (t==',' || t==')')) {
            // don't collect ','
            // can't use unget token, since we extract things using offsets
            pu.offset--;
            break;
        }
        if(t=='{') {
            bracket_nest_level++;
        } else if (t=='}') {
            bracket_nest_level--;
        }
        if(t=='(') {
            paren_nest_level++;
        } else if (t==')') {
            paren_nest_level--;
        }
        t = nextNonWsNonEol(pu);
    }
    tgt.default_value = pu.slice().mid(start_offset,pu.offset-start_offset);
    QString df=tgt.default_value.toString();
    if(tgt.default_value.startsWith(tgt.type_name)) {
        if(tgt.default_value.mid(tgt.type_name.size())==QLatin1String("()")) {
            qDebug() << "Replacing explicit constructor call with {}";
            tgt.default_value = braced_default;
        }
        else
            qWarning() << "Invocable function with default argument that uses type constructor directly, will likely not work";
    }
}
static bool parseDeclArguments(ParseHead &pu,MethodDecl &tgt) {
    while(!pu.atEnd()) {
        ArgTypeDecl arg;
        if(!parseArgTypeDecl(pu,arg)) {
            return false;
        }
        Token t = nextNonWsNonEol(pu);
        if(t.token_type!=Ident) {
            return false;
        }
        arg.arg_name = t.data;
        t = nextNonWsNonEol(pu);
        if(t=='=') {
            parseArgumentDefault(pu,arg);
            t = nextNonWsNonEol(pu); // take the optional ','
        }
        if(t!=',') {
            ungetToken(pu,t);
        }
        tgt.args.push_back(arg);
    }
    return true;
}
static bool parseField(ParseHead &pu,FieldDecl &tgt) {

    int offset = pu.offset;
    if(!parseArgTypeDecl(pu,tgt.type)) {
        return false;
    }
    Token  t = nextNonWsNonEol(pu);
    if(t=='(') { // not a field, but a method/constructor def
        pu.offset = offset; // rewind to allow function parsing to work.
        return false;
    }
    if(t.token_type!=Ident) {
        return false;
    }
    tgt.type.arg_name = t.data;
    t = nextNonWsNonEol(pu);
    if(t=='=' || t=='{') {
        if(t=='{') {
            // allow proper '{' '}' nesting to work.
            ungetToken(pu,t);
        }
        parseArgumentDefault(pu,tgt.type);
    }
    if(t=='[') {
        int square_bracket_nesting=1;
        int start_offset = pu.offset;
        while(t!=';' && t!=END) {
            t = nextNonWsNonEol(pu);
            if(t.token_type==DontCare) {
                if(t=='[') {
                    square_bracket_nesting++;
                } else if(t==']') {
                    square_bracket_nesting--;
                }
            }
        }
        // -2 to skip closing ']' and ';'
        QStringView bracket_arg = pu.slice().mid(start_offset,pu.offset-start_offset-2);

        const QString lat("][");
        auto parts = bracket_arg.split(lat);
        for(const auto part : parts) {
            // multidimensional arrays are supported
            bool parsed=false;
            int dim = part.toInt(&parsed);
            if(!parsed) {
                pu.error = "soc only supports array sizes of numeric value";
                return false;
            }
            tgt.array_dims.push_back(dim);
        }
    }
    if(t==';') {
        return true;
    }
    return false;
}
static bool extractField(ParseHead &pu,TS_Type *target) {

    FieldDecl decl;
    if(!parseField(pu,decl)) {
        return false;
    }
    TS_Field *fld = new TS_Field(decl.type.arg_name.toString());
    fld->field_type = convertToTref(decl.type);
    if(!decl.array_dims.empty()) {
        // augment type with array info.
        fld->field_type.type_kind = TypeRefKind::Array;
        QStringList dims;
        for(int d : decl.array_dims) {
            dims.push_back(QString::number(d));
        }
        QString array_bounds = "[" + dims.join("][") + "]";
        fld->field_type.name += array_bounds;

    }
    target->add_child(fld);

    return true;

}

static void parseDeclAttrib(ParseHead &pu,MethodDecl &tgt) {
    // (/s+ [ virtual | static | constexpr | inline ] )*
    Token tok = nextNonWsNonEol(pu);
    while(tok.token_type==Ident) {
        if(tok==QLatin1String("virtual")) {
            tgt.is_virtual=true;
        }
        else if(tok==QLatin1String("static")) {
            tgt.is_static=true;
        }
        else if(tok==QLatin1String("constexpr")) {
            tgt.is_constexpr=true;
        }
        else if(tok==QLatin1String("inline")) {
        }
        else {
            // Not recognized as a decl attribute, rewind and return
            ungetToken(pu,tok);
            break;
        }
        tok = nextNonWsNonEol(pu);
    }
}

void addMethod(const ParseHead &pu,const MethodDecl &mdecl)
{
    assert(!pu.tu.nesting_stack.empty());
    TS_TypeLike *tl = pu.tu.nesting_stack.back();
    TS_Function *func = new TS_Function(mdecl.name.toString());

    func->return_type = convertToTref(mdecl.return_type);
    int idx=0;
    for(const auto &arg : mdecl.args) {
        func->arg_values.append(arg.arg_name.toString());
        func->arg_types.append(convertToTref(arg));
        if(!arg.default_value.isEmpty())
            func->arg_defaults[idx] = arg.default_value.toString();
        ++idx;
    }
    func->m_static = mdecl.is_static;
    func->m_virtual = mdecl.is_virtual;

    tl->add_child(func);
}

void addSignal(const ParseHead &pu,const MethodDecl &mdecl)
{
    assert(!pu.tu.nesting_stack.empty());
    TS_TypeLike *tl = pu.tu.nesting_stack.back();
    TS_Signal *func = new TS_Signal(mdecl.name.toString());

    assert(mdecl.return_type.type_name==QLatin1String("void"));

    int idx=0;
    for(const auto &arg : mdecl.args) {
        func->arg_values.append(arg.arg_name.toString());
        func->arg_types.append(convertToTref(arg));
        if(!arg.default_value.isEmpty())
            func->arg_defaults[idx] = arg.default_value.toString();
        ++idx;
    }
    tl->add_child(func);
}


// Not a very smart function decl parser.
// handles a small-ish subset of all possible decls
MethodDecl parseMethod(ParseHead &pu) {
    MethodDecl mdecl;

    parseDeclAttrib(pu,mdecl);

    if(!parseArgTypeDecl(pu,mdecl.return_type) ) {
        return mdecl; // returning empty mdecl ->means error
    }
    Token name_token = nextNonWsNonEol(pu);
    if(name_token.token_type!=Ident) {
        pu.error = "Failed to parse invocable method declaration";
        return mdecl; // returning empty mdecl ->means error
    }
    mdecl.name = name_token.data;

    Token next_token = nextNonWsNonEol(pu);
    // Arguments
    assert(next_token=='(');

    int start_args=pu.offset;
    int nesting_depth=1;
    Token fin_token = nextNonWsNonEol(pu);
    while(fin_token!=END && nesting_depth!=0) {
        if(fin_token=='(') {
            nesting_depth++;
        } else if(fin_token==')') {
            nesting_depth--;
        }
        fin_token = nextNonWsNonEol(pu);
    }
    ParseHead arg_block(pu,start_args,pu.offset-start_args-1); // -1 to account for the closing ')'
    parseDeclArguments(arg_block,mdecl);

    //dumpFuncDecl(mdecl);

    return mdecl;
}

static void processParameterlessMacro(ParseHead &pu,QStringView macroname) {
    if(macroname==QLatin1String("INVOCABLE")) {
        auto mdecl = parseMethod(pu);
        if(!mdecl.name.empty()) {
            addMethod(pu,mdecl);
        }
    } else if(macroname==QLatin1String("SIGNAL")) {
        auto mdecl = parseMethod(pu);
        if(!mdecl.name.empty()) {
            addSignal(pu,mdecl);
        }

    } else if(macroname==QLatin1String("SIGNALS")) {
        qCritical() << "Support for SE_SIGNALS is not finished yet";
        //TODO: finish this, the main problem is detecting all valid function definitions that follow.
        pu.collecting_signals = true;
    } else {
        qDebug()<< "Found unhandled parameterless macro"<<macroname;

    }
}

static void recordBlockName(ParseHead &pu) {
    QStringView substr(pu.slice().mid(0,pu.tu.open_brace_indices.back()));
    // search backwards for things that are 100% not a part of class/struct/namespace definition.
    //NOTE: This does not take into account some crazy things like class Foo : public Wow<";\"">
    QChar fin_chars[] = { ';','"','\'','{','}'};
    for(QChar c : fin_chars) {
        int prev_endchar_idx = substr.lastIndexOf(c);
        if(prev_endchar_idx!=-1) {
            // 'SOME_CHAR'.....END_OF_BUFFER
            substr=substr.mid(prev_endchar_idx+1);
        }
    }

    const QLatin1String incorrect[]= {QLatin1String("if"),QLatin1String("enum class"),QLatin1String("enum"),QLatin1String("while")};

    for(const QLatin1String &s : incorrect) {
        qsizetype prev_kw_idx = substr.lastIndexOf(s);
        if(prev_kw_idx!=-1) {
            QChar c;
            // check for ;|WS before and WS after the keyword
            if(prev_kw_idx!=0) {
                c = substr[prev_kw_idx-1];
                if(c!=';' && !c.isSpace())
                    continue;
            }
            c = substr[prev_kw_idx+s.size()];
            if(!c.isSpace())
                continue;
            // 'SOME_CHAR'.....END_OF_BUFFER
            substr=substr.mid(prev_kw_idx+s.size());
        }
    }
    if(substr.size()<7) {
        // not enough chars for the simplest case of `class A`
        return;
    }
    const QLatin1String keywords[]= {QLatin1String("class"),QLatin1String("struct"),QLatin1String("namespace")};
    int kw_idx=-1;
    int idx=0;
    for(const QLatin1String &s : keywords) {
        int prev_class_kw_idx = substr.lastIndexOf(s);
        if(prev_class_kw_idx!=-1) {
            QChar c;
            // check for ;|WS before and WS after the keyword
            if(prev_class_kw_idx!=0) {
                c = substr[prev_class_kw_idx-1];
                if(c!=';' && !c.isSpace())
                    continue;
            }
            c = substr[prev_class_kw_idx+s.size()];
            if(!c.isSpace())
                continue;
            kw_idx = idx;
            // 'SOME_CHAR'.....END_OF_BUFFER
            substr=substr.mid(prev_class_kw_idx+s.size());
        }
        idx++;
    }
    if(kw_idx==-1) {
        return;
    }
    substr=substr.trimmed();
    QStringView full_def=substr;
    if(kw_idx==0 || kw_idx==1) {
        // processing class_name : base_class
        int offset=0;
        int angle_nesting=0;
        //NOTE: this will fail in case of lshift or rshift operator use in the class/struct name
        for(int fin=substr.size(); offset<fin; ++offset) {
            QChar c = substr[offset];
            if(c=='>') {
                angle_nesting--;
            } else if (c=='<') {
                angle_nesting++;
            }
            if(angle_nesting!=0)
                continue;
            if(!(c.isLetterOrNumber() || c=='_')) {
                if(c==':' && (offset+1)<fin && substr[offset+1]==':') {
                    // double :: namespace/class path
                    offset++;
                    continue;
                }
                break;
            }
        }
        substr = substr.mid(0,offset);
    } else {
        // processing namespace name
        int offset=0;
        for(int fin=substr.size(); offset<fin; ++offset) {
            QChar c = substr[offset];
            if(!(c.isLetterOrNumber() || c=='_' || c==':')) {
                break;
            }
        }
        substr = substr.mid(0,offset);
    }
    if(!substr.isEmpty()) {
        pu.tu.name_stack.push_back({substr,full_def,pu.bracket_nesting_level,(BlockType)kw_idx});
    }
}
static void startBlock(ParseHead &pu,const Token &t) {
    assert(t.data.data()>=pu.tu.contents.data() && t.data.data()<pu.tu.contents.data()+pu.tu.contents.size());
    pu.tu.open_brace_indices.push_back(t.data.data()-pu.tu.contents.data());
    pu.bracket_nesting_level++;
    recordBlockName(pu);
}
/*
    simple tag grammar:
    SE_NAMESPACE can appear in two contexts:

    1. translation unit:
    `#pragma once
    SE_NAMESPACE(Foo)
    `
    The `Foo` namespace will be defined and apply to all following types

    2. block
    `
    namespace X {
        SE_NAMESPACE(X)
    }
    `
    The `X` will be defined and applied to all types defined in c++ namespace block.

    SE_ENUM(Name) must appear directly after enum definition
*/
int processBlock(ParseHead &pu) {
    bool added_ns=false;

    int line_counter = 0;
    bool valid_start=true; // set to false until we encounter [START,WS,EOL]

    while (!pu.atEnd()) {
        if(!pu.error.isEmpty()) {
            return -1;
        }
        Token t = nextToken(pu);
        TS_Type *collector = pu.tu.field_collectors.value(pu.bracket_nesting_level,nullptr);
        if(t.token_type == TokenType::DontCare)
        {
            if (t == '{') {
                startBlock(pu,t);
            }
            else if (t == '}') {
                pu.tu.open_brace_indices.pop_back();
                while (!pu.tu.brace_nesting_stack.isEmpty() && pu.bracket_nesting_level <= pu.tu.brace_nesting_stack.back()) {
                    endBlock(pu);
                }
                if (!pu.tu.name_stack.empty() && pu.tu.name_stack.back().level == pu.bracket_nesting_level) {
                    pu.tu.name_stack.pop_back();
                }
                pu.bracket_nesting_level--;
            }
            continue;
        }
        if(t.token_type==TokenType::String)
        {
            continue;
        }
        if (t.token_type == TokenType::WS || t.token_type == TokenType::EOL) {
            if (t.token_type == TokenType::EOL) {
                line_counter++;
                valid_start = true;
            }
            continue;
        }

        if(valid_start && t.token_type==TokenType::Ident)
        {
            QStringView partial(t.data);
            QString pp(partial.toString());
            // we search for start of on of the macro keywords
            if(!partial.startsWith(QLatin1String("SE_")))
            {
                if(collector) {
                    ungetToken(pu,t);
                    if(!extractField(pu,collector)) {
                        // extract field rewinds to pu.offset, but the 't' token was taken off the unget stack.
                            ungetToken(pu,t);
                    }
                }
                valid_start=false;
                continue;

            }
        } else {
            valid_start = false;
            continue;
        }

        // contents at idx are SE_...
        QStringView macro_name=t.data.mid(3);
        t = nextNonWsNonEol(pu);
        bool non_parametric_token=true; // SE_INVOCABLE, SE_SIGNALS etc.
        if(t.token_type==TokenType::DontCare) {
            non_parametric_token = t != '(';
        }
        else {
            ungetToken(pu,t); // can be IDENT for a function name etc.
        }

        if(non_parametric_token) {
            processParameterlessMacro(pu,macro_name);
            continue;
        }

        int end_of_macro = pu.searchForward(')');
        QStringView macro_params = pu.peek_slice(end_of_macro - pu.offset).trimmed();

        pu.consume(end_of_macro - pu.offset);

        if (macro_name == QLatin1String("NAMESPACE")) {
            if(macro_params.isEmpty()) {
                qWarning() << "SE_NAMESPACE requires a parameter";
                continue;
            }
            //qDebug() << "NS:" << macro_params << " in" << file;
            startNamespace(pu, macro_params);
            continue;
        }
        // BIND_GLOBAL_CONSTANT(SPKEY)
        if (macro_name == QLatin1String("CONSTANT")) {
            if(macro_params.isEmpty()) {
                qWarning() << "SE_CONSTANT requires a parameter";
                continue;
            }
            added_ns = ensureNS(pu);
            //qDebug() << "CONSTANT:" << macro_params << " in" << file;
            addConstant(pu, macro_params);
            continue;
        }
        if (macro_name == QLatin1String("ENUM")) {
            if(macro_params.isEmpty()) {
                qWarning() << "SE_ENUM requires a parameter";
                continue;
            }
            added_ns = ensureNS(pu);
            //qDebug() << "ENUM:" << macro_params << " in" << file;
            addEnum(pu, macro_params);
            continue;
        }
        if (macro_name == QLatin1String("CLASS")) {
            added_ns = ensureNS(pu);
            processSEClass(pu, macro_params);
            continue;
        }
        if (macro_name == QLatin1String("PROPERTY")) {
            added_ns = ensureNS(pu);
            processSEProperty(pu, macro_params);
            continue;
        }
        if (macro_name == QLatin1String("OPAQUE_TYPE")) {
            added_ns = ensureNS(pu);
            if(macro_params.isEmpty()) {
                qWarning() << "SE_OPAQUE_TYPE requires a parameter";
                continue;
            }
            //qDebug() << "NS:" << macro_params << " in" << file;
            processOpaqueType(pu, macro_params);
            continue;
        }

        if (macro_name == QLatin1String("END")) {
            endBlock(pu);
            if(!pu.tu.name_stack.empty() && pu.tu.name_stack.back().level==pu.bracket_nesting_level) {
                pu.tu.name_stack.pop_back();
            }
        }
    }
    if(added_ns) {
        endBlock(pu);
    }
    return 0;
}
void pseudoPreprocessor(QString &source) {
    // for now we only do a single thing: replace GODOT_EXPORT macros.
    QRegularExpression z(R"([\s\n]GODOT_EXPORT[\s\n])",QRegularExpression::MultilineOption);
    source.replace(z," ");
}
bool processFile(const QString &filename, QIODevice *dev) {
    TranslationUnit pu;
    pu.filename = filename;
    pu.contents = QString::fromUtf8(joinLineContinuations(removeComments(dev->readAll())));
    pseudoPreprocessor(pu.contents);

    ParseHead head(pu);
    bool res = processBlock(head)==0;
    assert(pu.field_collectors.empty());
    if(!res)
        qCritical()<<head.error;
    return res;
}

static bool save_cpp(const ReflectionData &data, QIODevice *io) {
    VisitorInterface *visitor = createCppVisitor();
    for (const auto *v : data.namespaces) {
        v->accept(visitor);
    }
    produceCppOutput(visitor,io);
    delete visitor;
    return true;
}

bool processHeader(const QString &fname, QIODevice *src) {
    return processFile(fname, src);
}
bool exportJson(QIODevice *tgt) {
    return save_to_file(g_rd, tgt);
}
bool exportCpp(QIODevice *tgt) {
    return save_cpp(g_rd, tgt);
}

void initContext() {
    for(auto x : g_rd.created_types) {
        delete x;
    }
    g_rd = {};
}

void setConfig(const ModuleConfig &mc)
{
    g_rd.config = mc;
}
