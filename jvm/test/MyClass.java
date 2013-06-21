
class MyClass {
    static int fact(int n) {
	int res;
	for (res = 1; n > 0; n--) res = res * n;
	return res;
    }
}