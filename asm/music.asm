!macro move(val16, adr) {
    lda #(val16&0xff)
    sta adr
    lda #(val16>>8)
    sta adr+1
}

%{

function be_word(data, offs)
    return data[offs+2] | (data[offs+1]<<8)
end

function le_word(data, offs)
    return data[offs+1] | (data[offs+2]<<8)
end

function read_string(data, offs, len)
    s = ''
    for i=offs+1,len+offs+1 do
        if data[i] == 0 then
            break
        end
        s = s .. string.char(data[i])
    end
    s = string.gsub(s, "%s+$", "")
    return s
end

function sid_parse(data)

    res = {
        start = be_word(data, 6),
        load =  be_word(data, 8),
        init =  be_word(data, 10),
        play =  be_word(data, 12),
        title = read_string(data, 0x16, 32),
        composer = read_string(data, 0x36, 32)
    }

    if res.load == 0 then
        print(res.start)
        res.load = le_word(data, res.start)
        res.start = res.start + 2
    end

    res.data = {}
    for i=res.start,#data do
        table.insert(res.data, data[i+1])
    end

    return res

end



}%


!macro memcpy(dst, src, len) {
    .HL = len>>8
    move(dst, block_copy.a1+1)
    move(src, block_copy.a0+1)
    !rept 4 { ldy  #.HL }
    jsr block_copy
    move(dst+(len - (len>>8)), mem_copy.a1+1)
    move(src+(len - (len>>8)), mem_copy.a0+1)
    ldx  #(len - (len>>8))
    jsr mem_copy
}


    data = load("../data/test.sid")
    sid = sid_parse(data)

    !section "main", $801

    !byte $0b, $08,$01,$00,$9e,$32,$30,$36,$31,$00,$00,$00

  start:

    sei
    ;memcpy(sid.load, music, 8320)
    lda #0
    jsr sid.init
.loop
    lda $d012
    cmp #130
    bne .loop
    lda #2
    sta $d020
    jsr sid.play
    lda #0
    sta $d020
    jmp .loop


; y : blocks
block_copy:
    ; copy 256 bytes
    ldx #0
.loop
.a0 lda $1234,x
.a1 sta $1234,x
    inx
.l0 bne .loop
    dey
    beq .done
    inc .a0+1
    inc .a1+1
    jmp .loop
.done
    rts

; x : bytes
mem_copy:
.loop
    dex
.a0 lda $1234,x
.a1 sta $1234,x
    cpx #0
    bne .loop
    rts

    !section "music", sid.load
music:
    !block sid.data

;    !section "main"
;!test copy
;    memcpy(sid.load, music, 8320)
;!endt


