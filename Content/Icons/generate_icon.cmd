setlocal

pushd "%~dp0"

set srcimg=G:\Downloads\Programacao\Satisfactory Modding\Output\Exports\FactoryGame\Content\FactoryGame\Resource\Equipment\GemstoneScanner\UI\ObjectScanner_256.png

set font=Arial-Bold
set fontSize=50
set stroke=black
set fill=white
set position=+0+20
set text=Efficiency\nChecker

convert -verbose "%srcimg%" -font "%font%" -pointsize %fontSize% -stroke "%stroke%" -strokewidth 2 -fill "%fill%" -gravity center -annotate "%position%" "%text%" -resize 256x256 ECM-icon-256.png
convert -verbose "%srcimg%" -font "%font%" -pointsize %fontSize% -stroke "%stroke%" -strokewidth 1 -fill "%fill%" -gravity center -annotate "%position%" "%text%" -resize 64x64 ECM-icon-64.png
