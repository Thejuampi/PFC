/********************************************************************
 * 	SpeedIT library
 * 	VER: 2.1
 * 	Copyrights 2010-2012 Vratis Lts.
 * 	email: support@vratis.com
 *
 * SpeedIT is utilising CUSP 0.3.0 for AINV and AMG preconditioners
 * CUSP 0.3.0 is distributed under APACHE license
 *
 *  Copyright 2008-2009 NVIDIA Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef __SPEEDIT_EX_H__
#define __SPEEDIT_EX_H__


#include <vector_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32

  #ifdef WINDLL
    //#error You should not compile this file as a part of dll library!
    #define DLLAPI __declspec(dllexport)
  #else
    #define DLLAPI __declspec(dllimport)
  #endif

#else
 #define DLLAPI
#endif

/*
  Error handling
*/

#define CUBLAS_ERR_BASE 10000

/*
 Matrix Handlers
 Version 2.0 introduces new matrix format named CMR - Compressed Multiple Row format by Z. Koza.
 CMR format is used for real single and double precision numbers.
*/
#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
/*
 Declaration of handlers for matrix formats
*/
DECLARE_HANDLE	(SI_CSR_DOUBLE_HANDLE);
DECLARE_HANDLE	(SI_CSR_FLOAT_HANDLE);
DECLARE_HANDLE	(SI_CSR_INT_HANDLE);
DECLARE_HANDLE	(SI_CSR_SCOMPLEX_HANDLE);
DECLARE_HANDLE	(SI_CSR_DCOMPLEX_HANDLE);
DECLARE_HANDLE	(SI_CMR_DOUBLE_HANDLE);
DECLARE_HANDLE	(SI_CMR_FLOAT_HANDLE);

DECLARE_HANDLE (SI_CSR_DOUBLE_AMG_HANDLE);
DECLARE_HANDLE (SI_CSR_DOUBLE_AINV_HANDLE);
DECLARE_HANDLE (SI_CSR_DOUBLE_AINV_NSYM_HANDLE);
DECLARE_HANDLE (SI_CSR_DOUBLE_AINV_SCALED_HANDLE);
DECLARE_HANDLE (SI_CSR_DOUBLE_DIAGONAL_HANDLE);
DECLARE_HANDLE (SI_CSR_DOUBLE_VOID_HANDLE);

DECLARE_HANDLE (SI_CSR_FLOAT_AMG_HANDLE);
DECLARE_HANDLE (SI_CSR_FLOAT_AINV_HANDLE);
DECLARE_HANDLE (SI_CSR_FLOAT_AINV_NSYM_HANDLE);
DECLARE_HANDLE (SI_CSR_FLOAT_AINV_SCALED_HANDLE);
DECLARE_HANDLE (SI_CSR_FLOAT_DIAGONAL_HANDLE);
DECLARE_HANDLE (SI_CSR_FLOAT_VOID_HANDLE);


DECLARE_HANDLE (SI_CMR_DOUBLE_DIAGONAL_HANDLE);
DECLARE_HANDLE (SI_CMR_DOUBLE_VOID_HANDLE);

DECLARE_HANDLE (SI_CMR_FLOAT_DIAGONAL_HANDLE);
DECLARE_HANDLE (SI_CMR_FLOAT_VOID_HANDLE);

//DECLARE_HANDLE (SI_CSR_DIAGONAL_HANDLE);
//DECLARE_HANDLE (SI_CSR_VOID_HANDLE);
//DECLARE_HANDLE (SI_CMR_DIAGONAL_HANDLE);
//DECLARE_HANDLE (SI_CMR_VOID_HANDLE);

enum {
    ERR_OK                      = 0,
    ERR_KO                      = 1,
    ERR_GPU_MEM_ALLOC           = 20000,
    ERR_BAD_GPU_POINTER         = 20001,
    ERR_WRONG_COPY_DIRECTION    = 20002,
    ERR_DOUBLE_UNSUPPORTED      = 20003,
    ERR_BICGSTAB_FAILED         = 20004,
    ERR_OMEGA_VANISHED          = 20005,
    ERR_TOO_LITTLE_ITER         = 20006,
    ERR_ZERO_ON_DIAGONAL        = 20007,
    ERR_CUDA_INVALID_VALUE      = 20008,

