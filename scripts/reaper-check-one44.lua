-- Headless-friendly check: instantiate 1.44 as a CLAP instrument.
-- Usage: reaper -nonewinst /path/to/scripts/reaper-check-one44.lua

local log_path = "/tmp/one44-reaper-check.txt"
local function log(msg)
	local f = io.open(log_path, "a")
	if f then
		f:write(msg .. "\n")
		f:close()
	end
	reaper.ShowConsoleMsg(msg .. "\n")
end

log("1.44 REAPER load check")
reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
if track == nil then
	log("FAIL: could not create track")
	return
end

local names = {
	"CLAP:1.44",
	"1.44",
	"CLAP: com.enazzzz.one-four-four",
	"com.enazzzz.one-four-four",
}

local fx = -1
local used = ""
for i = 1, #names do
	fx = reaper.TrackFX_AddByName(track, names[i], false, -1)
	if fx >= 0 then
		used = names[i]
		break
	end
end

if fx < 0 then
	log("FAIL: 1.44 not found as a CLAP instrument. Copy one44.clap to ~/.clap and rescan.")
	reaper.Main_OnCommand(40004, 0) -- File: Quit REAPER
	return
end

local retval, name = reaper.TrackFX_GetFXName(track, fx, "")
log("OK: instantiated with '" .. used .. "' as FX index " .. tostring(fx))
log("FX name: " .. tostring(name))
log("instrument=yes")
reaper.Main_OnCommand(40004, 0) -- File: Quit REAPER