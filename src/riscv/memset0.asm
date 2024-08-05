R"EOF(
memset0:
        andi    a5,a1,7
        lui     a4,%hi(.L4)
        addi    a4,a4,%lo(.L4)
        slli    a5,a5,2
        add     a5,a5,a4
        lw      a4,0(a5)
        andi    a1,a1,-8
        addi    a5,a1,8
        jr      a4
.L4:
        .word   .L2
        .word   .L10
        .word   .L9
        .word   .L8
        .word   .L7
        .word   .L6
        .word   .L5
        .word   .L3
.L18:
        slli    a5,a1,2
        add     a5,a0,a5
        sw      zero,-4(a5)
        addi    a1,a1,-8
        sw      zero,-8(a5)
.L11:
        sw      zero,-12(a5)
.L12:
        sw      zero,-16(a5)
.L13:
        sw      zero,-20(a5)
.L14:
        sw      zero,-24(a5)
.L15:
        sw      zero,-28(a5)
.L16:
        sw      zero,-32(a5)
.L2:
        bne     a1,zero,.L18
        ret
.L10:
        slli    a5,a5,2
        add     a5,a0,a5
        sw      zero,-32(a5)
        j       .L2
.L9:
        slli    a5,a5,2
        add     a5,a0,a5
        sw      zero,-28(a5)
        j       .L16
.L8:
        slli    a5,a5,2
        add     a5,a0,a5
        sw      zero,-24(a5)
        j       .L15
.L7:
        slli    a5,a5,2
        add     a5,a0,a5
        sw      zero,-20(a5)
        j       .L14
.L5:
        slli    a5,a5,2
        add     a5,a0,a5
        sw      zero,-12(a5)
        j       .L12
.L6:
        slli    a5,a5,2
        add     a5,a0,a5
        sw      zero,-16(a5)
        j       .L13
.L3:
        slli    a5,a5,2
        add     a5,a0,a5
        sw      zero,-8(a5)
        j       .L11
)EOF"