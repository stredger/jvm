
class ntest {

    public void tt(int a, float b, long c, double d) {
	
	a = 50;
	b = 1.1f;
	c = 555552222;
	d = 12334535674785687.3452345123;

	return;
    }

    public void ttt(){
	byte a = 0x01;
	short b = 1;
    }

    public long lll(int a, int b, int c, int d, int e, int f, int g, int h) {
	long la = 0;
	long lb = 1;
	long lc = 2;
	long ld = 3;
	long le = 4;
	a = 1;
	b ++;
	c += 1;
	d += c;
	float fa = 0f;
	fa += 5.5f;
	g = (int) la;
	return le;
    }



    public static void dddd(double i, double j, long k) {
	i = 0;
	j = 60;
	k = 1;
    }

    /*
    public static void ds(double a, double b) {
	a = b;
    }


    
    public static double cast(int i, float f, double d, long l) {
	i = (int) f;
        i = (int) d;
	f = (float) i;
	f = (float) d;
	d = (double) i;
	d = (double) l;
	l = (long) i;
	l = (long) d;
	return d;
    }
    */

    public int cmp(int i, int j, int f, double d) {
	if (i > j) {
	    j = i;
	} else if (j > i) {
	    i = j;
	} else if (i == j) {
	    i++;
	}
	if ( i != j ) {
	    j++;
	}
	
	if ( d > f ) {
	    d++;
	}

	return i;
    }

    public static void main(String[] args) {

	return;
    }
}