__kernel void multiply(global const float *a, global const float *b, global float *c, int n, global int *doub, int lnum)
{
	int row = get_global_id(0);
	int col = get_global_id(1);
        unsigned int i;
        unsigned int product = 0;

        for(int g = 0; g < lnum; g++)
           doub[g] = 1;
           // doub[g] = 0xffffffff;

/*	if (row < n && col < n) {
            for (i = 0; i < n; i++) 
                 product += a[row * n + i] * b[i* n + col]; 
	    c[row*n + col] = product;
	}
	*/
}
