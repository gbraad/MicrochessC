# MicrochessC
Peter Jennings' Microchess on your PC by Bill Forster


### Play Kim-1 Microchess on your PC
[Bill Forster](http://www.triplehappy.com/) of Wellington, New Zealand, was so intrigued by Microchess that he decided to create a version in C that would run on the PC (or any computer capable of compiling C programs). His approach to the problem was to emulate the 6502 op codes as C macros, so that the original Kim-1 source would be compiled into C instructions.

In the process of debugging his code, he discovered errors in the source files available on the web due to OCR errors when they were converted from paper to text documents. Finally, after much effort, he created a version of the program that feels a lot like what a hobbyist in 1977 would have created for his 6502 homebrew computer with a teletype output.

```
  00 01 02 03 04 05 06 07
-------------------------
|BR|BN|BB|BQ|BK|BB|BN|BR|00
-------------------------
|BP|BP|BP|BP|**|BP|BP|BP|10
-------------------------
|  |**|  |**|  |**|  |**|20
-------------------------
|**|  |**|  |BP|  |**|  |30
-------------------------
|  |**|  |**|WP|**|  |**|40
-------------------------
|**|  |**|  |**|  |**|  |50
-------------------------
|WP|WP|WP|WP|  |WP|WP|WP|60
-------------------------
|WR|WN|WB|WQ|WK|WB|WN|WR|70
-------------------------
 00 01 02 03 04 05 06 07
```

Download the [C source and a Windows console application](https://www.benlo.com/microchess/ForsterMicrochessC.zip) executable version of the program if you would like to play with it on your PC.

It is fascinating that twenty years on, the spirit of the early days of personal computing is still inspiring some of us to play with our computers in the same ways we did back then.

From: https://www.benlo.com/microchess/
