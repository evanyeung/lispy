# Lispy

Lispy is an implementation of a Lisp following the book found at http://www.buildyourownlisp.com/.

To compile and run, ensure you have cloned https://github.com/orangeduck/mpc as a submodule. You may then compile with
```
$ cc -std=c99 -Wall lispy.c mpc/mpc.c -ledit -lm -o lispy
```
on Mac and Linux, or
```
$ cc -std=c99 -Wall lispy.c mpc.c -o lispy
```
on Windows.

Enter the REPL with `$ ./lispy`. Scripts may also be run by including a filename, `$ ./lispy hello.lspy`.

Standard functions and utilities are included in `prelude.lspy`. This file may be included using the `load` function. e.g. `(load "prelude.lspy")`.

## Hello World
```
(print "Hello, World!")
```

## Built-in Functions
```
(+ 1 1) // 2
(- 1 1) // 0
(* 2 3) // 6
(/ 6 3) // 2
(% 5 2) // 1

(== 1 1) // 1
(== 1 2) // 0
(!= "Hello" "World") // 1
(> 3 1) // 1
(>= 3 3) // 1
(< 4 2) // 0
(<= 4 5) // 1

(list 1 2 3 4) // {1 2 3 4}
(head {"a" "b" "c"}) // {"a"}
(tail {"a" "b" "c"}) // {"b" "c"}
(join {1 2} {3 4}) // {1 2 3 4}
(eval {+ 4 4}) // 8

(print "hello") // "hello"
(error "UH OH") // Error: "UH OH"

(load "prelude.lspy") // ()

(def {x} 1) // () - NOTE: global scope
(def {l} 2) // () - NOTE: local scope
(print x) // 1
(print l) // 2
(def {y greet} 1 "hello") // ()
(print y) // 1
(print greet) // "hello"
(+ x y) // 2

(\ {x y} {+ x y}) // \ {x y} {+ x y}
((\ {x y} {+ x y}) 10 20) // 30
(def {add-two-nums} (\ {x y} {+ x y}))
(add-two-nums 1 2) // 3
```

## Included in Prelude
```
(true) // 1 
(false) // 0 
(nil) // {}

fun {mul-two-nums x y} {* x y}) // ()
(mul-two-nums 3 4) // 12
(unpack + {1 2 3}) // 6
(curry * {1 2 3}) // 6
(pack head {1 2 3}) // {1}
(uncurry tail {1 2 3}) // {2 3}

(do (def {x} "hello") (print x)) // "hello"

(let {do (= {m} 100) (m)})
(print m) // Error: Symbol 'm' not defined.

(not true) // 0
(or true false) // 1
(and true false) // 0

((flip def) 1 {x}) // ()
(print x) // 1
(ghost - 5 4) // 1

(fun {add-one x} {+ x 1}) // ()
(fun {add-two x} {+ x 2}) // ()
(comp add-one add-two 5) // 8

(fst {1 2 3}) // 1
(snd {1 2 3}) // 2
(trd {1 2 3}) // 3
(nth 4 {"a" "b" "c" "d" "e" "f"}) // "e"

(last {"a" "b" "c" "d"}) // "d"
(take 3 {"a" "b" "c" "d" "e" "f"}) // {"a" "b" "c"}
(drop 3 {"a" "b" "c" "d" "e" "f"}) // {"d" "e" "f"}
(split 3 {"a" "b" "c" "d" "e" "f"}) // {{"a" "b" "c"} {"d" "e" "f"}}

(elem "b" {"a" "b" "c" "d" "e" "f"}) // 1
(elem "g" {"a" "b" "c" "d" "e" "f"}) // 0

(map - {1 2 3}) // {-1 -2 -3}
(map (\ {x} {+ x 1}) {1 2 3}) // {2 3 4}
(filter (\ {x} {>= x 3}) {1 2 3 4 5}) // {3 4 5}
(foldl * 1 {1 2 3 4}) // 24

(fun {month-day-suffix i} {
  select
    {(== i 0)  "st"}
    {(== i 1)  "nd"}
    {(== i 3)  "rd"}
    {otherwise "th"}
}) // ()
(fun {day-name x} {
  case x
    {0 "Monday"}
    {1 "Tuesday"}
    {2 "Wednesday"}
    {3 "Thursday"}
    {4 "Friday"}
    {5 "Saturday"}
    {6 "Sunday"}
}) // ()

(fib 5) // 5
(fib 8) // 21
```
