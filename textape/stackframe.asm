; ============================================================================
; Stack frame procedure template (AMD64)
; ============================================================================

proc_name proc
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32                 ; Shadow space + locals
    
    ; Your code here
    
    leave
    ret
proc_name endp

