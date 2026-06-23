-- VortexEffect.lua
-- Pluggable vortex effect for Mode 7 engine

local VortexEffect = {}

-- Create a new vortex effect
function VortexEffect.Create(options)
	options = options or {}

	local effect = {
		-- Vortex parameters
		centerX = options.centerX or 512,
		centerY = options.centerY or 512,
		spiralStrength = options.spiralStrength or 2.0,
		pullStrength = options.pullStrength or 1.0,
		waveAmplitude = options.waveAmplitude or 50,
		waveFrequency = options.waveFrequency or 3,
		timeScale = options.timeScale or 1.0,
		colorShift = 0,

		-- Visual settings
		tilemapSize = options.tilemapSize or 32,
		tileSize = options.tileSize or 32,

		-- Internal
		tilemap = {},
		mode7System = nil
	}

	-- Attach methods to the effect instance
	effect.onAttach = function(mode7System)
		effect.mode7System = mode7System
		print("🔗 VortexEffect attached to Mode7System")
	end

	effect.transformPoint = function(screenX, screenY, time)
		return VortexEffect.transformPoint(effect, screenX, screenY, time)
	end

	effect.sample = function(worldX, worldY, distance, time)
		return VortexEffect.sample(effect, worldX, worldY, distance, time)
	end

	effect.render = function(system, time)
		print("🌀 VortexEffect.render called at time", time) -- Debug print

		for y = 1, system.ScreenHeight do
			local scanline = system.Scanlines[y]
			if scanline then
				for x = 1, #scanline.pixels do
					local screenX = (x - 1) * system.SegmentWidth + system.SegmentWidth / 2

					-- Use vortex transformation instead of standard Mode 7
					local worldX, worldY, distance = VortexEffect.transformPoint(effect, screenX, y, time)

					-- Sample the vortex pattern
					local color = VortexEffect.sample(effect, worldX, worldY, distance, time)

					scanline.pixels[x].BackgroundColor3 = Color3.fromRGB(
						math.clamp(color[1], 0, 255),
						math.clamp(color[2], 0, 255),
						math.clamp(color[3], 0, 255)
					)
				end
			end
		end
	end

	-- Generate vortex tilemap
	VortexEffect.GenerateTilemap(effect)

	return effect
end

-- Generate the spiral pattern tilemap
function VortexEffect.GenerateTilemap(effect)
	local size = effect.tilemapSize
	effect.tilemap = {}

	for y = 1, size do
		effect.tilemap[y] = {}
		for x = 1, size do
			local centerX, centerY = size / 2, size / 2
			local dx = x - centerX
			local dy = y - centerY
			local distance = math.sqrt(dx * dx + dy * dy)
			local angle = math.atan2(dy, dx)

			-- Create spiral bands
			local spiralValue = (distance * 0.3 + angle * 2) % (math.pi * 2)

			if distance < 2 then
				-- Center core - bright white
				effect.tilemap[y][x] = {255, 255, 255}
			elseif spiralValue < math.pi then
				-- Spiral arms - bright colors
				local intensity = math.max(0.1, 1 - distance / 16)
				effect.tilemap[y][x] = {
					math.floor(255 * intensity),
					math.floor(100 * intensity),
					math.floor(255 * intensity)
				}
			else
				-- Gaps between arms - darker
				local intensity = math.max(0.05, 0.3 - distance / 20)
				effect.tilemap[y][x] = {
					math.floor(80 * intensity),
					math.floor(40 * intensity),
					math.floor(120 * intensity)
				}
			end
		end
	end
end

-- Called when effect is attached to Mode 7 system
function VortexEffect.onAttach(effect, mode7System)
	effect.mode7System = mode7System
	print("🔗 VortexEffect attached to Mode7System")
end

-- Custom transformation for vortex effect
function VortexEffect.transformPoint(effect, screenX, screenY, time)
	local system = effect.mode7System

	-- Center screen coordinates
	local centeredX = screenX - system.ScreenWidth / 2
	local centeredY = screenY - system.ScreenHeight / 2

	-- Convert to polar coordinates
	local distance = math.sqrt(centeredX * centeredX + centeredY * centeredY)
	local angle = math.atan2(centeredY, centeredX)

	-- Apply vortex effects
	local spiralAngle = angle + (distance * effect.spiralStrength * 0.01) + (time * effect.timeScale)
	local pullDistance = distance * (1 - effect.pullStrength * 0.001)

	-- Add wave distortion
	local waveOffset = math.sin(time * 3 + distance * 0.1) * effect.waveAmplitude
	pullDistance = pullDistance + waveOffset

	-- Convert back to world coordinates
	local worldX = effect.centerX + (math.cos(spiralAngle) * pullDistance * system.Camera.scale)
	local worldY = effect.centerY + (math.sin(spiralAngle) * pullDistance * system.Camera.scale)

	return worldX, worldY, distance
end

