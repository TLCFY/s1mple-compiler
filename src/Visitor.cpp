#include "Visitor.h"

#include "exceptions/NotImplementedException.h"
#include "exceptions/VariableNotFoundException.h"
#include "exceptions/ProcedureNotFoundException.h"
#include "builtin/StandardProcedure.h"

using namespace antlr4;
using namespace PascalS;

llvm::Value *Visitor::getVariable(const std::string &name)
{
    for (auto it = scopes.rbegin(); it != scopes.rend(); it++)
    {
        auto variable = it->getVariable(name);
        if (variable)
            return variable;
    }
    return nullptr;
}

void Visitor::fromFile(const std::string &path)
{
    std::ifstream stream(path);

    auto input = new ANTLRInputStream(stream);
    auto lexer = new PascalSLexer(input);
    auto tokens = new CommonTokenStream(lexer);
    auto parser = new PascalSParser(tokens);

    // parser->setBuildParseTree(true);
    // tree::ParseTree *tree = parser->program();
    // std::cout << tree->toStringTree(&parser) << std::endl;

    auto program = parser->program();

    visitProgram(program);
}

void Visitor::visitProgram(PascalSParser::ProgramContext *context)
{
    auto functionType = llvm::FunctionType::get(builder.getVoidTy(), {}, false);
    auto programName = visitProgramHeading(context->programHeading());
    auto function = llvm::Function::Create(functionType, llvm::GlobalValue::LinkageTypes::ExternalLinkage, "main", module.get());

    scopes.push_back(Scope());
    scopes.back().setVariable(programName, function);

    visitBlock(context->block(), function);

    builder.CreateRetVoid();
}

std::string Visitor::visitProgramHeading(PascalSParser::ProgramHeadingContext *context)
{
    return visitIdentifier(context->identifier());
}

std::string Visitor::visitIdentifier(PascalSParser::IdentifierContext *context)
{
    return context->IDENT()->getText();
}

llvm::Value *Visitor::visitBlock(PascalSParser::BlockContext *context, llvm::Function *function)
{
    for (const auto &procedureAndFunctionDeclarationPart : context->procedureAndFunctionDeclarationPart())
    {
        visitProcedureAndFunctionDeclarationPart(procedureAndFunctionDeclarationPart);
    }
    auto block = llvm::BasicBlock::Create(*llvm_context, "", function);
    builder.SetInsertPoint(block);
    for (const auto &constantDefinitionPartContext : context->constantDefinitionPart())
    {
        visitConstantDefinitionPart(constantDefinitionPartContext);
    }
    for (const auto &variableDeclarePartContext : context->variableDeclarationPart())
    {
        visitVariableDeclarationPart(variableDeclarePartContext);
    }
    for (const auto &typeDefinitionPartContext : context->typeDefinitionPart())
    {
        visitTypeDefinitionPart(typeDefinitionPartContext);
    }

    return visitCompoundStatement(context->compoundStatement());
}

void Visitor::visitTypeDefinitionPart(PascalSParser::TypeDefinitionPartContext *context)
{
    for (const auto &typeDefinitionContext : context->typeDefinition())
    {
        visitTypeDefinition(typeDefinitionContext);
    }
}

void Visitor::visitTypeDefinition(PascalSParser::TypeDefinitionContext *context)
{
    auto identifier = visitIdentifier(context->identifier());

    if (auto typeSimpleTypeContext = dynamic_cast<PascalSParser::TypeSimpleTypeContext *>(context->type_()))
    {
        // TODO: fix type defi
        auto type = visitTypeSimpleType(typeSimpleTypeContext);
        auto addr = builder.CreateAlloca(type, nullptr);
        builder.CreateStore(llvm::UndefValue::get(type), addr);
        scopes.back().setVariable(identifier, addr);
    }
    else if (auto typeStructuredTypeContext = dynamic_cast<PascalSParser::TypeStructuredTypeContext *>(context->type_()))
    {
        // visitTypeStructuredType(typeStructuredTypeContext);
    }
    else
        throw NotImplementedException();
}

