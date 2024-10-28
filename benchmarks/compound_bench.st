
fn testCompoundOperator(n) {
    let sum = 0;
    let start = time_ms();
    for let i = 0; i < n; i = i + 1 {
        sum += 1;
        sum *= 4;
        sum /= 2;
        sum -= 2;
    }
    let end = time_ms();
    return end - start;
}

fn testBasicAssignment(n) {
    let sum = 0;
    let start = time_ms();
    for let i = 0; i < n; i = i + 1 {
        sum = sum + 1;
        sum = sum * 4;
        sum = sum / 2;
        sum = sum - 2;
    }
    let end = time_ms();
    return end - start;
}

let iterations = 100000000;
let compoundTime = testCompoundOperator(iterations);
let assignmentTime = testBasicAssignment(iterations);

print("Compound operator time:");
print(compoundTime);
print("Basic assignment time:");
print(assignmentTime);

if compoundTime < assignmentTime {
    print("Compound operators are faster.");
} else if assignmentTime < compoundTime {
    print("Basic assignment is faster.");
} else {
    print("Both methods took the same time.");
}
