0:	PUSH	666
1:	STORE	0
2:	PUSH	777
3:	STORE	1
4:	PUSH	0
5:	STORE	2
6:	PUSH	1
7:	PUSH	3
8:	PUSH	5
9:	PUSH	3
10:	STORE	2
11:	PUSH	0
12:	LOAD	2
13:	COMPARE	0
14:	JUMP_YES	23
15:	LOAD	2
16:	PUSH	1
17:	SUB
18:	STORE	2
19:	STORE	3
20:	LOAD	3
21:	PRINT
22:	JUMP	11
23:	NOP
24:	STOP