    ERR_INVALID_PARAMETER 		= 20009,
    ERR_INVALID_PRECOND         = 30000,

    ERR_INVALID_ORIGIN_TYPE   	= 40000,
	//FROM MATRIX READER
    ERR_MM_FILE_OPENING                 = 50000,
    ERR_MM_WRONG_BANNER                 = 50001,
    ERR_MM_UNSUPPORTED_MATRIX_FORMAT    = 50002,
    ERR_MM_UNSUPPORTED_DATA_TYPE        = 50003,
    ERR_MM_READING_FAILED               = 50004,

    ERR_LICENSE_NOT_FOUND = 60000,
    ERR_CLIENT_KEY_NOT_FOUND,
    ERR_WRONG_AUTH_FILE,
    ERR_INVALID_CLIENT_KEY,
    ERR_INVALID_CLIENT_KEY_INTEGRITY,
    ERR_INVALID_LICENSE,
    ERR_INVALID_EXP_LICENSE,
    ERR_LICENSE_EXPIRED,
    ERR_NOT_LICENSED_HARDWARE,

    ERR_UNKNOWN               = 20999
} ;

typedef float2 si_scomplex;
typedef double2 si_dcomplex;


DLLAPI
const char* si_errstr(int err_code) ;


//TODO: Deprecated. We are moving to handlers.
typedef enum {
  P_NONE = 0,
  P_DIAG,
  P_AINV_B,
  P_AINV_SB,
  P_AINV_NB,
  P_AMG
} PRECOND_TYPE ;

typedef enum {
	HOST = 0,
	DEVICE
	
} SI_ORIGIN;


/*
  Initialization
  If you will not specify the device by default (device = -1) 
	the speedit will search for best device for use.
*/
DLLAPI
int si_init (int device = -1) ;
DLLAPI
int si_shutdown (void) ;

/*
  Matrix handler operations
*/

/*
 * Create matrix handler from give ndata.
 * SI_ORIGIN: HOST - data will be taken from host and copied to GPU
 * 					 DEVICE -  Function will create handler to this data
                                they will not be copied again in GPU memory to save storage
 *
 * Host data are not removed from memory in case HOST origin
 */

//CRS on double type
DLLAPI
SI_CSR_DOUBLE_HANDLE si_dhcsr (double* values, int* col_idx, int* row_offset, int rows, int cols, int nnz, SI_ORIGIN origin);

//CRS on float type
DLLAPI
SI_CSR_FLOAT_HANDLE si_shcsr (float* values, int* col_idx, int* row_offset, int rows, int cols, int nnz, SI_ORIGIN origin);

//CRS on integer type
DLLAPI
SI_CSR_INT_HANDLE si_ihcsr(int* values, int* col_idx, int* row_offset, int rows, int cols, int nnz, SI_ORIGIN origin);

//CRS on complex float type
DLLAPI
SI_CSR_SCOMPLEX_HANDLE si_schcsr(si_scomplex* values, int* col_idx, int* row_offset, int rows, int cols, int nnz, SI_ORIGIN origin);

//CRS on complex double type
DLLAPI
SI_CSR_DCOMPLEX_HANDLE si_dchcsr(si_dcomplex* values, int* col_idx, int* row_offset, int rows, int cols, int nnz, SI_ORIGIN origin); 

//CMR on double type 
DLLAPI
SI_CMR_DOUBLE_HANDLE si_dhcmr (double* values, int* col_idx, int* row_offset, int rows, int cols, int nnz, SI_ORIGIN origin);

//CMR on float type
DLLAPI
SI_CMR_FLOAT_HANDLE si_shcmr (float* values, int* col_idx, int* row_offset, int rows, int cols, int nnz, SI_ORIGIN origin);

/*
 * Release handlers
 * Removes data from memory
 */
DLLAPI
int si_dhreleasecsr(SI_CSR_DOUBLE_HANDLE* handle);

DLLAPI
int si_shreleasecsr(SI_CSR_FLOAT_HANDLE* handle);