llvm::Value *Visitor::visitCompoundStatement(PascalSParser::CompoundStatementContext *context)
{
    return visitStatements(context->statements());
}

llvm::Value *Visitor::visitStatements(PascalSParser::StatementsContext *context)
{
    for (const auto &statementContext : context->statement())
    {
        if (auto simpleStatementContext = dynamic_cast<PascalSParser::SimpleStateContext *>(statementContext))
            visitSimpleState(simpleStatementContext);
        // else if(auto structuredStatementContext = dynamic_cast<PascalSParser::StructuredStateContext *>(statementContext))
        //     visitStructuredState(structuredStatementContext);
        else
            throw NotImplementedException();
    }

    //此处只是示例，为了编译可以通过
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvm_context), -10);
}

void Visitor::visitSimpleState(PascalSParser::SimpleStateContext *context)
{
    // if (auto assignmentStatementContext = dynamic_cast<PascalSParser::SimpleStateAssignContext *>(context->simpleStatement()))
    //     visitSimpleStateAssign(assignmentStatementContext);
    // else
    if (auto procedureStatementContext = dynamic_cast<PascalSParser::SimpleStateProcContext *>(context->simpleStatement()))
        visitSimpleStateProc(procedureStatementContext);
    else if (auto emptyStatementContext = dynamic_cast<PascalSParser::SimpleStateEmptyContext *>(context->simpleStatement()))
        visitSimpleStateEmpty(emptyStatementContext);
    else
        throw NotImplementedException();
}

void Visitor::visitSimpleStateProc(PascalSParser::SimpleStateProcContext *context)
{
    visitProcedureStatement(context->procedureStatement());
}

void Visitor::visitProcedureStatement(PascalSParser::ProcedureStatementContext *context)
{
    auto identifier = visitIdentifier(context->identifier());
    if (getVariable(identifier))
    {
        // 调用
    }
    else if (StandardProcedure::hasProcedure(identifier))
    {
        // 获取原型
        // 构��参敄1�71ￄ1�77
        // 调用
    }
    else
        throw ProcedureNotFoundException(identifier);
}

std::vector<llvm::Value *> *Visitor::visitParameterList(PascalSParser::ParameterListContext *context)
{
    auto params = new std::vector<llvm::Value *>;
    return params;
}

void Visitor::visitSimpleStateEmpty(PascalSParser::SimpleStateEmptyContext *context)
{
    visitEmptyStatement_(context->emptyStatement_());
}

void Visitor::visitEmptyStatement_(PascalSParser::EmptyStatement_Context *context)
{
}

void Visitor::visitConstantDefinition(PascalSParser::ConstantDefinitionContext *context)
{
    // identifier = const
    auto id = visitIdentifier(context->identifier());
    // switch const
    if (auto constIdentifierContext = dynamic_cast<PascalSParser::ConstIdentifierContext *>(context->constant()))
    {
        auto addr = visitConstIdentifier(constIdentifierContext);
        scopes.back().setVariable(id, addr);
    }
    else if (auto constStringContext = dynamic_cast<PascalSParser::ConstStringContext *>(context->constant()))
    {
        auto v = visitConstString(constStringContext);
        auto value = llvm::ConstantDataArray::getString(*llvm_context, v);
        auto addr = builder.CreateAlloca(value->getType(), nullptr);
        builder.CreateStore(value, addr);
        scopes.back().setVariable(id, addr);
    }
    else if (auto ConstunumberCtx = dynamic_cast<PascalSParser::ConstUnsignedNumberContext *>(context->constant()))
    {
        auto value = visitConstUnsignedNumber(ConstunumberCtx);
        auto addr = builder.CreateAlloca(value->getType(), nullptr);
        builder.CreateStore(value, addr);
        scopes.back().setVariable(id, addr);
    }
    else if (auto ConstnumberCtx = dynamic_cast<PascalSParser::ConstSignedNumberContext *>(context->constant()))
    {
        auto value = visitConstSignedNumber(ConstnumberCtx);
        auto addr = builder.CreateAlloca(value->getType(), nullptr);
        builder.CreateStore(value, addr);
        scopes.back().setVariable(id, addr);
    }
    else if (auto ConstsIdentifierCtx = dynamic_cast<PascalSParser::ConstSignIdentifierContext *>(context->constant()))
    {
        auto addr = visitConstSignIdentifier(ConstsIdentifierCtx);
        scopes.back().setVariable(id, addr);
    }
    else
    {
        throw NotImplementedException();
    }
}

