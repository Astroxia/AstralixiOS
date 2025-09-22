# AstralixiOS Documentation 

## Contents
**1. Introduction**  
**2. Getting Started**  
**3. Basic Commands**  
**4. Full Command List**  
**5. Common Mistakes**  
**6. Conclusion and What Next?**  

## Introduction
So, I'm guessing that you have AstralixiOS installed and you are confused about what to do, or you are looking forward to getting started with AstralixiOS. If that's the case, you've come to the right place! In this documentation, we will learn how to download, flash, and set up the OS, as well as some useful commands. Near the end, we will cover some common mistakes that beginners might make, but I recommend reading through the whole documentation before going there. So, why don't we jump right in?  

## Getting Started
To get AstralixiOS running on your PicoCalc, it's actually VERY simple, even though it looks complicated. First of all, you will need to head to the AstralixiOS GitHub (where you most likely found this documentation) and locate a section called "Releases". Open that, and find the latest release to download. Click on the file, hit download, and wait patiently. The next step is for you to get a micro USB-C cable, something thin and pointy (like a needle), and obviously, your PicoCalc. Plug one end of the cable into your laptop/computer and, while plugging in the other end to your PicoCalc, use the pointy object to hold down the BOOTSEL button on your Pico. If done correctly, the Pico should appear as a drive on your computer. If not, try again, and if you continue to experience issues, reach out for help on the ClockworkPi forums or reddit. Once the drive appears, drag and drop the AstralixiOS .uf2 file into the drive and unplug the micro USB-C cable from your PicoCalc. Now, your PicoCalc will have the OS on it, but that's just the beginning!  

The next step is the setup, which can be a confusing process, but you will learn a lot from it. When you turn your PicoCalc on with the OS, it will display "Welcome to Astralixi OS", and then a login prompt will appear. This is where most people get confused. Before choosing your own username and password, note that the default username and password are both "admin", so type that in for both fields, and you've successfully navigated through the most challenging part of the setup.  

You will be presented with a command prompt, but now that we have started with this great OS, we can begin learning some basic commands!  

## Basic Commands
These basic commands will be the ones you use most frequently if you are an average user or a beginner. There is a list of 11 commands, ranging from very basic to still basic, but kind of not!?  

1. **"hello"**  
   The Hello Command, when used, will say hello back to you.  

2. **"exit"**  
   This exits the Command Prompt, but I don't recommend doing it...  

3. **"help"**  
   This provides a list of all the basic and intermediate commands.  

4. **"time" and "settime"**  
   This prints out the time, and you can also set the time to your current time.  
   *currently time isn't counted, so if you set it as 16:00 it will always be 16:00*

5. **"whoami"**  
   This will print out your username. 

6. **"usernm"**
   This is to change your username.

7. **"passwd"**  
   This is to change your password.  

8. **"cls"**
   This command is for clearing the screen.

9. **"uname"**
   Not a very useful command, but is good for testing if you have setup Astralixi correctly.

10. **"memory"**
   Will show memory in use and remaining.

11. **"pico"**
   This will show stats about the raspberry pi pico 2W board.


## Full Command List (Other Than Basic Commands)
This will be a list of some advanced commands, ranging from intermediate to slightly more intermediate...

1. **"pwd"**
   To print the current working directory.

2. **"ls"**
   To list the files you have on your SD card.

3. **"history"**
   Prints the last 25 commands in order, that you have used.

4. **"tempcheck"**
   Prints onboard sensor temperature in Celcius and Farenheit.

5. **"echo [parameter]"**
   This command is to print text to the screen.

6. **"cd"**
   To change the current working directory.

7. **"reboot"**
   To restart the main chip (which restarts the OS).

8. **"suspend"**
   To go into a low power mode and turn screen off (once in, hit enter/return to go back.)

9. **"open file"**
   Is used to read the contents of a file and print it out to the screen.

10. **"mkdir"**
   To create a directory inside of your current working directory.

11. **"rmdir"**
   To delete a directory inside of your current working directory.

12. **"mk file"**
   To create a new file inside of your current working directory.

13. **"rm file"** 
   To delete an existing file inside of your current working directory.

14. **"wifi disable"** 
   Disables the wifi chip (saves a bit of power)

15. **"wifi enable"** 
   Enables/turns on the wifi chip.

16. **"wifi scan"** 
   Scan for nearby networks and get basic information about them.

17. **"rn"**
   Rename file/directory, two parameters, current name and new name.

18. **"mv"**
   Moves file/directory, two paraeters, name and destination.


## Common Mistakes
A very common mistake made by a beginner, is to not know what the username and password is when they first startup AstralixiOS.
So, I'm going to give it to you straight. The default username and password is admin for both. You can change it later using the
usernm and passwd command, and it will save the credentials to a secure hidden file, so that on next startup, you won't have
to use the default credentials. Here are some more common mistakes made by people:

1. **"why doesn't ... command work???"**
   This is incredibly common, as many commands in AstralixiOS seem like they don't work, but really, they actually do. This is because
   of a kernel backend system which loads after the first command is used, so most commands will work, when used as the first after startup,
   but a few won't because of a kernel system built-in. So if you encounter this issue, just try the same command again, and if that doesn't 
   work, reboot your PicoCalc. If you still find problems from there, contact the people on the forums.

2. **Using the exit command**
   This command should almost never be used, if what you are doing on the PicoCalc is important, and unsaved. The "exit" command just exits
   out of the command line, and results you with a frozen cursor. From here you can't even type a command such as "reboot" or anything, so 
   you are resorted to restart the PicoCalc manually.

3. **"Why has my screen frozen?"**
   This issue can be either due to using the exit command, or because you have used all of your memory. Memory is the SRAM on your Pico2W
   board, you have 520kb of SRAM, and the operating system takes about 64kb of it. If you have somehow filled up the rest, that means that 
   you would have to use the clear command, and it will clear as much memory as possible, while also clearing the screen. If you can't even 
   type a command, manually restart the PicoCalc.


## Conclusion and What Next?
If you have read through all of this documentation, good job! If you found something that this documentation is lacking, such as information 
for something specific, or something like that, feel free to contact Astrox, either on the forums or on reddit.
If you are a beginner and looking for what to do next, first look at some of the youtube videos on the Astrox channel 
(https://youtube.com/@astroxia) and see what you can find there, and if you want more from there, look at some posts on the Astralixi Pico
OS Megathread or look at some posts from Astrox's Reddit (https://www.reddit.com/u/Astrox_YT/).

Thanks for reading!