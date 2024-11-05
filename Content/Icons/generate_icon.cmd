setlocal

pushd "%~dp0"

set srcimg=G:\Downloads\Programacao\Satisfactory Modding\Output\Exports\FactoryGame\Content\FactoryGame\Resource\Equipment\GemstoneScanner\UI\ObjectScanner_256.png

set font=Arial-Bold
set fontSize=50
set fontSize2=50
set stroke=black
set fill=white
set position=+0+20
set text=ECM
set overlay=effcheck2.png
set targetName=ECM

convert -verbose "%srcimg%" -font "%font%" -pointsize %fontSize% -stroke "%stroke%" -strokewidth 2 -fill "%fill%" -gravity center -annotate "%position%" "%text%" -resize 256x256 "%targetName%-icon-256.png"
convert -verbose "%srcimg%" -font "%font%" -pointsize %fontSize% -stroke "%stroke%" -strokewidth 1 -fill "%fill%" -gravity center -annotate "%position%" "%text%" -resize 64x64 "%targetName%-icon-64.png"

REM convert -verbose "%srcimg%" -fuzz 5%% -transparent #eba448 ( %overlay% -alpha remove -alpha off -virtual-pixel transparent -distort Perspective "0,0,69,84 512,0,124,102 512,512,124,162 0,512,70,140" ) -compose DstOver -composite ^
	 REM -font "%font%" -pointsize %fontSize2% -stroke "%stroke%" -strokewidth 2 -fill "%fill%" -gravity south -annotate "%position%" "%text%" -resize 256x256 "%targetName%-icon2-256.png"
REM convert -verbose "%srcimg%" -fuzz 5%% -transparent #eba448 ( %overlay% -alpha remove -alpha off -virtual-pixel transparent -distort Perspective "0,0,69,84 512,0,124,102 512,512,124,162 0,512,70,140" ) -compose DstOver -composite ^
	 REM -font "%font%" -pointsize %fontSize2% -fill "%fill%" -gravity south -annotate "%position%" "%text%" -resize 64x64 "%targetName%-icon2-64.png"

convert -verbose "%srcimg%" ( %overlay% -alpha remove -alpha off -virtual-pixel transparent -distort Perspective "0,0,69,84 512,0,124,102 512,512,124,162 0,512,70,140" ) -compose SrcOver -composite ^
	-font "%font%" -pointsize %fontSize2% -stroke "%stroke%" -strokewidth 2 -fill "%fill%" -gravity south -annotate "%position%" "%text%" ^
	-resize 256x256 "%targetName%-icon2-256.png"
convert -verbose "%srcimg%" ( %overlay% -alpha remove -alpha off -virtual-pixel transparent -distort Perspective "0,0,69,84 512,0,124,102 512,512,124,162 0,512,70,140" ) -compose SrcOver -composite ^
	-font "%font%" -pointsize %fontSize2% -fill "%fill%" -gravity south -annotate "%position%" "%text%"  ^
	-resize 64x64 "%targetName%-icon2-64.png"
