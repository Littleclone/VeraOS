# Vera OS Kernel Prototype

This is a From Scratch RISC-V Kernel, to be honest, with Documentation i had help by Gemini Pro 3.1. The Code ist is if not marked with a Comment all made by myself! I love Programming in C and on hardware but i have problems with Documentations.

This was my First Try at making an Operating System, but for right now a Kernel and i had a lot of fun, even if most code was fine code for prototyping.

# What was my Initial Goal?

My Initial goal was to make a Commercial operating system from scratch and learning how to make something that i am deeply interested in, but since it too longer time with Documentations and i want to try to get my life going, i had not much time anymore to work on this. 

But as a Portfolio i hope its good enough. 


# Where does it Start?
It Starts in Lily, Lily is a Pre-Stage "added" onto the Kernel and planned to be later removed after Kernel initialised. It starts with `start.asm` and goes to the `lily_main.c` and lily makes preperations, after it it goes back to `start.asm` and then goes to `kernel.c`in the Kernel Source Directory.


# What can it do right now?

It starts on 64-Bit and Can go Through the Device Tree Blob (DTB), can make Paging (Sv39) and uses virtuell Addresses. It also has a Bsaic Memory controller and Allocator and a system für Driver initializing through the Device Tree. On QEMU it prints all of what it does to the Terminal

# What does Vera Stands for?

To be honest, i don't know anymore, But i like it even without knowing the Original Longer name of it.


# How can i run it?
Its currently made with MacOS and not tested on Linux or Windows, but if everything works, you only need to run
```bash
./debug.sh
```
and it should work on its own. 

# Note on Code Quality
This project started alongside my journey of learning C. It was a personal sandbox and learning environment. While some parts are meant for rapid prototyping, it reflects my genuine problem-solving process on a bare-metal level.


# Will this Continue?

Probably not? If i get the time then for sure, but otherwise unlikely (even i really would want to continue).