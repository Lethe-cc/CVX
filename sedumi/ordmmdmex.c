/*
 perm = ordmmdmex(X)
   Computes multiple-minimum-degree permutation, for sparse
   Cholesky. X is a sparse symmetric matrix; only its off-
   diagonal sparsity structure is used.

   Invokes SPARSPAK-A Release III.

% This file is part of SeDuMi 1.1 by Imre Polik and Oleksandr Romanko
% Copyright (C) 2005 McMaster University, Hamilton, CANADA  (since 1.1)
%
% Copyright (C) 2001 Jos F. Sturm (up to 1.05R5)
%   Dept. Econometrics & O.R., Tilburg University, the Netherlands.
%   Supported by the Netherlands Organization for Scientific Research (NWO).
%
% Affiliation SeDuMi 1.03 and 1.04Beta (2000):
%   Dept. Quantitative Economics, Maastricht University, the Netherlands.
%
% Affiliations up to SeDuMi 1.02 (AUG1998):
%   CRL, McMaster University, Canada.
%   Supported by the Netherlands Organization for Scientific Research (NWO).
%
% This program is free software; you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation; either version 2 of the License, or
% (at your option) any later version.
%
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
% You should have received a copy of the GNU General Public License
% along with this program; if not, write to the Free Software
% Foundation, Inc.,  51 Franklin Street, Fifth Floor, Boston, MA
% 02110-1301, USA

*/

#include "mex.h"

#define PERM_OUT plhs[0]

#define X_IN prhs[0]

/* ============================================================
   SUBROUTINES:
   ============================================================ */
/* ------------------------------------------------------------
   GETADJ - Copies off-diagonal entries from C-style sparse
     matrix (cjc,cir) to Fortran style sparse matrix (forjc,forir).
     On input, n is number of columns.
   ------------------------------------------------------------ */
void getadj(int *forjc,int *forir,const int *cjc,const int *cir,const int n)
{
	int i,j,inz,ix;

	inz = 0;
    for(j = 0; j < n; j++){
		forjc[j] = inz + 1;
		for(ix = cjc[j]; ix < cjc[j+1]; ix++)
			if((i = cir[ix]) != j)
				forir[inz++] = ++i;
	}
	forjc[n] = ++inz;
}

/* ============================================================
   MAIN: MEXFUNCTION
   ============================================================ */
/* ************************************************************
   PROCEDURE mexFunction - Entry for Matlab
   perm = ordmmdmex(X) where X is symmetric sparse.
   ************************************************************ */
void mexFunction(const int nlhs, mxArray *plhs[],
                 const int nrhs, const mxArray *prhs[])
{
  int m, i, iwsiz, flag, nofsub;
  double *permPr;
  int *iwork,*Xjc,*Xir, *xadj,*adjncy,*perm,*invp;
  mwIndex *mwXjc, *mwXir;

 /* ------------------------------------------------------------
    Check for proper number of arguments
    ------------------------------------------------------------ */
  mxAssert(nrhs == 1, "ordmmd requires 1 input argument.");
  mxAssert(nlhs == 1, "ordmmd generates 1 output argument.");
/* ------------------------------------------------------------
   Check input X 
   ------------------------------------------------------------ */
  mxAssert(mxIsSparse(X_IN), "Input matrix must be sparse");
  m = (int) mxGetM(X_IN);
  mxAssert( m == (int) mxGetN(X_IN), "X should be square.");
/* ------------------------------------------------------------
   Get input X
   ------------------------------------------------------------ */
  mwXjc = mxGetJc(X_IN);
  mwXir = mxGetIr(X_IN);
  Xjc=(int *) mxCalloc(m+1, sizeof(int));
  for(i=0;i<=m;i++)
      Xjc[i]=(int) mwXjc[i];
  Xir=(int *) mxCalloc(Xjc[m],sizeof(int));
  for(i=0;i<Xjc[m];i++)
      Xir[i]=(int) mwXir[i];
/* ------------------------------------------------------------
   Create output vector PERM
   ------------------------------------------------------------ */
  PERM_OUT = mxCreateDoubleMatrix(m, 1, mxREAL);
  permPr = mxGetPr(PERM_OUT);
/* ------------------------------------------------------------
   Allocate working arrays:
   int xadj(m+1), adjncy(Xnnz), perm(m), invp(m), iwork(iwsiz)
   ------------------------------------------------------------ */
  xadj   = (int *) mxCalloc(m+1,sizeof(int));
  adjncy = (int *) mxCalloc(Xjc[m],sizeof(int));
  perm   = (int *) mxCalloc(m,sizeof(int));
  invp   = (int *) mxCalloc(m,sizeof(int));
  iwsiz  = 4 * m;
  iwork = (int *) mxCalloc(iwsiz,sizeof(int));
/* ------------------------------------------------------------
   Convert C-style symmetric matrix to adjacency structure
   (xadj,adjncy) in Fortran-style.
   ------------------------------------------------------------ */
  getadj(xadj,adjncy, Xjc,Xir,m);
/* ------------------------------------------------------------
   Compute multiple minimum degree ordering (J. Liu, in Fortran)
   ------------------------------------------------------------ */
  ordmmd_(&m,xadj,adjncy, invp,perm, &iwsiz,iwork, &nofsub, &flag);
  if(flag == -1)
    mexErrMsgTxt("Error in ordmmd.");
/* ------------------------------------------------------------
   Convert PERM to floating point.
   ------------------------------------------------------------ */
  for(i = 0; i < m; i++){
    permPr[i] = perm[i];
  }
/* ------------------------------------------------------------
   Release working arrays
   ------------------------------------------------------------ */
  mxFree(Xjc);
  mxFree(Xir);
  mxFree(iwork);
  mxFree(invp);
  mxFree(perm);
  mxFree(xadj);
  mxFree(adjncy);
}
