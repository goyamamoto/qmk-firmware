# Generic register file: remembers whatever is written and reads it back.
# Enough for init code that writes an enable bit and polls for it.
if request.IsInit:
    regs = {}
elif request.IsRead:
    request.Value = regs.get(request.Offset, 0)
    self.NoisyLog("read  0x%03x -> 0x%08x" % (request.Offset, request.Value))
elif request.IsWrite:
    regs[request.Offset] = request.Value
    self.NoisyLog("write 0x%03x <- 0x%08x" % (request.Offset, request.Value))
