Ducq = require("LuaDucq")

command = "sub"
route = "*"
payload = "last"

NORMAL = "\27[39m"
GREY = "\27[90m"
RED = "\27[91m"
GREEN = "\27[92m"

print("Initialization...")

onMessage = function(ducq, msg)
	print(NORMAL .. msg.payload)
	return 0
end

onProtocol = function(ducq, msg)
	print(GREY .. msg.payload .. NORMAL)
	return 0
end

onError = function(ducq, msg)
	print(RED .. msg.payload .. NORMAL)
	return -1
end

log = function(level, str)
	print(GREEN .. "[" .. level .. "]" .. str .. NORMAL)
end

finalize = function()
	print("goodbye :)")
end
