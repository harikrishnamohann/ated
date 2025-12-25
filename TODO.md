- [x] up and down movements  
- [x] sticky scrolling  
- [x] split editor operations into component level  
- [x] make line updation incremental *(using gap buffer)*  
- [x] make Editor window attachable  
- [x] handle horizontal text overflows  
- [x] undo-redo  
- [x] implement lazy line updation technique and use gap buffer for lines
- [x] tab handling and horizontal rendering
- [x] del key
- [x] auto pair insetion
- [x] auto newline to matching pairs
- [ ] ensure correctness of sticky cursor with line indentation
- [x] save and load files
- [x] colors
- [ ] selection
- [x] status line  
- [x] indentation
- [ ] command palette
- [ ] syntax highlighting 
- [ ] multiple instances
- [ ] search
- [ ] replace

**NOTES** to myself
I feel kind of cozy working too much on this project, although I love what
I see. My brain isn't working, so I am taking a short break.

I was working on text selection. I only figured the how to make the mouse work
in ncurses. That should be where you should catch up (in main.c).
I layed out a new field in Editor type for handling selection. But,
BEFORE GETTING INTO SELECTION, you should learn how this current ai-generated
draw() function work and re-make it yourself. That's the only black-box in
existance.

**IDEA:** An optimized way to write files
A new variable in Editortype can be used to track the lowest modified index.
When writing to file, start writing from this index inside the buffer instead
of writing the entire file. This will add complexity to insert and remove
operations.

Then you can design the command palette/fuzzy finder
Also, colors.c is messy right now