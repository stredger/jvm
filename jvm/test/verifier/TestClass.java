
class TestClass {

    public int add(int a, int b) {
	int c = 0;
	c = a + b;
	return c;
    }

    public static void main(String args[]) {
	int a = 1;
	int b = 2;
	TestClass t = new TestClass();
	int c = t.add(a, b);
    }
}