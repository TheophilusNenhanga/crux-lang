let list1 = [0, 1];
let last_index = 1;
let max_index = 65535-1;


let duration = 10;
let time_start = time_s();
while time_s() - time_start < duration {
    array_add(list1, list1[last_index-1] + list1[last_index]);
    last_index = last_index + 1;
    if last_index >= max_index {
        break;
    }
}
let time_end = time_s();
print(list1);
print("time taken = ");
print(time_end - time_start);

