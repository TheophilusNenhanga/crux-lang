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

### Multiple Return Values and Unpacking

Stella supports returning multiple values from functions and unpacking them into variables. Note that values are returned in reverse order of how they are written in the return statement.

#### Basic Multiple Returns
```stella
// Function returning multiple values (will return 3, 2, 1)
let numbers = fn() {
    return 1, 2, 3;
};

// Unpacking multiple values
let a, b, c = numbers();
println(a);  // 3
println(b);  // 2
println(c);  // 1

// To get values in natural order, either:
// 1. Return them in reverse:
let naturalOrder = fn() {
    return 3, 2, 1;  // Will be received as 1, 2, 3
};

// 2. Or unpack in reverse order:
let c, b, a = numbers();  // a=1, b=2, c=3
```

#### Working with Multiple Returns in Nested Functions

When working with nested function calls and multiple returns, keep in mind the reverse ordering:

```stella
// Example of nested function calls with multiple returns
let nested = fn() {
    let inner = fn() {
        return 1, 2, 3;  // Will be returned as 3, 2, 1
    };
    let x, y, z = inner();  // x=3, y=2, z=1
    return x + 1, y + 1, z + 1;  // Will return 2, 3, 4
};

// Values will be received in reverse order
let a, b, c = nested();  // a=4, b=3, c=2
```

#### Using Multiple Returns with Expressions

Multiple returns work with expressions and computations:

```stella
// Multiple returns with expressions
let withExpr = fn() {
    return 1 + 1, 2 * 2, 3 + 3;  // Returns 6, 4, 2
};

// Conditional returns
let conditional = fn(x) {
    if (x) {
        return 1, 2;  // Returns 2, 1
    }
    return 3, 4, 5;  // Returns 5, 4, 3
};
```

#### Best Practices

1. When order matters, either:
    - Write return values in reverse order
    - Unpack variables in reverse order
    - Document the expected order clearly in your code

2. When working with nested functions, be mindful of the reverse ordering at each level

3. When using multiple returns in computations, consider writing intermediate values to variables for clarity:
```stella
// Clear and maintainable
let complex = fn() {
    let x, y, z = someFunction();
    let a = x + 1;
    let b = y * 2;
    let c = z - 3;
    return a, b, c;
};

// Rather than
let complex = fn() {
    let x, y, z = someFunction();
    return x + 1, y * 2, z - 3;  // Harder to predict ordering
};
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
