local core = require "sys.core"
local testaux = require "testaux"

local total = 30
local WAIT

local function gen_closure(n)
	local now = core.now()
	return function ()
		local delta = core.now() - now
		delta = math.abs(delta - 100 - n * 10)
		--precise is 10ms
		testaux.assertle(delta, 10, "timer check delta")
		total = total - 1
		if total == 0 then
			core.wakeup(WAIT)
		end
	end
end

return function()
	for i = 1, 30 do
		local f = gen_closure(i)
		core.timeout(100 + i * 10, f)
	end
	WAIT = core.running()
	core.wait()
end

