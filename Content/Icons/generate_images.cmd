for %%i in (EfficienceCheckerPipe.png EfficienceCheckerSide.png EfficienceCheckerSlim.png EfficienceCheckerWall.png EfficienceCheckerWallPipeAbove.png EfficienceCheckerWallPipeBellow.png) do (
	for %%s in (512 256) do (
		convert -verbose "%%~i" -resize %%sx%%s "%%~ni-%%s.png"
	)
)
