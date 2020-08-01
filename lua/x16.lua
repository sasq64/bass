
-- Vera state
vram = { 0 }
vregs = { 0 }
vsel = 1
vadr = { 0, 0 }
vinc = { 0, 0 }

increments = {
    0, 0, 1, -1, 2, -2, 4, -4, 8, -8, 16, -16, 32, -32,
    64, -64, 128, -128, 256, -256, 512, -512,
    40, -40, 80, -80, 160, -160, 320, -320, 640, -640,
}

map_bank_read(0x9f, 1, function(adr)
    local offset = adr & 0xff
    -- print("Read", offset)
    if offset >= 0x20 and offset < 0x40 then
        if offset == 0x20 then
            return vadr[vsel] & 0xff
        elseif offset == 0x21 then
            return (vadr[vsel] >> 8) & 0xff
        elseif offset == 0x22 then
            return (vadr[vsel] >> 16) | (vinc[vsel]<<3)
        elseif offset == 0x23 then
            res = vram[vadr[1]+1]
            vadr[1] = vadr[1] + increments[vinc[1]+1]
            return res
        elseif offset == 0x24 then
            res = vram[vadr[2]+1]
            vadr[2] = vadr[2] + increments[vinc[2]+1]
            return res
        end
        res =  vregs[offset-0x20+1]
        return res
    else
        return mem_read(adr)
    end
end)

map_bank_write(0x9f, 1, function(adr, val)
    offset = adr & 0xff
    if offset >= 0x20 and offset < 0x40 then
        if offset == 0x20 then
            vadr[vsel] = (vadr[vsel] & 0x1ff00) | val
        elseif offset == 0x21 then
            vadr[vsel] = (vadr[vsel] & 0x100ff) | (val<<8)
        elseif offset == 0x22 then
            vadr[vsel] = (vadr[vsel] & 0xffff) | ((val&1)<<16)
            vinc[vsel] = val>>3
        elseif offset == 0x23 then
            -- print(string.format("Vram write %x to %x", val, vadr[1]))
            vram[vadr[1]+1] = val
            vadr[1] = vadr[1] + increments[vinc[1]+1]
        elseif offset == 0x24 then
            -- print(string.format("Vram write %x to %x", val, vadr[2]))
            vram[vadr[2]+1] = val
            vadr[2] = vadr[2] + increments[vinc[2]+1]
        end
        -- print(string.format("Write %x to %x", val, offset))
        vregs[offset-0x20+1] = val
    else
        mem_write(adr, val)
    end
end)

function get_vram()
    return vram
end
