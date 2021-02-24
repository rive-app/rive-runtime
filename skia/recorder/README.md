QUESTIONS: 

- how to deal with errors ( im raising exceptions so something falls through to the end... )
    feels like cpp likes returning ints :D 
    obvs i'm just doing one error type now, was maybe going to add a few? 
    good way to add stack traces in?
- how to run a debugger, segfaults are a pain
- what to do with this guy
    ```// hmm "no default constructor exists bla bla... "
	rive::SkiaRenderer renderer(rasterCanvas);```
  not looked much yet. but i was hoping to declae that i got em.. but then i 'd get thi default constructor errr... 
- headers and stuff.. all imports into the headers? 
    good way to strip unused imports?
- i read a crazy way to generate. but really
    i'd like to be able to generate all frames as part of the loop.. that would feel "clean" to be fair i think i just rename i to frame and call it a day on the main loop :P 