llvm::Value *Visitor::visitConstIdentifier(PascalSParser::ConstIdentifierContext *context)
{
    auto s = visitIdentifier(context->identifier());
    if (getVariable(s))
    {
        // connst cannot be altered, so i assume them in the same addr
        auto addr = getVariable(s);
        return addr;
    }
    else
    {
        throw VariableNotFoundException(s);
    }
}

std::string Visitor::visitConstString(PascalSParser::ConstStringContext *context)
{
    return visitString(context->string());
}

llvm::Value *Visitor::visitConstSignedNumber(PascalSParser::ConstSignedNumberContext *context)
{
    auto sign = context->sign()->getText();
    int flag = 1;
    if (sign == "+")
    {
        flag = 1;
    }
    else if (sign == "-")
    {
        flag = -1;
    }
    else
    {
        throw NotImplementedException();
    }
    if (auto intContext = dynamic_cast<PascalSParser::UnsignedNumberIntegerContext *>(context->unsignedNumber()))
    {
        auto v = visitUnsignedNumberInteger(intContext) * flag;
        auto value = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvm_context), v);
        return value;
    }
    else if (auto realContext = dynamic_cast<PascalSParser::UnsignedNumberRealContext *>(context->unsignedNumber()))
    {
        auto v = visitUnsignedNumberReal(realContext) * flag;
        auto value = llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvm_context), v);
        return value;
    }
    else
    {
        throw NotImplementedException();
    }
}

llvm::Value *Visitor::visitConstUnsignedNumber(PascalSParser::ConstUnsignedNumberContext *context)
{
    if (auto intContext = dynamic_cast<PascalSParser::UnsignedNumberIntegerContext *>(context->unsignedNumber()))
    {
        auto v = visitUnsignedNumberInteger(intContext);
        auto value = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvm_context), v);
        // std::cout<<value->getType()->isIntegerTy()<<std::endl;
        return value;
    }
    else if (auto realContext = dynamic_cast<PascalSParser::UnsignedNumberRealContext *>(context->unsignedNumber()))
    {
        auto v = visitUnsignedNumberReal(realContext);
        auto value = llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvm_context), v);
        return value;
    }
    else
    {
        throw NotImplementedException();
    }
}

llvm::Value *Visitor::visitConstSignIdentifier(PascalSParser::ConstSignIdentifierContext *context)
{
    auto sign = context->sign()->getText();
    int flag = 1;
    if (sign == "+")
    {
        flag = 1;
    }
    else if (sign == "-")
    {
        flag = -1;
    }
    else
    {
        throw NotImplementedException();
    }
    auto s = visitIdentifier(context->identifier());
    if (getVariable(s))
    {
        auto addr = getVariable(s);
        auto value1 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvm_context), flag);
        auto value2 = builder.CreateLoad(addr);
        auto dest = builder.CreateMul(value1, value2);
        auto addr_dest = builder.CreateAlloca(dest->getType(), nullptr);
        builder.CreateStore(dest, addr_dest);
        return addr_dest;
    }
    else
    {
        throw VariableNotFoundException(s);
    }
}

std::string Visitor::visitString(PascalSParser::StringContext *context)
{
    return context->STRING_LITERAL()->getText();
}

int Visitor::visitUnsignedNumberInteger(PascalSParser::UnsignedNumberIntegerContext *context)
{
    return visitUnsignedInteger(context->unsignedInteger());
}

float Visitor::visitUnsignedNumberReal(PascalSParser::UnsignedNumberRealContext *context)
{
    return visitUnsignedReal(context->unsignedReal());
}

