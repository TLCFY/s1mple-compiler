; ModuleID = './test/HelloWorld.bc'
source_filename = "./test/HelloWorld.pas"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

define void @main() {
  %1 = alloca i32, align 4
  store i32 undef, i32* %1, align 4
  ret void
}
