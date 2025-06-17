#define D3 3
#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define D8 8
#define D9 9
#define D10 10
#define D11 11
#define D12 12
#define D13 13

/*
Mapping of ECLIPSE7 modified keyboard, in the 5x5 keyboard matrix
The Button number is created as (row * 5 + column)
Note that all Trim increment buttons are in row 0 and
all trim decrement buttons in row 1. I did noy use it in the current
code version but it may be helpful.

(Row, Column) ButtonNumber  Function
(0,1)         1             Throttle Trim Up
(1,1)         6             Throttle Trim Down
(1,0)         5             Rudder Trim Left
(0,0)         0             Rudder Trim Right
(0,2)         2             Pitch Trim Up
(1,2)         7             Pitch Trim Down
(1,3)         8             Roll Trim Left
(0,3)         3             Roll Trim Right
(3,0)         15            Edit Up
(3,1)         16            Edit Down
(3,2)         17            Cursor Left
(3,3)         18            Cursor Right
(2,1)         11            Data Increase
(2,0)         10            Data Decrease
(2,2)         12            Clear
(4,4)         24            Engine Lock
(2,3)         13            Engine Cut

Adding the 8 buttons of the extender, all buttons are presented below in numerical order

Button vJoy
code    btn
0              Rudder Trim Right
1              Throttle Trim Up
2              Pitch Trim Up
3              Roll Trim Right
4              -
5              Rudder Trim Left
6              Throttle Trim Down
7              Pitch Trim Down
8              Roll Trim Left
9              -
10   1         Data Decrease
11   2         Data Increase
12   3         Clear
13   4         Engine Cut
14             -
15   6         Edit Up
16   7         Edit Down
17   8         Cursor Left
18   9         Cursor Right
19             -
20             -
21             -
22             -
23             -
24  15         Engine Lock
25  16         Ch. 7
26  17         Flt. Cond.
27  18         Ail. D/R
28  19         Gear
29  20         Flt. Mode UP
30  21         Flt. Mode Down
31  22         Ele D/R
32  23         Rudd D/R



*/