int Visitor::visitUnsignedInteger(PascalSParser::UnsignedIntegerContext *context)
{
    auto s = context->NUM_INT()->getText();
    return atoi(s.c_str());
}

float Visitor::visitUnsignedReal(PascalSParser::UnsignedRealContext *context)
{
    auto s = context->NUM_REAL()->getText();
    return atof(s.c_str());
}

void Visitor::visitConstantDefinitionPart(PascalSParser::ConstantDefinitionPartContext *context)
{
    for (const auto &constDefinitionContext : context->constantDefinition())
    {
        visitConstantDefinition(constDefinitionContext);
    }
}

void Visitor::visitVariableDeclarationPart(PascalSParser::VariableDeclarationPartContext *context)
{
    for (const auto &vDeclarationContext : context->variableDeclaration())
    {
        visitVariableDeclaration(vDeclarationContext);
    }
}

llvm::Type *Visitor::visitVariableDeclaration(PascalSParser::VariableDeclarationContext *context)
{
    auto idList = visitIdentifierList(context->identifierList());
    if (auto typeSimpleContext = dynamic_cast<PascalSParser::TypeSimpleTypeContext *>(context->type_()))
    {
        auto type = visitTypeSimpleType(typeSimpleContext);
        for (auto id : idList)
        {
            auto addr = builder.CreateAlloca(type, nullptr);
            builder.CreateStore(llvm::UndefValue::get(type), addr);
            scopes.back().setVariable(id, addr);
        }
        return type;
    }
    else if (auto typeStructureContext = dynamic_cast<PascalSParser::TypeStructuredTypeContext *>(context->type_()))
    {
        auto type = visitTypeStructuredType(typeStructureContext, idList);
        for (auto id : idList)
        {
            auto addr = builder.CreateAlloca(type, nullptr);
            scopes.back().setVariable(id, addr);
        }
        return type;
    }
    else
    {
        throw NotImplementedException();
    }
}

llvm::Type *Visitor::visitTypeSimpleType(PascalSParser::TypeSimpleTypeContext *context)
{
    return visitSimpleType(context->simpleType());
}

llvm::Type *Visitor::visitTypeStructuredType(PascalSParser::TypeStructuredTypeContext *context, std::vector<std::string> idList)
{
    if (auto arrayContext = dynamic_cast<PascalSParser::StructuredTypeArrayContext *>(context->structuredType()))
    {
        return visitStructuredTypeArray(arrayContext, idList);
    }
    else if (auto recordContext = dynamic_cast<PascalSParser::StructuredTypeRecordContext *>(context->structuredType()))
    {
        return visitStructuredTypeRecord(recordContext, idList);
    }
    else
    {
        throw NotImplementedException();
    }
}

llvm::Type *Visitor::visitStructuredTypeArray(PascalSParser::StructuredTypeArrayContext *context, std::vector<std::string> idList)
{
    if (auto array1Context = dynamic_cast<PascalSParser::ArrayType1Context *>(context->arrayType()))
    {
        return visitArrayType1(array1Context, idList);
    }
    else if (auto array2Context = dynamic_cast<PascalSParser::ArrayType2Context *>(context->arrayType()))
    {
        return visitArrayType2(array2Context, idList);
    }
    else
    {
        throw NotImplementedException();
    }
}

llvm::Type *Visitor::visitStructuredTypeRecord(PascalSParser::StructuredTypeRecordContext *context, std::vector<std::string> idList)
{
    return visitRecordType(context->recordType(), idList);
}

llvm::Type *Visitor::visitArrayType1(PascalSParser::ArrayType1Context *context, std::vector<std::string> idList)
{
    auto ranges = visitPeriods(context->periods());
    int space = 1;
    // calculate the space of array
    for (auto iter = ranges.begin(); iter != ranges.end(); iter++)
    {
        auto v1 = *iter;
        auto v2 = *(++iter);
        space *= (v2 - v1 + 1);
    }

    if (auto typeSimpleContext = dynamic_cast<PascalSParser::TypeSimpleTypeContext *>(context->type_()))
    {
        auto type = visitTypeSimpleType(typeSimpleContext);
        return llvm::ArrayType::get(type, space);
    }
    else if (auto typeStructureContext = dynamic_cast<PascalSParser::TypeStructuredTypeContext *>(context->type_()))
    {
        auto type = visitTypeStructuredType(typeStructureContext, idList);
        return llvm::ArrayType::get(type, space);
    }
    else
    {
        throw NotImplementedException();
    }
}