DLLAPI
int si_ihreleasecsr (SI_CSR_INT_HANDLE* handle);

DLLAPI
int si_dchreleasecsr(SI_CSR_DCOMPLEX_HANDLE* handle);

DLLAPI
int si_schreleasecsr (SI_CSR_SCOMPLEX_HANDLE* handle);

DLLAPI
int si_dhreleasecmr(SI_CMR_DOUBLE_HANDLE* handle);

DLLAPI
int si_shreleasecmr(SI_CMR_FLOAT_HANDLE* handle);

/*
    CMR matrix options
 */
DLLAPI
int si_sortmatrix(bool sort);

/*
 * HANDLERS FOR PRECONDITIONERS
*/
 /* AMG */
DLLAPI
SI_CSR_DOUBLE_AMG_HANDLE si_gdcsramg( const SI_CSR_DOUBLE_HANDLE matrix_handle, double theta = 0);

DLLAPI
SI_CSR_FLOAT_AMG_HANDLE si_gscsramg( const SI_CSR_FLOAT_HANDLE matrix_handle, float theta = 0);

/* AINV */
DLLAPI
SI_CSR_DOUBLE_AINV_HANDLE si_gdcsrainv( const SI_CSR_DOUBLE_HANDLE matrix_handle,double drop_tolerance = 0.1,
		int nonzero_per_row = -1,
		bool lin_dropping = false,
		int  lin_param = 1);

DLLAPI
SI_CSR_FLOAT_AINV_HANDLE si_gscsrainv( const SI_CSR_FLOAT_HANDLE matrix_handle,
			float drop_tolerance = 0.1,
			int nonzero_per_row = -1,
			bool lin_dropping = false,
			int  lin_param = 1);
DLLAPI
SI_CSR_DOUBLE_AINV_SCALED_HANDLE si_gdcsrainvscaled( const SI_CSR_DOUBLE_HANDLE matrix_handle,
			double drop_tolerance = 0.1,
			int nonzero_per_row = -1,
			bool lin_dropping = false,
			int  lin_param = 1);

DLLAPI
SI_CSR_FLOAT_AINV_SCALED_HANDLE si_gscsrainvscaled( const SI_CSR_FLOAT_HANDLE matrix_handle,
			float drop_tolerance = 0.1,
			int nonzero_per_row = -1,
			bool lin_dropping = false,
			int  lin_param = 1);
DLLAPI
SI_CSR_DOUBLE_AINV_NSYM_HANDLE si_gdcsrainvnsym( const SI_CSR_DOUBLE_HANDLE matrix_handle,
			double drop_tolerance = 0.1,
			int nonzero_per_row = -1,
			bool lin_dropping = false,
			int  lin_param = 1);

DLLAPI
SI_CSR_FLOAT_AINV_NSYM_HANDLE si_gscsrainvnsym( const SI_CSR_FLOAT_HANDLE matrix_handle,
			float drop_tolerance = 0.1,
			int nonzero_per_row = -1,
			bool lin_dropping = false,
			int  lin_param = 1);

/* DIAGONAL */
DLLAPI
SI_CSR_DOUBLE_DIAGONAL_HANDLE si_gdcsrdiagonal( const SI_CSR_DOUBLE_HANDLE matrix_handle );

DLLAPI
SI_CSR_FLOAT_DIAGONAL_HANDLE si_gscsrdiagonal( const SI_CSR_FLOAT_HANDLE matrix_handle );

DLLAPI
SI_CMR_DOUBLE_DIAGONAL_HANDLE si_gdcmrdiagonal( const SI_CMR_DOUBLE_HANDLE matrix_handle );

DLLAPI
SI_CMR_FLOAT_DIAGONAL_HANDLE si_gscmrdiagonal( const SI_CMR_FLOAT_HANDLE matrix_handle );

/* VOID */
DLLAPI
SI_CSR_DOUBLE_VOID_HANDLE si_gdcsrvoid( const SI_CSR_DOUBLE_HANDLE matrix_handle);

