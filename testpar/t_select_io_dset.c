/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 *
 * Purpose:     Test selection I/O
 */

#include "h5test.h"
#include "testpar.h"

#define FILENAME "pselect_io.h5"

/* MPI variables */
int mpi_size;
int mpi_rank;

/* Number of errors */
int nerrors      = 0;
int curr_nerrors = 0;

#define P_TEST_ERROR                                                                                         \
    do {                                                                                                     \
        nerrors++;                                                                                           \
        H5_FAILED();                                                                                         \
        AT();                                                                                                \
    } while (0)

#define CHECK_PASSED()                                                                                       \
    do {                                                                                                     \
        int err_result = (nerrors > curr_nerrors);                                                           \
                                                                                                             \
        MPI_Allreduce(MPI_IN_PLACE, &err_result, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);                       \
                                                                                                             \
        if (MAINPROCESS) {                                                                                   \
            if (err_result == 0)                                                                             \
                PASSED();                                                                                    \
            else                                                                                             \
                HDputs("     ***TEST FAILED***");                                                            \
        }                                                                                                    \
    } while (0)

/*
 * Test configurations
 */
typedef enum {
    TEST_NO_TYPE_CONV,           /* no type conversion (null case) */
    TEST_NO_SIZE_CHANGE_NO_BKG,  /* no size change, no bkg buffer */
    TEST_LARGER_MEM_NO_BKG,      /* larger memory type, no bkg buffer */
    TEST_SMALLER_MEM_NO_BKG,     /* smaller memory type, no bkg buffer */
    TEST_CMPD_WITH_BKG,          /* compound types with bkg buffer */
    TEST_TYPE_CONV_SEL_EMPTY,    /* some processes have null/empty selections and with type conversion */
    TEST_MULTI_CONV_NO_BKG,      /* multi dataset test 1 */
    TEST_MULTI_CONV_BKG,         /* multi dataset test 2 */
    TEST_MULTI_CONV_SIZE_CHANGE, /* multi dataset test 3 */
    TEST_MULTI_CONV_SEL_EMPTY,   /* multi dataset test 4 */
    TEST_MULTI_ALL,              /* multi dataset test 5 */
    TEST_SELECT_NTESTS
} test_select_config_t;

#define DSET_SELECT_DIM       100
#define DSET_SELECT_CHUNK_DIM 10

#define DSET_CONTIG_NO_CONV_NTRANS "contig_no_conv_ntrans"
#define DSET_CONTIG_NO_CONV_TRANS  "contig_no_conv_trans"
#define DSET_CHK_NO_CONV_NTRANS    "chunked_no_conv_ntrans"
#define DSET_CHK_NO_CONV_TRANS     "chunked_no_conv_trans"

#define DSET_CONTIG_NO_SIZE_CHANGE_NO_BKG  "contig_no_size_change_no_bkg"
#define DSET_CHUNKED_NO_SIZE_CHANGE_NO_BKG "chunked_no_size_change_no_bkg"

#define DSET_CONTIG_LARGER_NO_BKG_NTRANS "contig_larger_mem_no_bkg_ntrans"
#define DSET_CONTIG_LARGER_NO_BKG_TRANS  "contig_larger_mem_no_bkg_trans"
#define DSET_CHK_LARGER_NO_BKG_NTRANS    "chunked_larger_mem_no_bkg_ntrans"
#define DSET_CHK_LARGER_NO_BKG_TRANS     "chunked_larger_mem_no_bkg_trans"

#define DSET_CONTIG_SMALLER_NO_BKG_NTRANS "contig_smaller_mem_no_bkg_ntrans"
#define DSET_CONTIG_SMALLER_NO_BKG_TRANS  "contig_smaller_mem_no_bkg_trans"
#define DSET_CHK_SMALLER_NO_BKG_NTRANS    "chk_smaller_mem_no_bkg_ntrans"
#define DSET_CHK_SMALLER_NO_BKG_TRANS     "chk_smaller_mem_no_bkg_trans"

#define DSET_CONTIG_CMPD_WITH_BKG  "contig_cmpd_with_bkg"
#define DSET_CHUNKED_CMPD_WITH_BKG "chunked_cmpd_with_bkg"

#define DSET_CONTIG_TCONV_SEL_EMPTY_NTRANS "contig_tconv_sel_ntrans"
#define DSET_CONTIG_TCONV_SEL_EMPTY_TRANS  "contig_tconv_sel_trans"
#define DSET_CHK_TCONV_SEL_EMPTY_NTRANS    "chunked_tconv_sel_ntrans"
#define DSET_CHK_TCONV_SEL_EMPTY_TRANS     "chunked_tconv_sel_trans"

#define MULTI_NUM_DSETS 3
#define MULTI_MIN_DSETS 3
#define DSET_NAME_LEN   35

/* Compound type */
typedef struct s1_t {
    int a;
    int b;
    int c;
    int d;
} s1_t;

/*
 * Variation of s1 with:
 * --no conversion for 2 member types
 * --1 larger mem type,
 * --1 smaller mem type
 */
typedef struct s2_t {
    int       a;
    long long b;
    int       c;
    short     d;
} s2_t;

/* Variation of s1: reverse of s1_t */
typedef struct s3_t {
    int d;
    int c;
    int b;
    int a;
} s3_t;

/* Variations of s1: only 2 members in s1_t */
typedef struct s4_t {
    unsigned int b;
    unsigned int d;
} s4_t;

/* Defines for test_multi_dsets_all() */
typedef enum {
    DSET_WITH_NO_CONV,         /* Dataset with no type conversion */
    DSET_WITH_CONV_AND_NO_BKG, /* Dataset with type conversion but no background buffer */
    DSET_WITH_CONV_AND_BKG,    /* Dataset with type conversion and background buffer */
    DSET_NTTYPES
} multi_dset_type_t;

/* Test setting A and B */
#define SETTING_A 1
#define SETTING_B 2

/*
 * Helper routine to set dxpl
 * --selection I/O mode
 * --type of I/O
 * --type of collective I/O
 */
static void
set_dxpl(hid_t dxpl, H5D_selection_io_mode_t select_io_mode, H5FD_mpio_xfer_t mpio_type,
         H5FD_mpio_collective_opt_t mpio_coll_opt)
{
    if (H5Pset_selection_io(dxpl, select_io_mode) < 0)
        P_TEST_ERROR;

    if (H5Pset_dxpl_mpio(dxpl, mpio_type) < 0)
        P_TEST_ERROR;

    if (H5Pset_dxpl_mpio_collective_opt(dxpl, mpio_coll_opt) < 0)
        P_TEST_ERROR;

} /* set_dxpl() */

/*
 * Helper routine to check actual I/O mode on a dxpl
 */
static void
check_io_mode(hid_t dxpl, unsigned chunked)
{
    H5D_mpio_actual_io_mode_t actual_io_mode = H5D_MPIO_NO_COLLECTIVE;

    if (H5Pget_mpio_actual_io_mode(dxpl, &actual_io_mode) < 0)
        P_TEST_ERROR;

    if (chunked) {
        if (actual_io_mode != H5D_MPIO_CHUNK_COLLECTIVE) {
            nerrors++;
            if (MAINPROCESS)
                HDprintf("\n     Failed: Incorrect I/O mode (expected chunked, returned %u)",
                         (unsigned)actual_io_mode);
        }
    }
    else if (actual_io_mode != H5D_MPIO_CONTIGUOUS_COLLECTIVE) {
        nerrors++;
        if (MAINPROCESS)
            HDprintf("\n     Failed: Incorrect I/O mode (expected contiguous, returned %u)",
                     (unsigned)actual_io_mode);
    }

} /* check_io_mode() */

/*
 *  Case 1: single dataset read/write, no type conversion (null case)
 */
