SETUPSEG=0x9020
entry start
start:
    mov ax,#SETUPSEG
    mov es,ax

!print "Now we are in SETUP ..."
    mov ah,#0x03
    xor bh,bh
    int 0x10
    
    mov cx,#29
    mov bx,#0x0007
    mov bp,#msg
    mov ax,#0x1301
    int 0x10

msg:
    .byte 13,10
    .ascii "Now we are in SETUP ..."
    .byte 13,10,13,10
