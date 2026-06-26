--      (c) Copyright 2010      by Pali Rohár

function SetupAnimation(filename, w, h, x, y, framecntX, framecntY, backwards, pauseFrameCnt, speedScale, menu)
   print("[SA] enter "..filename.." w="..w.." h="..h)
   local g = CGraphic:New(filename)
   print("[SA] CGraphic:New ok")
   -- math.floor(... + 0.5) is true round-half-up. Upstream's
   -- math.ceil(... + 0.5) bumps exact integers by 1 (e.g. 24 * 1.5 = 36
   -- becomes 37), which makes head sprites a pixel wider than the bg
   -- they sit on and offsets every position by a fractional pixel.
   local headW = math.floor(w / framecntX * (Video.Width / 320) + 0.5)
   local headH = math.floor(h / framecntY * (Video.Height / 200) + 0.5)
   print("[SA] headW="..headW.." headH="..headH.." resize to "..(headW*framecntX).."x"..(headH*framecntY))
   g:Load()
   print("[SA] g:Load ok")
   g:Resize(headW * framecntX, headH * framecntY)
   print("[SA] g:Resize ok")
   local head = ImageWidget(g)
   print("[SA] ImageWidget ok")
   local headClip = Container()
   print("[SA] Container ok")
   headClip:setOpaque(false)
   headClip:setBorderSize(0)
   headClip:setWidth(headW)
   headClip:setHeight(headH)
   print("[SA] headClip configured")
   headClip:add(head, 0, 0)
   print("[SA] headClip:add(head) ok")
   menu:add(headClip, x * Video.Width / 320, y * Video.Height / 200)
   print("[SA] menu:add(headClip) ok")

   local animTable = {}

   if framecntX > 1 then
      -- animation: frames horizontally
      for i=0,framecntX-1,1 do
         for j=0,speedScale,1 do
            animTable[#animTable + 1] = { -i * headW, 0 }
         end
      end
      if backwards then
         for i=framecntX-1,0,-1 do
            for j=0,speedScale,1 do
               animTable[#animTable + 1] = { -i * headW, 0 }
            end
         end
      end
   else
      -- animation: frames vertically
      for i=0,framecntY-1,1 do
         for j=0,speedScale,1 do
            animTable[#animTable + 1] = { 0, -i * headH }
         end
      end
      if backwards then
         for i=framecntY-1,0,-1 do
            for j=0,speedScale,1 do
               animTable[#animTable + 1] = { 0, -i * headH }
            end
         end
      end
   end

   for i=0,pauseFrameCnt,1 do
      animTable[#animTable + 1] = { 0, 0 }
   end

  local frame = 1
  local function animationCb()
     headClip:remove(head)
     headClip:add(head, animTable[frame][1], animTable[frame][2])
     frame = frame + 1
     if frame > #animTable then
        frame = 1
     end
  end

  return {animationCb, headClip}
end

function Briefing(title, objs, bgImg, mapbg, mapVideo, text, voices)
  print("[brief] enter race="..tostring(currentRace).." bgImg="..tostring(bgImg))
  SetPlayerData(GetThisPlayer(), "RaceName", currentRace)
  print("[brief] after SetPlayerData")

  local menu = WarMenu()
  print("[brief] WarMenu ok")

  local voice = 0
  local channel = -1

  local bg1 = CGraphic:New(bgImg)
  print("[brief] bg1 New ok")
  bg1:Load()
  bg1:Resize(Video.Width, Video.Height)
  print("[brief] bg1 Load ok")
  local bg2 = nil
  if CanAccessFile(mapbg) then
     bg2 = CGraphic:New(mapbg)
     bg2:Load()
     bg2:Resize(Video.Width, Video.Height)
     print("[brief] bg2 Load ok")
  else
     print("[brief] mapbg miss "..tostring(mapbg))
  end

  local bg = ImageButton()
  print("[brief] ImageButton new ok")
  bg:setNormalImage(bg1)
  print("[brief] setNormalImage ok")
  menu:add(bg, 0, 0)
  print("[brief] bg added")

  local animations = {}

  if (currentRace == "human") then
    print("[brief] PlayMusic human")
    PlayMusic(HumanBriefingMusic)
    print("[brief] LoadUI human")
    LoadUI("human", Video.Width, Video.Height)
    print("[brief] anim 1")
    animations[1] = SetupAnimation("graphics/428.png", 120, 24, 83, 37, 5, 1, false, 20, 2, menu)
    print("[brief] anim 2")
    -- Wizard: half-pixel nudge so the scaled position rounds to one
    -- pixel left and one pixel down on a 480x272 LCD (the discrete-pixel
    -- art lands on the right cell of the stretched bg this way).
    animations[2] = SetupAnimation("graphics/429.png", 67, 42 * 21, 206.5, 29.5, 1, 21, false, 0, 2, menu)
    print("[brief] anim 3")
    animations[3] = SetupAnimation("graphics/430.png", 24, 504, 21, 17, 1, 21, true, 0, 0, menu)
    print("[brief] anim 4")
    animations[4] = SetupAnimation("graphics/431.png", 20, 462, 275, 16, 1, 21, true, 0, 0, menu)
    print("[brief] all 4 anims ok")

    bg1:SetPaletteColor(255, 48, 56, 56)
    -- -- color cycle the shadow pixels to give a sense of flickering light
    -- local coloridx = 1
    -- local colors = {{48, 56, 56}, {44, 48, 48}, {36, 48, 44}, {52, 64, 64}, {56, 68, 64}, {68, 80, 76}, {72, 84, 80}}
    -- local function animateFlicker()
    --    bg1:SetPaletteColor(255, colors[coloridx][1], colors[coloridx][2], colors[coloridx][3])
    --    coloridx = coloridx + 1
    --    if coloridx > #colors then
    --       coloridx = 1
    --    end
    -- end
    -- animations[5] = {animateFlicker, nil}

  elseif (currentRace == "orc") then
    PlayMusic(OrcBriefingMusic)
    LoadUI("orc", Video.Width, Video.Height)

    animations[1] = SetupAnimation("graphics/426.png", 280, 67, 18, 67, 5, 1, true, 20, 2, menu)
    animations[2] = SetupAnimation("graphics/427.png", 345, 58, 202, 52, 5, 1, true, 0, 2, menu)
    animations[3] = SetupAnimation("graphics/425.png", 50, 1023, 145, 70, 1, 31, false, 0, 0, menu)
  else
    StopMusic()
  end

  local frameTime = 0
  local function animateHeads()
     frameTime = frameTime + 1
     if frameTime % 3 == 0 then
        for i=1,#animations,1 do
           animations[i][1]()
        end
     end
  end

  print("[brief] making LuaActionListener")
  -- D: was global `listener = ...` — that wrote _G.listener and got
  -- overwritten by the next briefing's run, defeating the logic-callback
  -- anchor for the prior briefing. Local-scope it so each Briefing's
  -- listener is independent. (Note: even with A1, this is the right
  -- scoping; we don't want briefings sharing a global slot.)
  local listener = LuaActionListener(animateHeads)
  menu:addLogicCallback(listener)
  print("[brief] addLogicCallback ok")

  Objectives = objs

  if (title ~= nil) then
     local headline = title
     if (objs ~= nil) then
        headline = title .. " - " .. objectives[1]
     end
     menu:addLabel(headline, 0.1 * Video.Width, 0.1 * Video.Height, Fonts["large"], false)
  end
  print("[brief] LoadBuffer text="..tostring(text))
  local t = LoadBuffer(text)
  print("[brief] LoadBuffer ok len="..tostring(t and #t or "nil"))
  local sw = ScrollingWidget(0.7 * 320, 0.6 * 200)
  print("[brief] sw ok")
  sw:setBackgroundColor(Color(0,0,0,0))
  sw:setSpeed(0.12)
  print("[brief] MLL new")

  local listener
  local l = MultiLineLabel(t)
  print("[brief] MLL setForegroundColor")
  l:setForegroundColor(Color(0, 0, 0, 255))
  l:setFont(Fonts["large"])
  l:setAlignment(MultiLineLabel.CENTER)
  l:setVerticalAlignment(MultiLineLabel.CENTER)
  l:setLineWidth(0.7 * 320)
  l:adjustSize()
  l:setHeight(0.9 * 200)
  print("[brief] sw:add(l)")
  sw:add(l, 0, 0)
  print("[brief] menu:add(sw)")
  menu:add(sw, 0.15 * Video.Width, 0.2 * Video.Height)
  print("[brief] sw added")

  function PlayNextVoice()
    voice = voice + 1
    if (voice <= table.getn(voices)) then
      channel = PlaySoundFile(voices[voice], PlayNextVoice);
    else
      channel = -1
    end
  end
  print("[brief] PlayNextVoice")
  PlayNextVoice()
  print("[brief] PlayNextVoice ok")

  print("[brief] GetGameSpeed")
  local speed = GetGameSpeed()
  print("[brief] SetGameSpeed")
  SetGameSpeed(30)
  print("[brief] gamespeed ok")

  local currentAction = nil
  local overall
  function action2()
     print("[brief] action2 fire mapVideo="..tostring(mapVideo))
     if (channel ~= -1) then
        voice = table.getn(voices)
        StopChannel(channel)
     end
     StopMusic()
     MusicStopped()

     local scenem = Movie()
     -- Keep the upstream widget + logic-callback flow even though our
     -- Movie:Load is synchronous: the briefing menu's per-frame
     -- Invalidate between Load returning and menu:stop firing is what
     -- repaints the bg2 (map png) into the OVL buffer the cinematic
     -- clobbered. Without that frame, the mission's first render starts
     -- on a buffer with cedar's last frame still in it and tiles end
     -- up not visible (units/buildings float on a black background).
     local loaded = scenem:Load(mapVideo, Video.Width, Video.Height)
     if loaded then
        currentAction = function() menu:stop() end
        local movieWidget = ImageWidget(scenem)
        menu:add(movieWidget, 0, 0)
        menu:add(overall, 0, 0)
        local function playEnd()
           if not scenem:IsPlaying() then
              menu:stop()
           end
        end
        listener = LuaActionListener(playEnd)
        menu:addLogicCallback(listener)
     else
        menu:stop()
     end
  end
  function action1()
     print("[brief] action1 fire bg2="..tostring(bg2 ~= nil).." mapVideo="..tostring(mapVideo))
     if bg2 ~= nil then
        bg:setNormalImage(bg2)
        for i=1,#animations,1 do
           if animations[i][2] then
              animations[i][2]:setVisible(false)
           end
        end
        currentAction = action2
        print("[brief] action1 -> map shown, next click will play movie")
     else
        action2()
     end
  end
  currentAction = action1
  print("[brief] action funcs ok")

  overall = ImageButton()
  print("[brief] overall ImageButton ok")
  overall:setWidth(Video.Width)
  overall:setHeight(Video.Height)
  overall:setBorderSize(0)
  overall:setBaseColor(Color(0, 0, 0, 0))
  overall:setForegroundColor(Color(0, 0, 0, 0))
  overall:setBackgroundColor(Color(0, 0, 0, 0))
  print("[brief] overall configured")
  overall:setActionCallback(function()
        currentAction()
  end)
  print("[brief] overall setActionCallback ok")
  overall:setHotKey("return")
  print("[brief] overall setHotKey ok")

  l:setActionCallback(action2)
  print("[brief] l:setActionCallback ok")
  sw:setActionCallback(action2)
  print("[brief] sw:setActionCallback ok")

  menu:add(overall, 0, 0)
  print("[brief] menu:run()")
  menu:run()
  print("[brief] menu:run returned")

  SetGameSpeed(speed)
end

function GetCampaignState(race)
  if (race == "orc") then
    return preferences.CampaignOrc
  elseif (race == "human") then
    return preferences.CampaignHuman
  end
  return 1
end

function IncreaseCampaignState(race, state)
  -- Loaded saved game could have other old state
  -- Make sure that we use saved state from config file
  if (race == "orc") then
    if (state ~= preferences.CampaignOrc) then return end
    preferences.CampaignOrc = preferences.CampaignOrc + 1
  elseif (race == "human") then
    if (state ~= preferences.CampaignHuman) then return end
    preferences.CampaignHuman = preferences.CampaignHuman + 1
  end
  -- Make sure that we immediately save state
  SavePreferences()
end

function CreateEndingStep(bg, text, voice, video)
  return function()
      print ("Ending in " .. bg .. " with " .. text .. " and " .. voice)
	  local menu = WarMenu(nil, bg, true)
	  StopMusic()

          if (video ~= nil) then
             PlayMovie(video)
          end

          if currentRace == "orc" then
             -- there's animations here
             local animation = SetupAnimation("graphics/460.png", 153, 651, 89, 0, 1, 31, true, 0, 2, menu)
             local frameTime = 0
             local function animateHeads()
                frameTime = frameTime + 1
                if frameTime % 3 == 0 then
                   animation[1]()
                end
             end
             listener = LuaActionListener(animateHeads)
             menu:addLogicCallback(listener)
          end
          
	  local t = LoadBuffer(text)
	  t = "\n\n\n\n\n\n\n\n\n\n" .. t .. "\n\n\n\n\n\n\n\n\n\n\n\n\n"
	  local sw = ScrollingWidget(160, 85 * Video.Height / 200)
	  sw:setBackgroundColor(Color(0,0,0,80))
	  sw:setSpeed(0.12)
	  local l = MultiLineLabel(t)
	  l:setFont(Fonts["large"])
	  l:setAlignment(MultiLineLabel.LEFT)
	  l:setVerticalAlignment(MultiLineLabel.CENTER)
	  l:setLineWidth(160)
	  l:adjustSize()
	  sw:add(l, 0, 0)
	  menu:add(sw, 35 * Video.Width / 320, 40 * Video.Height / 200)
	  local channel = -1
	  menu:addHalfButton("~!Continue", "c", 227 * Video.Width / 320, 220 * Video.Height / 200,
		function()
		  if (channel ~= -1) then
			StopChannel(channel)
		  end
		  menu:stop()
		  StopMusic()
		end)
          channel = PlaySoundFile(voice, function() end);
	  menu:run()
	  GameResult = GameVictory
  end
end

function CreatePictureStep(bg, sound, title, text)
  return function()
    SetPlayerData(GetThisPlayer(), "RaceName", currentRace)
    PlayMusic(sound)
    local menu = WarMenu(nil, bg)
    local offx = (Video.Width - 320) / 2
    local offy  = (Video.Height - 200) / 2
    menu:addLabel(title, offx + 160, offy + 120 - 33, Fonts["large-title"], true)
    menu:addLabel(text, offx + 160, offy + 120 - 12, Fonts["small-title"], true)
    menu:addHalfButton("~!Continue", "c", 227 * Video.Width / 320, 220 * Video.Height / 200,
      function() menu:stop() end)
    menu:run()
    GameResult = GameVictory
  end
end

-- Pre-mission briefing — wraps upstream Briefing() with the per-mission
-- assets resolved to our catalog paths. Bg/mapbg drop the "../graphics/"
-- prefix because the catalog key is "ui/<race>/briefing.png" /
-- "graphics/<r>map<NN>.png". Intro txt + voice come from the
-- campaigns/<race>/ tree (gunzipped at build time).
function WAR1_PreMissionBriefing(race, map)
  -- Briefing() reads currentRace; ensure it's set even if the UI hasn't
  -- already loaded for this race (early load-from-save path).
  currentRace = race
  local race_prefix = string.lower(string.sub(race, 1, 1))
  local prefix = "campaigns/" .. race .. "/"
  Briefing(
    "Mission " .. tonumber(map),
    nil,
    "ui/" .. race .. "/briefing.png",
    "graphics/" .. race_prefix .. "map" .. map .. ".png",
    "videos/" .. race_prefix .. "map" .. map .. ".ogv",
    prefix .. map .. "_intro.txt",
    {prefix .. map .. "_intro.wav"}
  )
end

function CreateMapStep(race, map)
  return function()
    -- If there is a pre-setup step, run it, if that fails, don't worry
    local prefix = "campaigns/" .. race .. "/"
    pcall(function () Load(prefix .. map .. "_prerun.lua") end)
    Load(prefix .. map .. "_c2.sms")  -- objectives + DefineAllow rules

    WAR1_PreMissionBriefing(race, map)

    war1gus.InCampaign = true
    -- Tear down all menu screens before the game's render loop takes
    -- over. Otherwise the campaign sub-menu's ImageWidget bg stays as
    -- Gui top and DrawGuichanWidgets keeps painting the WARCRAFT logo
    -- over the live map every frame.
    if WAR1_CloseAllMenus then WAR1_CloseAllMenus() end
    Load(prefix .. map .. ".smp")
    RunMap(prefix .. map .. ".smp", preferences.FogOfWar)
    war1gus.InCampaign = false
    if (GameResult == GameVictory) then
      IncreaseCampaignState(currentRace, currentState)
    end
  end
end

function CreateVictoryStep(bg, text, voices)
  return function()
    Briefing(nil, nil, bg, text, voices)
    GameResult = GameVictory
  end
end

-- Jupiter port: campaign titles tables are baked into the script so
-- RunCampaignSubmenu doesn't depend on the Lua chunk loader resolving
-- `campaigns/<race>/campaign_titles.lua` at click time. The titles are
-- copied verbatim from data.War1gus/campaigns/{orc,human}/campaign_titles.lua
-- (upstream War1gus 3.3.3) — same Roman-numeral mission names that
-- ship in Blizzard's WC1.
local CAMPAIGN_TITLES_HUMAN = {
   "I. Regent",
   "II. Grand Hamlet",
   "III. Kyross",
   "IV. Dead Mines",
   "V. Elwynn Forest",
   "VI. Northshire Abbey",
   "VII. Sunnyglade",
   "VIII. Medivh's Tower",
   "IX. Black Morass",
   "X. Temple of the Damned",
   "XI. Rockard & Stoneard",
   "XII. Black Rock Spire",
   "Ending"}
local CAMPAIGN_TITLES_ORC = {
   "I. Swamps of Sorrow",
   "II. Kyross",
   "III. Grand Hamlet",
   "IV. Dead Mines",
   "V. Red Ridge Mountains",
   "VI. Sunnyglade",
   "VII. Black Morass",
   "VIII. Northshire Abbey",
   "IX. Center of the Human Lands",
   "X. Elwynn Forest",
   "XI. Goldshire & Moonbrook",
   "XII. Stormwind Keep",
   "Ending"}

function CampaignButtonTitle(race, i)
  local titles = (race == "orc") and CAMPAIGN_TITLES_ORC or CAMPAIGN_TITLES_HUMAN
  local title = titles[i] or "xxx"

  -- Jupiter port: upstream targets 640-wide screens with 160 px buttons
  -- and trims long titles at 20 chars (sub(1,19)..".." → 22 max). Our
  -- 480 LCD makes addFullButton's standard half-width buttons 127 px,
  -- which fits ~18 chars of the 6×8 Fonts["large"] glyphs before the
  -- text runs past the button's right edge. Tighten the cap accordingly:
  -- > 16 → 14 + "..." = 17 char max.
  if ( string.len(title) > 16 ) then
	  title = string.sub(title, 1, 14) .. "..."
  end

  return title
end

function CampaignButtonFunction(campaign, race, i, menu)
  return function()
    position = campaign.menu[i]
    currentCampaign = campaign
    currentRace = race
    currentState = i
    menu:stop()
    RunCampaign(campaign)
  end
end

function RunCampaignSubmenu(race)
  Load("scripts/campaigns.lua")
  campaign = CreateCampaign(race)

  currentRace = race
  SetPlayerData(GetThisPlayer(), "RaceName", currentRace)

  local menu = WarMenu()
  local offx = (Video.Width - 320) / 2
  local offy = (Video.Height - 200) / 2

  local show_buttons = GetCampaignState(race)
  local half = math.ceil(show_buttons/2)

  -- Jupiter port: shifted the button block down from upstream's
  -- (offy + 32) start. The 320×200 title_screen.png upstream centers
  -- inside a 640×480 menu screen with the WARCRAFT logo + "ORCS &
  -- HUMANS" subtitle in the upper portion — at our 480×272 LCD the
  -- aspect-fit-scaled bg fills vertically (435×272 pillarbox), so the
  -- combined logo+subtitle takes roughly y=20–115. Pushing the campaign
  -- list to (offy + 84) drops it below them. Previous Menu shifted to
  -- (offy + 214) so it clears the 6th button without crashing into the
  -- © 1994 BLIZZARD strip at the bottom edge.
  for i=1,half do
    menu:addFullButton(CampaignButtonTitle(race, i), ".", offx + 31, offy + 84 + (18 * (i - 1)), CampaignButtonFunction(campaign, race, i, menu))
  end

  for i=1+half,show_buttons do
    menu:addFullButton(CampaignButtonTitle(race, i), ".", offx + 164, offy + 84 + (18 * (i - 1 - half)), CampaignButtonFunction(campaign, race, i, menu))
  end

  menu:addFullButton("~!Previous Menu", "p", offx + 96, offy + 214,
    function() menu:stop(); currentCampaign = nil; currentRace = nil; currentState = nil; RunCampaignGameMenu() end)
  menu:run()

end

function RunCampaign(campaign)
  if (campaign ~= currentCampaign or position == nil) then
    position = 1
  end

  currentCampaign = campaign

  while (position <= table.getn(campaign.steps)) do
    campaign.steps[position]()
    if (GameResult == GameVictory) then
      position = position + 1
    elseif (GameResult == GameDefeat) then
    elseif (GameResult == GameDraw) then
    elseif (GameResult == GameNoResult) then
      currentCampaign = nil
      return
    else
      break -- quit to menu
    end
  end

  RunCampaignSubmenu(currentRace)

  currentCampaign = nil
end

function RunCampaignGameMenu()
  local menu = WarMenu()
  local offx = (Video.Width - 320) / 2
  local offy = (Video.Height - 200) / 2

  menu:addFullButton("~!Orc campaign", "o", offx + 96, offy + 106 + (18 * 0),
    function() RunCampaignSubmenu("orc"); menu:stop() end)
  menu:addFullButton("~!Human campaign", "h", offx + 96, offy + 106 + (18 * 1),
    function() RunCampaignSubmenu("human"); menu:stop() end)

  menu:addFullButton("~!Previous Menu", "p", offx + 96, offy + 106 + (18 * 4),
    function() RunSinglePlayerSubMenu(); menu:stop() end)

  menu:run()
end