static void
test_no_type_conv(hid_t fid, unsigned chunked, unsigned dtrans)
{
    int         i;
    hid_t       did         = H5I_INVALID_HID;
    hid_t       sid         = H5I_INVALID_HID;
    hid_t       dcpl        = H5I_INVALID_HID;
    hid_t       dxpl        = H5I_INVALID_HID;
    hid_t       ntrans_dxpl = H5I_INVALID_HID;
    hid_t       fspace_id   = H5I_INVALID_HID;
    hid_t       mspace_id   = H5I_INVALID_HID;
    hsize_t     dims[1];
    hsize_t     cdims[1];
    hsize_t     start[1], stride[1], count[1], block[1];
    int         wbuf[DSET_SELECT_DIM];
    int         trans_wbuf[DSET_SELECT_DIM];
    int         rbuf[DSET_SELECT_DIM];
    const char *expr = "2*x";

    curr_nerrors = nerrors;

    /* Create 1d data space */
    dims[0] = DSET_SELECT_DIM;
    if ((sid = H5Screate_simple(1, dims, NULL)) < 0)
        P_TEST_ERROR;

    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;

        /* Create 1d chunked dataset with/without data transform */
        if ((did = H5Dcreate2(fid, dtrans ? DSET_CHK_NO_CONV_TRANS : DSET_CHK_NO_CONV_NTRANS, H5T_NATIVE_INT,
                              sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }
    else {

        /* Create 1d contiguous dataset with/without data transform */
        if ((did = H5Dcreate2(fid, dtrans ? DSET_CONTIG_NO_CONV_TRANS : DSET_CONTIG_NO_CONV_NTRANS,
                              H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    /* Initialize data */
    for (i = 0; i < (int)block[0]; i++) {
        wbuf[i]       = i + (int)start[0];
        trans_wbuf[i] = 2 * wbuf[i];
    }

    /* Create a memory dataspace */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;

    /* Create a file dataspace */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;

    if (H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    if ((ntrans_dxpl = H5Pcopy(dxpl)) < 0)
        P_TEST_ERROR;

    /* Set data transform */
    if (dtrans)
        if (H5Pset_data_transform(dxpl, expr) < 0)
            P_TEST_ERROR;

    /* Write data to the dataset with/without data transform */
    if (H5Dwrite(did, H5T_NATIVE_INT, mspace_id, fspace_id, dxpl, wbuf) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read data from the dataset (if dtrans, without data transform set in dxpl) */
    if (H5Dread(did, H5T_NATIVE_INT, mspace_id, fspace_id, ntrans_dxpl, rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read (if dtrans, verify data is transformed) */
    for (i = 0; i < (int)block[0]; i++)
        if (rbuf[i] != (dtrans ? trans_wbuf[i] : wbuf[i])) {
            nerrors++;
            HDprintf("\n     Error in first data verification:\n");
            HDprintf("     At index %d: %d, %d\n", i + (int)start[0], dtrans ? trans_wbuf[i] : wbuf[i],
                     rbuf[i]);
            break;
        }

    if (dtrans) {

        /* Read the data from the dataset with data transform set in dxpl */
        if (H5Dread(did, H5T_NATIVE_INT, mspace_id, fspace_id, dxpl, rbuf) < 0)
            P_TEST_ERROR;

        /* Verify data read is transformed a second time */
        for (i = 0; i < (int)block[0]; i++)
            if (rbuf[i] != (2 * trans_wbuf[i])) {
                nerrors++;
                HDprintf("\n     Error in second data verification:.\n");
                HDprintf("     At index %d: %d, %d\n", i + (int)start[0], 2 * trans_wbuf[i], rbuf[i]);
                break;
            }
    }

    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;

    if (H5Sclose(sid) < 0)
        P_TEST_ERROR;
    if (H5Dclose(did) < 0)
        P_TEST_ERROR;
    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(ntrans_dxpl) < 0)
        P_TEST_ERROR;

    CHECK_PASSED();

    return;
} /* test_no_type_conv() */

/*
 *  Case 2: single dataset read/write, no size change, no background buffer
 */
static void
test_no_size_change_no_bkg(hid_t fid, unsigned chunked)
{
    int     i;
    hid_t   did       = H5I_INVALID_HID;
    hid_t   sid       = H5I_INVALID_HID;
    hid_t   dcpl      = H5I_INVALID_HID;
    hid_t   dxpl      = H5I_INVALID_HID;
    hid_t   fspace_id = H5I_INVALID_HID;
    hid_t   mspace_id = H5I_INVALID_HID;
    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];
    char   *wbuf = NULL;
    char   *rbuf = NULL;

    curr_nerrors = nerrors;

    if ((wbuf = (char *)HDmalloc((size_t)(4 * DSET_SELECT_DIM))) == NULL)
        P_TEST_ERROR;
    if ((rbuf = (char *)HDmalloc((size_t)(4 * DSET_SELECT_DIM))) == NULL)
        P_TEST_ERROR;

    /* Create 1d data space */
    dims[0] = DSET_SELECT_DIM;
    if ((sid = H5Screate_simple(1, dims, NULL)) < 0)
        P_TEST_ERROR;
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
    }

    /* Create 1d dataset */
    if ((did =
             H5Dcreate2(fid, chunked ? DSET_CHUNKED_NO_SIZE_CHANGE_NO_BKG : DSET_CONTIG_NO_SIZE_CHANGE_NO_BKG,
                        H5T_STD_I32BE, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
        P_TEST_ERROR;

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    /* Initialize data */
    for (i = 0; i < (int)block[0]; i++) {
        wbuf[i * 4 + 3] = 0x1;
        wbuf[i * 4 + 2] = 0x2;
        wbuf[i * 4 + 1] = 0x3;
        wbuf[i * 4 + 0] = 0x4;
    }

    /* Create a memory dataspace independently */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;

    /* Create a file dataspace independently */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;

    if (H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    /* Write the data to the dataset with little endian */
    if (H5Dwrite(did, H5T_STD_I32LE, mspace_id, fspace_id, dxpl, wbuf) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read the data from the dataset with little endian */
    if (H5Dread(did, H5T_STD_I32LE, mspace_id, fspace_id, dxpl, rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    for (i = 0; i < (int)block[0]; i++) {
        if (rbuf[4 * i + 0] != wbuf[4 * i + 0] || rbuf[4 * i + 1] != wbuf[4 * i + 1] ||
            rbuf[4 * i + 2] != wbuf[4 * i + 2] || rbuf[4 * i + 3] != wbuf[4 * i + 3]) {
            nerrors++;
            HDprintf("\n     Error in data verification:\n");
            HDprintf("\n     Error in data verification at index %d\n", i + (int)start[0]);
            break;
        }
    }

    /* Read the data from the dataset with big endian */
    if (H5Dread(did, H5T_STD_I32BE, mspace_id, fspace_id, dxpl, rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    for (i = 0; i < (int)block[0]; i++) {
        if (rbuf[4 * i + 0] != wbuf[4 * i + 3] || rbuf[4 * i + 1] != wbuf[4 * i + 2] ||
            rbuf[4 * i + 2] != wbuf[4 * i + 1] || rbuf[4 * i + 3] != wbuf[4 * i + 0]) {
            nerrors++;
            HDprintf("\n     Error in data verification at index %d\n", i + (int)start[0]);
            break;
        }
    }

    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;

    if (H5Sclose(sid) < 0)
        P_TEST_ERROR;
    if (H5Dclose(did) < 0)
        P_TEST_ERROR;

    if (wbuf)
        HDfree(wbuf);

    if (rbuf)
        HDfree(rbuf);

    CHECK_PASSED();

    return;
} /* test_no_size_change_no_bkg() */

/*
 *  Case 3: single dataset read/write, larger mem type, no background buffer
 */
static void
test_larger_mem_type_no_bkg(hid_t fid, unsigned chunked, unsigned dtrans)
{
    int         i;
    hid_t       did         = H5I_INVALID_HID;
    hid_t       sid         = H5I_INVALID_HID;
    hid_t       dcpl        = H5I_INVALID_HID;
    hid_t       dxpl        = H5I_INVALID_HID;
    hid_t       ntrans_dxpl = H5I_INVALID_HID;
    hid_t       fspace_id   = H5I_INVALID_HID;
    hid_t       mspace_id   = H5I_INVALID_HID;
    hsize_t     dims[1];
    hsize_t     cdims[1];
    hsize_t     start[1], stride[1], count[1], block[1];
    long        wbuf[DSET_SELECT_DIM];
    long        trans_wbuf[DSET_SELECT_DIM];
    long long   rbuf[DSET_SELECT_DIM];
    const char *expr = "100 - x";

    curr_nerrors = nerrors;

    /* Create 1d data space */
    dims[0] = DSET_SELECT_DIM;
    if ((sid = H5Screate_simple(1, dims, NULL)) < 0)
        P_TEST_ERROR;
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
        /* Create 1d chunked dataset with/without data transform */
        if ((did = H5Dcreate2(fid, dtrans ? DSET_CHK_LARGER_NO_BKG_TRANS : DSET_CHK_LARGER_NO_BKG_NTRANS,
                              H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }
    else {
        /* Create 1d contiguous dataset with/without data transform */
        if ((did =
                 H5Dcreate2(fid, dtrans ? DSET_CONTIG_LARGER_NO_BKG_TRANS : DSET_CONTIG_LARGER_NO_BKG_NTRANS,
                            H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    /* Initialize data */
    for (i = 0; i < (int)block[0]; i++) {
        wbuf[i]       = i + (int)start[0];
        trans_wbuf[i] = 100 - wbuf[i];
    }

    /* Create a memory dataspace */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;

    /* Create a file dataspace */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;

    if (H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    if ((ntrans_dxpl = H5Pcopy(dxpl)) < 0)
        P_TEST_ERROR;

    /* Set data transform */
    if (dtrans)
        if (H5Pset_data_transform(dxpl, expr) < 0)
            P_TEST_ERROR;

    /* Write data to the dataset with/without data transform set in dxpl */
    if (H5Dwrite(did, H5T_NATIVE_LONG, mspace_id, fspace_id, dxpl, wbuf) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read data from the dataset (if dtrans, without data transform set in dxpl) */
    if (H5Dread(did, H5T_NATIVE_LLONG, mspace_id, fspace_id, ntrans_dxpl, rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read (if dtrans, verify data is transformed) */
    for (i = 0; i < (int)block[0]; i++)
        if (rbuf[i] != (long long)(dtrans ? trans_wbuf[i] : wbuf[i])) {
            nerrors++;
            HDprintf("\n     Error in first data verification:\n");
            HDprintf("     At index %d: %lld, %lld\n", i + (int)start[0],
                     (long long)(dtrans ? trans_wbuf[i] : wbuf[i]), rbuf[i]);
            break;
        }

    if (dtrans) {

        /* Read data from the dataset with data transform set in dxpl */
        if (H5Dread(did, H5T_NATIVE_LLONG, mspace_id, fspace_id, dxpl, rbuf) < 0)
            P_TEST_ERROR;

        /* Verify data read is transformed a second time */
        for (i = 0; i < (int)block[0]; i++)
            if (rbuf[i] != (long long)(100 - trans_wbuf[i])) {
                nerrors++;
                HDprintf("\n     Error in second data verification:.\n");
                HDprintf("     At index %d: %lld, %lld\n", i + (int)start[0],
                         (long long)(100 - trans_wbuf[i]), rbuf[i]);
                break;
            }
    }
    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(sid) < 0)
        P_TEST_ERROR;
    if (H5Dclose(did) < 0)
        P_TEST_ERROR;
    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(ntrans_dxpl) < 0)
        P_TEST_ERROR;

    CHECK_PASSED();

    return;

} /* test_larger_mem_type_no_bkg() */

/*
 *  Case 4: single dataset reader/write, smaller mem type, no background buffer
 */
static void
test_smaller_mem_type_no_bkg(hid_t fid, unsigned chunked, unsigned dtrans)
{
    int         i;
    hid_t       did         = H5I_INVALID_HID;
    hid_t       sid         = H5I_INVALID_HID;
    hid_t       dcpl        = H5I_INVALID_HID;
    hid_t       dxpl        = H5I_INVALID_HID;
    hid_t       ntrans_dxpl = H5I_INVALID_HID;
    hid_t       fspace_id   = H5I_INVALID_HID;
    hid_t       mspace_id   = H5I_INVALID_HID;
    hsize_t     dims[1];
    hsize_t     cdims[1];
    hsize_t     start[1], stride[1], count[1], block[1];
    short       wbuf[DSET_SELECT_DIM];
    short       trans_wbuf[DSET_SELECT_DIM];
    short       rbuf[DSET_SELECT_DIM];
    const char *expr = "2 * (10 + x)";

    curr_nerrors = nerrors;

    /* Create 1d data space */
    dims[0] = DSET_SELECT_DIM;
    if ((sid = H5Screate_simple(1, dims, NULL)) < 0)
        P_TEST_ERROR;
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;

        /* Create 1d chunked dataset with/without data transform */
        if ((did = H5Dcreate2(fid, dtrans ? DSET_CHK_SMALLER_NO_BKG_TRANS : DSET_CHK_SMALLER_NO_BKG_NTRANS,
                              H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }
    else {

        /* Create 1d contiguous dataset with/without data transform */
        if ((did = H5Dcreate2(fid,
                              dtrans ? DSET_CONTIG_SMALLER_NO_BKG_TRANS : DSET_CONTIG_SMALLER_NO_BKG_NTRANS,
                              H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    /* Initialize data */
    for (i = 0; i < (int)block[0]; i++) {
        wbuf[i]       = (short)(i + (int)start[0]);
        trans_wbuf[i] = (short)(2 * (10 + wbuf[i]));
    }

    /* Create a memory dataspace */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;

    /* Create a file dataspace */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;

    if (H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    if ((ntrans_dxpl = H5Pcopy(dxpl)) < 0)
        P_TEST_ERROR;

    /* Set data transform */
    if (dtrans) {
        if (H5Pset_data_transform(dxpl, expr) < 0)
            P_TEST_ERROR;
    }

    /* Write data to the dataset with/without data transform in dxpl */
    if (H5Dwrite(did, H5T_NATIVE_SHORT, mspace_id, fspace_id, dxpl, wbuf) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read data from the dataset (if dtrans, without data transform set in dxpl) */
    if (H5Dread(did, H5T_NATIVE_SHORT, mspace_id, fspace_id, ntrans_dxpl, rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read (if dtrans, verify data is transformed) */
    for (i = 0; i < (int)block[0]; i++)
        if (rbuf[i] != (dtrans ? trans_wbuf[i] : wbuf[i])) {
            nerrors++;
            HDprintf("\n     Error in first data verification:\n");
            HDprintf("     At index %d: %d, %d\n", i + (int)start[0], wbuf[i], rbuf[i]);
            break;
        }

    if (dtrans) {

        /* Read data from the dataset with data transform set in dxpl */
        if (H5Dread(did, H5T_NATIVE_SHORT, mspace_id, fspace_id, dxpl, rbuf) < 0)
            P_TEST_ERROR;

        /* Verify data read is transformed a second time */
        for (i = 0; i < (int)block[0]; i++)
            if (rbuf[i] != (2 * (10 + trans_wbuf[i]))) {
                nerrors++;
                HDprintf("\n     Error in second data verification:.\n");
                HDprintf("     At index %d: %d, %d\n", i + (int)start[0], (2 * (10 - trans_wbuf[i])),
                         rbuf[i]);
                break;
            }
    }

    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;

    if (H5Sclose(sid) < 0)
        P_TEST_ERROR;
    if (H5Dclose(did) < 0)
        P_TEST_ERROR;
    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(ntrans_dxpl) < 0)
        P_TEST_ERROR;

    CHECK_PASSED();

    return;

} /* test_smaller_mem_type_no_bkg() */

/*
 *  Case 5: single dataset reade/write, compound types with background buffer
 *
 *  (a) Initialize compound buffer in memory with unique values
 *      Write all compound fields to disk
 *      Verify values read
 *  (b) Update all fields of the compound type in memory write buffer with new unique values
 *      Write some but not all all compound fields to disk
 *      Read the entire compound type
 *      Verify the fields have the correct (old or new) values
 *  (c) Update all fields of the compound type in memory read buffer with new unique values
 *      Read some but not all the compound fields to memory
 *      Verify the fields have the correct (old, middle  or new) values
 *  (d) Set up a different compound type which has:
 *      --no conversion for member types
 *      --a field with larger mem type
 *      --a field with smaller mem type
 *      Write this compound type to disk
 *      Read the entire compound type
 *      Verify the values read
 */
static void
test_cmpd_with_bkg(hid_t fid, unsigned chunked)
{
    int     i;
    hid_t   did       = H5I_INVALID_HID;
    hid_t   sid       = H5I_INVALID_HID;
    hid_t   dcpl      = H5I_INVALID_HID;
    hid_t   dxpl      = H5I_INVALID_HID;
    hid_t   s1_tid    = H5I_INVALID_HID;
    hid_t   s2_tid    = H5I_INVALID_HID;
    hid_t   ss_ac_tid = H5I_INVALID_HID;
    hid_t   ss_bc_tid = H5I_INVALID_HID;
    hid_t   fspace_id = H5I_INVALID_HID;
    hid_t   mspace_id = H5I_INVALID_HID;
    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];
    s1_t   *s1_wbuf = NULL;
    s1_t   *s1_rbuf = NULL;
    s2_t   *s2_wbuf = NULL;
    s2_t   *s2_rbuf = NULL;

    curr_nerrors = nerrors;

    /* Allocate buffers for datasets */
    if (NULL == (s1_wbuf = (s1_t *)HDmalloc(sizeof(s1_t) * DSET_SELECT_DIM)))
        P_TEST_ERROR;
    if (NULL == (s1_rbuf = (s1_t *)HDmalloc(sizeof(s1_t) * DSET_SELECT_DIM)))
        P_TEST_ERROR;
    if (NULL == (s2_wbuf = (s2_t *)HDmalloc(sizeof(s2_t) * DSET_SELECT_DIM)))
        P_TEST_ERROR;
    if (NULL == (s2_rbuf = (s2_t *)HDmalloc(sizeof(s2_t) * DSET_SELECT_DIM)))
        P_TEST_ERROR;

    /* Create the memory data type */
    if ((s1_tid = H5Tcreate(H5T_COMPOUND, sizeof(s1_t))) < 0)
        P_TEST_ERROR;

    if (H5Tinsert(s1_tid, "a", HOFFSET(s1_t, a), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s1_tid, "b", HOFFSET(s1_t, b), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s1_tid, "c", HOFFSET(s1_t, c), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s1_tid, "d", HOFFSET(s1_t, d), H5T_NATIVE_INT) < 0)
        P_TEST_ERROR;

    /* Create 1d data space */
    dims[0] = DSET_SELECT_DIM;
    if ((sid = H5Screate_simple(1, dims, NULL)) < 0)
        P_TEST_ERROR;
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
    }

    /* Case 5(a) */

    /* Create 1d dataset */
    if ((did = H5Dcreate2(fid, chunked ? DSET_CHUNKED_CMPD_WITH_BKG : DSET_CONTIG_CMPD_WITH_BKG, s1_tid, sid,
                          H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
        P_TEST_ERROR;

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    /* Initialize data */
    for (i = 0; i < (int)block[0]; i++) {
        s1_wbuf[i].a = 4 * (i + (int)start[0]);
        s1_wbuf[i].b = 4 * (i + (int)start[0]) + 1;
        s1_wbuf[i].c = 4 * (i + (int)start[0]) + 2;
        s1_wbuf[i].d = 4 * (i + (int)start[0]) + 3;
    }

    /* Create a memory dataspace */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;

    /* Create a file dataspace */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;

    if (H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    /* Write all the data to the dataset */
    if (H5Dwrite(did, s1_tid, mspace_id, fspace_id, dxpl, s1_wbuf) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read all the data from the dataset */
    if (H5Dread(did, s1_tid, mspace_id, fspace_id, dxpl, s1_rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    for (i = 0; i < (int)block[0]; i++)
        if (s1_wbuf[i].a != s1_rbuf[i].a || s1_wbuf[i].b != s1_rbuf[i].b || s1_wbuf[i].c != s1_rbuf[i].c ||
            s1_wbuf[i].d != s1_rbuf[i].d) {
            nerrors++;
            HDprintf("\n     Error in 1st data verification:\n");
            HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", i + (int)start[0], s1_wbuf[i].a,
                     s1_rbuf[i].a, s1_wbuf[i].b, s1_rbuf[i].b, s1_wbuf[i].c, s1_rbuf[i].c, s1_wbuf[i].d,
                     s1_rbuf[i].d);
            break;
        }

    /* Case 5(b) */

    /* Update s1_wbuf with unique values */
    for (i = 0; i < (int)block[0]; i++) {
        s1_wbuf[i].a = 4 * (i + (int)start[0]) + DSET_SELECT_DIM;
        s1_wbuf[i].b = 4 * (i + (int)start[0]) + DSET_SELECT_DIM + 1;
        s1_wbuf[i].c = 4 * (i + (int)start[0]) + DSET_SELECT_DIM + 2;
        s1_wbuf[i].d = 4 * (i + (int)start[0]) + DSET_SELECT_DIM + 3;
    }

    /* Create a compound type same size as s1_t */
    if ((ss_ac_tid = H5Tcreate(H5T_COMPOUND, sizeof(s1_t))) < 0)
        P_TEST_ERROR;

    /* but contains only subset members of s1_t */
    if (H5Tinsert(ss_ac_tid, "a", HOFFSET(s1_t, a), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(ss_ac_tid, "c", HOFFSET(s1_t, c), H5T_NATIVE_INT) < 0)
        P_TEST_ERROR;

    /* Write s1_wbuf to the dataset but with only subset members in ss_tid */
    if (H5Dwrite(did, ss_ac_tid, mspace_id, fspace_id, dxpl, s1_wbuf) < 0)
        P_TEST_ERROR;

    /* Read the whole compound back */
    if (H5Dread(did, ss_ac_tid, mspace_id, fspace_id, dxpl, s1_rbuf) < 0)
        P_TEST_ERROR;

    /* Verify the compound fields have the correct (old or new) values */
    for (i = 0; i < (int)block[0]; i++)
        if (s1_rbuf[i].a != s1_wbuf[i].a || s1_rbuf[i].b != (4 * (i + (int)start[0]) + 1) ||
            s1_rbuf[i].c != s1_wbuf[i].c || s1_rbuf[i].d != (4 * (i + (int)start[0]) + 3)) {
            nerrors++;
            HDprintf("\n     Error in 2nd data verification:\n");
            HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", i + (int)start[0], s1_wbuf[i].a,
                     s1_rbuf[i].a, (4 * (i + (int)start[0]) + 1), s1_rbuf[i].b, s1_wbuf[i].c, s1_rbuf[i].c,
                     (4 * (i + (int)start[0]) + 3), s1_rbuf[i].d);
            break;
        }

    /* Case 5(c) */

    /* Update s1_rbuf with new unique values */
    for (i = 0; i < (int)block[0]; i++) {
        s1_rbuf[i].a = (4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM);
        s1_rbuf[i].b = (4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM) + 1;
        s1_rbuf[i].c = (4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM) + 2;
        s1_rbuf[i].d = (4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM) + 3;
    }

    /* Create a compound type same size as s1_t */
    if ((ss_bc_tid = H5Tcreate(H5T_COMPOUND, sizeof(s1_t))) < 0)
        P_TEST_ERROR;

    /* but contains only subset members of s1_t */
    if (H5Tinsert(ss_bc_tid, "b", HOFFSET(s1_t, b), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(ss_bc_tid, "c", HOFFSET(s1_t, c), H5T_NATIVE_INT) < 0)
        P_TEST_ERROR;

    /* Read the dataset: will read only what is set in ss_bc_tid */
    if (H5Dread(did, ss_bc_tid, mspace_id, fspace_id, dxpl, s1_rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    for (i = 0; i < (int)block[0]; i++)
        if (s1_rbuf[i].a != ((4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM)) ||
            s1_rbuf[i].b != (4 * (i + (int)start[0]) + 1) ||
            s1_rbuf[i].c != (4 * (i + (int)start[0]) + DSET_SELECT_DIM + 2) ||
            s1_rbuf[i].d != ((4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM) + 3)) {
            nerrors++;
            HDprintf("\n     Error in 3rd data verification:\n");
            HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", i + (int)start[0],
                     ((4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM)), s1_rbuf[i].a,
                     (4 * (i + (int)start[0]) + 1), s1_rbuf[i].b,
                     (4 * (i + (int)start[0]) + DSET_SELECT_DIM + 2), s1_rbuf[i].c,
                     ((4 * (i + (int)start[0])) + (2 * DSET_SELECT_DIM) + 3), s1_rbuf[i].d);
            break;
        }

    /* Case 5(d) */

    /* Create s2_t compound type with:
     * --no conversion for 2 member types,
     * --1 larger mem type
     * --1 smaller mem type
     */
    if ((s2_tid = H5Tcreate(H5T_COMPOUND, sizeof(s2_t))) < 0)
        P_TEST_ERROR;

    if (H5Tinsert(s2_tid, "a", HOFFSET(s2_t, a), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s2_tid, "b", HOFFSET(s2_t, b), H5T_NATIVE_LLONG) < 0 ||
        H5Tinsert(s2_tid, "c", HOFFSET(s2_t, c), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s2_tid, "d", HOFFSET(s2_t, d), H5T_NATIVE_SHORT) < 0)
        P_TEST_ERROR;

    /* Update s2_wbuf with unique values */
    for (i = 0; i < (int)block[0]; i++) {
        s2_wbuf[i].a = (8 * (i + (int)start[0]));
        s2_wbuf[i].b = (long long)(8 * (i + (int)start[0]) + 1);
        s2_wbuf[i].c = (8 * (i + (int)start[0]) + 2);
        s2_wbuf[i].d = (short)(8 * (i + (int)start[0]) + 3);
    }
    if (H5Dwrite(did, s2_tid, mspace_id, fspace_id, dxpl, s2_wbuf) < 0)
        P_TEST_ERROR;

    /* Read it back */
    if (H5Dread(did, s2_tid, mspace_id, fspace_id, dxpl, s2_rbuf) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    for (i = 0; i < (int)block[0]; i++)
        if (s2_wbuf[i].a != s2_rbuf[i].a || s2_wbuf[i].b != s2_rbuf[i].b || s2_wbuf[i].c != s2_rbuf[i].c ||
            s2_wbuf[i].d != s2_rbuf[i].d) {
            nerrors++;
            HDprintf("\n     Error in 4th data verification:\n");
            HDprintf("     At index %d: %d/%d, %lld/%lld, %d/%d, %d/%d\n", i + (int)start[0], s2_wbuf[i].a,
                     s2_rbuf[i].a, s2_wbuf[i].b, s2_rbuf[i].b, s2_wbuf[i].c, s2_rbuf[i].c, s2_wbuf[i].d,
                     s2_rbuf[i].d);
            break;
        }

    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;

    if (H5Sclose(sid) < 0)
        P_TEST_ERROR;
    if (H5Tclose(s1_tid) < 0)
        P_TEST_ERROR;
    if (H5Tclose(s2_tid) < 0)
        P_TEST_ERROR;
    if (H5Tclose(ss_ac_tid) < 0)
        P_TEST_ERROR;
    if (H5Tclose(ss_bc_tid) < 0)
        P_TEST_ERROR;
    if (H5Dclose(did) < 0)
        P_TEST_ERROR;

    /* Release buffers */
    HDfree(s1_wbuf);
    HDfree(s1_rbuf);
    HDfree(s2_wbuf);
    HDfree(s2_rbuf);

    CHECK_PASSED();

    return;

} /* test_cmpd_with_bkg() */

/*
 *  Case 6: Type conversions + some processes have null/empty selections in datasets
 */
static void
test_type_conv_sel_empty(hid_t fid, unsigned chunked, unsigned dtrans)
{
    int     i;
    hid_t   did         = H5I_INVALID_HID;
    hid_t   sid         = H5I_INVALID_HID;
    hid_t   dcpl        = H5I_INVALID_HID;
    hid_t   dxpl        = H5I_INVALID_HID;
    hid_t   ntrans_dxpl = H5I_INVALID_HID;
    hid_t   fspace_id   = H5I_INVALID_HID;
    hid_t   mspace_id   = H5I_INVALID_HID;
    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];

    long      lwbuf[DSET_SELECT_DIM];
    long      trans_lwbuf[DSET_SELECT_DIM];
    long      lrbuf[DSET_SELECT_DIM];
    short     srbuf[DSET_SELECT_DIM];
    short     swbuf[DSET_SELECT_DIM];
    short     trans_swbuf[DSET_SELECT_DIM];
    long long llrbuf[DSET_SELECT_DIM];

    const char *expr = "2*x";

    curr_nerrors = nerrors;

    /* Create 1d data space */
    dims[0] = DSET_SELECT_DIM;
    if ((sid = H5Screate_simple(1, dims, NULL)) < 0)
        P_TEST_ERROR;
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
        /* Create 1d chunked dataset with/without data transform */
        if ((did = H5Dcreate2(fid, dtrans ? DSET_CHK_TCONV_SEL_EMPTY_TRANS : DSET_CHK_TCONV_SEL_EMPTY_NTRANS,
                              H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }
    else {
        /* Create 1d contiguous dataset with/without data transform */
        if ((did = H5Dcreate2(fid,
                              dtrans ? DSET_CONTIG_TCONV_SEL_EMPTY_TRANS : DSET_CONTIG_TCONV_SEL_EMPTY_NTRANS,
                              H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    if ((ntrans_dxpl = H5Pcopy(dxpl)) < 0)
        P_TEST_ERROR;

    /* Set data transform */
    if (dtrans) {
        if (H5Pset_data_transform(dxpl, expr) < 0)
            P_TEST_ERROR;
    }

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    /* Initialize data */
    for (i = 0; i < (int)block[0]; i++) {
        lwbuf[i]       = i + (int)start[0];
        trans_lwbuf[i] = 2 * lwbuf[i];
    }

    /* Case 6(a) process 0: hyperslab; other processes: select none */

    /* Create a file dataspace */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;
    if (MAINPROCESS) {
        if (H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }
    else {
        if (H5Sselect_none(fspace_id) < 0)
            P_TEST_ERROR;
    }

    /* Create a memory dataspace */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;
    if (mpi_rank) {
        if (H5Sselect_none(mspace_id) < 0)
            P_TEST_ERROR;
    }

    /* Write data to the dataset with/without data transform in dxpl */
    if (H5Dwrite(did, H5T_NATIVE_LONG, mspace_id, fspace_id, dxpl, lwbuf) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read the data from the dataset: type conversion int-->long */
    /* If dtrans, without data transform set in dxpl */
    if (H5Dread(did, H5T_NATIVE_LONG, mspace_id, fspace_id, ntrans_dxpl, lrbuf) < 0)
        P_TEST_ERROR;

    if (MAINPROCESS) {
        for (i = 0; i < (int)block[0]; i++)
            if (lrbuf[i] != (dtrans ? trans_lwbuf[i] : lwbuf[i])) {
                nerrors++;
                HDprintf("\n     Error in first data verification:\n");
                HDprintf("     At index %d: %ld, %ld\n", i + (int)start[0],
                         dtrans ? trans_lwbuf[i] : lwbuf[i], lrbuf[i]);
                break;
            }
    }

    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;

    /* Case 6(b) process 0: get 0 row; other processes: hyperslab */

    block[0]  = mpi_rank ? (dims[0] / (hsize_t)mpi_size) : 0;
    stride[0] = mpi_rank ? block[0] : 1;
    count[0]  = 1;
    start[0]  = mpi_rank ? ((hsize_t)mpi_rank * block[0]) : 0;

    /* Create a file dataspace */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;

    if (H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Create a memory dataspace */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;

    /* need to make memory space to match for process 0 */
    if (MAINPROCESS) {
        if (H5Sselect_hyperslab(mspace_id, H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }

    /* Write data to the dataset with/without data transform */
    if (H5Dwrite(did, H5T_NATIVE_LONG, mspace_id, fspace_id, dxpl, lwbuf) < 0)
        P_TEST_ERROR;

    /* Read the data from the dataset: type conversion int-->short */
    /* If dtrans, without data transform set in dxpl */
    if (H5Dread(did, H5T_NATIVE_SHORT, mspace_id, fspace_id, ntrans_dxpl, srbuf) < 0)
        P_TEST_ERROR;

    if (mpi_rank) {
        for (i = 0; i < (int)block[0]; i++)
            if (srbuf[i] != (short)(dtrans ? trans_lwbuf[i] : lwbuf[i])) {
                HDprintf("\n     Error in second data verification:\n");
                HDprintf("     At index %d: %d, %d\n", i + (int)start[0],
                         (short)(dtrans ? trans_lwbuf[i] : lwbuf[i]), srbuf[i]);
                break;
            }
    }

    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;

    /* Case 6(c) process 0: select none; other processes: select all */

    /* Initialize data */
    block[0] = DSET_SELECT_DIM;
    for (i = 0; i < (int)block[0]; i++) {
        swbuf[i]       = (short)(i + DSET_SELECT_DIM);
        trans_swbuf[i] = (short)(2 * swbuf[i]);
    }

    /* Create a file dataspace */
    if ((fspace_id = H5Dget_space(did)) < 0)
        P_TEST_ERROR;
    if (MAINPROCESS) {
        if (H5Sselect_none(fspace_id) < 0)
            P_TEST_ERROR;
    }
    else {
        if (H5Sselect_all(fspace_id) < 0)
            P_TEST_ERROR;
    }

    /* Create a memory dataspace */
    if ((mspace_id = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;
    if (MAINPROCESS) {
        if (H5Sselect_none(mspace_id) < 0)
            P_TEST_ERROR;
    }

    /* Write data to the dataset with/without data transform */
    if (H5Dwrite(did, H5T_NATIVE_SHORT, mspace_id, fspace_id, dxpl, swbuf) < 0)
        P_TEST_ERROR;

    /* Read the data from the dataset: type conversion int-->llong */
    /* If dtrans, without data transform set in dxpl */
    if (H5Dread(did, H5T_NATIVE_LLONG, mspace_id, fspace_id, ntrans_dxpl, llrbuf) < 0)
        P_TEST_ERROR;

    if (mpi_rank) {
        for (i = 0; i < (int)block[0]; i++)
            if (llrbuf[i] != (long long)(dtrans ? trans_swbuf[i] : swbuf[i])) {
                HDprintf("\n     Error in third data verification:\n");
                HDprintf("     At index %d: %lld, %lld\n", i + (int)start[0],
                         (long long)(dtrans ? trans_swbuf[i] : swbuf[i]), llrbuf[i]);
                break;
            }
    }

    if (H5Sclose(mspace_id) < 0)
        P_TEST_ERROR;
    if (H5Sclose(fspace_id) < 0)
        P_TEST_ERROR;

    if (H5Sclose(sid) < 0)
        P_TEST_ERROR;
    if (H5Dclose(did) < 0)
        P_TEST_ERROR;
    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(ntrans_dxpl) < 0)
        P_TEST_ERROR;

    CHECK_PASSED();

    return;

} /* test_type_conv_sel_empty() */

/*
 *  Test 1 for multi-dataset:
 *  --Datasets with/without type conversion+smaller/larger mem type+no background buffer
 *
 *  Create datasets: randomized H5T_NATIVE_INT or H5T_NATIVE_LONG
 *
 *  Case a--setting for multi write/read to ndsets:
 *    Datatype for all datasets: H5T_NATIVE_INT
 *
 *  Case b--setting for multi write/read to ndsets:
 *    Datatype for all datasets: H5T_NATIVE_LONG
 */
static void
test_multi_dsets_no_bkg(hid_t fid, unsigned chunked, unsigned dtrans)
{
    size_t  ndsets;
    int     i, j;
    hid_t   dcpl        = H5I_INVALID_HID;
    hid_t   dxpl        = H5I_INVALID_HID;
    hid_t   ntrans_dxpl = H5I_INVALID_HID;
    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];

    hid_t file_sids[MULTI_NUM_DSETS];
    hid_t mem_sids[MULTI_NUM_DSETS];
    hid_t mem_tids[MULTI_NUM_DSETS];

    char  dset_names[MULTI_NUM_DSETS][DSET_NAME_LEN];
    hid_t dset_dids[MULTI_NUM_DSETS];

    size_t buf_size;

    int *total_wbuf       = NULL;
    int *total_trans_wbuf = NULL;
    int *total_rbuf       = NULL;

    long *total_lwbuf       = NULL;
    long *total_trans_lwbuf = NULL;
    long *total_lrbuf       = NULL;

    int *wbufi[MULTI_NUM_DSETS];
    int *trans_wbufi[MULTI_NUM_DSETS];
    int *rbufi[MULTI_NUM_DSETS];

    long *lwbufi[MULTI_NUM_DSETS];
    long *trans_lwbufi[MULTI_NUM_DSETS];
    long *lrbufi[MULTI_NUM_DSETS];

    const void *wbufs[MULTI_NUM_DSETS];
    void       *rbufs[MULTI_NUM_DSETS];
    const char *expr = "2*x";

    curr_nerrors = nerrors;

    ndsets = MAX(MULTI_MIN_DSETS, MULTI_NUM_DSETS);

    dims[0] = DSET_SELECT_DIM;

    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
    }

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    if ((ntrans_dxpl = H5Pcopy(dxpl)) < 0)
        P_TEST_ERROR;

    /* Set data transform */
    if (dtrans)
        if (H5Pset_data_transform(dxpl, expr) < 0)
            P_TEST_ERROR;

    /* Set up file space ids and dataset ids */
    for (i = 0; i < (int)ndsets; i++) {
        if ((file_sids[i] = H5Screate_simple(1, dims, NULL)) < 0)
            P_TEST_ERROR;

        /* Create ith dataset */
        if (chunked) {
            if (snprintf(dset_names[i], DSET_NAME_LEN,
                         dtrans ? "multi_chk_trans_dset%u" : "multi_chk_ntrans_dset%u", i) < 0)
                P_TEST_ERROR;
        }
        else {
            if (snprintf(dset_names[i], DSET_NAME_LEN,
                         dtrans ? "multi_contig_trans_dset%u" : "multi_contig_ntrans_dset%u", i) < 0)
                P_TEST_ERROR;
        }

        if ((dset_dids[i] =
                 H5Dcreate2(fid, dset_names[i], ((HDrandom() % 2) ? H5T_NATIVE_LONG : H5T_NATIVE_INT),
                            file_sids[i], H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;
    }

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    for (i = 0; i < (int)ndsets; i++) {
        if ((mem_sids[i] = H5Screate_simple(1, block, NULL)) < 0)
            P_TEST_ERROR;

        if (H5Sselect_hyperslab(file_sids[i], H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }

    buf_size = ndsets * DSET_SELECT_DIM * sizeof(int);

    /* Allocate buffers for all datasets */
    if (NULL == (total_wbuf = (int *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_trans_wbuf = (int *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_rbuf = (int *)HDmalloc(buf_size)))
        P_TEST_ERROR;

    buf_size = ndsets * DSET_SELECT_DIM * sizeof(long);

    if (NULL == (total_lwbuf = (long *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_trans_lwbuf = (long *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_lrbuf = (long *)HDmalloc(buf_size)))
        P_TEST_ERROR;

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        wbufi[i]       = total_wbuf + (i * DSET_SELECT_DIM);
        rbufi[i]       = total_rbuf + (i * DSET_SELECT_DIM);
        trans_wbufi[i] = total_trans_wbuf + (i * DSET_SELECT_DIM);

        wbufs[i] = wbufi[i];
        rbufs[i] = rbufi[i];
    }

    /* Case a */

    /* Initialize the buffer data */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            wbufi[i][j]       = j + (int)start[0];
            trans_wbufi[i][j] = 2 * wbufi[i][j];
        }

    /* Datatype setting for multi write/read */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = H5T_NATIVE_INT;

    /* Write data to the dataset with/without data transform */
    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read data from the dataset (if dtrans, without data transform set in dxpl) */
    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, ntrans_dxpl, rbufs) < 0)
        P_TEST_ERROR;

    /* Verify */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++)
            if (rbufi[i][j] != (dtrans ? trans_wbufi[i][j] : wbufi[i][j])) {
                nerrors++;
                HDprintf("\n     Error in 1st data verification for dset %d:\n", i);
                HDprintf("     At index %d: %d, %d\n", j + (int)start[0],
                         dtrans ? trans_wbufi[i][j] : wbufi[i][j], rbufi[i][j]);
                break;
            }

    if (dtrans) {

        /* Read the data from the dataset with data transform set in dxpl */
        if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
            P_TEST_ERROR;

        /* Verify */
        for (i = 0; i < (int)ndsets; i++)
            for (j = 0; j < (int)block[0]; j++)
                if (rbufi[i][j] != (2 * trans_wbufi[i][j])) {
                    nerrors++;
                    HDprintf("\n     Error in 1st (with dtrans) data verification for dset %d:\n", i);
                    HDprintf("     At index %d: %d, %d\n", j + (int)start[0], 2 * trans_wbufi[i][j],
                             rbufi[i][j]);
                    break;
                }
    }

    /* Case b */

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        lwbufi[i]       = total_lwbuf + (i * DSET_SELECT_DIM);
        trans_lwbufi[i] = total_trans_lwbuf + (i * DSET_SELECT_DIM);
        lrbufi[i]       = total_lrbuf + (i * DSET_SELECT_DIM);
        wbufs[i]        = lwbufi[i];
        rbufs[i]        = lrbufi[i];
    }

    /* Initialize the buffer data */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            lwbufi[i][j]       = j + (int)start[0] + DSET_SELECT_DIM;
            trans_lwbufi[i][j] = 2 * lwbufi[i][j];
        }

    /* Datatype setting for multi write/read */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = H5T_NATIVE_LONG;

    /* Write data to the dataset with/without data transform */
    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    /* Read data from the dataset (if dtrans, with data transform again in dxpl */
    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        for (j = 0; j < (int)block[0]; j++) {
            if (lrbufi[i][j] != (dtrans ? (2 * trans_lwbufi[i][j]) : lwbufi[i][j])) {
                nerrors++;
                HDprintf("\n     Error in 2nd data verification for dset %d:\n", i);
                HDprintf("     At index %d: %ld/%ld\n", j + (int)start[0],
                         (dtrans ? (2 * trans_lwbufi[i][j]) : lwbufi[i][j]), lrbufi[i][j]);
                break;
            }
        }
    }

    if (H5Pclose(dcpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(ntrans_dxpl) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        if (H5Sclose(file_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sclose(mem_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Dclose(dset_dids[i]) < 0)
            P_TEST_ERROR;
    }

    HDfree(total_wbuf);
    HDfree(total_rbuf);
    HDfree(total_trans_wbuf);
    HDfree(total_lwbuf);
    HDfree(total_trans_lwbuf);
    HDfree(total_lrbuf);

    CHECK_PASSED();

    return;

} /* test_multi_dsets_no_bkg() */

/*
 * Test 2 for multi-dataset:
 *   Datasets with compound types+background buffer
 *
 *   Create datasets with the same compound type
 *   Case (a) Initialize compound buffer in memory with unique values
 *       All datasets:
 *       --Write all compound fields to disk
 *       --Read the entire compound type for all datasets
 *       --Verify values read
 *   Case (b) Update all fields of the compound type in memory write buffer with new unique values
 *       dset0:
 *       --Write some but not all all compound fields to disk
 *       --Read and verify the fields have the correct (old(a) or new) values
 *       Remaining datasets:
 *       --Untouched
 *       --Read and verify the fields have the correct old(a) values
 *   Case (c) Update all fields of the compound type in memory read buffer with new unique values
 *       Randomized <mm> dataset:
 *       --Read some but not all the compound fields to memory
 *       --Verify the fields have the correct (old(a) or new) values
 *       dset0:
 *       --Untouched
 *       --Read and verify the fields have the correct (old(a) or middle(b)) values
 *       Remaining datasets:
 *       --Untouched
 *       --Read and verify the fields have the correct old(a) values
 *  Case (d)  Set up a different compound type which has:
 *      --no type conversion for 2 member types
 *      --a field with larger mem type
 *      --a field with smaller mem type
 *      All datasets:
 *      --Write the compound fields to disk
 *      --Read the entire compound type
 *      --Verify values read
 */
static void
test_multi_dsets_cmpd_with_bkg(hid_t fid, unsigned chunked)
{
    size_t  ndsets;
    int     i, j, mm;
    hid_t   dcpl = H5I_INVALID_HID;
    hid_t   dxpl = H5I_INVALID_HID;
    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];

    hid_t file_sids[MULTI_NUM_DSETS];
    hid_t mem_sids[MULTI_NUM_DSETS];
    hid_t mem_tids[MULTI_NUM_DSETS];

    hid_t s1_tid    = H5I_INVALID_HID;
    hid_t ss_ac_tid = H5I_INVALID_HID;
    hid_t ss_bc_tid = H5I_INVALID_HID;
    hid_t s2_tid    = H5I_INVALID_HID;

    char  dset_names[MULTI_NUM_DSETS][DSET_NAME_LEN];
    hid_t dset_dids[MULTI_NUM_DSETS];

    size_t buf_size;
    size_t s2_buf_size;

    s1_t *total_wbuf = NULL;
    s1_t *total_rbuf = NULL;

    s2_t *s2_total_wbuf = NULL;
    s2_t *s2_total_rbuf = NULL;

    s1_t *wbufi[MULTI_NUM_DSETS];
    s1_t *rbufi[MULTI_NUM_DSETS];

    s2_t *s2_wbufi[MULTI_NUM_DSETS];
    s2_t *s2_rbufi[MULTI_NUM_DSETS];

    const void *wbufs[MULTI_NUM_DSETS];
    void       *rbufs[MULTI_NUM_DSETS];

    curr_nerrors = nerrors;

    ndsets = MAX(MULTI_MIN_DSETS, MULTI_NUM_DSETS);

    dims[0] = DSET_SELECT_DIM;
    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
    }

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    /* Create the memory data type */
    if ((s1_tid = H5Tcreate(H5T_COMPOUND, sizeof(s1_t))) < 0)
        P_TEST_ERROR;

    if (H5Tinsert(s1_tid, "a", HOFFSET(s1_t, a), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s1_tid, "b", HOFFSET(s1_t, b), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s1_tid, "c", HOFFSET(s1_t, c), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s1_tid, "d", HOFFSET(s1_t, d), H5T_NATIVE_INT) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        if ((file_sids[i] = H5Screate_simple(1, dims, NULL)) < 0)
            P_TEST_ERROR;
        if ((mem_sids[i] = H5Screate_simple(1, block, NULL)) < 0)
            P_TEST_ERROR;

        if (snprintf(dset_names[i], DSET_NAME_LEN,
                     chunked ? "multi_chk_cmpd_dset%u" : "multi_contig_cmpd_dset%u", i) < 0)
            P_TEST_ERROR;
        if ((dset_dids[i] =
                 H5Dcreate2(fid, dset_names[i], s1_tid, file_sids[i], H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
            P_TEST_ERROR;

        if (H5Sselect_hyperslab(file_sids[i], H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }

    buf_size    = ndsets * DSET_SELECT_DIM * sizeof(s1_t);
    s2_buf_size = ndsets * DSET_SELECT_DIM * sizeof(s2_t);

    /* Allocate buffers for all datasets */
    if (NULL == (total_wbuf = (s1_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_rbuf = (s1_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;

    if (NULL == (s2_total_wbuf = (s2_t *)HDmalloc(s2_buf_size)))
        P_TEST_ERROR;
    if (NULL == (s2_total_rbuf = (s2_t *)HDmalloc(s2_buf_size)))
        P_TEST_ERROR;

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        wbufi[i] = total_wbuf + (i * DSET_SELECT_DIM);
        rbufi[i] = total_rbuf + (i * DSET_SELECT_DIM);

        wbufs[i] = wbufi[i];
        rbufs[i] = rbufi[i];
    }

    /* Case a */

    /* Initialize the buffer data for all the datasets */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            wbufi[i][j].a = 4 * (j + (int)start[0]);
            wbufi[i][j].b = 4 * (j + (int)start[0]) + 1;
            wbufi[i][j].c = 4 * (j + (int)start[0]) + 2;
            wbufi[i][j].d = 4 * (j + (int)start[0]) + 3;
        }

    /* Datatype setting for multi write */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = s1_tid;

    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            if (wbufi[i][j].a != rbufi[i][j].a || wbufi[i][j].b != rbufi[i][j].b ||
                wbufi[i][j].c != rbufi[i][j].c || wbufi[i][j].d != rbufi[i][j].d) {
                nerrors++;
                HDprintf("\n     Error in 1st data verification for dset %d:\n", i);
                HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", j + (int)start[0], wbufi[i][j].a,
                         rbufi[i][j].a, wbufi[i][j].b, rbufi[i][j].b, wbufi[i][j].c, rbufi[i][j].c,
                         wbufi[i][j].d, rbufi[i][j].d);

                break;
            }
        }

    /* Case b */

    /* Update data in wbufi for dset0 with unique values */
    for (j = 0; j < (int)block[0]; j++) {
        wbufi[0][j].a = (4 * (j + (int)start[0])) + DSET_SELECT_DIM;
        wbufi[0][j].b = (4 * (j + (int)start[0])) + DSET_SELECT_DIM + 1;
        wbufi[0][j].c = (4 * (j + (int)start[0])) + DSET_SELECT_DIM + 2;
        wbufi[0][j].d = (4 * (j + (int)start[0])) + DSET_SELECT_DIM + 3;
    }

    /* Create a compound type same size as s1_t */
    if ((ss_ac_tid = H5Tcreate(H5T_COMPOUND, sizeof(s1_t))) < 0)
        P_TEST_ERROR;

    /* but contains only subset members of s1_t */
    if (H5Tinsert(ss_ac_tid, "a", HOFFSET(s1_t, a), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(ss_ac_tid, "c", HOFFSET(s1_t, c), H5T_NATIVE_INT) < 0)
        P_TEST_ERROR;

    /* Datatype setting for write to dset0 */
    mem_tids[0] = ss_ac_tid;

    /* Untouched memory and file spaces for other datasets */
    for (i = 0; i < (int)ndsets; i++) {
        if (i == 0)
            continue;

        if (H5Sselect_none(mem_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sselect_none(file_sids[i]) < 0)
            P_TEST_ERROR;
    }

    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    for (i = 0; i < (int)ndsets; i++)
        if (i == 0) { /* dset0 */
            for (j = 0; j < (int)block[0]; j++)
                if (wbufi[i][j].a != rbufi[i][j].a || (4 * (j + (int)start[0]) + 1) != rbufi[i][j].b ||
                    wbufi[i][j].c != rbufi[i][j].c || (4 * (j + (int)start[0]) + 3) != rbufi[i][j].d) {
                    nerrors++;
                    HDprintf("\n     Error in 2nd data verification for dset %d:\n", i);
                    HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", j + (int)start[0],
                             wbufi[i][j].a, rbufi[i][j].a, (4 * (j + (int)start[0]) + 1), rbufi[i][j].b,
                             wbufi[i][j].c, rbufi[i][j].c, (4 * (j + (int)start[0]) + 3), rbufi[i][j].d);
                    break;
                }
        }
        else { /* other datasets */
            for (j = 0; j < (int)block[0]; j++)
                if ((4 * (j + (int)start[0])) != rbufi[i][j].a ||
                    (4 * (j + (int)start[0]) + 1) != rbufi[i][j].b ||
                    (4 * (j + (int)start[0]) + 2) != rbufi[i][j].c ||
                    (4 * (j + (int)start[0]) + 3) != rbufi[i][j].d) {
                    nerrors++;
                    HDprintf("\n     Error in 2nd data verification for dset %d:\n", i);
                    HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", j + (int)start[0],
                             (4 * (j + (int)start[0])), rbufi[i][j].a, (4 * (j + (int)start[0]) + 1),
                             rbufi[i][j].b, (4 * (j + (int)start[0]) + 2), rbufi[i][j].c,
                             (4 * (j + (int)start[0]) + 3), rbufi[i][j].d);
                    break;
                }
        }

    /* Case c */
    mm = HDrandom() % (int)ndsets;
    if (!mm)
        mm++;

    /* Update data in rbufi for <mm> dset with new unique values */
    for (j = 0; j < (int)block[0]; j++) {
        rbufi[mm][j].a = (4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM);
        rbufi[mm][j].b = (4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM) + 1;
        rbufi[mm][j].c = (4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM) + 2;
        rbufi[mm][j].d = (4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM) + 3;
    }

    /* Create a compound type same size as s1_t */
    if ((ss_bc_tid = H5Tcreate(H5T_COMPOUND, sizeof(s1_t))) < 0)
        P_TEST_ERROR;

    /* but contains only subset members of s1_t */
    if (H5Tinsert(ss_bc_tid, "b", HOFFSET(s1_t, b), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(ss_bc_tid, "c", HOFFSET(s1_t, c), H5T_NATIVE_INT) < 0)
        P_TEST_ERROR;

    /* Reset memory and file spaces for <mm> dset */
    if (H5Sselect_all(mem_sids[mm]) < 0)
        P_TEST_ERROR;
    if (H5Sselect_all(file_sids[mm]) < 0)
        P_TEST_ERROR;
    if (H5Sselect_hyperslab(file_sids[mm], H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Untouched memory and file space for other datasets */
    for (i = 0; i < (int)ndsets; i++) {
        if (i == 0 || i == mm)
            continue;
        if (H5Sselect_none(mem_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sselect_none(file_sids[i]) < 0)
            P_TEST_ERROR;
    }

    /* Datatype setting for read from <mm> dataset */
    mem_tids[mm] = ss_bc_tid;

    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    /* Verify data read */
    /* dset0 */
    for (j = 0; j < (int)block[0]; j++)
        if (wbufi[0][j].a != rbufi[0][j].a || ((4 * (j + (int)start[0])) + 1) != rbufi[0][j].b ||
            wbufi[0][j].c != rbufi[0][j].c || ((4 * (j + (int)start[0])) + 3) != rbufi[0][j].d) {
            nerrors++;
            HDprintf("\n     Error in 3rd data verification for dset0:\n");
            HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", j + (int)start[0], wbufi[0][j].a,
                     rbufi[0][j].a, (4 * (j + (int)start[0]) + 1), rbufi[0][j].b, wbufi[0][j].c,
                     rbufi[0][j].c, (4 * (j + (int)start[0]) + 3), rbufi[0][j].d);
            break;
        }

    /* <mm> dset */
    for (j = 0; j < (int)block[0]; j++)
        if (rbufi[mm][j].a != ((4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM)) ||
            rbufi[mm][j].b != ((4 * (j + (int)start[0])) + 1) ||
            rbufi[mm][j].c != ((4 * (j + (int)start[0])) + 2) ||
            rbufi[mm][j].d != ((4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM) + 3)) {
            nerrors++;
            HDprintf("\n     Error in 3rd data verification for dset %d:\n", mm);
            HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", j + (int)start[0],
                     ((4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM)), rbufi[mm][j].a,
                     ((4 * (j + (int)start[0])) + 1), rbufi[mm][j].b, ((4 * (j + (int)start[0])) + 2),
                     rbufi[mm][j].c, ((4 * (j + (int)start[0])) + (2 * DSET_SELECT_DIM) + 3), rbufi[mm][j].d);
            break;
        }

    /* other datasets */
    for (i = 0; i < (int)ndsets; i++) {
        if (i == 0 || i == mm)
            continue;

        for (j = 0; j < (int)block[0]; j++)
            if (rbufi[i][j].a != ((4 * (j + (int)start[0]))) ||
                rbufi[i][j].b != ((4 * (j + (int)start[0])) + 1) ||
                rbufi[i][j].c != ((4 * (j + (int)start[0])) + 2) ||
                rbufi[i][j].d != ((4 * (j + (int)start[0])) + 3)) {
                nerrors++;
                HDprintf("\n     Error in 3rd data verification for dset %d:\n", i);
                HDprintf("     At index %d: %d/%d, %d/%d, %d/%d, %d/%d\n", j + (int)start[0],
                         ((4 * (j + (int)start[0]))), rbufi[i][j].a, ((4 * (j + (int)start[0])) + 1),
                         rbufi[i][j].b, ((4 * (j + (int)start[0])) + 2), rbufi[i][j].c,
                         ((4 * (j + (int)start[0])) + 3), rbufi[i][j].d);
                break;
            }
    }

    /* Case d */

    /* Create s2_t compound type with:
     * --no conversion for 2 member types,
     * --1 larger mem type
     * --1 smaller mem type
     */
    if ((s2_tid = H5Tcreate(H5T_COMPOUND, sizeof(s2_t))) < 0)
        P_TEST_ERROR;

    if (H5Tinsert(s2_tid, "a", HOFFSET(s2_t, a), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s2_tid, "b", HOFFSET(s2_t, b), H5T_NATIVE_LLONG) < 0 ||
        H5Tinsert(s2_tid, "c", HOFFSET(s2_t, c), H5T_NATIVE_INT) < 0 ||
        H5Tinsert(s2_tid, "d", HOFFSET(s2_t, d), H5T_NATIVE_SHORT) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        s2_wbufi[i] = s2_total_wbuf + (i * DSET_SELECT_DIM);
        s2_rbufi[i] = s2_total_rbuf + (i * DSET_SELECT_DIM);

        wbufs[i] = s2_wbufi[i];
        rbufs[i] = s2_rbufi[i];

        mem_tids[i] = s2_tid;

        if (H5Sselect_all(mem_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sselect_all(file_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sselect_hyperslab(file_sids[i], H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }

    /* Initialize the buffer data for all the datasets */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            s2_wbufi[i][j].a = 8 * (j + (int)start[0]);
            s2_wbufi[i][j].b = (long long)((8 * (j + (int)start[0])) + 1);
            s2_wbufi[i][j].c = (8 * (j + (int)start[0])) + 2;
            s2_wbufi[i][j].d = (short)((8 * (j + (int)start[0])) + 3);
        }

    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        for (j = 0; j < (int)block[0]; j++)
            if (s2_rbufi[i][j].a != s2_wbufi[i][j].a || s2_rbufi[i][j].b != s2_wbufi[i][j].b ||
                s2_rbufi[i][j].c != s2_wbufi[i][j].c || s2_rbufi[i][j].d != s2_wbufi[i][j].d) {
                nerrors++;
                HDprintf("\n     Error in 3rd data verification for dset %d:\n", i);
                HDprintf("     At index %d: %d/%d, %lld/%lld, %d/%d, %d/%d\n", j + (int)start[0],
                         s2_wbufi[i][j].a, s2_rbufi[i][j].a, s2_wbufi[i][j].b, s2_rbufi[i][j].b,
                         s2_wbufi[i][j].c, s2_rbufi[i][j].c, s2_wbufi[i][j].d, s2_rbufi[i][j].d);
                break;
            }
    }

    if (H5Pclose(dcpl) < 0)
        P_TEST_ERROR;

    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        if (H5Sclose(file_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sclose(mem_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Dclose(dset_dids[i]) < 0)
            P_TEST_ERROR;
    }

    HDfree(total_wbuf);
    HDfree(total_rbuf);
    HDfree(s2_total_wbuf);
    HDfree(s2_total_rbuf);

    CHECK_PASSED();

    return;

} /* test_multi_dsets_cmpd_with_bkg() */

/*
 *  Test 3 for multi-dataset:
 *  --Datasets with/without type conv+size change+no background buffer
 *
 *  Create dset0: H5T_STD_I32BE
 *  Create other dateasets: randomized H5T_STD_I64LE or H5T_STD_I16LE
 *
 *  Case a--setting for multi write/read to ndsets:
 *    Datatype for all datasets: H5T_STD_I32BE
 *
 *  Case b--setting for multi write/read to ndsets
 *    Datatype for all datasets: H5T_STD_I64BE
 *
 *  Case c--setting for multi write/read to ndsets
 *    Datatype for all datasets: H5T_STD_I16BE
 */
static void
test_multi_dsets_size_change_no_bkg(hid_t fid, unsigned chunked)
{
    size_t  ndsets;
    int     i, j;
    hid_t   dcpl = H5I_INVALID_HID;
    hid_t   dxpl = H5I_INVALID_HID;
    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];

    hid_t file_sids[MULTI_NUM_DSETS];
    hid_t mem_sids[MULTI_NUM_DSETS];
    hid_t mem_tids[MULTI_NUM_DSETS];

    char  dset_names[MULTI_NUM_DSETS][DSET_NAME_LEN];
    hid_t dset_dids[MULTI_NUM_DSETS];

    size_t   buf_size, ss;
    uint8_t *total_wbuf  = NULL;
    uint8_t *total_rbuf  = NULL;
    uint8_t *total_lwbuf = NULL;
    uint8_t *total_lrbuf = NULL;
    uint8_t *total_swbuf = NULL;
    uint8_t *total_srbuf = NULL;

    uint8_t *wbufi[MULTI_NUM_DSETS];
    uint8_t *rbufi[MULTI_NUM_DSETS];
    uint8_t *lwbufi[MULTI_NUM_DSETS];
    uint8_t *lrbufi[MULTI_NUM_DSETS];
    uint8_t *swbufi[MULTI_NUM_DSETS];
    uint8_t *srbufi[MULTI_NUM_DSETS];

    const void *wbufs[MULTI_NUM_DSETS];
    void       *rbufs[MULTI_NUM_DSETS];

    curr_nerrors = nerrors;

    ndsets = MAX(MULTI_MIN_DSETS, MULTI_NUM_DSETS);

    dims[0] = DSET_SELECT_DIM;

    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
    }

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    /* Set up file space ids, mem space ids, and dataset ids */
    for (i = 0; i < (int)ndsets; i++) {
        if ((file_sids[i] = H5Screate_simple(1, dims, NULL)) < 0)
            P_TEST_ERROR;

        if ((mem_sids[i] = H5Screate_simple(1, dims, NULL)) < 0)
            P_TEST_ERROR;

        /* Create ith dataset */
        if (snprintf(dset_names[i], DSET_NAME_LEN,
                     chunked ? "multi_chk_size_dset%u" : "multi_contig_size_dset%u", i) < 0)
            P_TEST_ERROR;

        if (i == 0) {
            if ((dset_dids[i] = H5Dcreate2(fid, dset_names[i], H5T_STD_I32BE, file_sids[i], H5P_DEFAULT, dcpl,
                                           H5P_DEFAULT)) < 0)
                P_TEST_ERROR;
        }
        else {
            if ((dset_dids[i] =
                     H5Dcreate2(fid, dset_names[i], ((HDrandom() % 2) ? H5T_STD_I64LE : H5T_STD_I16LE),
                                file_sids[i], H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
                P_TEST_ERROR;
        }
    }

    /* Each process takes x number of elements */
    block[0]  = dims[0] / (hsize_t)mpi_size;
    stride[0] = block[0];
    count[0]  = 1;
    start[0]  = (hsize_t)mpi_rank * block[0];

    for (i = 0; i < (int)ndsets; i++) {
        if ((mem_sids[i] = H5Screate_simple(1, block, NULL)) < 0)
            P_TEST_ERROR;

        if (H5Sselect_hyperslab(file_sids[i], H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }

    /* Case a */

    ss       = H5Tget_size(H5T_STD_I32BE);
    buf_size = ndsets * ss * DSET_SELECT_DIM;

    /* Allocate buffers for all datasets */
    if (NULL == (total_wbuf = (uint8_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_rbuf = (uint8_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        wbufi[i] = total_wbuf + (i * (int)ss * DSET_SELECT_DIM);
        rbufi[i] = total_rbuf + (i * (int)ss * DSET_SELECT_DIM);

        wbufs[i] = wbufi[i];
        rbufs[i] = rbufi[i];
    }

    /* Initialize the buffer data: big endian */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            wbufi[i][j * (int)ss + 0] = 0x1;
            wbufi[i][j * (int)ss + 1] = 0x2;
            wbufi[i][j * (int)ss + 2] = 0x3;
            wbufi[i][j * (int)ss + 3] = (uint8_t)(0x4 + j + (int)start[0]);
        }

    /* Datatype setting for multi write/read */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = H5T_STD_I32BE;

    /* Write data to the dataset */
    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Read data from the dataset */
    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    /* Verify */
    for (i = 0; i < (int)ndsets; i++)
        /* Only compare when it's at least the size of H5T_STD_I32BE */
        if (H5Tget_size(H5Dget_type(dset_dids[i])) >= ss) {
            for (j = 0; j < (int)block[0]; j++)
                if (rbufi[i][(int)ss * j + 0] != wbufi[i][(int)ss * j + 0] ||
                    rbufi[i][(int)ss * j + 1] != wbufi[i][(int)ss * j + 1] ||
                    rbufi[i][(int)ss * j + 2] != wbufi[i][(int)ss * j + 2] ||
                    rbufi[i][(int)ss * j + 3] != wbufi[i][(int)ss * j + 3]) {
                    H5_FAILED();
                    HDprintf("    Read different values than written.\n");
                    HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                    P_TEST_ERROR;
                }
        }

    /* Case b */

    ss       = H5Tget_size(H5T_STD_I64BE);
    buf_size = ndsets * (ss * DSET_SELECT_DIM);

    /* Allocate buffers for all datasets */
    if (NULL == (total_lwbuf = (uint8_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_lrbuf = (uint8_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        lwbufi[i] = total_lwbuf + (i * (int)ss * DSET_SELECT_DIM);
        lrbufi[i] = total_lrbuf + (i * (int)ss * DSET_SELECT_DIM);

        wbufs[i] = lwbufi[i];
        rbufs[i] = lrbufi[i];
    }

    /* Initialize the buffer data: big endian */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            lwbufi[i][j * (int)ss + 0] = 0x1;
            lwbufi[i][j * (int)ss + 1] = 0x2;
            lwbufi[i][j * (int)ss + 2] = 0x3;
            lwbufi[i][j * (int)ss + 3] = 0x4;
            lwbufi[i][j * (int)ss + 4] = 0x5;
            lwbufi[i][j * (int)ss + 5] = 0x6;
            lwbufi[i][j * (int)ss + 6] = 0x7;
            lwbufi[i][j * (int)ss + 7] = (uint8_t)(0x8 + j + (int)start[0]);
        }

    /* Datatype setting for multi write/read */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = H5T_STD_I64BE;

    /* Write data to the dataset */
    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    /* Read data from the dataset */
    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    /* Verify */
    for (i = 0; i < (int)ndsets; i++)
        /* Only compare when it's the size of H5T_STD_I64BE */
        if (H5Tget_size(H5Dget_type(dset_dids[i])) >= ss) {
            for (j = 0; j < (int)block[0]; j++)
                if (lrbufi[i][(int)ss * j + 0] != lwbufi[i][(int)ss * j + 0] ||
                    lrbufi[i][(int)ss * j + 1] != lwbufi[i][(int)ss * j + 1] ||
                    lrbufi[i][(int)ss * j + 2] != lwbufi[i][(int)ss * j + 2] ||
                    lrbufi[i][(int)ss * j + 3] != lwbufi[i][(int)ss * j + 3] ||
                    lrbufi[i][(int)ss * j + 4] != lwbufi[i][(int)ss * j + 4] ||
                    lrbufi[i][(int)ss * j + 5] != lwbufi[i][(int)ss * j + 5] ||
                    lrbufi[i][(int)ss * j + 6] != lwbufi[i][(int)ss * j + 6] ||
                    lrbufi[i][(int)ss * j + 7] != lwbufi[i][(int)ss * j + 7]) {
                    H5_FAILED();
                    HDprintf("    Read different values than written.\n");
                    HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                    P_TEST_ERROR;
                }
        }

    /* Case c */

    ss       = H5Tget_size(H5T_STD_I16BE);
    buf_size = ndsets * (ss * DSET_SELECT_DIM);

    /* Allocate buffers for all datasets */
    if (NULL == (total_swbuf = (uint8_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_srbuf = (uint8_t *)HDmalloc(buf_size)))
        P_TEST_ERROR;

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        swbufi[i] = total_swbuf + (i * (int)ss * DSET_SELECT_DIM);
        srbufi[i] = total_srbuf + (i * (int)ss * DSET_SELECT_DIM);

        wbufs[i] = swbufi[i];
        rbufs[i] = srbufi[i];
    }

    /* Initialize the buffer data: big endian */
    for (i = 0; i < (int)ndsets; i++)
        for (j = 0; j < (int)block[0]; j++) {
            swbufi[i][j * (int)ss + 0] = 0x1;
            swbufi[i][j * (int)ss + 1] = (uint8_t)(0x2 + j + (int)start[0]);
        }

    /* Datatype setting for multi write/read */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = H5T_STD_I16BE;

    /* Write data to the dataset */
    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    /* Read data from the dataset */
    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
        P_TEST_ERROR;

    /* Verify */
    for (i = 0; i < (int)ndsets; i++)
        /* Can compare for all cases */
        for (j = 0; j < (int)block[0]; j++)
            if (srbufi[i][(int)ss * j + 0] != swbufi[i][(int)ss * j + 0] ||
                srbufi[i][(int)ss * j + 1] != swbufi[i][(int)ss * j + 1]) {
                H5_FAILED();
                HDprintf("    Read different values than written.\n");
                HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                P_TEST_ERROR;
            }

    if (H5Pclose(dcpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        if (H5Sclose(file_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sclose(mem_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Dclose(dset_dids[i]) < 0)
            P_TEST_ERROR;
    }

    HDfree(total_wbuf);
    HDfree(total_rbuf);
    HDfree(total_lwbuf);
    HDfree(total_lrbuf);
    HDfree(total_swbuf);
    HDfree(total_srbuf);

    CHECK_PASSED();

    return;

} /* test_multi_dsets_size_change_no_bkg() */

/*
 *  Test 4 for multi-dataset:
 *    Datasets with type conversions+some processes have null/empty selections
 *
 *  Create dset0: H5T_NATIVE_INT
 *  Create other datasets: randomized H5T_NATIVE_LLONG or H5T_NATIVE_SHORT
 *    Type conversions + some processes have null/empty selections in datasets
 *
 *  Case (a): dset0
 *    process 0: hyperslab; other processes: select none
 *  Case (b): randomized dset <mm>
 *    process 0: get 0 row; other processes: hyperslab
 *  Case (c): randomized dset <ll>
 *    process 0: select none; other processes: select all
 *
 *  Memory datatype for multi write to all datasets: H5T_NATIVE_INT
 *  --this will not trigger type conversion for case (a) but
 *    type conversion for cases (b) & (c)
 *  Memory datatype for multi read to all datasets: H5T_NATIVE_LONG
 *  --this will trigger type conversion for (a), (b) & (c)
 */
static void
test_multi_dsets_conv_sel_empty(hid_t fid, unsigned chunked, unsigned dtrans)
{
    size_t ndsets;
    int    i, j;
    hid_t  dcpl        = H5I_INVALID_HID;
    hid_t  dxpl        = H5I_INVALID_HID;
    hid_t  ntrans_dxpl = H5I_INVALID_HID;

    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];

    hid_t file_sids[MULTI_NUM_DSETS];
    hid_t mem_sids[MULTI_NUM_DSETS];
    hid_t mem_tids[MULTI_NUM_DSETS];

    char  dset_names[MULTI_NUM_DSETS][DSET_NAME_LEN];
    hid_t dset_dids[MULTI_NUM_DSETS];

    size_t buf_size;
    int   *total_wbuf       = NULL;
    int   *total_trans_wbuf = NULL;
    long  *total_lrbuf      = NULL;

    int  *wbufi[MULTI_NUM_DSETS];
    int  *trans_wbufi[MULTI_NUM_DSETS];
    long *l_rbufi[MULTI_NUM_DSETS];

    const void *wbufs[MULTI_NUM_DSETS];
    void       *rbufs[MULTI_NUM_DSETS];

    int save_block0;
    int mm, ll;

    const char *expr = "2*x";

    curr_nerrors = nerrors;

    ndsets = MAX(MULTI_MIN_DSETS, MULTI_NUM_DSETS);

    /* Create 1d data space */
    dims[0] = DSET_SELECT_DIM;

    if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
        P_TEST_ERROR;

    if (chunked) {
        cdims[0] = DSET_SELECT_CHUNK_DIM;
        if (H5Pset_chunk(dcpl, 1, cdims) < 0)
            P_TEST_ERROR;
    }

    /* Create dataset transfer property list */
    if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
        P_TEST_ERROR;

    /* Set selection I/O mode, type of I/O and type of collective I/O */
    set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

    if ((ntrans_dxpl = H5Pcopy(dxpl)) < 0)
        P_TEST_ERROR;

    /* Set data transform */
    if (dtrans)
        if (H5Pset_data_transform(dxpl, expr) < 0)
            P_TEST_ERROR;

    /* Set up file space ids and dataset ids */
    for (i = 0; i < (int)ndsets; i++) {
        if ((file_sids[i] = H5Screate_simple(1, dims, NULL)) < 0)
            P_TEST_ERROR;

        if (chunked) {
            if (snprintf(dset_names[i], DSET_NAME_LEN,
                         dtrans ? "multi_chk_trans_sel_dset%u" : "multi_chk_ntrans_sel_dset%u", i) < 0)
                P_TEST_ERROR;
        }
        else {
            if (snprintf(dset_names[i], DSET_NAME_LEN,
                         dtrans ? "multi_contig_trans_sel_dset%u" : "multi_contig_ntrans_sel_dset%u", i) < 0)
                P_TEST_ERROR;
        }

        if (i == 0) {
            if ((dset_dids[i] = H5Dcreate2(fid, dset_names[i], H5T_NATIVE_INT, file_sids[i], H5P_DEFAULT,
                                           dcpl, H5P_DEFAULT)) < 0)
                P_TEST_ERROR;
        }
        else {
            if ((dset_dids[i] =
                     H5Dcreate2(fid, dset_names[i], ((HDrandom() % 2) ? H5T_NATIVE_LLONG : H5T_NATIVE_SHORT),
                                file_sids[i], H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
                P_TEST_ERROR;
        }
    }

    buf_size = ndsets * DSET_SELECT_DIM * sizeof(int);

    /* Allocate buffers for all datasets */
    if (NULL == (total_wbuf = (int *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_trans_wbuf = (int *)HDmalloc(buf_size)))
        P_TEST_ERROR;
    if (NULL == (total_lrbuf = (long *)HDmalloc(ndsets * DSET_SELECT_DIM * sizeof(long))))
        P_TEST_ERROR;

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        wbufi[i]       = total_wbuf + (i * DSET_SELECT_DIM);
        trans_wbufi[i] = total_trans_wbuf + (i * DSET_SELECT_DIM);

        wbufs[i] = wbufi[i];
    }

    /*
     * Case (a): dset0
     *   process 0: hyperslab; other processes: select none
     */

    /* Each process takes x number of elements */
    block[0]    = dims[0] / (hsize_t)mpi_size;
    save_block0 = (int)block[0];
    stride[0]   = block[0];
    count[0]    = 1;
    start[0]    = (hsize_t)mpi_rank * block[0];

    /* Get file dataspace */
    if ((file_sids[0] = H5Dget_space(dset_dids[0])) < 0)
        P_TEST_ERROR;

    if (MAINPROCESS) {
        if (H5Sselect_hyperslab(file_sids[0], H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }
    else {
        if (H5Sselect_none(file_sids[0]) < 0)
            P_TEST_ERROR;
    }

    /* Create memory dataspace */
    if ((mem_sids[0] = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;
    if (mpi_rank) {
        if (H5Sselect_none(mem_sids[0]) < 0)
            P_TEST_ERROR;
    }

    /* Initialize data for wbufi[0] */
    for (j = 0; j < (int)block[0]; j++) {
        wbufi[0][j]       = j + (int)start[0];
        trans_wbufi[0][j] = 2 * wbufi[0][j];
    }

    /*
     * Case (b): choose a dataset -- dset <mm>
     *   process 0: get 0 row; other processes: hyperslab
     */

    mm = HDrandom() % (int)ndsets;
    if (mm == 0)
        mm++;

    block[0]  = mpi_rank ? (dims[0] / (hsize_t)mpi_size) : 0;
    stride[0] = mpi_rank ? block[0] : 1;
    count[0]  = 1;
    start[0]  = mpi_rank ? ((hsize_t)mpi_rank * block[0]) : 0;

    /* Get file dataspace */
    if ((file_sids[mm] = H5Dget_space(dset_dids[mm])) < 0)
        P_TEST_ERROR;

    if (H5Sselect_hyperslab(file_sids[mm], H5S_SELECT_SET, start, stride, count, block) < 0)
        P_TEST_ERROR;

    /* Create a memory dataspace */
    if ((mem_sids[mm] = H5Screate_simple(1, block, NULL)) < 0)
        P_TEST_ERROR;

    /* need to make memory space to match for process 0 */
    if (MAINPROCESS) {
        if (H5Sselect_hyperslab(mem_sids[mm], H5S_SELECT_SET, start, stride, count, block) < 0)
            P_TEST_ERROR;
    }
    else {
        if (H5Sselect_all(mem_sids[mm]) < 0)
            P_TEST_ERROR;
    }

    /* Initialize data for wbufi[1] */
    for (j = 0; j < (int)block[0]; j++) {
        wbufi[mm][j]       = j + (int)start[0];
        trans_wbufi[mm][j] = 2 * wbufi[mm][j];
    }

    /*
     * Case (c): choose a dataset -- dset <ll>
     *   process 0: select none; other processes: select all
     */

    ll = mm + 1;
    if ((ll % (int)ndsets) == 0)
        ll = 1;

    /* Get file dataspace */
    if ((file_sids[ll] = H5Dget_space(dset_dids[ll])) < 0)
        P_TEST_ERROR;
    if (MAINPROCESS) {
        if (H5Sselect_none(file_sids[ll]) < 0)
            P_TEST_ERROR;
    }
    else {
        if (H5Sselect_all(file_sids[ll]) < 0)
            P_TEST_ERROR;
    }

    /* Create a memory dataspace */
    if ((mem_sids[ll] = H5Screate_simple(1, dims, NULL)) < 0)
        P_TEST_ERROR;
    if (MAINPROCESS) {
        if (H5Sselect_none(mem_sids[ll]) < 0)
            P_TEST_ERROR;
    }
    else if (H5Sselect_all(mem_sids[ll]) < 0)
        P_TEST_ERROR;

    /* Initialize data for wbufi[ll] */
    for (j = 0; j < (int)dims[0]; j++) {
        wbufi[ll][j]       = (int)j + DSET_SELECT_DIM;
        trans_wbufi[ll][j] = 2 * wbufi[ll][j];
    }

    /* Set up remaining dsets */
    for (i = 0; i < (int)ndsets; i++) {
        if (i == 0 || i == mm || i == ll)
            continue;
        /* Get file dataspace */
        if ((file_sids[i] = H5Dget_space(dset_dids[i])) < 0)
            P_TEST_ERROR;
        if (H5Sselect_none(file_sids[i]) < 0)
            P_TEST_ERROR;

        if ((mem_sids[i] = H5Screate_simple(1, dims, NULL)) < 0)
            P_TEST_ERROR;
        if (H5Sselect_none(mem_sids[i]) < 0)
            P_TEST_ERROR;
    }

    /* Set up mem_tids[] for multi write */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = H5T_NATIVE_INT;

    /* Write data to the dataset with/without data transform */
    if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
        P_TEST_ERROR;

    check_io_mode(dxpl, chunked);

    /* Initialize buffer indices */
    for (i = 0; i < (int)ndsets; i++) {
        l_rbufi[i] = total_lrbuf + (i * DSET_SELECT_DIM);
        rbufs[i]   = l_rbufi[i];
    }

    /* Set up mem_tids[] for multi read */
    for (i = 0; i < (int)ndsets; i++)
        mem_tids[i] = H5T_NATIVE_LONG;

    if (H5Dread_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, ntrans_dxpl, rbufs) < 0)
        P_TEST_ERROR;

    if (MAINPROCESS) {
        /* Case a: verify dset0 */
        for (j = 0; j < save_block0; j++)
            if (l_rbufi[0][j] != (dtrans ? (long)trans_wbufi[0][j] : (long)wbufi[0][j])) {
                nerrors++;
                HDprintf("     Verify dset0 at index %d: %ld, %ld\n", j + (int)start[0],
                         dtrans ? (long)trans_wbufi[0][j] : (long)wbufi[0][j], l_rbufi[0][j]);
                break;
            }
    }

    if (mpi_rank) {
        /* Case b: verify dset <mm> */
        for (j = 0; j < (int)block[0]; j++)
            if (l_rbufi[mm][j] != (long)(dtrans ? trans_wbufi[mm][j] : wbufi[mm][j])) {
                nerrors++;
                HDprintf("     Verify dset %d at index %d: %ld, %ld\n", mm, j + (int)start[0],
                         (long)(dtrans ? trans_wbufi[mm][j] : wbufi[mm][j]), l_rbufi[mm][j]);
                break;
            }

        /* Case c: verify dset <ll> */
        for (j = 0; j < (int)dims[0]; j++)
            if (l_rbufi[ll][j] != (long)(dtrans ? trans_wbufi[ll][j] : wbufi[ll][j])) {
                nerrors++;
                HDprintf("     Verify dset %d at index %d: %ld, %ld\n", ll, j,
                         (long)(dtrans ? trans_wbufi[ll][j] : wbufi[ll][j]), l_rbufi[ll][j]);
                break;
            }
    }

    if (H5Pclose(dcpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(dxpl) < 0)
        P_TEST_ERROR;
    if (H5Pclose(ntrans_dxpl) < 0)
        P_TEST_ERROR;

    for (i = 0; i < (int)ndsets; i++) {
        if (H5Sclose(file_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Sclose(mem_sids[i]) < 0)
            P_TEST_ERROR;
        if (H5Dclose(dset_dids[i]) < 0)
            P_TEST_ERROR;
    }

    HDfree(total_wbuf);
    HDfree(total_trans_wbuf);
    HDfree(total_lrbuf);

    CHECK_PASSED();

    return;

} /* test_multi_dsets_conv_sel_empty() */

/*
 * Test 5 for multi-dataset:
 *
 * Repeat the following test for niter times to ensure the
 * random combinations of all dataset types are hit.
 *
 * Create randomized contiguous or chunked datasets with:
 * --DSET_WITH_NO_CONV:
 *   --with no type conversion
 *   --dataset with H5T_NATIVE_INT
 * --DSET_WITH_CONV_AND_NO_BKG:
 *   --type conversion without background buffer
 *   --dataset with H5T_NATIVE_LONG
 * --DSET_WITH_CONV_AND_BKG:
 *   --type conversion with background buffer
 *   --dataset with compound type s1_t
 *
 * Do H5Dwrite_multi() and H5Dread_multi() for the above randomized
 * datasets with the settings below:
 * Setting A:
 * --DSET_WITH_NO_CONV:
 *   --write: mem_tids[] = H5T_NATIVE_INT
 *   --read: r_mem_tids[] = H5T_NATIVE_INT
 * --DSET_WITH_CONV_AND_NO_BKG:
 *   --write: mem_tids[] = H5T_NATIVE_ULONG
 *   --read: r_mem_tids[] = H5T_NATIVE_LONG
 * --DSET_WITH_CONV_AND_BKG:
 *   --write: mem_tids[] = s1_tid;
 *   --read: r_mem_tids[i] = s3_tid;
 *
 * Setting B:
 * --DSET_WITH_NO_CONV:
 *   --write: mem_tids[] = H5T_NATIVE_INT
 *   --read: r_mem_tids[] = H5T_NATIVE_INT
 * --DSET_WITH_CONV_AND_NO_BKG:
 *   --write: mem_tids[] = H5T_NATIVE_LONG;
 *   --read: r_mem_tids[] = H5T_NATIVE_SHORT;
 * --DSET_WITH_CONV_AND_BKG:
 *   --write: mem_tids[] = s4_tid;
 *   --read: r_mem_tids[i] = s1_tid;
 *
 * Verify the result read as below:
 * Setting A:
 * --DSET_WITH_NO_CONV:
 *   --verify data read in rbufi1[i][j] is same as wbufi1[i][j]
 * --DSET_WITH_CONV_AND_NO_BKG:
 *   --verify data read in l_rbufi2[i][j] is all LONG_MAX
 * --DSET_WITH_CONV_AND_BKG:
 *   --verify all fields read in s3_rbufi3[i][j] is the
 *     reverse of s1_wbufi3[i][j]
 * Setting B:
 * --DSET_WITH_NO_CONV:
 *   --verify data read in rbufi1[i][j] is same as wbufi1[i][j]
 * --DSET_WITH_CONV_AND_NO_BKG:
 *   --verify data read in s_rbufi2[i][j] is all SHRT_MAX
 * --DSET_WITH_CONV_AND_BKG:
 *   --verify fields read in s1_rbufi3[i][j] is as follows:
 *     --fields 'a' and 'c' are as  s1_wbufi3[i][j].a and s1_wbufi3[i][j].c
 *     --fields 'b' and 'd' are (DSET_SELECT_DIM + j + start[0])
 */
static void
test_multi_dsets_all(int niter, hid_t fid, unsigned chunked)
{
    size_t ndsets;
    int    i, j, mm;
    int    s, n;
    hid_t  dcpl = H5I_INVALID_HID;
    hid_t  dxpl = H5I_INVALID_HID;

    hsize_t dims[1];
    hsize_t cdims[1];
    hsize_t start[1], stride[1], count[1], block[1];

    hid_t file_sids[MULTI_NUM_DSETS];
    hid_t mem_sids[MULTI_NUM_DSETS];
    hid_t mem_tids[MULTI_NUM_DSETS];
    hid_t r_mem_tids[MULTI_NUM_DSETS];

    multi_dset_type_t dset_types[MULTI_NUM_DSETS];

    hid_t s1_tid = H5I_INVALID_HID;
    hid_t s3_tid = H5I_INVALID_HID;
    hid_t s4_tid = H5I_INVALID_HID;

    char  dset_names[MULTI_NUM_DSETS][DSET_NAME_LEN];
    hid_t dset_dids[MULTI_NUM_DSETS];

    size_t buf_size;

    int *total_wbuf1 = NULL;
    int *total_rbuf1 = NULL;

    int *wbufi1[MULTI_NUM_DSETS];
    int *rbufi1[MULTI_NUM_DSETS];

    unsigned long *ul_total_wbuf2 = NULL;
    long          *l_total_rbuf2  = NULL;
    unsigned long *ul_wbufi2[MULTI_NUM_DSETS];
    long          *l_rbufi2[MULTI_NUM_DSETS];

    long  *l_total_wbuf2 = NULL;
    short *s_total_rbuf2 = NULL;
    long  *l_wbufi2[MULTI_NUM_DSETS];
    short *s_rbufi2[MULTI_NUM_DSETS];

    s1_t *s1_total_wbuf3 = NULL;
    s3_t *s3_total_rbuf3 = NULL;
    s1_t *s1_wbufi3[MULTI_NUM_DSETS];
    s3_t *s3_rbufi3[MULTI_NUM_DSETS];

    s4_t *s4_total_wbuf3 = NULL;
    s1_t *s1_total_rbuf3 = NULL;
    s4_t *s4_wbufi3[MULTI_NUM_DSETS];
    s1_t *s1_rbufi3[MULTI_NUM_DSETS];

    const void *wbufs[MULTI_NUM_DSETS];
    void       *rbufs[MULTI_NUM_DSETS];

    /* for n niter to ensure that all randomized dset_types with multi_dset_type_t will be covered */
    for (n = 0; n < niter; n++) {

        /* Set up the number of datasets for testing */
        ndsets = MAX(MULTI_MIN_DSETS, MULTI_NUM_DSETS);

        /* Create dataset transfer property list */
        if ((dxpl = H5Pcreate(H5P_DATASET_XFER)) < 0)
            P_TEST_ERROR;

        /* Set selection I/O mode, type of I/O and type of collective I/O */
        set_dxpl(dxpl, H5D_SELECTION_IO_MODE_ON, H5FD_MPIO_COLLECTIVE, H5FD_MPIO_COLLECTIVE_IO);

        /* Set dataset layout: contiguous or chunked */
        dims[0] = DSET_SELECT_DIM;
        if ((dcpl = H5Pcreate(H5P_DATASET_CREATE)) < 0)
            P_TEST_ERROR;

        if (chunked) {
            cdims[0] = DSET_SELECT_CHUNK_DIM;
            if (H5Pset_chunk(dcpl, 1, cdims) < 0)
                P_TEST_ERROR;
        }

        /* Each process takes x number of elements */
        block[0]  = dims[0] / (hsize_t)mpi_size;
        stride[0] = block[0];
        count[0]  = 1;
        start[0]  = (hsize_t)mpi_rank * block[0];

        /* Create compound data type: s1_t  */
        if ((s1_tid = H5Tcreate(H5T_COMPOUND, sizeof(s1_t))) < 0)
            P_TEST_ERROR;

        if (H5Tinsert(s1_tid, "a", HOFFSET(s1_t, a), H5T_NATIVE_INT) < 0 ||
            H5Tinsert(s1_tid, "b", HOFFSET(s1_t, b), H5T_NATIVE_INT) < 0 ||
            H5Tinsert(s1_tid, "c", HOFFSET(s1_t, c), H5T_NATIVE_INT) < 0 ||
            H5Tinsert(s1_tid, "d", HOFFSET(s1_t, d), H5T_NATIVE_INT) < 0)
            P_TEST_ERROR;

        /* Create compound data type: s3_t  */
        if ((s3_tid = H5Tcreate(H5T_COMPOUND, sizeof(s3_t))) < 0)
            P_TEST_ERROR;

        if (H5Tinsert(s3_tid, "a", HOFFSET(s3_t, a), H5T_NATIVE_INT) < 0 ||
            H5Tinsert(s3_tid, "b", HOFFSET(s3_t, b), H5T_NATIVE_INT) < 0 ||
            H5Tinsert(s3_tid, "c", HOFFSET(s3_t, c), H5T_NATIVE_INT) < 0 ||
            H5Tinsert(s3_tid, "d", HOFFSET(s3_t, d), H5T_NATIVE_INT) < 0)
            P_TEST_ERROR;

        /* Create compound data type: s4_t  */
        if ((s4_tid = H5Tcreate(H5T_COMPOUND, sizeof(s4_t))) < 0)
            P_TEST_ERROR;

        if (H5Tinsert(s4_tid, "b", HOFFSET(s4_t, b), H5T_NATIVE_UINT) < 0 ||
            H5Tinsert(s4_tid, "d", HOFFSET(s4_t, d), H5T_NATIVE_UINT) < 0)
            P_TEST_ERROR;

        /* Create dataset for i ndsets */
        for (i = 0; i < (int)ndsets; i++) {

            /* File space ids */
            if ((file_sids[i] = H5Screate_simple(1, dims, NULL)) < 0)
                P_TEST_ERROR;

            /* Memory space ids */
            if ((mem_sids[i] = H5Screate_simple(1, block, NULL)) < 0)
                P_TEST_ERROR;

            mm = HDrandom() % (int)ndsets;
            if (mm == 0) {
                dset_types[i] = DSET_WITH_NO_CONV;
                if (snprintf(dset_names[i], DSET_NAME_LEN,
                             chunked ? "multi_all_chk_nconv_dset%u" : "multi_all_contig_nconv_dset%u", i) < 0)
                    P_TEST_ERROR;
                if ((dset_dids[i] = H5Dcreate2(fid, dset_names[i], H5T_NATIVE_INT, file_sids[i], H5P_DEFAULT,
                                               dcpl, H5P_DEFAULT)) < 0)
                    P_TEST_ERROR;
            }
            else if (mm == 1) {
                dset_types[i] = DSET_WITH_CONV_AND_NO_BKG;
                if (snprintf(dset_names[i], DSET_NAME_LEN,
                             chunked ? "multi_all_chk_conv_nbkg_dset%u" : "multi_all_contig_conv_nbkg_dset%u",
                             i) < 0)
                    P_TEST_ERROR;
                if ((dset_dids[i] = H5Dcreate2(fid, dset_names[i], H5T_NATIVE_LONG, file_sids[i], H5P_DEFAULT,
                                               dcpl, H5P_DEFAULT)) < 0)
                    P_TEST_ERROR;
            }
            else {
                dset_types[i] = DSET_WITH_CONV_AND_BKG;
                if (snprintf(dset_names[i], DSET_NAME_LEN,
                             chunked ? "multi_all_chk_conv_bkg_dset%u" : "multi_all_contig_conv_bkg_dset%u",
                             i) < 0)
                    P_TEST_ERROR;
                if ((dset_dids[i] = H5Dcreate2(fid, dset_names[i], s1_tid, file_sids[i], H5P_DEFAULT, dcpl,
                                               H5P_DEFAULT)) < 0)
                    P_TEST_ERROR;
            }

            if (H5Sselect_hyperslab(file_sids[i], H5S_SELECT_SET, start, stride, count, block) < 0)
                P_TEST_ERROR;

        } /* end for i ndsets */

        /* Allocate buffers for all datasets */

        /* DSET_WITH_NO_CONV */
        buf_size = ndsets * DSET_SELECT_DIM * sizeof(int);
        if (NULL == (total_wbuf1 = (int *)HDmalloc(buf_size)))
            P_TEST_ERROR;
        if (NULL == (total_rbuf1 = (int *)HDmalloc(buf_size)))
            P_TEST_ERROR;

        /* DSET_WITH_CONV_AND_NO_BKG */
        buf_size = ndsets * DSET_SELECT_DIM * sizeof(unsigned long);
        if (NULL == (ul_total_wbuf2 = (unsigned long *)HDmalloc(buf_size)))
            P_TEST_ERROR;
        buf_size = ndsets * DSET_SELECT_DIM * sizeof(long);
        if (NULL == (l_total_rbuf2 = (long *)HDmalloc(buf_size)))
            P_TEST_ERROR;

        buf_size = ndsets * DSET_SELECT_DIM * sizeof(long);
        if (NULL == (l_total_wbuf2 = (long *)HDmalloc(buf_size)))
            P_TEST_ERROR;
        buf_size = ndsets * DSET_SELECT_DIM * sizeof(short);
        if (NULL == (s_total_rbuf2 = (short *)HDmalloc(buf_size)))
            P_TEST_ERROR;

        /* DSET_WITH_CONV_AND_BKG */
        buf_size = ndsets * DSET_SELECT_DIM * sizeof(s1_t);
        if (NULL == (s1_total_wbuf3 = (s1_t *)HDmalloc(buf_size)))
            P_TEST_ERROR;
        buf_size = ndsets * DSET_SELECT_DIM * sizeof(s3_t);
        if (NULL == (s3_total_rbuf3 = (s3_t *)HDmalloc(buf_size)))
            P_TEST_ERROR;

        buf_size = ndsets * DSET_SELECT_DIM * sizeof(s4_t);
        if (NULL == (s4_total_wbuf3 = (s4_t *)HDmalloc(buf_size)))
            P_TEST_ERROR;
        buf_size = ndsets * DSET_SELECT_DIM * sizeof(s1_t);
        if (NULL == (s1_total_rbuf3 = (s1_t *)HDmalloc(buf_size)))
            P_TEST_ERROR;

        /* Test with s settings for ndsets */
        for (s = SETTING_A; s <= SETTING_B; s++) {

            /* for i ndsets */
            for (i = 0; i < (int)ndsets; i++) {

                switch (dset_types[i]) {

                    case DSET_WITH_NO_CONV:
                        /* Initialize buffer indices */
                        wbufi1[i] = total_wbuf1 + (i * DSET_SELECT_DIM);
                        rbufi1[i] = total_rbuf1 + (i * DSET_SELECT_DIM);

                        wbufs[i] = wbufi1[i];
                        rbufs[i] = rbufi1[i];

                        /* Initialize the buffer data */
                        for (j = 0; j < (int)block[0]; j++)
                            wbufi1[i][j] = j + (int)start[0];

                        /* Same for all cases */
                        mem_tids[i]   = H5T_NATIVE_INT;
                        r_mem_tids[i] = H5T_NATIVE_INT;

                        break;

                    case DSET_WITH_CONV_AND_NO_BKG:
                        if (s == SETTING_A) {
                            /* Initialize buffer indices */
                            ul_wbufi2[i] = ul_total_wbuf2 + (i * DSET_SELECT_DIM);
                            l_rbufi2[i]  = l_total_rbuf2 + (i * DSET_SELECT_DIM);

                            wbufs[i] = ul_wbufi2[i];
                            rbufs[i] = l_rbufi2[i];

                            for (j = 0; j < (int)block[0]; j++)
                                ul_wbufi2[i][j] = ULONG_MAX - (unsigned long)(j + (int)start[0]);

                            mem_tids[i]   = H5T_NATIVE_ULONG;
                            r_mem_tids[i] = H5T_NATIVE_LONG;
                        }
                        else if (s == SETTING_B) {
                            /* Initialize buffer indices */
                            l_wbufi2[i] = l_total_wbuf2 + (i * DSET_SELECT_DIM);
                            s_rbufi2[i] = s_total_rbuf2 + (i * DSET_SELECT_DIM);

                            wbufs[i] = l_wbufi2[i];
                            rbufs[i] = s_rbufi2[i];

                            /* Initialize the buffer data */
                            for (j = 0; j < (int)block[0]; j++)
                                l_wbufi2[i][j] = LONG_MAX - (long)(j + (int)start[0]);

                            mem_tids[i]   = H5T_NATIVE_LONG;
                            r_mem_tids[i] = H5T_NATIVE_SHORT;
                        }

                        break;

                    case DSET_WITH_CONV_AND_BKG:

                        if (s == SETTING_A) {
                            /* Initialize buffer indices */
                            s1_wbufi3[i] = s1_total_wbuf3 + (i * DSET_SELECT_DIM);
                            s3_rbufi3[i] = s3_total_rbuf3 + (i * DSET_SELECT_DIM);

                            wbufs[i] = s1_wbufi3[i];
                            rbufs[i] = s3_rbufi3[i];

                            /* Initialize buffer data for s1_t */
                            for (j = 0; j < (int)block[0]; j++) {
                                s1_wbufi3[i][j].a = (4 * j + (int)start[0]);
                                s1_wbufi3[i][j].b = (4 * j + (int)start[0]) + 1;
                                s1_wbufi3[i][j].c = (4 * j + (int)start[0]) + 2;
                                s1_wbufi3[i][j].d = (4 * j + (int)start[0]) + 3;
                            }
                            mem_tids[i]   = s1_tid;
                            r_mem_tids[i] = s3_tid;
                        }
                        else if (s == SETTING_B) {
                            /* Initialize buffer indices */
                            s4_wbufi3[i] = s4_total_wbuf3 + (i * DSET_SELECT_DIM);
                            s1_rbufi3[i] = s1_total_rbuf3 + (i * DSET_SELECT_DIM);

                            wbufs[i] = s4_wbufi3[i];
                            rbufs[i] = s1_rbufi3[i];

                            /* Initialize buffer data for s4_t */
                            for (j = 0; j < (int)block[0]; j++) {
                                s4_wbufi3[i][j].b = DSET_SELECT_DIM + (unsigned int)(j + (int)start[0]);
                                s4_wbufi3[i][j].d = DSET_SELECT_DIM + (unsigned int)(j + (int)start[0]);
                            }
                            mem_tids[i]   = s4_tid;
                            r_mem_tids[i] = s1_tid;
                        }

                        break;

                    case DSET_NTTYPES:
                    default:
                        P_TEST_ERROR;

                } /* end switch dset_types */

            } /* end for i ndsets */

            if (H5Dwrite_multi(ndsets, dset_dids, mem_tids, mem_sids, file_sids, dxpl, wbufs) < 0)
                P_TEST_ERROR;
            if (H5Dread_multi(ndsets, dset_dids, r_mem_tids, mem_sids, file_sids, dxpl, rbufs) < 0)
                P_TEST_ERROR;

            check_io_mode(dxpl, chunked);

            /* Verify result read */
            /* for i ndsets */
            for (i = 0; i < (int)ndsets; i++) {
                switch (dset_types[i]) {

                    case DSET_WITH_NO_CONV:
                        for (j = 0; j < (int)block[0]; j++)
                            if (rbufi1[i][j] != wbufi1[i][j]) {
                                nerrors++;
                                HDprintf("    Read different values than written.\n");
                                HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                                break;
                            }

                        break;

                    case DSET_WITH_CONV_AND_NO_BKG:
                        if (s == SETTING_A) {
                            for (j = 0; j < (int)block[0]; j++)
                                if (l_rbufi2[i][j] != LONG_MAX) {
                                    nerrors++;
                                    HDprintf("    Read different values than written.\n");
                                    HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                                    break;
                                }
                        }
                        else if (s == SETTING_B) {
                            for (j = 0; j < (int)block[0]; j++)
                                if (s_rbufi2[i][j] != SHRT_MAX) {
                                    nerrors++;
                                    HDprintf("    Read different values than written.\n");
                                    HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                                    break;
                                }
                        }

                        break;

                    case DSET_WITH_CONV_AND_BKG:
                        if (s == SETTING_A) {
                            for (j = 0; j < (int)block[0]; j++)
                                if (s3_rbufi3[i][j].a != s1_wbufi3[i][j].a ||
                                    s3_rbufi3[i][j].b != s1_wbufi3[i][j].b ||
                                    s3_rbufi3[i][j].c != s1_wbufi3[i][j].c ||
                                    s3_rbufi3[i][j].d != s1_wbufi3[i][j].d) {
                                    nerrors++;
                                    HDprintf("    Read different values than written.\n");
                                    HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                                    break;
                                }
                        }
                        else if (s == SETTING_B) {
                            for (j = 0; j < (int)block[0]; j++)
                                if (s1_rbufi3[i][j].a != s1_wbufi3[i][j].a ||
                                    s1_rbufi3[i][j].b != (DSET_SELECT_DIM + j + (int)start[0]) ||
                                    s1_rbufi3[i][j].c != s1_wbufi3[i][j].c ||
                                    s1_rbufi3[i][j].d != (DSET_SELECT_DIM + j + (int)start[0])) {
                                    nerrors++;
                                    HDprintf("    Read different values than written.\n");
                                    HDprintf("    For dset %d at index %d\n", i, j + (int)start[0]);
                                    break;
                                }
                        }

                        break;

                    case DSET_NTTYPES:
                    default:
                        P_TEST_ERROR;

                } /* end switch dset_types */

            } /* end for i ndsets */

        } /* end for s settings */

        /* Closing */
        if (H5Pclose(dcpl) < 0)
            P_TEST_ERROR;
        if (H5Pclose(dxpl) < 0)
            P_TEST_ERROR;

        if (H5Tclose(s1_tid) < 0)
            P_TEST_ERROR;
        if (H5Tclose(s3_tid) < 0)
            P_TEST_ERROR;
        if (H5Tclose(s4_tid) < 0)
            P_TEST_ERROR;

        for (i = 0; i < (int)ndsets; i++) {
            if (H5Sclose(file_sids[i]) < 0)
                P_TEST_ERROR;
            if (H5Dclose(dset_dids[i]) < 0)
                P_TEST_ERROR;
            /* Don't delete the last set of datasets */
            if ((n + 1) != niter)
                if (H5Ldelete(fid, dset_names[i], H5P_DEFAULT) < 0)
                    P_TEST_ERROR;
        }

        /* Freeing */
        HDfree(total_wbuf1);
        HDfree(total_rbuf1);

        HDfree(ul_total_wbuf2);
        HDfree(l_total_rbuf2);
        HDfree(l_total_wbuf2);
        HDfree(s_total_rbuf2);

        HDfree(s1_total_wbuf3);
        HDfree(s3_total_rbuf3);
        HDfree(s4_total_wbuf3);
        HDfree(s1_total_rbuf3);

    } /* end for n niter */

    CHECK_PASSED();

    return;

} /* test_multi_dsets_all() */

/*-------------------------------------------------------------------------
 * Function:    main
 *
 * Purpose:     Runs tests with all combinations of configuration
 *              flags.
 *
 * Return:      Success:        0
 *              Failure:        1
 *
 *-------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
    int      ret;
    hid_t    fapl = H5I_INVALID_HID;
    hid_t    fid  = H5I_INVALID_HID;
    int      test_select_config;
    unsigned chunked;
    unsigned dtrans;

    h5_reset();

    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    if ((fapl = H5Pcreate(H5P_FILE_ACCESS)) < 0)
        P_TEST_ERROR;

    /* Set MPIO file driver */
    if (H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL) < 0)
        P_TEST_ERROR;

    if ((fid = H5Fcreate(FILENAME, H5F_ACC_TRUNC, H5P_DEFAULT, fapl)) < 0)
        P_TEST_ERROR;

    /* Test with contiguous or chunked dataset */
    for (chunked = FALSE; chunked <= TRUE; chunked++) {

        /* Data transforms only apply to integer or floating-point datasets */
        /* therefore, not all tests are run with data transform */
        for (dtrans = FALSE; dtrans <= TRUE; dtrans++) {

            if (MAINPROCESS) {
                if (chunked) {
                    if (dtrans)
                        HDputs("\nTesting for selection I/O with chunked dataset and data transform\n");
                    else
                        HDputs("\nTesting for selection I/O with chunked dataset and no data transform\n");
                }
                else {
                    if (dtrans)
                        HDputs("\nTesting for selection I/O with contiguous dataset and data transform\n");
                    else
                        HDputs("\nTesting for selection I/O with contiguous dataset and no data transform\n");
                }
            }

            for (test_select_config = (int)TEST_NO_TYPE_CONV; test_select_config < (int)TEST_SELECT_NTESTS;
                 test_select_config++) {

                switch (test_select_config) {

                    case TEST_NO_TYPE_CONV: /* case 1 */
                        if (MAINPROCESS)
                            TESTING_2("No type conversion (null case)");

                        test_no_type_conv(fid, chunked, dtrans);

                        break;

                    case TEST_NO_SIZE_CHANGE_NO_BKG: /* case 2 */
                        if (MAINPROCESS)
                            TESTING_2("No size change, no background buffer");

                        /* Data transforms does not apply to the dataset datatype for this test */
                        if (dtrans) {
                            if (MAINPROCESS)
                                SKIPPED();
                            continue;
                        }

                        test_no_size_change_no_bkg(fid, chunked);

                        break;

                    case TEST_LARGER_MEM_NO_BKG: /* case 3 */
                        if (MAINPROCESS)
                            TESTING_2("Larger memory type, no background buffer");

                        test_larger_mem_type_no_bkg(fid, chunked, dtrans);

                        break;

                    case TEST_SMALLER_MEM_NO_BKG: /* case 4 */
                        if (MAINPROCESS)
                            TESTING_2("Smaller memory type, no background buffer");

                        test_smaller_mem_type_no_bkg(fid, chunked, dtrans);

                        break;

                    case TEST_CMPD_WITH_BKG: /* case 5 */
                        if (MAINPROCESS)
                            TESTING_2("Compound types with background buffer");
                        /* Data transforms does not apply to the dataset datatype for this test */
                        if (dtrans) {
                            if (MAINPROCESS)
                                SKIPPED();
                            continue;
                        }

                        test_cmpd_with_bkg(fid, chunked);

                        break;

                    case TEST_TYPE_CONV_SEL_EMPTY: /* case 6 */
                        if (MAINPROCESS)
                            TESTING_2("Empty selections + Type conversion");

                        test_type_conv_sel_empty(fid, chunked, dtrans);

                        break;

                    case TEST_MULTI_CONV_NO_BKG: /* case 7 */
                        if (MAINPROCESS)
                            TESTING_2("multi-datasets: type conv + no bkg buffer");

                        test_multi_dsets_no_bkg(fid, chunked, dtrans);

                        break;

                    case TEST_MULTI_CONV_BKG: /* case 8 */
                        if (MAINPROCESS)
                            TESTING_2("multi-datasets: type conv + bkg buffer");

                        /* Data transforms does not apply to the dataset datatype for this test */
                        if (dtrans) {
                            if (MAINPROCESS)
                                SKIPPED();
                        }
                        else
                            test_multi_dsets_cmpd_with_bkg(fid, chunked);

                        break;

                    case TEST_MULTI_CONV_SIZE_CHANGE: /* case 9 */
                        if (MAINPROCESS)
                            TESTING_2("multi-datasets: type conv + size change + no bkg buffer");

                        /* Data transforms does not apply to the dataset datatype for this test */
                        if (dtrans) {
                            if (MAINPROCESS)
                                SKIPPED();
                        }
                        else
                            test_multi_dsets_size_change_no_bkg(fid, chunked);

                        break;

                    case TEST_MULTI_CONV_SEL_EMPTY: /* case 10 */
                        if (MAINPROCESS)
                            TESTING_2("multi-datasets: type conv + empty selections");

                        test_multi_dsets_conv_sel_empty(fid, chunked, dtrans);

                        break;

                    case TEST_MULTI_ALL: /* case 11 */
                        if (MAINPROCESS)
                            TESTING_2("multi-datasets: no conv + conv without bkg + conv with bkg");

                        /* Data transforms does not apply to the dataset datatype for this test */
                        if (dtrans) {
                            if (MAINPROCESS)
                                SKIPPED();
                        }
                        else
                            test_multi_dsets_all(2, fid, chunked);

                        break;

                    case TEST_SELECT_NTESTS:
                    default:
                        P_TEST_ERROR;
                        break;

                } /* end switch */

            } /* end for test_select_config */

        } /* end dtrans */
    }     /* end chunked */

    if (H5Pclose(fapl) < 0)
        P_TEST_ERROR;

    if (H5Fclose(fid) < 0)
        P_TEST_ERROR;

    /* Barrier to make sure all ranks are done before deleting the file, and
     * also to clean up output (make sure PASSED is printed before any of the
     * following messages) */
    if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS)
        P_TEST_ERROR;

    /* Delete file */
    if (MAINPROCESS)
        if (MPI_File_delete(FILENAME, MPI_INFO_NULL) != MPI_SUCCESS)
            P_TEST_ERROR;

    /* Gather errors from all processes */
    MPI_Allreduce(&nerrors, &ret, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    nerrors = ret;

    if (MAINPROCESS) {
        printf("\n===================================\n");
        if (nerrors)
            HDprintf("***Parallel selection I/O dataset tests detected %d errors***\n", nerrors);
        else
            HDprintf("Parallel selection I/O dataset tests finished with no errors\n");
        printf("===================================\n");
    }

    /* close HDF5 library */
    H5close();

    /* MPI_Finalize must be called AFTER H5close which may use MPI calls */
    MPI_Finalize();

    /* cannot just return (nerrors) because exit code is limited to 1 byte */
    return (nerrors != 0);
} /* end main() */