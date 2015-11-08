#caslock 
A generic buffer compare-and-swap C library, 
suitable for dropping into other libraries that 
need to keep some global state, but don't mind 
if it's *really* slow to access.

built over pthread-less-locks, which are built over
posix stdio locks, which provide (in theory) 
multithreading mutexes in C without explicitly 
linking a threading library.

#build
 todo

#documentation
 todo

#examples
 todo

