# Stella Lang

A memory managed, interpreted programming language.

## Features

- Dynamic typing
- Garbage collection
- First-class functions
- Object-oriented programming with classes
- Built-in data structures (arrays and tables)
- Lexical scoping
- Closures
- Native functions for core operations

## Installation

### Windows 

```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../src
cmake --build . --config Release
```

### Linux 

```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../src
make
```

## Language Guide

### Variables and Data types

```stella
// Variable declaration
let name = "Stella";
let age = 25;
let isActive = true;
let nothing = nil;

// Numbers
let integer = 42;
let float = 3.14;

// Arithmetic
let sum = 5 + 3;
let difference = 10 - 4;
let product = 6 * 7;
let quotient = 15 / 3;
let modulus = 7 % 3;

// Bitwise operations
let leftShift = 1 << 2;  // 4
let rightShift = 8 >> 1; // 4
```

### Control Flow

```stella
// If statements
if condition {
    println("True");
} else {
    println("False");
}

// While loops
let i = 0;
while i < 5 {
    println(i);
    i = i + 1;
}

// For loops
for let i = 0; i < 5; i = i + 1 {
    println(i);
}
```

### Functions

```stella
// Function declaration
fn add(a, b) {
    return a + b;
}

// Functions are first-class
let multiply = fn(a, b) {
    return a * b;
};

// Closures
fn makeCounter() {
    let count = 0;
    return fn() {
        count = count + 1;
        return count;
    };
}
```

### Data Structures

```stella
// Arrays
let numbers = [1, 2, 3, 4, 5];
array_add(numbers, 6);       // Add element
array_rem(numbers);          // Remove last element
let length = len(numbers);   // Get length
let first = numbers[0];      // Array indexing

// Tables (Hash Maps)
let person = {
    "name": "John",
    "age": 30,
    "city": "New York"
};
person["age"] = 31;          // Update value
let name = person["name"];   // Access value
```

### Classes and Objects

```stella
class Animal {
    fn init(name) {
        self.name = name;
    }

    fn speak() {
        println(self.name + " makes a sound");
    }
}

class Dog < Animal {
    fn speak() {
        println(self.name + " barks");
    }
}

let dog = Dog("Rex");
dog.speak();  // "Rex barks"
```

### Built-in functions


- `print()` - Print without newline
- `println()` - Print with newline
- `len()`  - Length of a collection 
- `time_s()` - Time in seconds     
- `time_ms()` - Time in milliseconds 

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `and`, `or`, `not`
- Bitwise: `<<`, `>>`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`
