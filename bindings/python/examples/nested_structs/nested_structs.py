import librpc


def main():
    t = librpc.Typing()
    t.load_types_dir('.')
    Struct1 = librpc.build('com.test.Struct1')
    Struct2 = librpc.build('com.test.Struct2')

    s1 = Struct1(
        number1=1,
        number2=2
    )
    s2 = Struct2(
        string='test',
        struct=s1
    )

    s1.number1 = 10
    print(repr(s1))
    print(repr(s2))
    print("Test passed" if s1.number1 == s2.struct.number1 else "Test failed")


if __name__ == '__main__':
    main()