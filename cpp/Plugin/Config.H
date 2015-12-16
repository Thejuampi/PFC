// This piece of code is here because of access to solverControls

    // read matrix type to know which structures have to be used;
    // it is additional dictionary entry with "matrix" key in fvSolve.
    word matrix_t(solverControls.lookup("matrix"));
    if ( string(matrix_t) == string("CSR")) {

        matrix_type = CSR;

    } else if( string(matrix_t) == string("CMR")) {

        matrix_type = CMR;
    } else if ( string(matrix_t) == string("")){
        std::cout << "WARNING: matrix key missing" << nl;
        matrix_type = UNKNOWN;
    }
    else  {
       matrix_type = UNKNOWN;
    }

    //read preconditioner type
    word precond_t(solverControls.lookup("preconditioner"));
    if ( string(precond_t) == string("SI_DIAGONAL"))
    {
        //Info << "Found SI_DIAGONAL" << nl;
        precond_type = SI_DIAGONAL;

    }else if ( string(precond_t) == string("SI_VOID"))
    {
        //Info << "Found SI_VOID" << nl;
        precond_type = SI_VOID;

    }else if ( string(precond_t) == string("SI_AMG"))
    {
        //Info << "Found SI_AMG" << nl;
        precond_type = SI_AMG;

    }else if ( string(precond_t) == string("SI_AINV"))
    {
        //Info << "Found SI_AINV" << nl;
        precond_type = SI_AINV;

    }else if ( string(precond_t) == string("SI_AINV_SC"))
    {
        //Info << "Found SI_AINV_SC" << nl;
        precond_type = SI_AINV_SC;

    }else if ( string(precond_t) == string("SI_AINV_NS"))
    {
        //Info << "Found SI_AINV_NS" << nl;
        precond_type = SI_AINV_NS;

    }else
        {
           std::cout << "WARNING: Unsupported preconditioner:  " << precond_t << ", setting none preconditioner" << nl;
                   std::cout << "Correct types are: "<<nl;
                   std::cout << "(";
                   std::cout << " SI_AMG" <<nl;
                   std::cout << " SI_AINV"<<nl;
                   std::cout << " SI_AINV_NS"<<nl;
                   std::cout << " SI_AINV_SC"<<nl;
                   std::cout << " SI_DIAGONAL"<<nl;
                   std::cout << ")"<<nl;
            precond_type = SI_VOID;
        }
