### 0x00
1. 编辑`bootsect.s`文件，搜索`msg1`，并修改其中的`.ascii "Loading System ..."`，假设改为`.ascii "Jupiter23 says Hello to this OS ..."`（`ascii`长度：35）；
2. 定位`msg1`首次出现的地方（`mov bp,#msg1`），将其上方的`cx`值修改为`35+2+4=41`，表示打印的字符个数；

### 0x01
1. 编写一个`setup.s`程序，例如：  
```asm
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

```