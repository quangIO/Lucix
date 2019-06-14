/*
 * Based on https://llvm.org/docs/tutorial/BuildingAJIT1.html
 */
#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include <memory>

namespace llvm::orc {
    class KaleidoscopeJIT {
    private:
        ExecutionSession ES;
        RTDyldObjectLinkingLayer ObjectLayer;
        IRCompileLayer CompileLayer;

        DataLayout DL;
        MangleAndInterner Mangle;
        ThreadSafeContext Ctx;

        KaleidoscopeJIT(JITTargetMachineBuilder JTMB, DataLayout DL)
                : ObjectLayer(ES,
                              []() { return llvm::make_unique<SectionMemoryManager>(); }),
                  CompileLayer(ES, ObjectLayer, ConcurrentIRCompiler(std::move(JTMB))),
                  DL(DL), Mangle(ES, this->DL),
                  Ctx(llvm::make_unique<LLVMContext>()) {
            ES.getMainJITDylib().setGenerator(
                    cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL)));
        }

    public:

        static Expected<std::unique_ptr<KaleidoscopeJIT>> Create() {
            auto JTMB = JITTargetMachineBuilder::detectHost();

            if (!JTMB)
                return JTMB.takeError();

            auto DL = JTMB->getDefaultDataLayoutForTarget();
            if (!DL)
                return DL.takeError();

            return llvm::make_unique<KaleidoscopeJIT>(std::move(*JTMB), std::move(*DL));
        }

        const DataLayout &getDataLayout() const { return DL; }

        LLVMContext &getContext() { return *Ctx.getContext(); }

        Error addModule(std::unique_ptr<Module> M) {
            return CompileLayer.add(ES.getMainJITDylib(),
                                    ThreadSafeModule(std::move(M), Ctx));
        }

        Expected<JITEvaluatedSymbol> lookup(StringRef Name) {
            return ES.lookup({&ES.getMainJITDylib()}, Mangle(Name.str()));
        }
    };

} // end namespace llvm