llvm::Type *Visitor::visitArrayType2(PascalSParser::ArrayType2Context *context, std::vector<std::string> idList)
{
    auto ranges = visitPeriods(context->periods());
    int space = 1;
    // calculate the space of array
    for (auto iter = ranges.begin(); iter != ranges.end(); iter++)
    {
        auto v1 = *iter;
        auto v2 = *(++iter);
        space *= (v2 - v1 + 1);
    }

    if (auto typeSimpleContext = dynamic_cast<PascalSParser::TypeSimpleTypeContext *>(context->type_()))
    {
        auto type = visitTypeSimpleType(typeSimpleContext);
        return llvm::ArrayType::get(type, space);
    }
    else if (auto typeStructureContext = dynamic_cast<PascalSParser::TypeStructuredTypeContext *>(context->type_()))
    {
        auto type = visitTypeStructuredType(typeStructureContext, idList);
        return llvm::ArrayType::get(type, space);
    }
    else
    {
        throw NotImplementedException();
    }
}

std::vector<int> Visitor::visitPeriods(PascalSParser::PeriodsContext *context)
{
    std::vector<int> ranges;
    for (const auto &periodContext : context->period())
    {
        auto range = visitPeriod(periodContext);
        for (int a : range)
        {
            ranges.push_back(a);
        }
    }
    return ranges;
}
// return 2 int
std::vector<int> Visitor::visitPeriod(PascalSParser::PeriodContext *context)
{
    auto vec = context->constant();
    if (vec.size() != 2)
    {
        throw NotImplementedException();
    }
    auto constContext = vec[0];
    llvm::Value *value1, *value2;
    if (auto constIdentifierContext = dynamic_cast<PascalSParser::ConstIdentifierContext *>(constContext))
    {
        auto addr = visitConstIdentifier(constIdentifierContext);
        auto value = builder.CreateLoad(addr);
        if (!value->getType()->isIntegerTy())
        {
            throw NotImplementedException();
        }
        else
        {
            value1 = value;
        }
    }
    else if (auto ConstunumberCtx = dynamic_cast<PascalSParser::ConstUnsignedNumberContext *>(constContext))
    {
        value1 = visitConstUnsignedNumber(ConstunumberCtx);
    }
    else if (auto ConstnumberCtx = dynamic_cast<PascalSParser::ConstSignedNumberContext *>(constContext))
    {
        value1 = visitConstSignedNumber(ConstnumberCtx);
    }
    else if (auto ConstsIdentifierCtx = dynamic_cast<PascalSParser::ConstSignIdentifierContext *>(constContext))
    {
        auto addr = visitConstSignIdentifier(ConstsIdentifierCtx);
        auto value = builder.CreateLoad(addr);
        if (!value->getType()->isIntegerTy())
        {
            throw NotImplementedException();
        }
        else
        {
            value1 = value;
        }
    }
    else
    {
        throw NotImplementedException();
    }

    constContext = vec[1];
    if (auto constIdentifierContext = dynamic_cast<PascalSParser::ConstIdentifierContext *>(constContext))
    {
        auto addr = visitConstIdentifier(constIdentifierContext);
        auto value = builder.CreateLoad(addr);
        if (!value->getType()->isIntegerTy())
        {
            throw NotImplementedException();
        }
        else
        {
            value2 = value;
        }
    }
    else if (auto ConstunumberCtx = dynamic_cast<PascalSParser::ConstUnsignedNumberContext *>(constContext))
    {
        value2 = visitConstUnsignedNumber(ConstunumberCtx);
    }
    else if (auto ConstnumberCtx = dynamic_cast<PascalSParser::ConstSignedNumberContext *>(constContext))
    {
        value2 = visitConstSignedNumber(ConstnumberCtx);
    }
    else if (auto ConstsIdentifierCtx = dynamic_cast<PascalSParser::ConstSignIdentifierContext *>(constContext))
    {
        auto addr = visitConstSignIdentifier(ConstsIdentifierCtx);
        auto value = builder.CreateLoad(addr);
        if (!value->getType()->isIntegerTy())
        {
            throw NotImplementedException();
        }
        else
        {
            value2 = value;
        }
    }
    else
    {
        throw NotImplementedException();
    }
    int constIntValue1, constIntValue2;
    if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(value1))
    {
        constIntValue1 = CI->getSExtValue();
    }
    else
    {
        throw NotImplementedException();
    }
    if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(value2))
    {
        constIntValue2 = CI->getSExtValue();
    }
    else
    {
        throw NotImplementedException();
    }
    std::vector<int> range;
    range.push_back(constIntValue1);
    range.push_back(constIntValue2);
    return range;
}

