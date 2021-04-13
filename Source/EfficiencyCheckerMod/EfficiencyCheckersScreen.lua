-- To use this script, you will need:
--	- A "Computer Case"
--  - A "CPU"
--  - A "GPU"
--	- Some "RAM"
--  - A large screen: name it "MainScreen"
--  - Connect network cables to all the checkers you want to monitor
--  - Name each one as "Checker (something)" (they must have the word "Checker", somewhere)
-- If you need more functionality (like a button that you just press to refresh the screen), it is up to you to improve as you want

gpu = computer.getGPUs()[1]
screen = component.proxy(component.findComponent("MainScreen"))[1]

gpu:bindScreen(screen)

-- columns = 60, lines = 22. Make your screen have the same number of columns, and ajust the lines accordingly to the size of the screen you made
gpu:setSize(60, 22)

w,h=gpu:getSize()

function resetBG()
    gpu:setBackground(0,0,0,1)
end

function roungDigits(value, digits)
    m = 10 ^ (digits or 4)
    return math.floor(value * m + 0.5) / m
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

function updateChecker()
    checkers = component.proxy(component.findComponent("Checker"))
    
    table.sort(checkers, function(a,b) return a.nick < b.nick end)

	line = 1

	resetBG()

	gpu:fill(0,0,w,h,' ')
	gpu:flush()

	gpu:setText(0,0,"Name")
	gpu:setText(w-26,0,"Input")
	gpu:setText(w-16,0,"Limit")
	gpu:setText(w-7,0,"Output")

	for _,checker in pairs(checkers) do
		gpu:setText(0,line, "- " .. checker.nick)

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
		gpu:setText(w-30,line, string.format("%9g", math.floor(checker:injectedInput() * 100) / 100))
		gpu:setBackground(0,0,0,1)

		if roungDigits(checker:limitedThroughput()) == roungDigits(checker:requiredOutput()) then
			setGreenBG()
		elseif roungDigits(checker:limitedThroughput()) < roungDigits(checker:requiredOutput()) then
			setRedBG()
		end
		gpu:setText(w-20,line, string.format("%9g", math.floor(checker:limitedThroughput() * 100) / 100))
		gpu:setBackground(0,0,0,1)

		if checker:isCustomRequiredOutput() then
		   setBlueBG()
		elseif roungDigits(checker:injectedInput()) == roungDigits(checker:requiredOutput()) and roungDigits(checker:limitedThroughput()) == roungDigits(checker:requiredOutput()) then
			setGreenBG()
		end
		gpu:setText(w-10,line, string.format("%9g", math.floor(checker:requiredOutput() * 100) / 100))
		gpu:setBackground(0,0,0,1)

		line = line + 1

		column = 5

		for _,item in pairs(checker:injectedItems()) do
			itemName = item:getName()

			if column + string.len(itemName) + 1 > w then
				column = 5
				line = line + 1
			end

			gpu:setText(column, line, itemName)
			column = column + string.len(itemName)
			gpu:setText(column, line, ';')
			column = column + 2

			--print("New column = " .. column)
		end

		gpu:setText(column - 2, line, ' ')

		line = line + 1
	end

	gpu:flush()
end

updateChecker()

sidePanel = component.proxy(component.findComponent("SidePanel"))[1]

if sidePanel then
	updateButton = sidePanel:getModule(0,10)

	event.listen(updateButton)

	while(true) do
		e, c = event.pull()

		print("Event: " .. e)
		
		if c == updateButton then
			print("Updating checkers...")
			updateChecker()
		end
	end
end
