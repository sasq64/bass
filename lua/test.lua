

local function test_me()
    local a = read_reg_6502(0)
    local lo = read_mem_6502(0x9f20)
    local mid = read_mem_6502(0x9f21)
    print("Hey there ", a, lo, mid)
end

set_break_fn(1, test_me)


