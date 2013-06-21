class atest {

    public static void main(String[] args) {
      int[] i = new int[5];
      double[] d = new double[4];
      long[] l = new long[3];
      String[] s = new String[2];
      
      int[][] ii = new int[3][3];
      double[][][] ddd = new double[2][2][2];
      long[][][] lll = new long[2][2][2];
      String[][]ssss = new String[1][1];
      
      i[0] = 0;
      d[0] = 0.0;
      l[0] = 1234;
      s[0] = "hello";
      
      String t = s[0];
      
      MyClass[][] m = new MyClass[5][3];
      
    }
    
    static int[] ret_i (int[] i, double[] d, long[] l, String[][] s) {
      return i; 
    }
    
    static double[] return_d (double[] d) {
      return d;
    }
}

class MyClass {

}