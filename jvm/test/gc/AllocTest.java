

class AllocTest {

    public static void main(String[] args) {

	//System.out.println("Hello World!");
    String d = "Garbage";
    d = "Not Garbage";
	System.gc();

    }

}