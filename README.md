Tomasulo
========

An application to simulate Tomasulo's algorithm

This is intended to be an exercise to understand and implement the Tomasulo algorithm to dynamically schedule instructions in an out of order fashion.

```
CK 4, PC 4
#+BEGIN_SRC
  0     LD  R1    234     R0
  1    ADD  R2     R1    234
  2    SUB  R3     R4    456
  3   MULD  F1     F2     F3
> 4     SD  F1   3321     R0
  5   ADDD  F3     F4     F1
#+END_SRC
** Instruction status
|------------------------+---------+------------+---------|
| Instruction            | Issue   | Exec comp  | Write   |
|------------------------+---------+------------+---------|
|    LD  R1    234     R0|       1 |          4 |       0 |
|   ADD  R2     R1    234|       2 |          0 |       0 |
|   SUB  R3     R4    456|       3 |          0 |       0 |
|  MULD  F1     F2     F3|       4 |          0 |       0 |
|------------------------+---------+------------+---------|
** Reservation station
|----------+--------+--------+--------+--------+--------+--------+--------|
| Name     | Busy   | Addr.  | Op     | Vj     | Vk     | Qj     | Qk     |
|----------+--------+--------+--------+--------+--------+--------+--------|
| FLT_MUL0 |      1 |      0 |   MULD | 0.2000 | 0.8000 |      - |      - |
| INT_ADD0 |      1 |      0 |    ADD |      0 |    234 |    LD0 |      - |
| INT_ADD1 |      1 |      0 |    SUB |    999 |    456 |      - |      - |
|      LD0 |      1 |    235 |     LD |      1 |      0 |      - |      - |
|----------+--------+--------+--------+--------+--------+--------+--------|
** Register result status (Qi)
| R1: LD0 | R2: INT_ADD0 | R3: INT_ADD1 |
| F1: FLT_MUL0 |
** Register file
| R0: 1 | R4: 999 |
 F2: 0.200000 | F3: 0.800000 | F4: 2.700000 |
```
