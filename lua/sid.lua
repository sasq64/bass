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
        res.load = le_word(data, res.start)
        res.start = res.start + 2
    end

    res.data = {}
    for i=res.start,#data do
        table.insert(res.data, data[i+1])
    end

    return res

end