llvm::Type *Visitor::visitRecordType(PascalSParser::RecordTypeContext *context, std::vector<std::string> idList)
{
    return visitRecordField(context->recordField(), idList);
}

llvm::Type *Visitor::visitRecordField(PascalSParser::RecordFieldContext *context, std::vector<std::string> idList)
{
    std::vector<llvm::Type *> elements;
    for (const auto &varDeclareCtx : context->variableDeclaration())
    {
        auto e = visitVariableDeclaration(varDeclareCtx);
        elements.push_back(e);
    }
    for (auto id : idList)
    {
        llvm::StructType *testStruct = llvm::StructType::create(*llvm_context, id);
        testStruct->setBody(elements);
        return testStruct;
    }
    return elements[0];
}

void Visitor::visitProcedureAndFunctionDeclarationPart(PascalSParser::ProcedureAndFunctionDeclarationPartContext *context)
{
    const auto &ProOrFuncDec = context->procedureOrFunctionDeclaration();
    if (auto procedureDeclarationContext = dynamic_cast<PascalSParser::ProOrFuncDecProContext *>(ProOrFuncDec))
        visitProOrFuncDecPro(procedureDeclarationContext);
    else if (auto functionDeclarationContext = dynamic_cast<PascalSParser::ProOrFuncDecFuncContext *>(ProOrFuncDec))
        visitProOrFuncDecFunc(functionDeclarationContext);
    else
        throw NotImplementedException();
}

void Visitor::visitProOrFuncDecPro(PascalSParser::ProOrFuncDecProContext *context)
{
    visitProcedureDeclaration(context->procedureDeclaration());
}

void Visitor::visitProOrFuncDecFunc(PascalSParser::ProOrFuncDecFuncContext *context)
{
    visitFunctionDeclaration(context->functionDeclaration());
}

void Visitor::visitProcedureDeclaration(PascalSParser::ProcedureDeclarationContext *context)
{

    auto identifier = visitIdentifier(context->identifier());

    //返回值类类型
    llvm::SmallVector<llvm::Type *> ParaTypes;

    //参数类型
    visitFormalParameterList(context->formalParameterList(), ParaTypes);

    auto functionType = llvm::FunctionType::get(builder.getVoidTy(), ParaTypes, false);

    auto function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, llvm::Twine(identifier), module.get());

    scopes.push_back(Scope());
    scopes.back().setVariable(identifier, function);

    //存储参数
    int n = 0;
    for (auto argsItr = function->arg_begin(); argsItr != function->arg_end(); argsItr++)
    {
        llvm::Value *arg = argsItr;
        arg->setName(FormalParaIdList[n]);
        scopes.back().setVariable(FormalParaIdList[n++], arg);
    }

    visitBlock(context->block(), function);

    builder.CreateRetVoid();
}

