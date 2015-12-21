
/*
 * Create SpeedIT csr matrix from OpenFOAMs LDU format
 */

	int n_rows = matrix_.diag().size() ;
    int nnz = matrix_.lower().size() + matrix_.upper().size() + matrix_.diag().size() ;

    std::vector<scalar> vals(nnz) ;
    std::vector<int> c_idx(nnz) ;
    std::vector<int> r_idx(n_rows+2, 1) ;

    ldu2csr(matrix_, c_idx, r_idx, vals) ;

    //moved outside to avoid warning
    //scalar* p_vals = &(vals[0]) ;
    //int* p_c_idx = &(c_idx[0]) ;
    //int* p_r_idx = &(r_idx[0]) ;

    //
    // Get pointers for vectors containing solution and right hand side.
    //
    const scalar* B = source.cdata() ;
    scalar* X = psi.data() ;
    //
    //	Copy data to GPU memory.
    //
    scalar* pgpu_B    = NULL ;
    scalar* pgpu_X    = NULL ;

    cu_err += si_cuda_malloc(reinterpret_cast<void**>(&pgpu_X),     n_rows * sizeof(scalar)) ;
    cu_err += si_cuda_malloc(reinterpret_cast<void**>(&pgpu_B),     n_rows * sizeof(scalar)) ;

    if (0 != cu_err) {
        si_cuda_free (pgpu_B) ;
        si_cuda_free (pgpu_X) ;
        Foam::FatalError << "Can not allocate GPU memory for X, B" << Foam::exit(Foam::FatalError, -2) ;
    } ;

    cu_err += si_cuda_memcpy (pgpu_B,     B,       n_rows     * sizeof(scalar), C2G) ;
    cu_err += si_cuda_memcpy (pgpu_X,     X,       n_rows     * sizeof(scalar), C2G) ;

    if (0 != cu_err) {
        si_cuda_free (pgpu_B) ;
        si_cuda_free (pgpu_X) ;

        Foam::FatalError << "Can not copy data to GPU memory" << Foam::exit(Foam::FatalError, -3) ;
    } ;

