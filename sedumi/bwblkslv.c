/*
   y = bwblkslv(L,b, [y])
   Given block sparse Cholesky structure L, as generated by
   SPARCHOL, this solves the equation   "L.L' * y(L.perm) = b",
   i.e. y(L.perm) = L.L'\b.   The diagonal of L.L is taken to
   be all-1, i.e. it uses eye(n) + tril(L.L,-1).
   CAUTION: If y and b are SPARSE, then L.perm is NOT used, i.e. y = L.L'\b.

   If b is SPARSE, then the 3rd argument (y) must give the sparsity
   structure of the output variable y. See symbbwslv.c

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
#include "blksdp.h"
#include <string.h>

#define Y_OUT plhs[0]

#define L_IN prhs[0]
#define B_IN prhs[1]
#define MINNPARIN 2
#define Y_IN prhs[2]
#define NPARIN 3

/* ============================================================
   BACKWARD SOLVE:
   ============================================================ */
/* ************************************************************
   PROCEDURE bwsolve -- Solve y from L'*y = b, where
     L is lower-triangular.
   INPUT
     Ljc, Lir, Lpr - sparse lower triangular matrix
     xsuper - starting column in L for each (dense) supernode.
     nsuper - number of super nodes
   UPDATED
     y - full xsuper[nsuper]-vector, yOUTPUT = L' \ yINPUT.
   WORKING ARRAY
     fwork - length max(collen[i] - superlen[i]) <= m-1, where
       collen[i] := L.jc[xsuper[i]+1]-L.jc[xsuper[i]] and
       superlen[i] := xsuper[i+1]-xsuper[i].
   ************************************************************ */
void bwsolve(double *y, const mwIndex *Ljc, const mwIndex *Lir, const double *Lpr,
             const mwIndex *xsuper, const mwIndex nsuper, double *fwork)
{
  mwIndex jsup,i,j,inz,k,jnnz;
  double yj;

  /* ------------------------------------------------------------
     For each supernode jsup:
     ------------------------------------------------------------ */
  j = xsuper[nsuper];      /* column after current snode (j=m)*/
  for(jsup = nsuper; jsup > 0; jsup--){
    i = j;
    mxAssert(j == xsuper[jsup],"");
    inz = Ljc[--j];
    inz++;                        /* jump over diagonal entry */
    if(j <= xsuper[jsup-1]){
/* ------------------------------------------------------------
   If supernode is singleton j, then simply y[j] -= L(j+1:m,j)'*y(j+1:m)
   ------------------------------------------------------------ */
      if(inz < Ljc[i]){
        yj = Lpr[inz] * y[Lir[inz]];
        for(++inz; inz < Ljc[i]; inz++)
          yj += Lpr[inz] * y[Lir[inz]];
        y[j] -= yj;
      }
    }
    else{
/* ------------------------------------------------------------
   For a "real" supernode: Let fwork = sparse(y(i:m)),
   then let y[j] -= L(i:m,j)'*fwork for all j in supernode
   ------------------------------------------------------------ */
      for(jnnz = 0; inz < Ljc[i]; inz++)
        fwork[jnnz++] = y[Lir[inz]];
      if(jnnz > 0)
        while(i > xsuper[jsup-1]){
          yj = realdot(Lpr+Ljc[i]-jnnz, fwork, jnnz);
          mxAssert(i>0,"");
          y[--i] -= yj;
        }
      k = 1;
      do{
  /* ------------------------------------------------------------
     It remains to do a dense bwsolve on nodes j-1:-1:xsuper[jsup]
     The equation L(:,j)'*yNEW = yOLD(j), yields
       y(j) -= L(j+(1:k),j)'*y(j+(1:k)),   k=1:i-xsuper[jsup]-1.
     ------------------------------------------------------------ */
        mxAssert(j>0,"");
          --j;
        y[j] -= realdot(Lpr+Ljc[j]+1, y+j+1, k++);
      } while(j > xsuper[jsup-1]);
    }
  }
}

