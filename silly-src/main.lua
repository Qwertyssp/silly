local server = require("server")
local raw = require("rawpacket")

print("hello lua")

local packet = raw.create()

server.recv(function (msg)
        local fd, type = raw.push(packet, msg)
        print("lua.server.push", fd, type)

        local sid, data = raw.pop(packet)
        if (sid and data) then
                print("lua.server.pop", sid, #data, data)
                for i = 1, #data  do
                        print (i, data:byte(i))
                end
        end
end)
