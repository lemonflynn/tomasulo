Tomasulo
========

An application to simulate Tomasulo's algorithm

This is intended to be an exercise to understand and implement the Tomasulo algorithm to dynamically schedule instructions in an out of order fashion.

```
CK 6, PC 5
#+BEGIN_SRC
  0   MULD  F1     F2     F3
  1     LD  R1    234     R0
  2    ADD  R2     R1    234
  3    SUB  R3     R4    456
  4   MULD  F3     F4     F1
> 5     SD  F1   3321     R0
#+END_SRC
** Instruction status
|------------------------+---------+------------+---------|
| Instruction            | Issue   | Exec comp  | Write   |
|------------------------+---------+------------+---------|
|  MULD  F1     F2     F3|       1 |          0 |       0 |
|    LD  R1    234     R0|       2 |          5 |       6 |
|   ADD  R2     R1    234|       3 |          0 |       0 |
|   SUB  R3     R4    456|       4 |          6 |       0 |
|  MULD  F3     F4     F1|       5 |          0 |       0 |
|------------------------+---------+------------+---------|
** Reorder buffer 
|----------+--------+--------+--------+--------+--------+---------|
| Entry | Busy |     Instruction    |     state    | Dest | Value |
|     0 | Yes  | MULD  F1   F2   F1 |         busy |  F1  |   -   |
|     1 | Yes  |   LD  R1  234   F1 | result ready |  R1  | 0.000 |
|     2 | Yes  |  ADD  R2   R1   F1 |         busy |  R2  |   -   |
|     3 | Yes  |  SUB  R3   R4   F1 |         busy |  R3  |   -   |
|     4 | Yes  | MULD  F3   F4   F1 |         busy |  F3  |   -   |
|----------+--------+--------+--------+--------+--------+---------|
** Reservation station
|----------+--------+--------+--------+--------+--------+----+----+-----|
| Name     | Busy   | Addr.  | Op     | Vj     | Vk     | Qj | Qk | Dst |
|----------+--------+--------+--------+--------+--------+----+----+-----|
| INT_ADD0 |      1 |      0 |    ADD |      0 |    234 | -1 | -1 |   2 |
| INT_ADD1 |      1 |      0 |    SUB |    999 |    456 | -1 | -1 |   3 |
| FLT_MUL0 |      1 |      0 |   MULD | 0.2000 | 0.8000 | -1 | -1 |   0 |
| FLT_MUL1 |      1 |      0 |   MULD | 2.7000 | 0.0000 | -1 |  0 |   4 |
|----------+--------+--------+--------+--------+--------+----+----+-----|
** Register result status (Qi)
| R1: 1 | R2: 2 | R3: 3 |
| F1: 0 | F3: 4 |
** Register file
| R0: 1 | R4: 999 |
| F2: 0.200000 | F3: 0.800000 | F4: 2.700000 |
```