/* ************************************************************
   PROCEDURE selbwsolve -- Solve ynew from L'*y = yold, where
     L is lower-triangular and y is SPARSE.
   INPUT
     Ljc, Lir, Lpr - sparse lower triangular matrix
     xsuper - length nsuper+1, start of each (dense) supernode.
     nsuper - number of super nodes
     snode - length m array, mapping each node to the supernode containing it.
     yir   - length ynnz array, listing all possible nonzeros entries in y.
     ynnz  - number of nonzeros in y (from symbbwslv).
   UPDATED
     y - full vector, on input y = rhs, on output y = L'\rhs.
        only the yir(0:ynnz-1) entries are used and defined.
   ************************************************************ */
void selbwsolve(double *y, const mwIndex *Ljc, const mwIndex *Lir, const double *Lpr,
                const mwIndex *xsuper, const mwIndex nsuper,
                const mwIndex *snode, const mwIndex *yir, const mwIndex ynnz)
{
  mwIndex jsup,j,inz,jnz,nk, k;
  double yj;

  if(ynnz <= 0)
    return;
/* ------------------------------------------------------------
   Backward solve on each nonzero supernode snode[yir[jnz]] (=jsup-1).
   ------------------------------------------------------------ */
  jnz = ynnz;           /* point just beyond last nonzero (super)node in y */
  while(jnz > 0){
    j = yir[--jnz];                   /* j is last subnode to be used */
    jsup = snode[j];
    nk = j - xsuper[jsup];            /* nk+1 = length supernode jsup in y */
    jnz -= nk;                /* point just beyond prev. nonzero supernode */
    for(k = 0; k <= nk; k++, j--){
/* ------------------------------------------------------------
   The equation L(:,j)'*yNEW = yOLD(j), yields
   y(j) -= L(j+1:m,j)'*y.
   ------------------------------------------------------------ */
      inz = Ljc[j];
      inz++;                        /* jump over diagonal entry */
      yj = realdot(Lpr+inz, y+j+1, k);       /* super-nodal part */
      for(inz += k; inz < Ljc[j+1]; inz++)
	yj += Lpr[inz] * y[Lir[inz]];      /* sparse part */
      y[j] -= yj;
    }
  }
}

/* ============================================================
   MAIN: MEXFUNCTION
   ============================================================ */
/* ************************************************************
   PROCEDURE mexFunction - Entry for Matlab
   y = bwblksolve(L,b, [y])
     y(L.fullperm) = L.L' \ b
   ************************************************************ */
