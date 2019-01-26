__kernel void vadd(global const float *a, global const float *b, global float *c, int n)
{
	int i = get_global_id(0);
	int j = get_global_id(1);

	if (i < n && j < n) {
                int idx = i * n + j;
		c[idx] = a[i] + b[j];
	}
}
