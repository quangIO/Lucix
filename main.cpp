#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Support/TargetSelect.h"
#include "KaleidoscopeJIT.h"
#include "gen/LucixLexer.h"
#include "gen/LucixBaseVisitor.h"

using namespace llvm;

class CodeGen : LucixBaseVisitor {
    LLVMContext llvmContext;
    IRBuilder<> builder;
    std::vector<std::map<std::string, AllocaInst *>> symbolTables;
    std::map<std::string, Function *> functionTable;
    std::unique_ptr<legacy::FunctionPassManager> fpm;
    std::map<std::string, Type *> typeMap;
    std::unique_ptr<orc::KaleidoscopeJIT> jit;

    void initModuleAndFunctionPassManager() {
        module = std::make_unique<Module>("simple jit", llvmContext);
        module->setDataLayout(jit->getDataLayout());
        fpm = std::make_unique<legacy::FunctionPassManager>(module.get());
        fpm->add(createInstructionCombiningPass());
        fpm->add(createReassociatePass());
        fpm->add(createGVNPass());
        fpm->add(createCFGSimplificationPass());
        fpm->add(createPromoteMemoryToRegisterPass());
        fpm->doInitialization();
    }

public:
    std::unique_ptr<Module> module;

    CodeGen() : builder(IRBuilder<>(llvmContext)) {
        typeMap = {{"i32", Type::getInt32Ty(llvmContext)},
                   {"",    Type::getVoidTy(llvmContext)}};
        orc::KaleidoscopeJIT::Create().get();
        jit = std::move(orc::KaleidoscopeJIT::Create().get());
        initModuleAndFunctionPassManager();
    }

    antlrcpp::Any visitApplication(LucixParser::ApplicationContext *ctx) override {
        LucixBaseVisitor::visitApplication(ctx);
        return antlrcpp::Any();
    }

private:
    inline void createLocalVariable(std::string const &name, std::string const &type, Value *value) {
        createLocalVariable(name, typeMap[type], value);
    }

    inline void createLocalVariable(std::string const &name, Type *type, Value *value) {
        auto ptr = symbolTables.back()[name] = builder.CreateAlloca(type, nullptr, name);
        if (value) builder.CreateStore(value, ptr);
    }

    inline Value *resolveSymbol(std::string const &name) {
        for (int i = static_cast<int>(symbolTables.size()) - 1; i >= 0; --i)
            if (symbolTables[i][name])
                return symbolTables[i][name];
        llvm_unreachable(("cannot resolve symbol " + name).c_str());
    }

    antlrcpp::Any visitVariableDeclaration(LucixParser::VariableDeclarationContext *context) override {
        auto varName = context->ID()->getText();
        auto varType = context->type()->getText();
        Value *value = context->expression() ? visit(context->expression()).as<Value *>() : nullptr;
        createLocalVariable(varName, varType, value);
        return antlrcpp::Any();
    }

    antlrcpp::Any visitFunctionDeclaration(LucixParser::FunctionDeclarationContext *context) override {
        auto parameters = (context->functionParameters()) ? context->functionParameters()->parameter()
                                                          : std::vector<LucixParser::ParameterContext *>{};
        std::vector<Type *> types;
        types.reserve(parameters.size());
        for (auto parameter: parameters)
            types.push_back(typeMap[parameter->type()->getText()]);

        auto prototype = FunctionType::get(typeMap[context->type()->getText()], types, false);

        auto function = Function::Create(prototype, Function::ExternalLinkage, context->ID()->getText(), module.get());
        auto block = BasicBlock::Create(llvmContext, "entry", function);
        builder.SetInsertPoint(block);

        symbolTables.emplace_back();
        int i = 0;
        for (auto &arg: function->args()) {
            createLocalVariable(parameters[i]->ID()->getText(), types[i], &arg);
            i++;
        }
        visitChildren(context);
        symbolTables.pop_back();
        functionTable[function->getName()] = function;
        module->print(errs(), nullptr);

        auto t = jit->addModule(std::move(module));
        auto symbol = jit->lookup("test");
        assert(symbol);

        auto FP = Expected<JITTargetAddress>(symbol->getAddress());
        auto x = (int (*)()) (intptr_t) FP.get();
        std::cout << x() << std::endl;

        return antlrcpp::Any();
    }

