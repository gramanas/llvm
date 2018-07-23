;; RUN: opt < %s -debugify -instcombine -S | FileCheck %s

;; This is a sample test file for doing the testing while
;; the patch is WIP. Once it's finished it will be incorporated
;; in the debuginfo-variables.ll file.

define i32 @icmp_div(i1 %condA, i1 %condB) {
entry:
  br i1 %condA, label %then, label %exit

then:
  br label %exit

exit:
;; CHECK-LABEL: exit:
;; CHECK:         [[phi:%.*]] = phi i32 {{.*}}
;; CHECK-NEXT:    call void @llvm.dbg.value(metadata i32 [[phi]], {{.*}}, metadata !DIExpression())
  %phi = phi i1 [ false, %entry ], [ %condB, %then ]
  %zext = zext i1 %phi to i32
  call void asm sideeffect "", "r"(i32 %zext)
  ret i32 %zext
}
