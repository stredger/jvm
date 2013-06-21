README



Issues:
  - Array Syntax:
      - There is different formats everywhere 
            LL vs. L vs. J
            DD vs. D
            A[ALMyClass vs. A[LMyClass
            java.lang.String vs. java/lang/String;
  
      - In the assignment specs, it states the use of 
            A[I
            A[L
            A[D
            A[A[Ljava.lang.String
      - In the values taken from the constant pool (multi-dimensional arrays), we get 
            [[I                     which we turn into    A[A[I
            [[[J                    which we turn into    A[A[A[J
            [[[D                    which we turn into    A[A[A[D
            [[Ljava/lang/String;    which we turn into    A[A[Ljava/lang/String;
      - In the initialization (i.e. arguments passed into a method), we get
            A[I
            A[Ll
            A[Dd
            A[A[ALjava/lang/String