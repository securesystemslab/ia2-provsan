; ModuleID = 'basic.c'
source_filename = "basic.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [12 x i8] c"ptr != NULL\00", align 1
@.str.1 = private unnamed_addr constant [8 x i8] c"basic.c\00", align 1
@__PRETTY_FUNCTION__.check = private unnamed_addr constant [18 x i8] c"void check(int *)\00", align 1
@.str.2 = private unnamed_addr constant [9 x i8] c"num = %d\00", align 1

; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local i8* @trusted_malloc(i64 %0, i64 %1) #0 !dbg !19 {
  %3 = alloca i64, align 8
  %4 = alloca i64, align 8
  store i64 %0, i64* %3, align 8
  call void @llvm.dbg.declare(metadata i64* %3, metadata !25, metadata !DIExpression()), !dbg !26
  store i64 %1, i64* %4, align 8
  call void @llvm.dbg.declare(metadata i64* %4, metadata !27, metadata !DIExpression()), !dbg !28
  %5 = load i64, i64* %3, align 8, !dbg !29
  %6 = call noalias i8* @malloc(i64 %5) #5, !dbg !30
  ret i8* %6, !dbg !31
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) #2

; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local void @check(i32* %0) #0 !dbg !32 {
  %2 = alloca i32*, align 8
  store i32* %0, i32** %2, align 8
  call void @llvm.dbg.declare(metadata i32** %2, metadata !35, metadata !DIExpression()), !dbg !36
  %3 = load i32*, i32** %2, align 8, !dbg !37
  %4 = icmp ne i32* %3, null, !dbg !37
  br i1 %4, label %5, label %6, !dbg !40

5:                                                ; preds = %1
  br label %7, !dbg !40

6:                                                ; preds = %1
  call void @__assert_fail(i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str, i64 0, i64 0), i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str.1, i64 0, i64 0), i32 12, i8* getelementptr inbounds ([18 x i8], [18 x i8]* @__PRETTY_FUNCTION__.check, i64 0, i64 0)) #6, !dbg !37
  unreachable, !dbg !37

7:                                                ; preds = %5
  %8 = load i32*, i32** %2, align 8, !dbg !41
  store i32 10, i32* %8, align 4, !dbg !42
  ret void, !dbg !43
}

; Function Attrs: noreturn nounwind
declare void @__assert_fail(i8*, i8*, i32, i8*) #3

; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local i32 @main() #0 !dbg !44 {
  %1 = alloca i32, align 4
  %2 = alloca i32*, align 8
  store i32 0, i32* %1, align 4
  call void @llvm.dbg.declare(metadata i32** %2, metadata !47, metadata !DIExpression()), !dbg !48
  %3 = call i8* @trusted_malloc(i64 4, i64 4), !dbg !49
  %4 = bitcast i8* %3 to i32*, !dbg !50
  store i32* %4, i32** %2, align 8, !dbg !48
  %5 = load i32*, i32** %2, align 8, !dbg !51
  call void @check(i32* %5), !dbg !52
  %6 = load i32*, i32** %2, align 8, !dbg !53
  %7 = load i32, i32* %6, align 4, !dbg !54
  %8 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.2, i64 0, i64 0), i32 %7), !dbg !55
  ret i32 0, !dbg !56
}

declare i32 @printf(i8*, ...) #4

attributes #0 = { noinline nounwind sspstrong uwtable "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #2 = { nounwind "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { noreturn nounwind "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind }
attributes #6 = { noreturn nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!13, !14, !15, !16, !17}
!llvm.ident = !{!18}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 12.0.1", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !3, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "basic.c", directory: "/home/paul/workspace/ia2/ia2-phase2/provsan/Passes/test")
!2 = !{}
!3 = !{!4, !10, !11}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!5 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint8_t", file: !6, line: 24, baseType: !7)
!6 = !DIFile(filename: "/usr/include/bits/stdint-uintn.h", directory: "")
!7 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint8_t", file: !8, line: 38, baseType: !9)
!8 = !DIFile(filename: "/usr/include/bits/types.h", directory: "")
!9 = !DIBasicType(name: "unsigned char", size: 8, encoding: DW_ATE_unsigned_char)
!10 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !{i32 7, !"Dwarf Version", i32 4}
!14 = !{i32 2, !"Debug Info Version", i32 3}
!15 = !{i32 1, !"wchar_size", i32 4}
!16 = !{i32 7, !"PIC Level", i32 2}
!17 = !{i32 7, !"PIE Level", i32 2}
!18 = !{!"clang version 12.0.1"}
!19 = distinct !DISubprogram(name: "trusted_malloc", scope: !1, file: !1, line: 7, type: !20, scopeLine: 7, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!20 = !DISubroutineType(types: !21)
!21 = !{!4, !22, !22}
!22 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !23, line: 46, baseType: !24)
!23 = !DIFile(filename: "/usr/lib/clang/12.0.1/include/stddef.h", directory: "")
!24 = !DIBasicType(name: "long unsigned int", size: 64, encoding: DW_ATE_unsigned)
!25 = !DILocalVariable(name: "size", arg: 1, scope: !19, file: !1, line: 7, type: !22)
!26 = !DILocation(line: 7, column: 32, scope: !19)
!27 = !DILocalVariable(name: "align", arg: 2, scope: !19, file: !1, line: 7, type: !22)
!28 = !DILocation(line: 7, column: 45, scope: !19)
!29 = !DILocation(line: 8, column: 28, scope: !19)
!30 = !DILocation(line: 8, column: 21, scope: !19)
!31 = !DILocation(line: 8, column: 3, scope: !19)
!32 = distinct !DISubprogram(name: "check", scope: !1, file: !1, line: 11, type: !33, scopeLine: 11, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!33 = !DISubroutineType(types: !34)
!34 = !{null, !11}
!35 = !DILocalVariable(name: "ptr", arg: 1, scope: !32, file: !1, line: 11, type: !11)
!36 = !DILocation(line: 11, column: 17, scope: !32)
!37 = !DILocation(line: 12, column: 3, scope: !38)
!38 = distinct !DILexicalBlock(scope: !39, file: !1, line: 12, column: 3)
!39 = distinct !DILexicalBlock(scope: !32, file: !1, line: 12, column: 3)
!40 = !DILocation(line: 12, column: 3, scope: !39)
!41 = !DILocation(line: 13, column: 4, scope: !32)
!42 = !DILocation(line: 13, column: 8, scope: !32)
!43 = !DILocation(line: 14, column: 1, scope: !32)
!44 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 16, type: !45, scopeLine: 16, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!45 = !DISubroutineType(types: !46)
!46 = !{!12}
!47 = !DILocalVariable(name: "num_ptr", scope: !44, file: !1, line: 17, type: !11)
!48 = !DILocation(line: 17, column: 8, scope: !44)
!49 = !DILocation(line: 17, column: 25, scope: !44)
!50 = !DILocation(line: 17, column: 18, scope: !44)
!51 = !DILocation(line: 18, column: 9, scope: !44)
!52 = !DILocation(line: 18, column: 3, scope: !44)
!53 = !DILocation(line: 19, column: 23, scope: !44)
!54 = !DILocation(line: 19, column: 22, scope: !44)
!55 = !DILocation(line: 19, column: 3, scope: !44)
!56 = !DILocation(line: 21, column: 3, scope: !44)
