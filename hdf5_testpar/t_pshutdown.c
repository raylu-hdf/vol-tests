/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Programmer:  Mohamad Chaarawi
 *              February 2015
 *
 * Purpose: This test creates a file and a bunch of objects in the
 * file and then calls MPI_Finalize without closing anything. The
 * library should exercise the attribute callback destroy attached to
 * MPI_COMM_SELF and terminate the HDF5 library closing all open
 * objects. The t_prestart test will read back the file and make sure
 * all created objects are there.
 */

#include "hdf5.h"
#include "testphdf5.h"

int nerrors = 0;			/* errors count */

const char *FILENAME[] = {
    "shutdown.h5",
    NULL
};

uint64_t vol_cap_flags;

int
main (int argc, char **argv)
{
    hid_t       file_id, dset_id, grp_id;
    hid_t       fapl, sid, mem_dataspace;
    hsize_t     dims[RANK], i;
    herr_t      ret;
    int         mpi_size, mpi_rank;
    MPI_Comm    comm  = MPI_COMM_WORLD;
    MPI_Info    info  = MPI_INFO_NULL;
    hsize_t     start[RANK];
    hsize_t     count[RANK];
    hsize_t     stride[RANK];
    hsize_t     block[RANK];
    DATATYPE   *data_array = NULL;	/* data buffer */

    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &mpi_size);
    MPI_Comm_rank(comm, &mpi_rank);  

    if(MAINPROCESS) {
        printf("Testing %-62s", "proper shutdown of HDF5 library");
        fflush(stdout);
    }
 
    /* Set up file access property list with parallel I/O access */
    fapl = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl >= 0), "H5Pcreate succeeded");

    /* Get the capability flag of the VOL connector being used */
    ret = H5Pget_vol_cap_flags(fapl, &vol_cap_flags);
    VRFY((ret >= 0), "H5Pget_vol_cap_flags succeeded");

    /* Make sure the connector supports the API functions being tested */
    if (!(vol_cap_flags & H5VL_CAP_FLAG_FILE_BASIC) || !(vol_cap_flags & H5VL_CAP_FLAG_GROUP_BASIC) ||
        !(vol_cap_flags & H5VL_CAP_FLAG_DATASET_BASIC)) {
        if(MAINPROCESS) {
            puts("SKIPPED");
            HDprintf("    API functions for basic file, group, or dataset aren't supported with this connector\n");
            fflush(stdout);
        }

        MPI_Finalize();
        return 0;
    }

    ret = H5Pset_fapl_mpio(fapl, comm, info);
    VRFY((ret >= 0), "");

    /* h5_fixname(FILENAME[0], fapl, filename, sizeof filename); */
    file_id = H5Fcreate(FILENAME[0], H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    VRFY((file_id >= 0), "H5Fcreate succeeded");
    grp_id = H5Gcreate2(file_id, "Group", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((grp_id >= 0), "H5Gcreate succeeded");

    dims[0] = (hsize_t)(ROW_FACTOR*mpi_size);
    dims[1] = (hsize_t)(COL_FACTOR*mpi_size);
    sid = H5Screate_simple (RANK, dims, NULL);
    VRFY((sid >= 0), "H5Screate_simple succeeded");

    dset_id = H5Dcreate2(grp_id, "Dataset", H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    VRFY((dset_id >= 0), "H5Dcreate succeeded");

    /* allocate memory for data buffer */
    data_array = (DATATYPE *)HDmalloc(dims[0]*dims[1]*sizeof(DATATYPE));
    VRFY((data_array != NULL), "data_array HDmalloc succeeded");

    /* Each process takes a slabs of rows. */
    block[0] = dims[0]/(hsize_t)mpi_size;
    block[1] = dims[1];
    stride[0] = block[0];
    stride[1] = block[1];
    count[0] = 1;
    count[1] = 1;
    start[0] = (hsize_t)mpi_rank*block[0];
    start[1] = 0;

    /* put some trivial data in the data_array */
    for(i=0 ; i<dims[0]*dims[1]; i++)
        data_array[i] = mpi_rank + 1;

    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret >= 0), "H5Sset_hyperslab succeeded");

    /* create a memory dataspace independently */
    mem_dataspace = H5Screate_simple (RANK, block, NULL);
    VRFY((mem_dataspace >= 0), "");

    /* write data independently */
    ret = H5Dwrite(dset_id, H5T_NATIVE_INT, mem_dataspace, sid,
                   H5P_DEFAULT, data_array);
    VRFY((ret >= 0), "H5Dwrite succeeded");

    /* release data buffers */
    if(data_array) 
        HDfree(data_array);

    MPI_Finalize();

    /* nerrors += GetTestNumErrs(); */

    if(MAINPROCESS) {
        if(0 == nerrors)
            {puts(" PASSED"); fflush(stdout);}
        else
            {puts("*FAILED*"); fflush(stdout);}
    }

    return (nerrors!=0);
}
