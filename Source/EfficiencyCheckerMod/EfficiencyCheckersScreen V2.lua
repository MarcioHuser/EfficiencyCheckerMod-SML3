-- To use this script, you will need:
--	- A "Computer Case"
--  - A "CPU"
--  - A "GPU"
--	- Some "RAM"
--  - A large screen: name it "MainScreen"
--  - Connect network cables to all the checkers you want to monitor
--  - Name each one as "Checker (something)" (they must have the word "Checker", somewhere)
-- If you need more functionality (like a button that you just press to refresh the screen), it is up to you to improve as you want

-- Make your screen have the same number of columns, and ajust the lines accordingly to the size of the screen you made
columns = 80
lines = 35

-- The prefix to use when searching for a checker
prefix = "Checker"
-- The name of the screen to display onto
screenname = "MainScreen"

-- The name of the sizePanel which has the refresh button - Optional
sidepanelname = "SidePanel"
-- The coordinates of the refresh button on the side panel - Optional
buttoncoordinates = {0, 10}

line = 0
lineOffset = 0
gpu = nil
gpus = computer.getPCIDevices(classes.GPUT1)
screens = component.proxy(component.findComponent(screenname))


table.sort(screens, function(a,b) return a.nick < b.nick end)
for i,screen in pairs(screens) do
	print("Binding gpu " .. i)

	gpus[i]:bindScreen(screen)
	
	gpus[i]:setSize(columns, lines)
end

function setScreenForLine(line)
	-- Flush current gpu
	gpu:flush()
	
	-- Use new gpu
	gpu = gpus[math.floor(math.min(line, lines * getTableLen(gpus) - 1) / lines) + 1]
	
	print("Using gpu " .. math.floor(line / lines))
end

function nextLine()
    if (line + 1) % lines == 0 then
		-- Jump first line from each screen
        line = line + 1
    end
	
    setLine(line + 1)
end

function setLine(newLine)
	line = newLine
	
	if newLine % lines % 2 == 1 then
		setGrayBG()
	else
		resetBG()
	end

	if line >= 0 and line < lines * getTableLen(gpus) and (line < lineOffset or line >= lineOffset + lines) then
		lineOffset = math.floor(line / lines) * lines
		
		setScreenForLine(line)
	end
end

function getRelativeLine()
	return line - lineOffset
end

function resetBG()
    gpu:setBackground(0,0,0,1)
end

function roungDigits(value, digits)
    local m = 10 ^ (digits or 4)
    return math.floor(value * m + 0.5) / m
end

function setGrayBG()
    gpu:setBackground(0.05,0.05,0.05,1)
end

function setBlueBG()
    gpu:setBackground(0,0,0.15,1)
end

function setGreenBG()
    gpu:setBackground(0,0.15,0,1)
end

function setRedBG()
    gpu:setBackground(0.15,0,0,1)
end

function setYellowBG()
    gpu:setBackground(0.25,0.15,0,1)
end

function getTableLen(tbl)
  local n = 0
  
  for _ in pairs(tbl) do 
    n = n + 1 
  end
  
  return n
end

function updateChecker()
	colInput = columns - 40
	colLimit = columns - 30
	colOutput = columns - 20
	colSpare = columns - 10

	for _,localGpu in pairs(gpus) do
		gpu = localGpu

		resetBG()

		gpu:fill(0, 0, columns, lines, ' ')

		gpu:setText(0, 0, "Name")
		gpu:setText(colInput + 4, 0, "Input")
		gpu:setText(colLimit + 4, 0, "Limit")
		gpu:setText(colOutput + 3, 0, "Output")
		gpu:setText(colSpare + 4, 0, "Spare")
		
		setGrayBG()
		
		for i=1,lines,2 do
			gpu:fill(0, i, columns, 1, ' ')
		end
	end
	
	setScreenForLine(1)
	
	setLine(1)

    checkers = component.proxy(component.findComponent(prefix))
    
    table.sort(checkers, function(a,b) return a.nick < b.nick end)

	for i,checker in pairs(checkers) do
		gpu:setText(0, getRelativeLine(), i .. " - " .. checker.nick:sub(prefix:len() + 1))

		checker:updateBuilding()
		if checker:isCustomInjectedInput() then
		   setBlueBG()
		elseif roungDigits(checker:injectedInput()) == roungDigits(checker:requiredOutput()) then
			setGreenBG()
		elseif roungDigits(checker:injectedInput()) < roungDigits(checker:requiredOutput()) then
			setRedBG()
		elseif roungDigits(checker:injectedInput()) > roungDigits(checker:limitedThroughput()) then
			setYellowBG()
		end
		gpu:setText(colInput, getRelativeLine(), string.format("%9g", math.floor(checker:injectedInput() * 100) / 100))
		setLine(line)

		if roungDigits(checker:limitedThroughput()) == roungDigits(checker:requiredOutput()) then
			setGreenBG()
		elseif roungDigits(checker:limitedThroughput()) < roungDigits(checker:requiredOutput()) then
			setRedBG()
		end
		gpu:setText(colLimit, getRelativeLine(), string.format("%9g", math.floor(checker:limitedThroughput() * 100) / 100))
		setLine(line)

		if checker:isCustomRequiredOutput() then
		   setBlueBG()
		elseif roungDigits(checker:injectedInput()) == roungDigits(checker:requiredOutput()) and roungDigits(checker:limitedThroughput()) == roungDigits(checker:requiredOutput()) then
			setGreenBG()
		end
		gpu:setText(colOutput, getRelativeLine(), string.format("%9g", math.floor(checker:requiredOutput() * 100) / 100))
		setLine(line)

		if checker:injectedInput() - checker:requiredOutput() < 0 then
			setRedBG()
		end
		gpu:setText(colSpare, getRelativeLine(), string.format("%9g", math.floor((checker:injectedInput() - checker:requiredOutput()) * 100) / 100))
		setLine(line)

		nextLine()

		column = 5

		if getTableLen(checker:injectedItems()) > 1 then
			for _,item in pairs(checker:injectedItems()) do
				itemName = item:getName()

				if column + string.len(itemName) + 1 > columns then
					column = 5
					nextLine()
				end

				gpu:setText(column, getRelativeLine(), itemName)
				column = column + string.len(itemName)
				gpu:setText(column, getRelativeLine(), ';')
				column = column + 2

				--print("New column = " .. column)
			end
			gpu:setText(column - 2, getRelativeLine(), ' ')

			nextLine()
		end
	end

	gpu:flush()
end

updateChecker()

sidePanel = component.proxy(component.findComponent(sidepanelname))[1]

if sidePanel then
	updateButton = sidePanel:getModule(buttoncoordinates[1],buttoncoordinates[2])
	
	listening = false

	if updateButton then
		print("test")
		event.listen(updateButton)
		listening = true
	end
	
	while(listening) do
		e, c = event.pull()

		print("Event: " .. e)
		
		if c == updateButton then
			print("Updating checkers...")
			updateChecker()
		end
	end
end