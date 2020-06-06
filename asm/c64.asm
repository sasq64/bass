%{
    function convertBit(b)
        if b == 0b00 then return 0b11 end
        if b == 0b11 then return 0b00 end
        return b
    end

    function convertByte(b)
        newB = 255 - b

        b0 = (newB >> 6) & 3
        b1 = (newB >> 4) & 3
        b2 = (newB >> 2) & 3
        b3 = newB & 3

        b0 = convertBit(b0)
        b1 = convertBit(b1)
        b2 = convertBit(b2)
        b3 = convertBit(b3)

        return b0 << 6 | b1 << 4 | b2 << 2 | b3
    end

    function convByte(b)

    return ((b & 0xaa) >> 1) | ((b & 0x55)<<1)


    end

    function convertAll(a)
        res = {}
        for i,v in ipairs(a) do
            table.insert(res, convertByte(v))
        end
        return res
    end
}%

    !section "main", 0x801
    !byte $0b, $08,$01,$00,$9e,str(start)," BY SASQ", $00,$00,$00

    !section "code", 0x901
start:
.loop
    lda $d012
    sta $d020
    sta $d021
    rts

charsetFile = "jumps.asm"

!define convert(v) { 

    !section "car0", *
charset0:
    !block convertAll(load(charsetFile))
    !section "car1", *
carset1:
    !fill load(charsetFile) { ((v & 0xaa) >> 1) | ((v & 0x55)<<1) } 

    !assert compare(section.car0.data, section.car1.data)

