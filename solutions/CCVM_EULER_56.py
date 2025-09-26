def digit_sum(number):
    total = 0
    for char in str(number):
        total += int(char)
    return total
 
def main():
    max = 0
    for i in range(1, 100):
        for j in range(1, 100):
            power = i ** j
            current_sum = digit_sum(power)
            if current_sum > max:
                max = current_sum
    print(max)


if __name__ == "__main__":
    main()

