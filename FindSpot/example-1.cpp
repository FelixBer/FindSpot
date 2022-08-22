
#include <iostream>


volatile int vx;

void __attribute__ ((noinline)) r0()
{
	vx = 0;
	std::cout << "call r0\n";
}

void __attribute__ ((noinline)) r1()
{
	vx = 1;
	std::cout << "call r1\n";
}

void __attribute__ ((noinline)) r2()
{
	vx = 2;
	std::cout << "call r2\n";
}

void __attribute__ ((noinline)) r3()
{
	vx = 3;
}






int main()
{
	std::cout << std::hex << "r0: " << (void*)&r0 << std::endl;
	std::cout << std::hex << "r1: " << (void*)&r1 << std::endl;
	std::cout << std::hex << "r2: " << (void*)&r2 << std::endl;
	std::cout << std::hex << "r3: " << (void*)&r3 << std::endl;
	int choice = 0;
	while(1)
	{
		std::cout << "choose: ";
		std::cin.clear();
		std::cin >> choice;
		std::cin.clear();
		std::cout << "choice: " << choice << std::endl;
		if(choice == 5)
			r1();
		if(choice == 6)
			r2();
		r3();
		r3();
	}
}