-- Sample the vortex pattern at world coordinates
function VortexEffect.sample(effect, worldX, worldY, distance, time)
	local tileX = math.floor(worldX / effect.tileSize) + 1
	local tileY = math.floor(worldY / effect.tileSize) + 1

	-- Wrap around
	tileX = ((tileX - 1) % effect.tilemapSize) + 1
	tileY = ((tileY - 1) % effect.tilemapSize) + 1

	if tileX < 1 then tileX = effect.tilemapSize end
	if tileY < 1 then tileY = effect.tilemapSize end

	local baseColor = effect.tilemap[tileY] and effect.tilemap[tileY][tileX] or {50, 20, 100}

	-- Add time-based color shifting
	local shift = math.sin(time * 2 + effect.colorShift) * 0.3
	local color = {
		math.floor(math.clamp(baseColor[1] * (1 + shift), 5, 255)),
		math.floor(math.clamp(baseColor[2] * (1 + shift * 0.5), 5, 255)),
		math.floor(math.clamp(baseColor[3] * (1 + shift), 5, 255))
	}

	-- Apply distance-based effects
	if effect.mode7System then
		local centerDistance = distance / (effect.mode7System.ScreenWidth / 2)
		local brightness = math.max(0.2, 1 - centerDistance * 0.5)

		color[1] = math.floor(color[1] * brightness)
		color[2] = math.floor(color[2] * brightness)
		color[3] = math.floor(color[3] * brightness)

		-- Add center glow
		if centerDistance < 0.3 then
			local glowIntensity = (0.3 - centerDistance) * 3
			color[1] = math.min(255, color[1] + glowIntensity * 100)
			color[2] = math.min(255, color[2] + glowIntensity * 50)
			color[3] = math.min(255, color[3] + glowIntensity * 100)
		end
	end

	return color
end

-- Custom render function that uses vortex transformation
function VortexEffect.render(effect, system, time)
	print("🌀 VortexEffect.render called at time", time) -- Debug print

	for y = 1, system.ScreenHeight do
		local scanline = system.Scanlines[y]
		if scanline then
			for x = 1, #scanline.pixels do
				local screenX = (x - 1) * system.SegmentWidth + system.SegmentWidth / 2

				-- Use vortex transformation instead of standard Mode 7
				local worldX, worldY, distance = VortexEffect.transformPoint(effect, screenX, y, time)

				-- Sample the vortex pattern
				local color = VortexEffect.sample(effect, worldX, worldY, distance, time)

				scanline.pixels[x].BackgroundColor3 = Color3.fromRGB(
					math.clamp(color[1], 0, 255),
					math.clamp(color[2], 0, 255),
					math.clamp(color[3], 0, 255)
				)
			end
		end
	end
end

-- Update vortex parameters
function VortexEffect.SetParameters(effect, params)
	if params.centerX then effect.centerX = params.centerX end
	if params.centerY then effect.centerY = params.centerY end
	if params.spiralStrength then effect.spiralStrength = params.spiralStrength end
	if params.pullStrength then effect.pullStrength = params.pullStrength end
	if params.waveAmplitude then effect.waveAmplitude = params.waveAmplitude end
	if params.timeScale then effect.timeScale = params.timeScale end
end

-- Update color shift for animation
function VortexEffect.Update(effect, dt)
	effect.colorShift = effect.colorShift + 0.01 * effect.timeScale
end

function VortexEffect.Serialize(effect)
	return {
		centerX = effect.centerX,
		centerY = effect.centerY,
		spiralStrength = effect.spiralStrength,
		pullStrength = effect.pullStrength,
		waveAmplitude = effect.waveAmplitude,
		waveFrequency = effect.waveFrequency,
		timeScale = effect.timeScale
	}
end

function VortexEffect.Deserialize(params)
	return VortexEffect.Create(params)
end


function VortexEffect.GenerateFrame(effect, system, time)
	local output = {}

	for y = 1, system.ScreenHeight do
		output[y] = {}
		for x = 1, system.ScreenWidth do
			local worldX, worldY, distance = VortexEffect.transformPoint(effect, x, y, time)
			local color = VortexEffect.sample(effect, worldX, worldY, distance, time)
			output[y][x] = color
		end
	end

	return output
end



-- Preset configurations
VortexEffect.Presets = {
	spiral = function(effect)
		VortexEffect.SetParameters(effect, {
			spiralStrength = 3,
			pullStrength = 1.5,
			waveAmplitude = 30
		})
	end,

	chaos = function(effect)
		VortexEffect.SetParameters(effect, {
			spiralStrength = 2,
			waveAmplitude = 80,
			timeScale = 2.5
		})
	end,

	calm = function(effect)
		VortexEffect.SetParameters(effect, {
			spiralStrength = 1,
			pullStrength = 0.5,
			waveAmplitude = 20,
			timeScale = 0.8
		})
	end,

	hypnotic = function(effect)
		VortexEffect.SetParameters(effect, {
			spiralStrength = 4,
			waveAmplitude = 30,
			timeScale = 0.5
		})
	end
}

return VortexEffect