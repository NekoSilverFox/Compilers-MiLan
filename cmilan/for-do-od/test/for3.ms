0:	PUSH	666
1:	STORE	0
2:	PUSH	777
3:	STORE	1
4:	PUSH	0
5:	STORE	2
6:	LOAD	0
7:	LOAD	1
8:	MULT
9:	LOAD	1
10:	PUSH	3
11:	ADD
12:	LOAD	1
13:	LOAD	0
14:	ADD
15:	PUSH	3
16:	STORE	2
17:	PUSH	0
18:	LOAD	2
19:	COMPARE	0
20:	JUMP_YES	29
21:	LOAD	2
22:	PUSH	1
23:	SUB
24:	STORE	2
25:	STORE	3
26:	LOAD	3
27:	PRINT
28:	JUMP	17
29:	NOP
30:	STOP