void Visitor::visitFunctionDeclaration(PascalSParser::FunctionDeclarationContext *context)
{
    auto identifier = visitIdentifier(context->identifier());

    //返回值类类型
    auto simpleType = visitSimpleType(context->simpleType(), false);

    //参数类型
    llvm::SmallVector<llvm::Type *> ParaTypes;
    visitFormalParameterList(context->formalParameterList(), ParaTypes);

    auto functionType = llvm::FunctionType::get(simpleType, ParaTypes, false);

    auto function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, llvm::Twine(identifier), module.get());

    scopes.push_back(Scope());
    scopes.back().setVariable(identifier, function);

    //存储参数
    int n = 0;
    for (auto argsItr = function->arg_begin(); argsItr != function->arg_end(); argsItr++)
    {
        llvm::Value *arg = argsItr;
        arg->setName(FormalParaIdList[n]);
        scopes.back().setVariable(FormalParaIdList[n++], arg);
    }

    llvm::Value *ret = visitBlock(context->block(), function);

    if (ret->getType() != simpleType)
        throw NotImplementedException();

    builder.CreateRet(ret);
}

void Visitor::visitFormalParameterList(PascalSParser::FormalParameterListContext *context, llvm::SmallVector<llvm::Type *> &ParaTypes)
{
    FormalParaIdList.clear();

    for (const auto &formalParameterSectionContext : context->formalParameterSection())
    {
        if (auto parameterGroupContext = dynamic_cast<PascalSParser::FormalParaSecGroupContext *>(formalParameterSectionContext))
        {
            visitFormalParaSecGroup(parameterGroupContext, ParaTypes);
        }
        else if (auto VAR_parameterGroupContext = dynamic_cast<PascalSParser::FormalParaSecVarGroupContext *>(formalParameterSectionContext))
        {
            visitFormalParaSecVarGroup(VAR_parameterGroupContext, ParaTypes);
        }
        else
            throw NotImplementedException();
    }
}

void Visitor::visitFormalParaSecGroup(PascalSParser::FormalParaSecGroupContext *context, llvm::SmallVector<llvm::Type *> &ParaTypes)
{
    visitParameterGroup(context->parameterGroup(), ParaTypes, false);
}

void Visitor::visitFormalParaSecVarGroup(PascalSParser::FormalParaSecVarGroupContext *context, llvm::SmallVector<llvm::Type *> &ParaTypes)
{
    visitParameterGroup(context->parameterGroup(), ParaTypes, true);
}

void Visitor::visitParameterGroup(PascalSParser::ParameterGroupContext *context, llvm::SmallVector<llvm::Type *> &ParaTypes, bool isVar)
{
    auto simpleType = visitSimpleType(context->simpleType(), isVar);
    auto IdList = visitIdentifierList(context->identifierList());
    for (int i = 0; i < IdList.size(); i++)
    {
        ParaTypes.push_back(simpleType);       //参数类型
        FormalParaIdList.push_back(IdList[i]); //参数名称
    }
}

std::vector<std::string> Visitor::visitIdentifierList(PascalSParser::IdentifierListContext *context)
{
    std::vector<std::string> idents;
    for (const auto &identfierContext : context->identifier())
    {
        idents.push_back(visitIdentifier(identfierContext));
    }
    return idents;
}

llvm::Type *Visitor::visitSimpleType(PascalSParser::SimpleTypeContext *context, bool isVar)
{
    if (auto charContext = context->CHAR())
    {
        if (isVar)
            return builder.getInt8PtrTy();
        else
            return builder.getInt8Ty();
    }
    else if (auto charContext = context->INTEGER())
    {
        if (isVar)
            return llvm::Type::getInt32PtrTy(*llvm_context);
        else
            return builder.getInt32Ty();
    }
    else if (auto charContext = context->BOOLEAN())
    {
        if (isVar)
            return llvm::Type::getInt1PtrTy(*llvm_context);
        else
            return builder.getInt1Ty();
    }
    else if (auto charContext = context->REAL())
    {
        if (isVar)
            return llvm::Type::getFloatPtrTy(*llvm_context);
        else
            return builder.getFloatTy();
    }
    else
        throw NotImplementedException();
}