DLLAPI
SI_CSR_FLOAT_VOID_HANDLE si_gscsrvoid( const SI_CSR_FLOAT_HANDLE matrix_handle );

DLLAPI
SI_CMR_DOUBLE_VOID_HANDLE si_gdcmrvoid(const SI_CMR_DOUBLE_HANDLE matrix_handle);

DLLAPI
SI_CMR_FLOAT_VOID_HANDLE si_gscmrvoid(const SI_CMR_FLOAT_HANDLE matrix_handle);


/* RELEASE FUNCTIONS FOR PRECONDITIONERS*/

DLLAPI
int si_dhcsrfreevoid(SI_CSR_DOUBLE_VOID_HANDLE* precond_handle);

DLLAPI
int si_dhcsrfreediagonal(SI_CSR_DOUBLE_DIAGONAL_HANDLE* precond_handle);

DLLAPI
int si_dhcsrfreeamg(SI_CSR_DOUBLE_AMG_HANDLE* precond_handle);

DLLAPI
int si_dhcsrfreeainv(SI_CSR_DOUBLE_AINV_HANDLE* precond_handle);

DLLAPI
int si_dhcsrfreeainvscaled(SI_CSR_DOUBLE_AINV_SCALED_HANDLE* precond_handle);

DLLAPI
int si_dhcsrfreeainvnsym(SI_CSR_DOUBLE_AINV_NSYM_HANDLE* precond_handle);

//float

DLLAPI
int si_shcsrfreevoid(SI_CSR_FLOAT_VOID_HANDLE* precond_handle);

DLLAPI
int si_shcsrfreediagonal(SI_CSR_FLOAT_DIAGONAL_HANDLE* precond_handle);

DLLAPI
int si_shcsrfreeamg(SI_CSR_FLOAT_AMG_HANDLE* precond_handle);

DLLAPI
int si_shcsrfreeainv(SI_CSR_FLOAT_AINV_HANDLE* precond_handle);

DLLAPI
int si_shcsrfreeainvscaled(SI_CSR_FLOAT_AINV_SCALED_HANDLE* precond_handle);

DLLAPI
int si_shcsrfreeainvnsym(SI_CSR_FLOAT_AINV_NSYM_HANDLE* precond_handle);


//CMR
DLLAPI
int si_dhcmrfreediagonal(SI_CMR_DOUBLE_DIAGONAL_HANDLE* precond_handle);
DLLAPI
int si_dhcmrfreevoid(SI_CMR_DOUBLE_VOID_HANDLE* precond_handle);
DLLAPI
int si_shcmrfreediagonal(SI_CMR_FLOAT_DIAGONAL_HANDLE* precond_handle);
DLLAPI
int si_shcmrfreevoid(SI_CMR_FLOAT_VOID_HANDLE* precond_handle);


