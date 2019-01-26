__kernel void multiply(global int *h_a, global int *result, global int *test)
{
    *test = 1;
    *result = *h_a;
}
