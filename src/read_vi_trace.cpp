#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "read_vi_trace.h"

int load_vi_data(vi_trace* vi, char* filename)
{
    float v;
    float a;

    char outstr [100];
    FILE* handle;
    handle = fopen (filename,"r");

    if (handle==NULL)
        return -1;

    fgets (outstr, 100, handle);

    int cnt = 0;
    while ( fgets(outstr, 100, handle) != NULL)
    {
        sscanf (outstr, "%e%e", &a, &v);
        cnt++;
    }
    rewind(handle);
    fgets (outstr, 100, handle); //pitch first row

    vi->volt = (float*) malloc(cnt*sizeof(float));
    vi->amp = (float*) malloc(cnt*sizeof(float));
    vi->cnt = cnt;

    for(int i=0; i < cnt; i++)
    {
        fgets(outstr, 100, handle);
        sscanf (outstr, "%e%e", &a, &v);
        vi->volt[i] = v;
        vi->amp[i] = a;

        if(i == 0)
        {
            vi->maxamp = a;
            vi->minamp = a;
        }

        if(a > vi->maxamp)
            vi->maxamp = a;
        if (a < vi->minamp)
            vi->minamp = a;
    }

    vi->di = ((float) (cnt-1))/(vi->maxamp - vi->minamp);

    fclose(handle);
    return 0;
}

void
vi_trace_cleanup(vi_trace* vi)
{
	free(vi->volt);
	free(vi->amp);
}