/*BICGStab solver with preconditioners from CUSP 0.3.0*/
//CMR
//CPU
DLLAPI
int si_cdhcmrbicgstabdiagonal(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcmrbicgstabvoid(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcmrbicgstabdiagonal(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcmrbicgstabvoid(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);

//GPU
DLLAPI
int si_gdhcmrbicgstabdiagonal(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcmrbicgstabvoid(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcmrbicgstabdiagonal(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcmrbicgstabvoid(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);

//CSR
//CPU
DLLAPI
int si_cdhcsrbicgstabamg(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrbicgstabainv(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrbicgstabainvscaled(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrbicgstabainvnsym(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrbicgstabdiagonal(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrbicgstabvoid(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);

DLLAPI
int si_cshcsrbicgstabamg(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrbicgstabainv(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrbicgstabainvscaled(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrbicgstabainvnsym(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrbicgstabdiagonal(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrbicgstabvoid(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);

//GPU
DLLAPI
int si_gdhcsrbicgstabamg(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrbicgstabainv(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrbicgstabainvscaled(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrbicgstabainvnsym(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrbicgstabdiagonal(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrbicgstabvoid(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);

DLLAPI
int si_gshcsrbicgstabamg(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrbicgstabainv(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrbicgstabainvscaled(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrbicgstabainvnsym(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrbicgstabdiagonal(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrbicgstabvoid(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);


/*CG interface functions with preconditioners from cusp 0.3.0*/

//CMR
//CPU
DLLAPI
int si_cdhcmrcgdiagonal(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcmrcgvoid(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcmrcgdiagonal(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcmrcgvoid(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);
//GPU
DLLAPI
int si_gdhcmrcgdiagonal(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcmrcgvoid(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CMR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcmrcgdiagonal(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcmrcgvoid(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CMR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);

//CSR
//CPU
DLLAPI
int si_cdhcsrcgamg(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrcgainv(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrcgainvscaled(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrcgainvnsym(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrcgdiagonal(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cdhcsrcgvoid(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrcgamg(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrcgainv(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrcgainvscaled(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrcgainvnsym(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrcgdiagonal(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_cshcsrcgvoid(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);
//GPU
DLLAPI
int si_gdhcsrcgamg(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrcgainv(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrcgainvscaled(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrcgainvnsym(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrcgdiagonal(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
		SI_CSR_DOUBLE_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gdhcsrcgvoid(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b,
        SI_CSR_DOUBLE_VOID_HANDLE precond, int* n_iter, double* eps);

DLLAPI
int si_gshcsrcgamg(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AMG_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrcgainv(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_AINV_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrcgainvscaled(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_SCALED_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrcgainvnsym(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_AINV_NSYM_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrcgdiagonal(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
		SI_CSR_FLOAT_DIAGONAL_HANDLE precond, int* n_iter, double* eps);
DLLAPI
int si_gshcsrcgvoid(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b,
        SI_CSR_FLOAT_VOID_HANDLE precond, int* n_iter, double* eps);


/*
  Memory management functions.
*/

// Allocate GPU buffer
//
DLLAPI
int si_gsmalloc(int size, float**  out_ptr) ;
DLLAPI
int si_gdmalloc(int size, double** out_ptr) ;
DLLAPI
int si_gimalloc(int size, int**    out_ptr) ;
DLLAPI
int si_gvmalloc(int size, void**   out_ptr) ;
//JPA
DLLAPI
int si_gszmalloc(int size, si_scomplex** out_ptr) ;
DLLAPI
int si_gdzmalloc(int size, si_dcomplex** out_ptr) ;

// Free GPU buffer
//
DLLAPI
int si_gsfree(float**  out_ptr) ;
DLLAPI
int si_gdfree(double** out_ptr) ;
DLLAPI
int si_gifree(int**    out_ptr) ;
DLLAPI
int si_gvfree(void**   out_ptr) ;
//JPA
DLLAPI
int si_gszfree(si_scomplex**   out_ptr) ;
DLLAPI 
int si_gdzfree(si_dcomplex**   out_ptr) ;

// Copy data from CPU to GPU memory
//
DLLAPI
int si_c2gvcopy(int size, const void*   in_ptr, void*   out_ptr) ;
DLLAPI
int si_c2gscopy(int size, const float*  in_ptr, float*  out_ptr) ;
DLLAPI
int si_c2gdcopy(int size, const double* in_ptr, double* out_ptr) ;
DLLAPI
int si_c2gicopy(int size, const int*    in_ptr, int*    out_ptr) ;

//JPA
DLLAPI
int si_c2gszcopy(int size, const si_scomplex* in_ptr, si_scomplex* out_ptr);
DLLAPI
int si_c2gdzcopy(int size, const si_dcomplex* in_ptr, si_dcomplex* out_ptr);


// Copy data from GPU to CPU memory
//
DLLAPI
int si_g2cvcopy(int size, const void*   in_ptr, void*   out_ptr) ;
DLLAPI
int si_g2cscopy(int size, const float*  in_ptr, float*  out_ptr) ;
DLLAPI
int si_g2cdcopy(int size, const double* in_ptr, double* out_ptr) ;
DLLAPI
int si_g2cicopy(int size, const int*    in_ptr, int*    out_ptr) ;
DLLAPI
int si_g2cszcopy(int size, const si_scomplex*    in_ptr, si_scomplex*    out_ptr) ;
DLLAPI
int si_g2cdzcopy(int size, const si_dcomplex*    in_ptr, si_dcomplex*    out_ptr) ;


// Allocate buffer in GPU memory and copy data fro CPU memory
//
DLLAPI
int si_c2gvmcopy(int size, const void*   in_ptr, void**   out_ptr) ;
DLLAPI
int si_c2gsmcopy(int size, const float*  in_ptr, float**  out_ptr) ;
DLLAPI
int si_c2gdmcopy(int size, const double* in_ptr, double** out_ptr) ;
DLLAPI
int si_c2gimcopy(int size, const int*    in_ptr, int**    out_ptr) ;
//JPA
DLLAPI
int si_c2gszmcopy(int size, const si_scomplex* in_ptr, si_scomplex** out_ptr) ;
DLLAPI
int si_c2gdzmcopy(int size, const si_dcomplex* in_ptr, si_dcomplex** out_ptr) ;

/*
  Functions to load matrix from mtx format
  */
//DLLAPI
//int si_gdload(const char* filename, int& nnz, int& n_rows, int& n_cols, double** values, int** r_idx, int** c_idx);

DLLAPI
int si_cdload(const char* filename, int& nnz, int& n_rows, int& n_cols, double** values, int** r_idx, int** c_idx);

//DLLAPI
//int si_gsload(const char* filename, int& nnz, int& n_rows, int& n_cols, float** values, int** r_idx, int** c_idx);

DLLAPI
int si_csload(const char* filename, int& nnz, int& n_rows, int& n_cols, float** values, int** r_idx, int** c_idx);
/*
  Sparse BLAS Level 3 routines
*/

// All pointers have to be addresses of buffers in CPU memory

//CPU spmv single precision
DLLAPI
int si_cscsrmv(int n_rows, int n_cols, const float*  vals, const int* c_idx, const int* r_idx,
                           const float*  x,    float*  y) ;

//CPU spmv double precision
DLLAPI
int si_cdcsrmv(int n_rows, int n_cols, const double* vals, const int* c_idx, const int* r_idx,
                           const double* x,    double* y) ;

//CPU spmv single precision complex
DLLAPI
int si_cszcsrmv(int n_rows, int n_cols, const si_scomplex* vals, const int* c_idx, const int* r_idx,
														const si_scomplex* x,  si_scomplex*  y);

//CPU spmv double precision complex
DLLAPI
int si_cdzcsrmv(int n_rows, int n_cols, const si_dcomplex* vals, const int* c_idx, const int* r_idx,
														const si_dcomplex* x, si_dcomplex*  y);

// All pointers have to be addresses of buffers in GPU memory

// GPU spmv single precision
DLLAPI
int si_gscsrmv(int n_rows, int n_cols, const float*  vals, const int* c_idx, const int* r_idx,
                           const float*  x,    float*  y) ;

// GPU spmv double precision
DLLAPI
int si_gdcsrmv(int n_rows, int n_cols, const double* vals, const int* c_idx, const int* r_idx,
                           const double* x,    double* y) ;

// GPU spmv single precision complex    
DLLAPI
int si_gszcsrmv(int n_rows, int n_cols, const si_scomplex* vals, const int* c_idx, const int* r_idx,
													 const si_scomplex* x,    si_scomplex* y);

// GPU spmv double precision complex
DLLAPI
int si_gdzcsrmv(int n_rows, int n_cols, const si_dcomplex* vals, const int* c_idx, const int* r_idx,
													const si_dcomplex* x, si_dcomplex*  y);
													
/*
 * Speedit 2.0 SPMV using matrix handlers
 * SPMV functions with use of matrix handlers, faster comparing to functions 
 * above because data origin is eliminated from it by matrix handlers which
 * are created before. Handler always points to GPU memory space
 */

/*
 * Pointers to x and y are in CPU memory
 */

//cpu csr single precision
DLLAPI
int si_cshcsrmv(SI_CSR_FLOAT_HANDLE handle, const float* x, float* y);

//cpu cmr single precision
DLLAPI
int si_cshcmrmv(SI_CMR_FLOAT_HANDLE handle, const float* x, float* y);

//cpu csr double precision
DLLAPI
int si_cdhcsrmv(SI_CSR_DOUBLE_HANDLE handle, const double* x, double* y);

//cpu cmr double precison
DLLAPI
int si_cdhcmrmv(SI_CMR_DOUBLE_HANDLE handle, const double* x, double* y);


/*
 * Pointers to x and y have to be in GPU memory
 */
// gpu csr single precision
DLLAPI
int si_gshcsrmv(SI_CSR_FLOAT_HANDLE handle, const float* x, float* y);

//gpu cmr single precision
DLLAPI
int si_gshcmrmv(SI_CMR_FLOAT_HANDLE handle, const float* x, float* y);

//gpu csr double precision
DLLAPI
int si_gdhcsrmv(SI_CSR_DOUBLE_HANDLE handle, const double* x, double* y);

//gpu cmr double precison
DLLAPI
int si_gdhcmrmv(SI_CMR_DOUBLE_HANDLE handle, const double* x, double* y);


/************************************************
 * 
 * 
 *        Linear equation system solvers 
 * 
 ************************************************/


/*
	SPEEDIT 2.0
	BICG solver using matrix handlers,
	Interface is much simpler :)
*/
//BICG in double precision on csr matrix with vectors located on CPU
DLLAPI
int si_cdhcsrbicgstab(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);
//BICG in double precision on csr matrix with vectors on GPU
DLLAPI
int si_cdhcmrbicgstab(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);
//BICG in single precision on csr matrix with wectors on CPU
DLLAPI
int si_cshcsrbicgstab(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);
//BICG on cmr matrix with vectors located on CPU
DLLAPI
int si_cshcmrbicgstab(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);


//BICG in double precision on csr matrix with vectors on GPU
DLLAPI
int si_gdhcsrbicgstab(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//BICG on cmr matrix with vectors located on GPU
DLLAPI
int si_gdhcmrbicgstab(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//BICG in single precision on csr matrix with vectors on GPU
DLLAPI
int si_gshcsrbicgstab(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//BICG on cmr matrix with vectors located on GPU
DLLAPI
int si_gshcmrbicgstab(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);

/*
	SPEEDIT 2.0
	CG solver using matrix handlers,
*/

//CG in double precision on csr matrix with vectors located on CPU
DLLAPI
int si_cdhcsrcg(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//CG in single precision on csr matrix with wectors on CPU
DLLAPI
int si_cshcsrcg(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//CG on cmr matrix with vectors located on CPU
DLLAPI
int si_cdhcmrcg(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//CG on cmr matrix with vectors located on CPU
DLLAPI
int si_cshcmrcg(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);


//CG in double precision on csr matrix with vectors on GPU
DLLAPI
int si_gdhcsrcg(SI_CSR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//CG on cmr matrix with vectors located on GPU
DLLAPI
int si_gdhcmrcg(SI_CMR_DOUBLE_HANDLE handle, double* x, const double* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//CG in single precision on csr matrix with vectors on GPU
DLLAPI
int si_gshcsrcg(SI_CSR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);

//CG on cmr matrix with vectors located on GPU
DLLAPI
int si_gshcmrcg(SI_CMR_FLOAT_HANDLE handle, float* x, const float* b, PRECOND_TYPE precond, int* n_iter, double* eps);




// All pointers have to be addresses of buffers in CPU memory

/*
 * BICG CPU
 */

//CPU Single precision BICG for CSR matrix
DLLAPI
int si_cscsrbicgstab(      int    n_rows, int n_cols,
                     const float* vals, const int* c_idx, const int *r_idx, 
                           float* x, 
                     const float* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

//CPU double precision BICG for CSR matrix                
DLLAPI
int si_cdcsrbicgstab(      int    n_rows, int n_cols,
                     const double* vals, const int* c_idx, const int *r_idx, 
                           double* x, 
                     const double* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

//CPU single precision COMPLEX BICG for CSR matrix                           
DLLAPI
int si_cszcsrbicgstab(      int    n_rows, int n_cols,
                     const si_scomplex* vals, const int* c_idx, const int *r_idx, 
                           si_scomplex* x, 
                     const si_scomplex* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

//CPU double precision COMPLEX BICG for CSR matrix
DLLAPI
int si_cdzcsrbicgstab(      int    n_rows, int n_cols,
                     const si_dcomplex* vals, const int* c_idx, const int *r_idx, 
                           si_dcomplex* x, 
                     const si_dcomplex* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

/*
 * CG CPU
 */

//CPU single precision CG for CSR matrix                           
DLLAPI
int si_cscsrcg(      int    n_rows, int n_cols,
                     const float* vals, const int* c_idx, const int *r_idx, 
                           float* x, 
                     const float* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;
//CPU double precision CG for CSR matrix                           
DLLAPI
int si_cdcsrcg(      int    n_rows, int n_cols,
                     const double* vals, const int* c_idx, const int *r_idx, 
                           double* x, 
                     const double* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;


// CPU single precision complex CG for CSR matrix
DLLAPI
int si_cszcsrcg(      int    n_rows, int n_cols,
                     const si_scomplex* vals, const int* c_idx, const int *r_idx, 
                           si_scomplex* x, 
                     const si_scomplex* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

// CPU double precision complex CG for CSR matrix 
DLLAPI
int si_cdzcsrcg(      int    n_rows, int n_cols,
                     const si_dcomplex* vals, const int* c_idx, const int *r_idx, 
                           si_dcomplex* x, 
                     const si_dcomplex* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

// All pointers except n_iter and eps have to be addresses of buffers in GPU 
// memory.

/*
 * BICG - GPU
 */
// GPU single precision BICG for CSR matrix
DLLAPI
int si_gscsrbicgstab(      int    n_rows, int n_cols,
                     const float* vals, const int* c_idx, const int *r_idx, 
                           float* x, 
                     const float* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;
                           
// GPU double precision BICG for CSR matrix                          
DLLAPI
int si_gdcsrbicgstab(      int    n_rows, int n_cols,
                     const double* vals, const int* c_idx, const int *r_idx, 
                           double* x, 
                     const double* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

// GPU single precision complex BICG for CSR matrix
DLLAPI
int si_gszcsrbicgstab(		        int   n_rows,
									int	  n_cols,
                    const si_scomplex *   vals, 
                            const int *   c_idx, 
                            const int *   r_idx, 
                          si_scomplex *   x, 
                    const si_scomplex *   b, 
                          PRECOND_TYPE    precond, 
                                  int*    n_iter, 
                               double*    eps) ;
                               
// GPU double precision complex BICG for CSR matrix                               
DLLAPI
int si_gdzcsrbicgstab(		        int     n_rows, int n_cols,
                    const si_dcomplex *   vals, 
                            const int *   c_idx, 
                            const int *   r_idx, 
                          si_dcomplex *   x, 
                    const si_dcomplex *   b, 
                          PRECOND_TYPE    precond, 
                                  int*    n_iter, 
                               double*    eps) ;                               
/*
 * CG GPU
 */
 
// GPU single precision CG for CSR matrix                          
DLLAPI
int si_gscsrcg(      int    n_rows, int n_cols,
                     const float* vals, const int* c_idx, const int *r_idx, 
                           float* x, 
                     const float* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

// GPU double precision CG for CSR matrix                           
DLLAPI
int si_gdcsrcg(      int    n_rows, int n_cols,
                     const double* vals, const int* c_idx, const int *r_idx, 
                           double* x, 
                     const double* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

// GPU single precision complex CG for CSR matrix                          
DLLAPI
int si_gszcsrcg(     int    n_rows, int n_cols,
                     const si_scomplex* vals, const int* c_idx, const int *r_idx, 
                           si_scomplex* x, 
                     const si_scomplex* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ;

// GPU double precision complex CG for CSR matrix                           
DLLAPI
int si_gdzcsrcg(      int    n_rows, int n_cols,
                     const si_dcomplex* vals, const int* c_idx, const int *r_idx, 
                           si_dcomplex* x, 
                     const si_dcomplex* b, 
                     PRECOND_TYPE precond, 
                           int*   n_iter, 
                           double* eps) ; 
                               

#ifdef __cplusplus
}
#endif

#endif