void mexFunction(const int nlhs, mxArray *plhs[],
  const int nrhs, const mxArray *prhs[])
{
 const mxArray *L_FIELD;
 mwIndex m,n, j, k, nsuper, inz;
 double *y, *fwork;
 const double *permPr, *b, *xsuperPr;
 const mwIndex *yjc, *yir, *bjc, *bir;
 mwIndex *perm, *xsuper, *iwork, *snode;
 jcir L;
 char bissparse;
 /* ------------------------------------------------------------
    Check for proper number of arguments 
    ------------------------------------------------------------ */
 mxAssert(nrhs >= MINNPARIN, "fwblkslv requires more input arguments.");
 mxAssert(nlhs == 1, "fwblkslv generates only 1 output argument.");
 /* ------------------------------------------------------------
    Disassemble block Cholesky structure L
    ------------------------------------------------------------ */
 mxAssert(mxIsStruct(L_IN), "Parameter `L' should be a structure.");
 L_FIELD = mxGetField(L_IN,0,"perm");                    /* L.perm */
 mxAssert( L_FIELD != NULL, "Missing field L.perm.");
 m = mxGetM(L_FIELD) * mxGetN(L_FIELD);
 permPr = mxGetPr(L_FIELD);
 L_FIELD = mxGetField(L_IN,0,"L");         /* L.L */
 mxAssert( L_FIELD != NULL, "Missing field L.L.");
 mxAssert( m == mxGetM(L_FIELD) && m == mxGetN(L_FIELD), "Size L.L mismatch.");
 mxAssert(mxIsSparse(L_FIELD), "L.L should be sparse.");
 L.jc = mxGetJc(L_FIELD);
 L.ir = mxGetIr(L_FIELD);
 L.pr = mxGetPr(L_FIELD);
 L_FIELD = mxGetField(L_IN,0,"xsuper");          /* L.xsuper */
 mxAssert( L_FIELD != NULL, "Missing field L.xsuper.");
 nsuper = mxGetM(L_FIELD) * mxGetN(L_FIELD) - 1;
 mxAssert( nsuper <= m, "Size L.xsuper mismatch.");
 xsuperPr = mxGetPr(L_FIELD);
 /* ------------------------------------------------------------
    Get rhs matrix b.
    If it is sparse, then we also need the sparsity structure of y.
    ------------------------------------------------------------ */
 b = mxGetPr(B_IN);
 mxAssert( mxGetM(B_IN) == m, "Size mismatch b.");
 n = mxGetN(B_IN);
 if( (bissparse = mxIsSparse(B_IN)) ){
   bjc = mxGetJc(B_IN);
   bir = mxGetIr(B_IN);
   mxAssert(nrhs >= NPARIN, "bwblkslv requires more inputs in case of sparse b.");
   mxAssert(mxGetM(Y_IN) == m && mxGetN(Y_IN) == n, "Size mismatch y.");
   mxAssert(mxIsSparse(Y_IN), "y should be sparse.");
 }
/* ------------------------------------------------------------
   Allocate output y. If bissparse, then Y_IN gives the sparsity structure.
   ------------------------------------------------------------ */
 if(!bissparse)
   Y_OUT = mxCreateDoubleMatrix(m, n, mxREAL);
 else{
   yjc = mxGetJc(Y_IN);
   yir = mxGetIr(Y_IN);
   Y_OUT = mxCreateSparse(m,n, yjc[n],mxREAL);
   memcpy(mxGetJc(Y_OUT), yjc, (n+1) * sizeof(mwIndex));
   memcpy(mxGetIr(Y_OUT), yir, yjc[n] * sizeof(mwIndex));
 }
 y = mxGetPr(Y_OUT);
 /* ------------------------------------------------------------
    Allocate working arrays
    ------------------------------------------------------------ */
 fwork = (double *) mxCalloc(m, sizeof(double));
 iwork = (mwIndex *) mxCalloc(2*m+nsuper+1, sizeof(mwIndex));
 perm = iwork;                   /* m */
 xsuper = iwork + m;             /*nsuper+1*/
 snode = xsuper + (nsuper+1);    /* m */
 /* ------------------------------------------------------------
    Convert real to integer array, and from Fortran to C style.
    ------------------------------------------------------------ */
 for(k = 0; k < m; k++)
   perm[k] = permPr[k] - 1;
 for(k = 0; k <= nsuper; k++)
   xsuper[k] = xsuperPr[k] - 1;
/* ------------------------------------------------------------
   In case of sparse b, we also create snode, which maps each subnode
   to the supernode containing it.
   ------------------------------------------------------------ */
 if(bissparse)
   for(j = 0, k = 0; k < nsuper; k++)
     while(j < xsuper[k+1])
       snode[j++] = k;
 /* ------------------------------------------------------------
    The actual job is done here: y(perm) = L'\b.
    ------------------------------------------------------------ */
 if(!bissparse)
   for(j = 0; j < n; j++){
     memcpy(fwork,b, m * sizeof(double));
     bwsolve(fwork,L.jc,L.ir,L.pr,xsuper,nsuper,y);  /* y(m) as work */
     for(k = 0; k < m; k++)            /* y(perm) = fwork */
       y[perm[k]] = fwork[k];
     y += m; b += m;
   }
 else{          /* sparse y,b: don't use perm */
   fzeros(fwork,m);
   for(j = 0; j < n; j++){
     inz = yjc[j];
     for(k = bjc[j]; k < bjc[j+1]; k++)            /* fwork = b */
       fwork[bir[k]] = b[k];
     selbwsolve(fwork,L.jc,L.ir,L.pr,xsuper,nsuper, snode,
                yir+inz,yjc[j+1]-inz);
     for(k = inz; k < yjc[j+1]; k++)
       y[k] = fwork[yir[k]];
     for(k = inz; k < yjc[j+1]; k++)            /* fwork = all-0 */
       fwork[yir[k]] = 0.0;
   }
 }
 /* ------------------------------------------------------------
    RELEASE WORKING ARRAYS.
    ------------------------------------------------------------ */
 mxFree(fwork);
 mxFree(iwork);
}
