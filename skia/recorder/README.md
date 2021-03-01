## HOW TO

- check out this branch :check:
- cd to root of the git project
- build the image (i called my image boo, you are allowed alternative names)
    - `docker build  -f Dockerfile.recorder -t boo .`
- run the image
    - now. inside the image you wanna convert riv files into mp4. to do that we need to get files there. so we're mounting a volume 
    - make a folder (maybe call it output) at the root level i guess. and then run the image up as a container
    - `docker run -it  -v $(pwd)/output:/output boo /bin/bash`
    - in a differnt tab, put some .riv files into `$(pwd)/output` if you havent done so already 
    - inside docker again: 
        - `./run.sh -s /output/dory.riv -d /output/dory.mp4`
    - that should be all (obvs include the watermark etc if you wanna. )


## QUESTIONS

- how to deal with errors ( im raising exceptions so something falls through to the end... )
  - feels like cpp likes returning ints :D
  - obvs i'm just doing one error type now, was maybe going to add a few?
  - good way to add stack traces in?
- how to run a debugger, segfaults are a pain
- what to do with this guy. I'm having to declare it each iteration, rather than once. i was hoping to declare it once but i ran into some no default constructor issue (as i initialized renderer in the header .. i guess)
  - ```// hmm "no default constructor exists bla bla... " rive::SkiaRenderer renderer(rasterCanvas);```
  - not looked much yet. but i was hoping to declare that i got em.. but then i 'd get thi default constructor errr...
- headers and stuff.. all imports into the headers?
    good way to strip unused imports?
- i read a crazy way to generate. but really
    i'd like to be able to generate all frames as part of the loop.. that would feel "clean" to be fair i think i just rename i to frame and call it a day on the main loop :P