    antlrcpp::Any visitIdExpr(LucixParser::IdExprContext *context) override {
        return static_cast<Value *>(builder.CreateLoad(resolveSymbol(context->ID()->getText()),
                                                       context->ID()->getText().c_str()));
    }

    antlrcpp::Any visitIntExpr(LucixParser::IntExprContext *context) override {
        auto val = stoi(context->INT()->getText());
        return static_cast<Value *>(ConstantInt::get(llvmContext, APInt(32, static_cast<uint64_t>(val))));
    }

    antlrcpp::Any visitAddSubExpr(LucixParser::AddSubExprContext *context) override {
        auto op = context->op->getText();
        auto l = visit(context->expression(0));
        auto r = visit(context->expression(1));
        if (op == "+")
            return builder.CreateAdd(l, r);
        return builder.CreateSub(l, r);
    }

    antlrcpp::Any visitMinusExpr(LucixParser::MinusExprContext *context) override {
        return builder.CreateNeg(visit(context->expression()));
    }

    antlrcpp::Any visitLeGeExpr(LucixParser::LeGeExprContext *context) override {
        auto op = context->op->getText();
        Value *l = visit(context->expression(0));
        Value *r = visit(context->expression(1));
        return builder.CreateIntCast(
                builder.CreateICmp((op == "<") ? CmpInst::Predicate::ICMP_SLT : CmpInst::Predicate::ICMP_SGT, l, r),
                Type::getInt32Ty(llvmContext), true);
    }

    antlrcpp::Any visitIfStatement(LucixParser::IfStatementContext *context) override {
        auto exp = visit(context->expression());
        auto condition = builder.CreateICmpNE(exp, ConstantInt::get(llvmContext, APInt(32, 0)), "if_cond");
        auto parentFunction = builder.GetInsertBlock()->getParent();

        std::vector<BasicBlock *> blocks{BasicBlock::Create(llvmContext, "then", parentFunction),
                                         BasicBlock::Create(llvmContext, "else", parentFunction)};
        auto doneBlock = BasicBlock::Create(llvmContext, "if_cont", parentFunction);
        builder.CreateCondBr(condition, blocks[0], blocks[1]);
        for (size_t i = 0; i < 2; ++i) {
            builder.SetInsertPoint(blocks[i]);
            if (context->block().size() > i)
                visitBlock(context->block(i));
            builder.CreateBr(doneBlock);
        }
        builder.SetInsertPoint(doneBlock);
        return antlrcpp::Any();
    }

    antlrcpp::Any visitAssign(LucixParser::AssignContext *context) override {
        auto varName = context->ID()->getText();
        auto val = visit(context->expression());
        return static_cast<Value *>(builder.CreateStore(val, resolveSymbol(varName)));
    }

    antlrcpp::Any visitJumpExpression(LucixParser::JumpExpressionContext *context) override {
        if (context->expression())
            return builder.CreateRet(visit(context->expression()));
        return builder.CreateRetVoid();
    }

    antlrcpp::Any visitBlock(LucixParser::BlockContext *context) override {
        symbolTables.emplace_back();
        visitChildren(context);
        symbolTables.pop_back();
        return antlrcpp::Any();
    }

    antlrcpp::Any visitCallExpr(LucixParser::CallExprContext *context) override {
        std::vector<Value *> args;
        auto expressionList = context->expressionList();
        if (expressionList)
            for (auto expression: expressionList->expression())
                args.emplace_back(visit(expression));
        return static_cast<Value *>(builder.CreateCall(functionTable[context->ID()->getText()], args));
    }
};

int main(int argc, char *argv[]) {
    auto file = std::ifstream(argv[1]);
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    antlr4::ANTLRInputStream inputStream(file);
    LucixLexer lexer(&inputStream);
    antlr4::CommonTokenStream tokens(&lexer);
    LucixParser parser(&tokens);
    auto tree = parser.application();
    auto codegen = CodeGen();
    codegen.visitApplication(tree);
    // codegen.module->print(errs(), nullptr);
}
