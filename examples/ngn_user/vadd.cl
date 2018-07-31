__kernel void vadd(global const float *a, global const float *b, global float *c, int n)
{
	int i = get_global_id(0);

	if (i < n) {
		c[i] = a[i] + b[i];
	}
}
