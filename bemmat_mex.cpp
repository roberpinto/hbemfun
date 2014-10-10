/*BEMMAT  Boundary element system matrices.
 *
 *   [U,T] = BEMMAT(nod,elt,typ,green,...) computes the boundary element
 *   system matrices. The Green's functions, specified as a full space
 *   solution (green='fs***') or a user specified solution (green='user'), are 
 *   integrated over the boundary element mesh defined by its nodes, elements
 *   and element types.
 *   
 *   Depending on the Green's function, the following syntax is used:
 *
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen2d_inplane0',E,nu)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen2d_outofplane0',mu)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen3d0',E,nu)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen2d_outofplane',Cs,Ds,rho,omega)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen2d_inplane',Cs,Cp,Ds,Dp,rho,omega)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen3d',Cs,Cp,Ds,Dp,rho,omega)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen3d',Cs,Cp,Ds,Dp,rho,omega)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreen3dt',Cs,Cp,rho,delt,t)
 *   [U,T] = BEMMAT(nod,elt,typ,'fsgreenf',Cs,Cp,Ds,Dp,rho,py,omega)
 *   [U,T] = BEMMAT(nod,elt,typ,'user',zs,r,z,ug,sg,sg0)
 * 
 *   [Ue,Te] = BEMMAT(nod,elt,typ,s,green,...)
 *
 *
 *
 *   nod      Nodes (nNod * 4). Each row has the layout [nodID x y z] where
 *            nodID is the node number and x, y, and z are the nodal
 *            coordinates.
 *   elt      Elements (nElt * nColumn). Each row has the layout
 *            [eltID typID n1 n2 n3 ... ] where eltID is the element number,
 *            typID is the element type number and n1, n2, n3, ... are the node 
 *            numbers representing the nodal connectivity of the element.
 *   typ      Element type definitions. Cell array with the layout
 *            {{typID type keyOpts} ... } where typID is the element type number,
 *            type is the element type (string) and keyOpts is a cell array of
 *            strings with key options.
 *   green    Green's function (string). 'fs***' for a full-space solution or 
 *            'user' for a user specified Green's function.
 *   mu       Shear modulus (1 * 1).
 *   E        Young's modulus (1 * 1).
 *   nu       Poisson coefficient (1 * 1).
 *   Cs       Shear wave velocity (1 * 1).
 *   Cp       Dilatational wave velocity (1 * 1).
 *   Ds       Shear damping ratio (1 * 1).
 *   Dp       Dilatational damping ratio (1 * 1).
 *   rho      Density (1 * 1).
 *   delt     Time step (1 * 1) in time domain BE analysis.
 *   t        Time (1*nTime).
 *   omega    Circular frequency (nFreq * 1).
 *   py       Slowness in y-direction, logarithmically sampled (nyWave * 1).
 *            If omega ~= 0, the wavenumber sampling is given by ky = omega * py.
 *            If omega == 0, the wavenumber sampling is given by ky = py.
 *   zs       Source locations (vertical coordinate) (nzSrc * 1).
 *   r        Receiver locations (x-coordinate) (nxRec * 1).
 *   z        Receiver locations (z-coordinate) (nzRec * 1).
 *   ug       Green's displacements.
 *   sg       Green's stresses.
 *   sg0      Static Green's stresses, used for the regularisation of the boundary 
 *            integral equation.
 *   U        Boundary element displacement system matrix (nDof * nDof * ...).
 *   T        Boundary element traction system matrix (nDof * nDof * ...).
 */

/* $Make: mex -O -output bemmat bemmat_mex.cpp bemmat.cpp eltdef.cpp 
              bemcollpoints.cpp shapefun.cpp bemintreg3d.cpp
              bemintreg3dperiodic.cpp bemintreg2d.cpp bemintregaxi.cpp 
              bemintsing3d.cpp bemintsing3dperiodic.cpp bemintsing2d.cpp bemintsingaxi.cpp gausspw.cpp search1.cpp bemnormal.cpp bemdimension.cpp bemisaxisym.cpp bemisperiodic.cpp greeneval2d.cpp greeneval3d.cpp greenrotate2d.cpp greenrotate3d.cpp fsgreenf.cpp fsgreen2d_inplane.cpp fsgreen2d_outofplane.cpp besselh.cpp fsgreen3d.cpp fsgreen3dt.cpp$*/



/*
mexFunction-|
            |
            |--> IntegrateGreenUser --->  bemIntegrate
            |
            |--> IntegrateFsGreenf  --->  bemIntegrate
            |
            |--> IntegrateFsGreen3d  --->  bemIntegrate
            |
            |--> IntegrateFsGreen2d_inplane --->  bemIntegrate
            |
            |--> IntegrateFsGreenf2d_outofplane --->  bemIntegrate
            |
            |--> ...
*/

#include "mex.h"
#include <string.h>
#include "eltdef.h"
#include "gausspw.h"
#include "shapefun.h"
#include "bemcollpoints.h"
#include "bemmat.h"
#include "bemdimension.h"
#include "bemisaxisym.h"
#include "bemisperiodic.h"
//#include "checklicense.h"
#include <math.h>
#include <new>
// #include "mex.h"
#include <time.h>
#include <assert.h>

#ifndef __GNUC__
#define strcasecmp _strcmpi
#endif

#ifndef _int64_
typedef long long int int64;
typedef unsigned long long int uint64;
#endif

using namespace std;
	
//==============================================================================	

	static bool CacheValid=false;

    static unsigned int nNod;
    static double* Nod;
	// static double* Nodtest;

    static double* Elt;
    static unsigned int nElt;
    static unsigned int maxEltColumn;

    static bool keyOpts=true;
    static unsigned int nEltType;
    static unsigned int maxKeyOpts = 50;  // Maximum number of keyoptions per element type
	static unsigned int* TypeID;
    static unsigned int* nKeyOpt;
      
    static char** TypeName;
    static char** TypeKeyOpts;
      
	static unsigned int probDim;
    static bool probAxi;
    static bool probPeriodic;
   	
    // COLLOCATION POINTS: NODAL OR CENTROID
    // static int* NodalColl=0;
	// static int* NodalColl=NULL;
	static unsigned int* NodalColl;
    // static int* CentroidColl=0;
	// static int* CentroidColl=NULL;
	static unsigned int* CentroidColl;
    static unsigned int nCentroidColl;
    static unsigned int nNodalColl;

    // COLLOCATION POINT COORDINATES
    static unsigned int nTotalColl;
    // static double* CollPoints=0;
	// static double* CollPoints=NULL;
	static double* CollPoints;
	
    // CHECK FOR COINCIDENT NODES
    // static double* CoincNodes=0;
	// static double* CoincNodes=NULL;
	static double* CoincNodes;
    static bool SlavesExist;

	
	static unsigned int* EltParent;
    static unsigned int* nEltNod;
    static unsigned int* nEltColl;
    static unsigned int* EltShapeN;
    static unsigned int* EltShapeM;
    static unsigned int* EltDim;
    static unsigned int* AxiSym;
    static unsigned int* Periodic;
    static unsigned int* nGauss;
    static unsigned int* nEltDiv;
    static unsigned int* nGaussSing;
    static unsigned int* nEltDivSing;
	
	static unsigned int* ncumulEltCollIndex;
	static unsigned int NEltCollIndex;
	static unsigned int* eltCollIndex;
	
	static unsigned int* RegularColl;
	
	static unsigned int* ncumulSingularColl;
	static unsigned int NSingularColl;
	
	
	static unsigned int* nRegularColl;
	static unsigned int* nSingularColl;
	
	static unsigned int* ncumulEltNod;
	static unsigned int NEltNod;
	static double* EltNod;
	
	static unsigned int* RefEltType;
	
	static unsigned int* ncumulnXi;
	static unsigned int NnXi;
	static unsigned int* nXi;
	
	static double* xi;
	static double* H;
	
	static unsigned int* ncumulNshape;
	static unsigned int NNshape;
	
	static double* Nshape;
	static double* Mshape;
	static double* dNshape;
	
//==============================================================================
void IntegrateGreenUser(mxArray* plhs[], int nrhs,
                        const mxArray* prhs[], const bool probAxi, const bool probPeriodic,
                        const unsigned int& probDim, const double* const Nod,
                        const unsigned int& nNod, const double* const Elt,
                        const unsigned int& nElt, const unsigned int* const TypeID,
                        const char* const TypeName[], const char* const TypeKeyOpts[],
                        const unsigned int* const nKeyOpt, const unsigned int& nEltType,
                        const double* const CollPoints, const unsigned int& nTotalColl,
                        // const int& nCentroidColl,
                        // const double* const CoincNodes, const bool& SlavesExist,
                        const bool& UmatOut,
						const bool& TmatOut,
                        const unsigned int& greenPos,
                        const double* const s, const unsigned int& ms, const unsigned int& ns,
						const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
						const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
						const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
						const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
						const unsigned int* const ncumulEltCollIndex,
						// const int& NEltCollIndex, 
						const unsigned int* const eltCollIndex,
						const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
						const unsigned int* const RegularColl,
						// const int* const nRegularColl, const int* const nSingularColl,
						const unsigned int* const ncumulEltNod, const double* const EltNod,
						const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
						const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
/* Initialize Green's function for user defined Green's function ('USER')
 *
 *    greenPtr[0]=&GreenFunType;   Green's function type identifier
 *    greenPtr[1]=&nGrSet;         Number of function sets (nFreq,nTime,nCase)
 *    greenPtr[...]                different definition for different
 *                                 Green's function types
 *
 */
//==============================================================================
{

	// mexPrintf("greenPos %d \n",greenPos);
	// mexPrintf("greenPos %d \n",greenPos+5);
	// mexPrintf("greenPos %d \n",greenPos+7);
	// mexPrintf("nrhs %d \n",nrhs);
	
  // INPUT ARGUMENT PROCESSING
  if (probPeriodic)
  {
    if (!(nrhs==13)) throw("Wrong number of input arguments.");
  }
  else
  {
    // if ((unsigned)nrhs>greenPos+7) throw("Too many input arguments.");
    // if ((unsigned)nrhs<greenPos+5) throw("Not enough input arguments."); //opnieuw!!
	if (nrhs>(greenPos+7)) throw("Too many input arguments.");
    if (nrhs<(greenPos+5)) throw("Not enough input arguments."); //opnieuw!!
  }

  // mexPrintf("nrhs<(greenPos+5): %s \n", (nrhs<(greenPos+5)) ? "true": "false");
	

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'zs' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'zs' must not be sparse.");
  if (mxIsEmpty(prhs[greenPos+1])) throw("Input argument 'zs' must not be empty.");
  const unsigned int nzs=mxGetNumberOfElements(prhs[greenPos+1]);
  const double* const zs=mxGetPr(prhs[greenPos+1]);
  for (unsigned int izs=1; izs<nzs; izs++) if (!(zs[izs-1]<zs[izs])) throw("Input argument 'zs' must be monotonically increasing.");

  // mexPrintf("nzs: %d \n",nzs);
 // for (int i=0; i<nzs ; i++)
	// {
		// mexPrintf("zs [%i]: %e \n",i,zs[i]);
	// }
  
  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'r' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'r' must not be sparse.");
  if (mxIsEmpty(prhs[greenPos+2])) throw("Input argument 'r' must not be empty.");
  const unsigned int nr=mxGetNumberOfElements(prhs[greenPos+2]);
  const double* const r=mxGetPr(prhs[greenPos+2]);
  for (unsigned int ir=1; ir<nr; ir++) if (!(r[ir-1]<r[ir])) throw("Input argument 'r' must be monotonically increasing.");

  if (!mxIsNumeric(prhs[greenPos+3])) throw("Input argument 'z' must be numeric.");
  if (mxIsSparse(prhs[greenPos+3])) throw("Input argument 'z' must not be sparse.");
  if (mxIsEmpty(prhs[greenPos+3])) throw("Input argument 'z' must not be empty.");
  const unsigned int nz=mxGetNumberOfElements(prhs[greenPos+3]);
  const double* const z=mxGetPr(prhs[greenPos+3]);
  for (unsigned int iz=1; iz<nz; iz++) if (!(z[iz-1]<z[iz])) throw("Input argument 'z' must be monotonically increasing.");

   // mexPrintf("nz: %d \n",nz);
	// for (int i=0; i<nz ; i++)
	// {
		// mexPrintf("z [%i]: %e \n",i,z[i]);
	// }
 
  
  if (!mxIsNumeric(prhs[greenPos+4])) throw("Input argument 'ug' must be numeric.");
  if (mxIsSparse(prhs[greenPos+4])) throw("Input argument 'ug' must not be sparse.");
  if (mxIsEmpty(prhs[greenPos+4])) throw("Input argument 'ug' must not be empty.");
  const unsigned int nugdim=mxGetNumberOfDimensions(prhs[greenPos+4]);
  const size_t* const ugdim=mxGetDimensions(prhs[greenPos+4]);
  const unsigned int nugComp=ugdim[0];

  const unsigned int nGreenDim=((nugdim>4)?(nugdim-4):1);
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  unsigned int nGrSet=1;
  greenDim[0]=1;
  if (nugdim>4)
  {
    for (unsigned int iDim=4; iDim<nugdim; iDim++)
    {
      greenDim[iDim-4]=ugdim[iDim];
      nGrSet=nGrSet*ugdim[iDim];
    }
  }
  if (probDim==2)
  {
    if (!(probAxi) && (!((nugComp==1)|(nugComp==4)|(nugComp==9)))) throw("The first dimension of input argument 'ug' for a 2D problem must be 1 (out-of-plane), 4 (in-plane) or 9 for a 2.5D problem.");
    if (probAxi  && (!(nugComp==5))) throw("The first dimension of input argument 'ug' must be 4 for an axisymmetric problem .");
  }
  else if (probDim==3) if (!(nugComp==5)) throw("The first dimension of input argument 'ug' must be 5 for a 3D problem.");
  if (!((unsigned)nzs == ((nugdim>1)? ugdim[1]:1))) throw("Input arguments 'ug' and 'zs' are incompatible");
  if (!((unsigned)nr  == ((nugdim>2)? ugdim[2]:1))) throw("Input arguments 'ug' and 'r' are incompatible");
  if (!((unsigned)nz  == ((nugdim>3)? ugdim[3]:1))) throw("Input arguments 'ug' and 'z' are incompatible");
  const double* const ugRe=mxGetPr(prhs[greenPos+4]);
  const double* const ugIm=mxGetPi(prhs[greenPos+4]);
  const bool ugCmplx=mxIsComplex(prhs[greenPos+4]);

  unsigned int ntgComp;
  if (nugComp==1) ntgComp=2;  // 2D, out-of-plane
  if (nugComp==4) ntgComp=6;  // 2D, in-plane
  if (nugComp==5) ntgComp=10; // 3D / axisymmetric
  if (nugComp==9) ntgComp=18; // 2.5D

  if (TmatOut)
  {
    if (nrhs<(greenPos+7)) throw("Not enough input arguments.");

    if (!mxIsNumeric(prhs[greenPos+5])) throw("Input argument 'sg' must be numeric.");
    if (mxIsSparse(prhs[greenPos+5])) throw("Input argument 'sg' must not be sparse.");
    const unsigned int ntgdim=mxGetNumberOfDimensions(prhs[greenPos+5]);
    const size_t* const tgdim=mxGetDimensions(prhs[greenPos+5]);
    if (!(tgdim[0]==(unsigned)ntgComp)) throw("The first dimension of input argument 'sg' has incorrect size.");
    if (!(nugdim==ntgdim)) throw("Matrix dimensions of input arguments 'ug' and 'sg' must agree.");
    for (unsigned int iDim=1; iDim<nugdim; iDim++)
    {
      if (!(ugdim[iDim]==tgdim[iDim])) throw("Matrix dimensions of input arguments 'ug' and 'sg' are incompatible.");
    }

    if (!mxIsNumeric(prhs[greenPos+6])) throw("Input argument 'sg0' must be numeric.");
    if (mxIsSparse(prhs[greenPos+6])) throw("Input argument 'sg0' must not be sparse.");
    const unsigned int ntg0dim=mxGetNumberOfDimensions(prhs[greenPos+6]);
    const size_t* const tg0dim=mxGetDimensions(prhs[greenPos+6]);
    if (!(tg0dim[0]==(unsigned)ntgComp)) throw("The first dimension of input argument 'sg0' has incorrect size.");
    if (!(nugdim==ntg0dim)) throw("Matrix dimensions of input arguments 'ug' and 'sg0' must agree.");
    for (unsigned int iDim=1; iDim<nugdim; iDim++)
    {
      if (!(ugdim[iDim]==tg0dim[iDim])) throw("Matrix dimensions of input arguments 'ug' and 'sg0' are incompatible.");
    }
  }
  
  const double* const tgRe= (TmatOut? mxGetPr(prhs[greenPos+5]):0);
  const double* const tgIm= (TmatOut? mxGetPi(prhs[greenPos+5]):0);
  const bool tgCmplx=(TmatOut? mxIsComplex(prhs[greenPos+5]):false);

  const double* const tg0Re= (TmatOut? mxGetPr(prhs[greenPos+6]):0);
  const double* const tg0Im= (TmatOut? mxGetPi(prhs[greenPos+6]):0);
  const bool tg0Cmplx=(TmatOut? mxIsComplex(prhs[greenPos+6]):false);

  // Number of degrees of freedom points per collocation point.
  unsigned int nColDof;
  if (nugComp==1) nColDof=1;                 // 2D, out-of-plane
  if (nugComp==4) nColDof=2;                 // 2D, in-plane
  if ((nugComp==5) && !(probAxi)) nColDof=3; // 3D
  if ((nugComp==5) &&   probAxi ) nColDof=2; // Axisymmetric
  if (nugComp==9) nColDof=3;                 // 2.5D

  // Vertical receiver coordinate is passed relative
  const bool zRel=false;   // ! No longer relative receiver grid ...

  // Copy variables to generic array of pointers greenPtr
  const unsigned int nGreenPtr=14;
  const unsigned int GreenFunType=1;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");
  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&nzs;
  greenPtr[2]=zs;
  greenPtr[3]=&nr;
  greenPtr[4]=r;
  greenPtr[5]=&nz;
  greenPtr[6]=z;
  greenPtr[7]=ugRe;
  greenPtr[8]=ugIm;
  greenPtr[9]=tgRe;
  greenPtr[10]=tgIm;
  greenPtr[11]=tg0Re;
  greenPtr[12]=tg0Im;
  greenPtr[13]=&zRel;
 
 // mexPrintf("nzs: %d \n",nzs);
 // for (int i=0; i<nzs ; i++)
	// {
		// mexPrintf("zs [%i]: %d \n",i,zs[i]);
	// }
 
 // mexPrintf("nz: %d \n",nz);
 // for (int i=0; i<nz ; i++)
	// {
		// mexPrintf("z [%i]: %d \n",i,z[i]);
	// }
 
 
  // Periodic problems 
  if (probPeriodic){
    if (!mxIsNumeric(prhs[greenPos+7])) throw("Input argument 'L' must be numeric.");
    if (mxIsSparse(prhs[greenPos+7])) throw("Input argument 'L' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+7])) throw("Input argument 'L' must be real.");
    if (!(mxGetNumberOfElements(prhs[greenPos+7])==1)) throw("Input argument 'L' must be a scalar.");
    
    if (!mxIsNumeric(prhs[greenPos+8])) throw("Input argument 'ky' must be numeric.");
    if (mxIsSparse(prhs[greenPos+8])) throw("Input argument 'ky' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+8])) throw("Input argument 'ky' must be real.");
    if ((mxGetNumberOfDimensions(prhs[greenPos+8])>2) ||
      ((mxGetM(prhs[greenPos+8])>1) && (mxGetN(prhs[greenPos+8])>1)))
                  throw("Input argument 'ky' must be a scalar or a vector.");
    
    if (!mxIsNumeric(prhs[greenPos+9])) throw("Input argument 'nmax' must be numeric.");
    if (mxIsSparse(prhs[greenPos+9])) throw("Input argument 'nmax' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+9])) throw("Input argument 'nmax' must be real.");
    if (!(mxGetNumberOfElements(prhs[greenPos+9])==1)) throw("Input argument 'nmax' must be a scalar.");
  }

  const double L=(probPeriodic ? mxGetScalar(prhs[greenPos+7]) : -1.0);
  const double* const ky=(probPeriodic ? mxGetPr(prhs[greenPos+8]) : 0);
  const unsigned int nWave=(probPeriodic ? mxGetNumberOfElements(prhs[greenPos+8]) : 0);
  const unsigned int nmax=(probPeriodic ? (unsigned int)mxGetScalar(prhs[greenPos+9]) : 0);

  
 
  
  // OUTPUT ARGUMENT POINTERS
  //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=(probPeriodic ? 3+nGreenDim : 2+nGreenDim);
  // size_t* const MatDim = new(nothrow) size_t[nMatDim];
  // if (MatDim==0) throw("Out of memory.");
  
  size_t* const MatDimU = new(nothrow) size_t[nMatDim];
  if (MatDimU==0) throw("Out of memory.");
  MatDimU[0]=(s==0 ? nDof : ms);
  MatDimU[1]=(s==0 ? nDof : ns);
  if (UmatOut==false){MatDimU[0]=0; MatDimU[1]=0;}
  
  for (unsigned int iDim=0; iDim<nGreenDim; iDim++)  MatDimU[2+iDim]=greenDim[iDim];
  if (probPeriodic) MatDimU[nMatDim-1]=nWave;
  
  bool uCmplx=false;
  if (ugCmplx || probPeriodic){uCmplx=true;}
  
  // plhs[0]=mxCreateNumericArray(nMatDim,MatDim,mxDOUBLE_CLASS,mxCOMPLEX);
  plhs[0]=mxCreateNumericArray(nMatDim,MatDimU,mxDOUBLE_CLASS,(uCmplx ? mxCOMPLEX : mxREAL)); 
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

//   mexPrintf("MatDimU[0]: %d\n",MatDimU[0]);
//   mexPrintf("MatDimU[1]: %d\n",MatDimU[1]);
  
  bool tCmplx=false;
  if (tgCmplx || probPeriodic){tCmplx=true;}
   
  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
	size_t* const MatDimT = new(nothrow) size_t[nMatDim];
	if (MatDimT==0) throw("Out of memory.");
	MatDimT[0]=(s==0 ? nDof : ms);
	MatDimT[1]=(s==0 ? nDof : ns);
  
    for (unsigned int iDim=0; iDim<nGreenDim; iDim++)  MatDimT[2+iDim]=greenDim[iDim];
	if (probPeriodic) MatDimT[nMatDim-1]=nWave;
	
    plhs[1]=mxCreateNumericArray(nMatDim,MatDimT,mxDOUBLE_CLASS,(tCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
	
	delete [] MatDimT;
  }
  delete [] MatDimU;
   
  // BEMMAT
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);

  delete [] greenPtr;
  delete [] greenDim;
}

//==============================================================================
void IntegrateFsGreenf(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                       const bool probAxi,  const bool probPeriodic, const unsigned int& probDim,
                       const double* const Nod,const unsigned int& nNod,
                       const double* const Elt,const unsigned int& nElt,
                       const unsigned int* const TypeID, char** const TypeName,
                       char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                       const unsigned int& nEltType, const double* const CollPoints,
                       const unsigned int& nTotalColl, 
					   // const int& nCentroidColl,
                       // const double* const CoincNodes, const bool& SlavesExist,
                       const bool& UmatOut,
					   const bool& TmatOut,
                       const unsigned int& greenPos,
                       const double* const s, const unsigned int& ms, const unsigned int& ns,
					   const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
					   const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
					   const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
					   const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
					   const unsigned int* const ncumulEltCollIndex, 
					   // const int& NEltCollIndex, 
					   const unsigned int* const eltCollIndex,
					   const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
					   const unsigned int* const RegularColl,
					   // const int* const nRegularColl, const int* const nSingularColl,
					   const unsigned int* const ncumulEltNod, const double* const EltNod,
					   const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
					   const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
/* Initialize 2.5D Green's function (fsgreenf)
 *
 */
//==============================================================================
{
  // INPUT ARGUMENT PROCESSING
  if (!(nrhs==11)) throw("Wrong number of input arguments.");

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'Cs' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'Cs' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'Cs' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'Cs' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'Cp' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'Cp' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+2])) throw("Input argument 'Cp' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+2])==1)) throw("Input argument 'Cp' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+3])) throw("Input argument 'Ds' must be numeric.");
  if (mxIsSparse(prhs[greenPos+3])) throw("Input argument 'Ds' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+3])) throw("Input argument 'Ds' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+3])==1)) throw("Input argument 'Ds' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+4])) throw("Input argument 'Dp' must be numeric.");
  if (mxIsSparse(prhs[greenPos+4])) throw("Input argument 'Dp' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+4])) throw("Input argument 'Dp' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+4])==1)) throw("Input argument 'Dp' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+5])) throw("Input argument 'rho' must be numeric.");
  if (mxIsSparse(prhs[greenPos+5])) throw("Input argument 'rho' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+5])) throw("Input argument 'rho' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+5])==1)) throw("Input argument 'rho' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+6])) throw("Input argument 'py' must be numeric.");
  if (mxIsSparse(prhs[greenPos+6])) throw("Input argument 'py' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+6])) throw("Input argument 'py' must be real.");
  if ((mxGetNumberOfDimensions(prhs[greenPos+6])>2) ||
      ((mxGetM(prhs[greenPos+6])>1) && (mxGetN(prhs[greenPos+6])>1)))
    throw("Input argument 'py' must be a scalar or a vector.");

  if (!mxIsNumeric(prhs[greenPos+7])) throw("Input argument 'omega' must be numeric.");
  if (mxIsSparse(prhs[greenPos+7])) throw("Input argument 'omega' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+7])) throw("Input argument 'omega' must be real.");
  if ((mxGetNumberOfDimensions(prhs[greenPos+7])>2) ||
      ((mxGetM(prhs[greenPos+7])>1) && (mxGetN(prhs[greenPos+7])>1)))
    throw("Input argument 'omega' must be a scalar or a vector.");

  const unsigned int nugComp=9;
  const unsigned int nColDof=3;

  const double Cs  = mxGetScalar(prhs[greenPos+1]);
  const double Cp  = mxGetScalar(prhs[greenPos+2]);
  const double Ds  = mxGetScalar(prhs[greenPos+3]);
  const double Dp  = mxGetScalar(prhs[greenPos+4]);
  const double rho = mxGetScalar(prhs[greenPos+5]);
  const double* const py = mxGetPr(prhs[greenPos+6]);
  const unsigned int nWave = mxGetNumberOfElements(prhs[greenPos+6]);
  const double* const omega = mxGetPr(prhs[greenPos+7]);
  const unsigned int nFreq = mxGetNumberOfElements(prhs[greenPos+7]);

  const unsigned int nGrSet=nFreq*nWave;
  const bool ugCmplx=true;
  const bool tgCmplx=true;
  const bool tg0Cmplx=true;

  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=2;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nWave;
  greenDim[1]=nFreq;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=10;
  const unsigned int GreenFunType=2;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Cp;
  greenPtr[3]=&Ds;
  greenPtr[4]=&Dp;
  greenPtr[5]=&rho;
  greenPtr[6]=&nWave;
  greenPtr[7]=&nFreq;
  greenPtr[8]=py;
  greenPtr[9]=omega;
  
  // OUTPUT ARGUMENT POINTERS
  //unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=2+nGreenDim;
  size_t* const MatDim = new(nothrow) size_t[nMatDim];
  if (MatDim==0) throw("Out of memory.");
  MatDim[0]=(s==0 ? nDof : ms);
  MatDim[1]=(s==0 ? nDof : ns);
    
  for (unsigned int iDim=2; iDim<nMatDim; iDim++)  MatDim[iDim]=greenDim[iDim-2];
  plhs[0]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(ugCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
    plhs[1]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(tgCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
  }
  delete [] MatDim;

  // BEMMAT
  const double L=-1.0;
  const double* const ky=0;
  const unsigned int nky=0;
  const unsigned int nmax=0;
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nky,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
}

//==============================================================================
void IntegrateFsGreen3d(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                        const bool probAxi,  const bool probPeriodic, const unsigned int& probDim,
                        const double* const Nod,const unsigned int& nNod,
                        const double* const Elt,const unsigned int& nElt,
                        const unsigned int* const TypeID, char** const TypeName,
                        char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                        const unsigned int& nEltType, const double* const CollPoints,
                        const unsigned int& nTotalColl, 
						// const int& nCentroidColl,
                        // const double* const CoincNodes, const bool& SlavesExist,
                        const bool& UmatOut,
						const bool& TmatOut,
                        const unsigned int& greenPos,
                        const double* const s, const unsigned int& ms, const unsigned int& ns,
						const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
						const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
						const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
						const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
						const unsigned int* const ncumulEltCollIndex, 
						// const int& NEltCollIndex, 
						const unsigned int* const eltCollIndex,
						const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
						const unsigned int* const RegularColl, 
						// const int* const nRegularColl, const int* const nSingularColl,
						const unsigned int* const ncumulEltNod, const double* const EltNod,
						const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
						const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
/* Initialize Green's function for 3D Full space solution (fsgreen3d)
 *
 */
//==============================================================================
{
  // INPUT ARGUMENT PROCESSING
  // if ((probPeriodic&&(!(nrhs==13))) | (!(probPeriodic)&&(!(nrhs==10)))) throw("Wrong number of input arguments.");
  
   if (probPeriodic)
  {
    if (!(nrhs==13)) throw("Wrong number of input arguments.");
  }
  else
  {
	if (nrhs>(greenPos+7)) throw("Too many input arguments.");
    if (nrhs<(greenPos+5)) throw("Not enough input arguments."); //opnieuw!!
  }

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'Cs' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'Cs' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'Cs' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'Cs' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'Cp' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'Cp' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+2])) throw("Input argument 'Cp' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+2])==1)) throw("Input argument 'Cp' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+3])) throw("Input argument 'Ds' must be numeric.");
  if (mxIsSparse(prhs[greenPos+3])) throw("Input argument 'Ds' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+3])) throw("Input argument 'Ds' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+3])==1)) throw("Input argument 'Ds' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+4])) throw("Input argument 'Dp' must be numeric.");
  if (mxIsSparse(prhs[greenPos+4])) throw("Input argument 'Dp' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+4])) throw("Input argument 'Dp' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+4])==1)) throw("Input argument 'Dp' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+5])) throw("Input argument 'rho' must be numeric.");
  if (mxIsSparse(prhs[greenPos+5])) throw("Input argument 'rho' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+5])) throw("Input argument 'rho' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+5])==1)) throw("Input argument 'rho' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+6])) throw("Input argument 'omega' must be numeric.");
  if (mxIsSparse(prhs[greenPos+6])) throw("Input argument 'omega' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+6])) throw("Input argument 'omega' must be real.");
  if ((mxGetNumberOfDimensions(prhs[greenPos+6])>2) ||
      ((mxGetM(prhs[greenPos+6])>1) && (mxGetN(prhs[greenPos+6])>1)))
    throw("Input argument 'omega' must be a scalar or a vector.");

  if (probPeriodic){
    if (!mxIsNumeric(prhs[greenPos+7])) throw("Input argument 'L' must be numeric.");
    if (mxIsSparse(prhs[greenPos+7])) throw("Input argument 'L' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+7])) throw("Input argument 'L' must be real.");
    if (!(mxGetNumberOfElements(prhs[greenPos+7])==1)) throw("Input argument 'L' must be a scalar.");
    
    if (!mxIsNumeric(prhs[greenPos+8])) throw("Input argument 'ky' must be numeric.");
    if (mxIsSparse(prhs[greenPos+8])) throw("Input argument 'ky' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+8])) throw("Input argument 'ky' must be real.");
    if ((mxGetNumberOfDimensions(prhs[greenPos+8])>2) ||
      ((mxGetM(prhs[greenPos+8])>1) && (mxGetN(prhs[greenPos+8])>1)))
                  throw("Input argument 'ky' must be a scalar or a vector.");
    
    if (!mxIsNumeric(prhs[greenPos+9])) throw("Input argument 'nmax' must be numeric.");
    if (mxIsSparse(prhs[greenPos+9])) throw("Input argument 'nmax' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+9])) throw("Input argument 'nmax' must be real.");
    if (!(mxGetNumberOfElements(prhs[greenPos+9])==1)) throw("Input argument 'nmax' must be a scalar.");
  }

  const unsigned int nugComp=5;
  const unsigned int nColDof=(probAxi ? 2 : 3);

  const double Cs=mxGetScalar(prhs[greenPos+1]);
  const double Cp=mxGetScalar(prhs[greenPos+2]);
  const double Ds=mxGetScalar(prhs[greenPos+3]);
  const double Dp=mxGetScalar(prhs[greenPos+4]);
  const double rho=mxGetScalar(prhs[greenPos+5]);
  const unsigned int nFreq=mxGetNumberOfElements(prhs[greenPos+6]);
  const double* const omega=mxGetPr(prhs[greenPos+6]);

  const double L=(probPeriodic ? mxGetScalar(prhs[greenPos+7]) : -1.0);
  const double* const ky=(probPeriodic ? mxGetPr(prhs[greenPos+8]) : 0);
  const unsigned int nWave=(probPeriodic ? mxGetNumberOfElements(prhs[greenPos+8]) : 0);
  const unsigned int nmax=(probPeriodic ? (unsigned int)mxGetScalar(prhs[greenPos+9]) : 0);

  const unsigned int nGrSet=nFreq;
  const bool ugCmplx=true;
  const bool tgCmplx=true;
  const bool tg0Cmplx=false;

  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=1;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nFreq;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=8;
  const unsigned int GreenFunType=3;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Cp;
  greenPtr[3]=&Ds;
  greenPtr[4]=&Dp;
  greenPtr[5]=&rho;
  greenPtr[6]=&nFreq;
  greenPtr[7]=omega;
  
  // OUTPUT ARGUMENT POINTERS
  //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=(probPeriodic ? 3+nGreenDim : 2+nGreenDim);
  size_t* const MatDim = new(nothrow) size_t[nMatDim];
  if (MatDim==0) throw("Out of memory.");
  MatDim[0]=(s==0 ? nDof : ms);
  MatDim[1]=(s==0 ? nDof : ns);
  
  for (unsigned int iDim=0; iDim<nGreenDim; iDim++)  MatDim[2+iDim]=greenDim[iDim];
  if (probPeriodic) MatDim[nMatDim-1]=nWave;
  
  bool uCmplx=false;
  if (ugCmplx || probPeriodic){uCmplx=true;}
  
  plhs[0]=mxCreateNumericArray(nMatDim,MatDim,mxDOUBLE_CLASS,(uCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

  bool tCmplx=false;
  if (tgCmplx || probPeriodic){tCmplx=true;}
  
  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
    plhs[1]=mxCreateNumericArray(nMatDim,MatDim,mxDOUBLE_CLASS,(tCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
  }
  delete [] MatDim;

  // BEMMAT
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
}

//==============================================================================
void IntegrateFsGreen3d0(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                         const bool probAxi, const bool probPeriodic, const unsigned int& probDim,
                         const double* const Nod,const unsigned int& nNod,
                         const double* const Elt,const unsigned int& nElt,
                         const unsigned int* const TypeID, char** const TypeName,
                         char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                         const unsigned int& nEltType, const double* const CollPoints,
                         const unsigned int& nTotalColl, 
						 // const int& nCentroidColl,
                         // const double* const CoincNodes, const bool& SlavesExist,
                         const bool& UmatOut,
						 const bool& TmatOut,
                         const unsigned int& greenPos,
                         const double* const s, const unsigned int& ms, const unsigned int& ns,
						 const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
						 const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
						 const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
						 const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
						 const unsigned int* const ncumulEltCollIndex, 
						 // const int& NEltCollIndex, 
						 const unsigned int* const eltCollIndex,
						 const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
						 const unsigned int* const RegularColl, 
						 // const int* const nRegularColl, const int* const nSingularColl,
						 const unsigned int* const ncumulEltNod, const double* const EltNod,
						 const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
						 const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
						  
						  
/* Initialize Green's function for static 3D Full space solution (FsGreen3d0)
 *
 *
 */
//==============================================================================
{
   // mexPrintf("TestIntegrate 3d0 \n");
   // mexPrintf("UmatOut: %s \n", UmatOut ? "true": "false");
  
  // INPUT ARGUMENT PROCESSING
  if ((probPeriodic&&(!(nrhs==10))) | (!(probPeriodic)&&(!((unsigned)nrhs==greenPos+3)))) throw("Wrong number of input arguments.");

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'E' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'E' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'E' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'E' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'nu' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'nu' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+2])) throw("Input argument 'nu' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+2])==1)) throw("Input argument 'nu' must be a scalar.");

  if (probPeriodic){
    if (!mxIsNumeric(prhs[greenPos+3])) throw("Input argument 'L' must be numeric.");
    if (mxIsSparse(prhs[greenPos+3])) throw("Input argument 'L' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+3])) throw("Input argument 'L' must be real.");
    if (!(mxGetNumberOfElements(prhs[greenPos+3])==1)) throw("Input argument 'L' must be a scalar.");
    
    if (!mxIsNumeric(prhs[greenPos+4])) throw("Input argument 'ky' must be numeric.");
    if (mxIsSparse(prhs[greenPos+4])) throw("Input argument 'ky' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+4])) throw("Input argument 'ky' must be real.");
    if ((mxGetNumberOfDimensions(prhs[greenPos+4])>2) ||
      ((mxGetM(prhs[greenPos+4])>1) && (mxGetN(prhs[greenPos+4])>1)))
                  throw("Input argument 'ky' must be a scalar or a vector.");
    
    if (!mxIsNumeric(prhs[greenPos+5])) throw("Input argument 'nmax' must be numeric.");
    if (mxIsSparse(prhs[greenPos+5])) throw("Input argument 'nmax' must not be sparse.");
    if (mxIsComplex(prhs[greenPos+5])) throw("Input argument 'nmax' must be real.");
    if (!(mxGetNumberOfElements(prhs[greenPos+5])==1)) throw("Input argument 'nmax' must be a scalar.");
  }

  const unsigned int nugComp=5;
  const unsigned int nColDof=(probAxi ? 2 : 3);

  const double E=mxGetScalar(prhs[greenPos+1]);
  const double nu=mxGetScalar(prhs[greenPos+2]);
  
  const double L=(probPeriodic ? mxGetScalar(prhs[greenPos+3]) : -1.0);
  const double* const ky=(probPeriodic ? mxGetPr(prhs[greenPos+4]) : 0);
  const unsigned int nWave=(probPeriodic ? mxGetNumberOfElements(prhs[greenPos+4]) : 0);
  const unsigned int nmax=(probPeriodic ? (unsigned int)mxGetScalar(prhs[greenPos+5]) : 0);

  const double mu=0.5*E/(1.0+nu);
  const double M=E*(1.0-nu)/(1.0+nu)/(1.0-2.0*nu);
  const double rho=1.0;

  const double Cs=sqrt(mu/rho);
  const double Cp=sqrt(M/rho);
  const double Ds=0.0;
  const double Dp=0.0;
  const unsigned int nFreq=1;
  double* const omega = new(nothrow) double[nFreq];
  omega[0]=0.0;

  const unsigned int nGrSet=nFreq;
  const bool ugCmplx=false;
  const bool tgCmplx=false;
  const bool tg0Cmplx=false;
  
  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=1;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nFreq;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=8;
  const unsigned int GreenFunType=3;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Cp;
  greenPtr[3]=&Ds;
  greenPtr[4]=&Dp;
  greenPtr[5]=&rho;
  greenPtr[6]=&nFreq;
  greenPtr[7]=omega;

   // OUTPUT ARGUMENT POINTERS
   //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=(probPeriodic ? 3+nGreenDim : 2+nGreenDim);
  // size_t* const MatDim = new(nothrow) size_t[nMatDim];
  // if (MatDim==0) throw("Out of memory.");
  // MatDim[0]=(s==0 ? nDof : ms);
  // MatDim[1]=(s==0 ? nDof : ns);

  size_t* const MatDimU = new(nothrow) size_t[nMatDim];
  if (MatDimU==0) throw("Out of memory.");
  MatDimU[0]=(s==0 ? nDof : ms);
  MatDimU[1]=(s==0 ? nDof : ns);
  if (UmatOut==false){MatDimU[0]=0; MatDimU[1]=0;}
  
  for (unsigned int iDim=0; iDim<nGreenDim; iDim++)  MatDimU[2+iDim]=greenDim[iDim];
  if (probPeriodic) MatDimU[nMatDim-1]=nWave;
  
  bool uCmplx=false;
  if (ugCmplx || probPeriodic){uCmplx=true;}
  
    
  plhs[0]=mxCreateNumericArray(nMatDim,MatDimU,mxDOUBLE_CLASS,(uCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);
  
  // double* URe=0;
  // double* UIm=0;
  // if (UmatOut){TRe=mxGetPr(plhs[1]); TIm=mxGetPi(plhs[1]);}
  

  bool tCmplx=false;
  if (tgCmplx || probPeriodic){tCmplx=true;}
  
  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
	size_t* const MatDimT = new(nothrow) size_t[nMatDim];
	if (MatDimT==0) throw("Out of memory.");
	MatDimT[0]=(s==0 ? nDof : ms);
	MatDimT[1]=(s==0 ? nDof : ns);
  
    for (unsigned int iDim=0; iDim<nGreenDim; iDim++)  MatDimT[2+iDim]=greenDim[iDim];
	if (probPeriodic) MatDimT[nMatDim-1]=nWave;
	
    plhs[1]=mxCreateNumericArray(nMatDim,MatDimT,mxDOUBLE_CLASS,(tCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
	
	delete [] MatDimT;
  }
  delete [] MatDimU;
  

  // BEMMAT
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
  delete [] omega;
}

//==============================================================================
void IntegrateFsGreen3dt(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                         const bool probAxi, const bool probPeriodic, const unsigned int& probDim,
                         const double* const Nod,const unsigned int& nNod,
                         const double* const Elt,const unsigned int& nElt,
                         const unsigned int* const TypeID, char** const TypeName,
                         char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                         const unsigned int& nEltType, const double* const CollPoints,
                         const unsigned int& nTotalColl, 
						 // const int& nCentroidColl,
                         // const double* const CoincNodes, const bool& SlavesExist,
                         const bool& UmatOut,
						 const bool& TmatOut,
                         const unsigned int& greenPos,
                         const double* const s, const unsigned int& ms, const unsigned int& ns,
						 const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
						 const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
						 const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
						 const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
						 const unsigned int* const ncumulEltCollIndex, 
						 // const int& NEltCollIndex, 
						 const unsigned int* const eltCollIndex,
						 const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
						 const unsigned int* const RegularColl, 
						 // const int* const nRegularColl, const int* const nSingularColl,
						 const unsigned int* const ncumulEltNod, const double* const EltNod,
						 const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
						 const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
//==============================================================================
{
  // INPUT ARGUMENT PROCESSING
  if (!(nrhs==9)) throw("Wrong number of input arguments.");

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'Cs' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'Cs' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'Cs' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'Cs' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'Cp' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'Cp' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+2])) throw("Input argument 'Cp' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+2])==1)) throw("Input argument 'Cp' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+3])) throw("Input argument 'rho' must be numeric.");
  if (mxIsSparse(prhs[greenPos+3])) throw("Input argument 'rho' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+3])) throw("Input argument 'rho' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+3])==1)) throw("Input argument 'rho' must be a scalar.");
  
  if (!mxIsNumeric(prhs[greenPos+4])) throw("Input argument 'delt' must be numeric.");
  if (mxIsSparse(prhs[greenPos+4])) throw("Input argument 'delt' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+4])) throw("Input argument 'delt' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+4])==1)) throw("Input argument 'delt' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+5])) throw("Input argument 't' must be numeric.");
  if (mxIsSparse(prhs[greenPos+5])) throw("Input argument 't' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+5])) throw("Input argument 't' must be real.");
  if ((mxGetNumberOfDimensions(prhs[greenPos+5])>2) ||
      ((mxGetM(prhs[greenPos+5])>1) && (mxGetN(prhs[greenPos+5])>1)))
    throw("Input argument 't' must be a scalar or a vector.");

  const unsigned int nugComp=5;
  const unsigned int nColDof=(probAxi ? 2 : 3);

  const double Cs=mxGetScalar(prhs[greenPos+1]);
  const double Cp=mxGetScalar(prhs[greenPos+2]);
  const double rho=mxGetScalar(prhs[greenPos+3]);
  const double delt=mxGetScalar(prhs[greenPos+4]);
  const unsigned int nTime=mxGetNumberOfElements(prhs[greenPos+5]);
  const double* const t=mxGetPr(prhs[greenPos+5]);

  const unsigned int nGrSet=nTime;
  const bool ugCmplx=false;
  const bool tgCmplx=false;
  const bool tg0Cmplx=false;

  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=1;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nTime;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=7;
  const unsigned int GreenFunType=7;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Cp;
  greenPtr[3]=&rho;
  greenPtr[4]=&delt;
  greenPtr[5]=&nTime;
  greenPtr[6]=t;
  
  // OUTPUT ARGUMENT POINTERS
  //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=2+nGreenDim;
  size_t* const MatDim = new(nothrow) size_t[nMatDim];
  if (MatDim==0) throw("Out of memory.");
  MatDim[0]=(s==0 ? nDof : ms);
  MatDim[1]=(s==0 ? nDof : ns);
  
  for (unsigned int iDim=2; iDim<nMatDim; iDim++)  MatDim[iDim]=greenDim[iDim-2];
  plhs[0]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(ugCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
    plhs[1]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(tgCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
  }
  delete [] MatDim;

  // BEMMAT
  const double L=-1.0;
  const double* const ky=0;
  const unsigned int nWave=0;
  const unsigned int nmax=0;
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,TypeName,
         TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,greenPtr,nGrSet,nugComp,ugCmplx,tgCmplx,
         tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
}

//==============================================================================
void IntegrateFsGreen2d_inplane(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                                const bool probAxi, const bool probPeriodic, const unsigned int& probDim,
                                const double* const Nod,const unsigned int& nNod,
                                const double* const Elt,const unsigned int& nElt,
                                const unsigned int* const TypeID, char** const TypeName,
                                char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                                const unsigned int& nEltType, const double* const CollPoints,
                                const unsigned int& nTotalColl, 
								// const int& nCentroidColl,
                                // const double* const CoincNodes, const bool& SlavesExist,
                                const bool& UmatOut,
								const bool& TmatOut,
                                const int& greenPos,
                                const double* const s, const unsigned int& ms, const unsigned int& ns,
								const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
								const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
								const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
								const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
								const unsigned int* const ncumulEltCollIndex, 
								// const int& NEltCollIndex, 
								const unsigned int* const eltCollIndex,
								const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
								const unsigned int* const RegularColl, 
								// const int* const nRegularColl, const int* const nSingularColl,
								const unsigned int* const ncumulEltNod, const double* const EltNod,
								const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
								const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
/* Initialize Green's function for user defined Green's function ('USER')
 *
 *    greenPtr[0]=&GreenFunType;   Green's function type identifier
 *    greenPtr[1]=&nGrSet;         Number of function sets (nFreq,nTime,nCase)
 *    greenPtr[...]                different definition for different
 *                                 Green's function types
 *
 */
//==============================================================================
{
  // INPUT ARGUMENT PROCESSING
  if (!(nrhs==10)) throw("Wrong number of input arguments.");

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'Cs' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'Cs' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'Cs' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'Cs' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'Cp' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'Cp' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+2])) throw("Input argument 'Cp' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+2])==1)) throw("Input argument 'Cp' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+3])) throw("Input argument 'Ds' must be numeric.");
  if (mxIsSparse(prhs[greenPos+3])) throw("Input argument 'Ds' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+3])) throw("Input argument 'Ds' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+3])==1)) throw("Input argument 'Ds' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+4])) throw("Input argument 'Dp' must be numeric.");
  if (mxIsSparse(prhs[greenPos+4])) throw("Input argument 'Dp' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+4])) throw("Input argument 'Dp' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+4])==1)) throw("Input argument 'Dp' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+5])) throw("Input argument 'rho' must be numeric.");
  if (mxIsSparse(prhs[greenPos+5])) throw("Input argument 'rho' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+5])) throw("Input argument 'rho' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+5])==1)) throw("Input argument 'rho' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+6])) throw("Input argument 'omega' must be numeric.");
  if (mxIsSparse(prhs[greenPos+6])) throw("Input argument 'omega' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+6])) throw("Input argument 'omega' must be real.");
  if ((mxGetNumberOfDimensions(prhs[greenPos+6])>2) ||
      ((mxGetM(prhs[greenPos+6])>1) && (mxGetN(prhs[greenPos+6])>1)))
    throw("Input argument 'omega' must be a scalar or a vector.");

  const unsigned int nugComp=4;
  const unsigned int nColDof=2;

  const double Cs=mxGetScalar(prhs[greenPos+1]);
  const double Cp=mxGetScalar(prhs[greenPos+2]);
  const double Ds=mxGetScalar(prhs[greenPos+3]);
  const double Dp=mxGetScalar(prhs[greenPos+4]);
  const double rho=mxGetScalar(prhs[greenPos+5]);
  const double* const omega=mxGetPr(prhs[greenPos+6]);
  const unsigned int nFreq=mxGetNumberOfElements(prhs[greenPos+6]);

  const unsigned int nGrSet=nFreq;
  const bool ugCmplx=true;
  const bool tgCmplx=true;
  const bool tg0Cmplx=true;

  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=1;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nFreq;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=8;
  const unsigned int GreenFunType=4;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Cp;
  greenPtr[3]=&Ds;
  greenPtr[4]=&Dp;
  greenPtr[5]=&rho;
  greenPtr[6]=&nFreq;
  greenPtr[7]=omega;

  // OUTPUT ARGUMENT POINTERS
  //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=2+nGreenDim;
  size_t* const MatDim = new(nothrow) size_t[nMatDim];
  if (MatDim==0) throw("Out of memory.");
  MatDim[0]=(s==0 ? nDof : ms);
  MatDim[1]=(s==0 ? nDof : ns);
  
  for (unsigned int iDim=2; iDim<nMatDim; iDim++)  MatDim[iDim]=greenDim[iDim-2];
  plhs[0]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(ugCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
    plhs[1]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(tgCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
  }
  delete [] MatDim;

  // BEMMAT
  const double L=-1.0;
  const double* const ky=0;
  const unsigned int nWave=0;
  const unsigned int nmax=0;
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
}

//==============================================================================
void IntegrateFsGreen2d_inplane0(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                                 const bool probAxi, const bool probPeriodic, const unsigned int& probDim,
                                 const double* const Nod,const unsigned int& nNod,
                                 const double* const Elt,const unsigned int& nElt,
                                 const unsigned int* const TypeID, char** const TypeName,
                                 char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                                 const unsigned int& nEltType, const double* const CollPoints,
                                 const unsigned int& nTotalColl, 
								 // const int& nCentroidColl,
                                 // const double* const CoincNodes, const bool& SlavesExist,
                                 const bool& UmatOut,
								 const bool& TmatOut,
                                 const unsigned int& greenPos,
                                 const double* const s, const unsigned int& ms, const unsigned int& ns,
								 const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
								 const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
								 const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
								 const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
								 const unsigned int* const ncumulEltCollIndex, 
								 // const int& NEltCollIndex, 
								 const unsigned int* const eltCollIndex,
								 const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
								 const unsigned int* const RegularColl, 
								 // const int* const nRegularColl, const int* const nSingularColl,
								 const unsigned int* const ncumulEltNod, const double* const EltNod,
								 const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
								 const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
/* Initialize Green's function for user defined Green's function ('FSGREEN2D_INPLANE0')
 *
 *    greenPtr[0]=&GreenFunType;   Green's function type identifier
 *    greenPtr[1]=&nGrSet;         Number of function sets (nFreq,nTime,nCase)
 *    greenPtr[...]                different definition for different
 *                                 Green's function types
 *
 */
//==============================================================================
{
  // INPUT ARGUMENT PROCESSING
  if (!(nrhs==6)) throw("Wrong number of input arguments.");

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'E' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'E' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'E' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'E' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'nu' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'nu' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+2])) throw("Input argument 'nu' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+2])==1)) throw("Input argument 'nu' must be a scalar.");

  const unsigned int nugComp=4;
  const unsigned int nColDof=2;

  const double E=mxGetScalar(prhs[greenPos+1]);
  const double nu=mxGetScalar(prhs[greenPos+2]);

  const double mu=0.5*E/(1.0+nu);
  const double M=E*(1.0-nu)/(1.0+nu)/(1.0-2.0*nu);
  const double rho=1.0;

  const double Cs=sqrt(mu/rho);
  const double Cp=sqrt(M/rho);
  const double Ds=0.0;
  const double Dp=0.0;
  const unsigned int nFreq=1;
  double* const omega = new(nothrow) double[nFreq];
  omega[0]=0.0;

  const unsigned int nGrSet=nFreq;
  const bool ugCmplx=false;
  const bool tgCmplx=false;
  const bool tg0Cmplx=false;

  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=1;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nFreq;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=8;
  const unsigned int GreenFunType=4;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Cp;
  greenPtr[3]=&Ds;
  greenPtr[4]=&Dp;
  greenPtr[5]=&rho;
  greenPtr[6]=&nFreq;
  greenPtr[7]=omega;

  // OUTPUT ARGUMENT POINTERS
  //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=2+nGreenDim;
  size_t* const MatDim = new(nothrow) size_t[nMatDim];
  if (MatDim==0) throw("Out of memory.");
  MatDim[0]=(s==0 ? nDof : ms);
  MatDim[1]=(s==0 ? nDof : ns);
  for (unsigned int iDim=2; iDim<nMatDim; iDim++)  MatDim[iDim]=greenDim[iDim-2];
  plhs[0]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(ugCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
    plhs[1]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(tgCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
  }
  delete [] MatDim;

  // BEMMAT
  const double L=-1.0;
  const double* const ky=0;
  const unsigned int nWave=0;
  const unsigned int nmax=0;
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
  delete [] omega;
}

//==============================================================================
void IntegrateFsGreen2d_outofplane(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                                   const bool probAxi, const bool probPeriodic, const unsigned int& probDim,
                                   const double* const Nod,const unsigned int& nNod,
                                   const double* const Elt,const unsigned int& nElt,
                                   const unsigned int* const TypeID, char** const TypeName,
                                   char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                                   const unsigned int& nEltType, const double* const CollPoints,
                                   const unsigned int& nTotalColl, 
								   // const int& nCentroidColl,
                                   // const double* const CoincNodes, const bool& SlavesExist,
                                   const bool& UmatOut,
								   const bool& TmatOut,
                                   const int& greenPos,
                                   const double* const s, const unsigned int& ms, const unsigned int& ns,
								   const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
								   const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
						           const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
						           const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
						           const unsigned int* const ncumulEltCollIndex, 
								   // const int& NEltCollIndex, 
								   const unsigned int* const eltCollIndex,
								   const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
						           const unsigned int* const RegularColl, 
								   // const int* const nRegularColl, const int* const nSingularColl,
								   const unsigned int* const ncumulEltNod, const double* const EltNod,
								   const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
								   const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
/* Initialize Green's function for user defined Green's function ('USER')
 *
 *    greenPtr[0]=&GreenFunType;   Green's function type identifier
 *    greenPtr[1]=&nGrSet;         Number of function sets (nFreq,nTime,nCase)
 *    greenPtr[...]                different definition for different
 *                                 Green's function types
 *
 */
//==============================================================================
{
  // INPUT ARGUMENT PROCESSING
  if (!(nrhs==8)) throw("Wrong number of input arguments.");

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'Cs' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'Cs' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'Cs' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'Cs' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+2])) throw("Input argument 'Ds' must be numeric.");
  if (mxIsSparse(prhs[greenPos+2])) throw("Input argument 'Ds' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+2])) throw("Input argument 'Ds' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+2])==1)) throw("Input argument 'Ds' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+3])) throw("Input argument 'rho' must be numeric.");
  if (mxIsSparse(prhs[greenPos+3])) throw("Input argument 'rho' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+3])) throw("Input argument 'rho' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+3])==1)) throw("Input argument 'rho' must be a scalar.");

  if (!mxIsNumeric(prhs[greenPos+4])) throw("Input argument 'omega' must be numeric.");
  if (mxIsSparse(prhs[greenPos+4])) throw("Input argument 'omega' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+4])) throw("Input argument 'omega' must be real.");
  if ((mxGetNumberOfDimensions(prhs[greenPos+4])>2) ||
      ((mxGetM(prhs[greenPos+4])>1) && (mxGetN(prhs[greenPos+4])>1)))
    throw("Input argument 'omega' must be a scalar or a vector.");

  const unsigned int nugComp=1;
  const unsigned int nColDof=1;

  const double Cs  = mxGetScalar(prhs[greenPos+1]);
  const double Ds  = mxGetScalar(prhs[greenPos+2]);
  const double rho = mxGetScalar(prhs[greenPos+3]);
  const double* const omega = mxGetPr(prhs[greenPos+4]);
  const unsigned int nFreq = mxGetNumberOfElements(prhs[greenPos+4]);

  const unsigned int nGrSet=nFreq;
  const bool ugCmplx=true;
  const bool tgCmplx=true;
  const bool tg0Cmplx=true;

  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=1;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nFreq;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=6;
  const unsigned int GreenFunType=5;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Ds;
  greenPtr[3]=&rho;
  greenPtr[4]=&nFreq;
  greenPtr[5]=omega;

  // OUTPUT ARGUMENT POINTERS
  //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=2+nGreenDim;
  size_t* const MatDim = new(nothrow) size_t[nMatDim];
  if (MatDim==0) throw("Out of memory.");
  MatDim[0]=(s==0 ? nDof : ms);
  MatDim[1]=(s==0 ? nDof : ns);
  for (unsigned int iDim=2; iDim<nMatDim; iDim++)  MatDim[iDim]=greenDim[iDim-2];
  plhs[0]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(ugCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
    plhs[1]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(tgCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
  }
  delete [] MatDim;

  // BEMMAT
  const double L=-1.0;
  const double* const ky=0;
  const unsigned int nWave=0;
  const unsigned int nmax=0;
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
}

//==============================================================================
void IntegrateFsGreen2d_outofplane0(mxArray* plhs[], int nrhs, const mxArray* prhs[],
                                    const bool probAxi, const bool probPeriodic, const unsigned int& probDim,
                                    const double* const Nod,const unsigned int& nNod,
                                    const double* const Elt,const unsigned int& nElt,
                                    const unsigned int* const TypeID, char** const TypeName,
                                    char** const TypeKeyOpts, const unsigned int* const nKeyOpt,
                                    const unsigned int& nEltType, const double* const CollPoints,
                                    const unsigned int& nTotalColl, 
									// const int& nCentroidColl,
                                    // const double* const CoincNodes, const bool& SlavesExist,
                                    const bool& UmatOut,
									const bool& TmatOut,
                                    const unsigned int& greenPos,
                                    const double* const s, const unsigned int& ms, const unsigned int& ns,
									const unsigned int* const EltParent, const unsigned int* const nEltNod, const unsigned int* const nEltColl,
									const unsigned int* const EltShapeN, const unsigned int* const EltShapeM, const unsigned int* const EltDim,
									const unsigned int* const AxiSym, const unsigned int* const Periodic, const unsigned int* const nGauss,
									const unsigned int* const nEltDiv, const unsigned int* const nGaussSing, const unsigned int* const nEltDivSing,
									const unsigned int* const ncumulEltCollIndex, 
									// const int& NEltCollIndex, 
									const unsigned int* const eltCollIndex,
									const unsigned int* const ncumulSingularColl, const unsigned int* const nSingularColl, const int& NSingularColl, 
									const unsigned int* const RegularColl, 
									// const int* const nRegularColl, const int* const nSingularColl,
									const unsigned int* const ncumulEltNod, const double* const EltNod,
									const unsigned int* const RefEltType, const unsigned int* const ncumulnXi, const unsigned int* const nXi, const double* const xi, const double* const H,
									const unsigned int* const ncumulNshape, const double* const Nshape, const double* const Mshape, const double* const dNshape)
/* Initialize Green's function for user defined Green's function ('IntegrateFsGreen2d_outofplane0')
 *
 *
 *    greenPtr[0]=&GreenFunType;   Green's function type identifier
 *    greenPtr[1]=&nGrSet;         Number of function sets (nFreq,nTime,nCase)
 *    greenPtr[...]                different definition for different
 *                                 Green's function types
 *
 */
//==============================================================================
{
  // INPUT ARGUMENT PROCESSING
  if (!(nrhs==5)) throw("Wrong number of input arguments.");

  if (!mxIsNumeric(prhs[greenPos+1])) throw("Input argument 'mu' must be numeric.");
  if (mxIsSparse(prhs[greenPos+1])) throw("Input argument 'mu' must not be sparse.");
  if (mxIsComplex(prhs[greenPos+1])) throw("Input argument 'mu' must be real.");
  if (!(mxGetNumberOfElements(prhs[greenPos+1])==1)) throw("Input argument 'mu' must be a scalar.");

  const unsigned int nugComp=1;
  const unsigned int nColDof=1;

  const double mu=mxGetScalar(prhs[greenPos+1]);
  const double rho=1.0;
  const double Cs=sqrt(mu/rho);
  const double Ds=0.0;
  const unsigned int nFreq=1;
  double* const omega = new(nothrow) double[1];
  omega[0]=0.0;

  const unsigned int nGrSet=nFreq;
  const bool ugCmplx=false;
  const bool tgCmplx=false;
  const bool tg0Cmplx=false;

  // OUTPUT ARGUMENT LAST DIMENSIONS
  const unsigned int nGreenDim=1;
  unsigned int* const greenDim=new(nothrow) unsigned int[nGreenDim];
  greenDim[0]=nFreq;

  // COPY VARIABLES TO GENERIC ARRAY OF POINTERS GREENPTR
  const unsigned int nGreenPtr=6;
  const unsigned int GreenFunType=5;
  const void** const greenPtr=new(nothrow) const void*[nGreenPtr];
    if (greenPtr==0) throw("Out of memory.");

  greenPtr[0]=&GreenFunType;
  greenPtr[1]=&Cs;
  greenPtr[2]=&Ds;
  greenPtr[3]=&rho;
  greenPtr[4]=&nFreq;
  greenPtr[5]=omega;

  // OUTPUT ARGUMENT POINTERS
  //  unsigned int nDof=nColDof*nTotalColl;
  uint64 nDof=nColDof*nTotalColl;
  const unsigned int nMatDim=2+nGreenDim;
  size_t* const MatDim = new(nothrow) size_t[nMatDim];
  if (MatDim==0) throw("Out of memory.");
  MatDim[0]=(s==0 ? nDof : ms);
  MatDim[1]=(s==0 ? nDof : ns);
  for (unsigned int iDim=2; iDim<nMatDim; iDim++)  MatDim[iDim]=greenDim[iDim-2];
  plhs[0]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(ugCmplx ? mxCOMPLEX : mxREAL));
  double* const URe=mxGetPr(plhs[0]);
  double* const UIm=mxGetPi(plhs[0]);

  double* TRe=0;
  double* TIm=0;
  if (TmatOut)
  {
    plhs[1]=mxCreateNumericArray(2+nGreenDim,MatDim,mxDOUBLE_CLASS,(tgCmplx ? mxCOMPLEX : mxREAL));
    TRe=mxGetPr(plhs[1]);
    TIm=mxGetPi(plhs[1]);
  }
  delete [] MatDim;

  // BEMMAT
  const double L=-1.0;
  const double* const ky=0;
  const unsigned int nWave=0;
  const unsigned int nmax=0;
  bemmat(probAxi,probPeriodic,probDim,nColDof,UmatOut,TmatOut,Nod,nNod,Elt,nElt,TypeID,
         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
         greenPtr,nGrSet,nugComp,
         ugCmplx,tgCmplx,tg0Cmplx,URe,UIm,TRe,TIm,s,ms,ns,L,ky,nWave,nmax,
		 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
		 ncumulEltCollIndex,eltCollIndex,
		 ncumulSingularColl,nSingularColl,NSingularColl, 
		 RegularColl,
		 ncumulEltNod,EltNod,
		 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
  delete [] greenPtr;
  delete [] greenDim;
  delete [] omega;
}


//==============================================================================
void cleanup()
{

	// mexPrintf("cleanup start.. \n");
	
	CacheValid=false;

	
	// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
	// mexPrintf("nNod: %d \n",nNod); // DEBUG
	// for(int i = 0; i < 4*nNod; i++) {
            // mexPrintf("Nod[%d]: %f\n",i,Nod[i]); // DEBUG
	// }
	
	// // // // Nod=0;
	
	if (Nod!=0){delete [] Nod;}
	// if (Nodtest!=0){delete [] Nodtest;}
	
	
	// mxFree(Nod);
		
	// // // // Elt=0;
    if (Elt!=0){delete [] Elt;}
    // mxFree(Elt);
	// /*
	
     for (unsigned int iTyp=0; iTyp<nEltType; iTyp++)
    {
      mxFree(TypeName[iTyp]);
      for (unsigned int iKeyOpt=0; iKeyOpt<nKeyOpt[iTyp]; iKeyOpt++) mxFree(TypeKeyOpts[iTyp+nEltType*iKeyOpt]);
    }

	
	// TypeID=0;
	if (TypeID!=0){delete [] TypeID;}
    
	// nKeyOpt=0;
	if (nKeyOpt!=0){delete [] nKeyOpt;}
	
	
	// TypeName=0;
	if (TypeName!=0){delete [] TypeName;}
    
	// TypeKeyOpts=0;
	if (TypeKeyOpts!=0){delete [] TypeKeyOpts;}
	
	
	// NodalColl=0;
	if (NodalColl!=0){delete [] NodalColl;}

	// CentroidColl=0;
	if (CentroidColl!=0){delete [] CentroidColl;}

	// CollPoints=0;
	if (CollPoints!=0){delete [] CollPoints;}
	
	// CoincNodes=0;
	if (CoincNodes!=0){delete [] CoincNodes;}
	
	
	// */
	
	nNod=0;
	nElt=0;
    maxEltColumn=0;

    keyOpts=0;
    nEltType=0;
    maxKeyOpts = 50;
	probDim=0;
	probAxi=0;
	probPeriodic=0;
   	nCentroidColl=0;
    nNodalColl=0;
	nTotalColl=0;
    SlavesExist=0;
	
	
	if (EltParent!=0){delete [] EltParent;}
	if (nEltNod!=0){delete [] nEltNod;}
	if (nEltColl!=0){delete [] nEltColl;}
	if (EltShapeN!=0){delete [] EltShapeN;}
	if (EltShapeM!=0){delete [] EltShapeM;}
	if (EltDim!=0){delete [] EltDim;}
	if (AxiSym!=0){delete [] AxiSym;}
	if (Periodic!=0){delete [] Periodic;}
	if (nGauss!=0){delete [] nGauss;}
	if (nEltDiv!=0){delete [] nEltDiv;}
	if (nGaussSing!=0){delete [] nGaussSing;}
	if (nEltDivSing!=0){delete [] nEltDivSing;}
	
	if (ncumulEltCollIndex!=0){delete [] ncumulEltCollIndex;}
	NEltCollIndex=0;
	if (eltCollIndex!=0){delete [] eltCollIndex;}
	
	if (RegularColl!=0){delete [] RegularColl;}
	
	if (nRegularColl!=0){delete [] nRegularColl;}
	if (nSingularColl!=0){delete [] nSingularColl;}
	
	if (ncumulEltNod!=0){delete [] ncumulEltNod;}
	NEltNod=0;
	if (EltNod!=0){delete [] EltNod;}
	
	
	if (ncumulSingularColl!=0){delete [] ncumulSingularColl;}
	NSingularColl=0;
		
	
	
	if (RefEltType!=0){delete [] RefEltType;}
		
	if (ncumulnXi!=0){delete [] ncumulnXi;}
	NnXi=0;
	if (nXi!=0){delete [] nXi;}
	
	if (xi!=0){delete [] xi;}
	if (H!=0){delete [] H;}
	
	if (ncumulNshape!=0){delete [] ncumulNshape;}
	NNshape=0;
	if (Nshape!=0){delete [] Nshape;}
	if (Mshape!=0){delete [] Mshape;}
	if (dNshape!=0){delete [] dNshape;}
	
	// mexPrintf("cleanup end... \n");
	// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
  }
//==============================================================================


//==============================================================================
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
//==============================================================================
{

// mexPrintf("Start mexFunction... \n");
// mexPrintf("maxKeyOpts: %d \n",maxKeyOpts);

 time_t  start_bemmat_mex = clock();  
 
  mexAtExit(cleanup);
  try
  {
    //checklicense();

    // INPUT ARGUMENT PROCESSING
    // if (nrhs<4) throw("Not enough input arguments.");
	if (nrhs<3) throw("Not enough input arguments.");
    if (nlhs>2) throw("Too many output arguments.");

	time_t  start_mesh = clock();  
	


	const bool TmatOut=(nlhs>1);
	
	bool Cache=false;
	if (nrhs==3 && !mxIsChar(prhs[0])){Cache=true;}
	bool getCache=false;
		
	
	bool UmatOut=true;
	unsigned int ns=0;
    unsigned int ms=0;
    double* s=0;
    unsigned int greenPos=3;  
	
	if (mxIsChar(prhs[0]))
	{
		getCache=true;
		greenPos=0;  
	}
	if (mxIsChar(prhs[1]))
	{
		getCache=true;
		greenPos=1;
		if (!mxIsNumeric(prhs[0])) throw("Input argument 's' must be numeric.");
        if (mxIsSparse(prhs[0])) throw("Input argument 's' must not be sparse.");
        if (mxIsComplex(prhs[0])) throw("Input argument 's' must be real.");
        ms=mxGetM(prhs[0]);
        ns=mxGetN(prhs[0]);
        s=mxGetPr(prhs[0]);
	}
	if (mxIsChar(prhs[2]))
	{
		getCache=true;
		UmatOut=false;
		greenPos=2;
	    		
	    if (!mxIsEmpty(prhs[0])) throw("Currently not supported...");
	    if (!mxIsNumeric(prhs[1])) throw("Input argument 's' must be numeric.");
        if (mxIsSparse(prhs[1])) throw("Input argument 's' must not be sparse.");
        if (mxIsComplex(prhs[1])) throw("Input argument 's' must be real.");
        ms=mxGetM(prhs[1]);
        ns=mxGetN(prhs[1]);
        s=mxGetPr(prhs[1]);
	}
	
	// mexPrintf("Cache: %s \n", Cache ? "true": "false");
	// mexPrintf("getCache: %s \n", getCache ? "true": "false");

	
	
	
	
	if (getCache==false)
	{
	
		// mexPrintf("Calculate mesh attributes ... \n");
		// cleanup();
		
		CacheValid=true;
		
		
		
		if (!mxIsNumeric(prhs[0])) throw("Input argument 'nod' must be numeric.");
		if (mxIsSparse(prhs[0])) throw("Input argument 'nod' must not be sparse.");
		if (mxIsComplex(prhs[0])) throw("Input argument 'nod' must be real.");
		if (!(mxGetN(prhs[0])==4)) throw("Input argument 'nod' should have 4 columns.");
		nNod=mxGetM(prhs[0]);
		
		// Nod=mxGetPr(prhs[0]);
		const double* const Nod0=mxGetPr(prhs[0]);
		
		
		
		// delete [] Nod;
		// Nod=new(nothrow) double[4*nNod];
		// if (Nod==0) throw("Out of memory.");
		// delete [] Nod;
		
		
		delete [] Nod;
		double* const Nodtest=new(nothrow) double[4*nNod];
		// mexPrintf("4*nNod: %d \n",4*nNod); // DEBUG
		// double* Nodtest=new(nothrow) double[4*nNod];
		if (Nodtest==0) throw("Out of memory.");
		
		for(unsigned int i = 0; i < 4*nNod; i++) {
		Nodtest[i]=Nod0[i];
		}
			
		Nod=Nodtest;
		
		
		
		// double* const Nod=new(nothrow) double[4*nNod];
		// if (Nod==0) throw("Out of memory.");
		
		
		
		// if (Nod==0) throw("Club brugge kampioen!");
		
		// delete [] Nodtest;
		// // const double* const Nodtest=new(nothrow) double[4*nNod];
		// double* const Nodtest=new(nothrow) double[4*nNod];
		// if (Nodtest==0) throw("Out of memory.");
		// delete [] Nodtest;
		
		// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
		// for(int i = 0; i < 4*nNod; i++) {
		// Nodtest[i]=Nod0[i];
		// }
		
		// Nod=Nodtest;
		
		
		// Nod=Nod0;
		
		// for(int i = 0; i < 4*nNod; i++) {
			// Nod[i]=Nod0[i];
		// }
	
		// for(int i = 0; i < 4*nNod; i++) {
                     // mexPrintf("Nod0[%d]: %f\n",i,Nod0[i]); // DEBUG
		// }

		
		// for(int i = 0; i < 4*nNod; i++) {
                     // mexPrintf("Nod[%d]: %f\n",i,Nod[i]); // DEBUG
		// }
		
		// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
		
		if (!mxIsNumeric(prhs[1])) throw("Input argument 'elt' must be numeric.");
		if (mxIsSparse(prhs[1])) throw("Input argument 'elt' must not be sparse.");
		if (mxIsComplex(prhs[1])) throw("Input argument 'elt' must be real.");
		if (mxGetN(prhs[1])<=3) throw("Input argument 'elt' should have at least 3 columns.");
		nElt=mxGetM(prhs[1]);
		maxEltColumn=mxGetN(prhs[1]);
		// Elt=mxGetPr(prhs[1]);
		const double* const Elt0=mxGetPr(prhs[1]);
	
		delete [] Elt;
		double* const Elttest=new(nothrow) double[maxEltColumn*nElt];
		// mexPrintf("maxEltColumn*nElt: %d \n",maxEltColumn*nElt); // DEBUG
		if (Elttest==0) throw("Out of memory.");
		
		for(unsigned int i = 0; i < maxEltColumn*nElt; i++) {
			Elttest[i]=Elt0[i];
		}
			
		Elt=Elttest;
		
	
		// delete [] Elt;
		// Elt=new(nothrow) double[maxEltColumn*nElt];
		// if (Elt==0) throw("Out of memory.");
		
		
		// for(int i = 0; i < maxEltColumn*nElt; i++) {
		// Elt[i]=Elt0[i];
		// }
		

		// for(int i = 0; i < 10*nElt; i++) {
                     // mexPrintf("Elt0[%d]: %f\n",i,Elt0[i]); // DEBUG
		// }

		// for(int i = 0; i < 10*nElt; i++) {
                     // mexPrintf("Elt[%d]: %f\n",i,Elt[i]); // DEBUG
		// }
		
		// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
		
		// /*
		
		// bool keyOpts=true;
		if (mxGetN(prhs[2])==3) keyOpts=true;
		else if  (mxGetN(prhs[2])==2) keyOpts=false;
		else throw("Input argument 'typ' should have 2 or 3 columns.");
		if (!(mxIsCell(prhs[2]))) throw("Input argument 'typ' should be a cell array.");
		nEltType=mxGetM(prhs[2]);
		// const int maxKeyOpts = 50;  // Maximum number of keyoptions per element type
		
		// mexPrintf("nEltType: %d \n",nEltType);
		
		

		delete [] TypeID;
		TypeID=new(nothrow) unsigned int[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (TypeID==0) throw("Out of memory.");
		
		delete [] nKeyOpt;
		nKeyOpt=new(nothrow) unsigned int[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (nKeyOpt==0) throw("Out of memory.");
		
		

		delete [] TypeName;
		TypeName=new(nothrow) char*[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (TypeName==0) throw("Out of memory.");
		
		delete [] TypeKeyOpts;
		TypeKeyOpts=new(nothrow) char*[nEltType*maxKeyOpts];
		// mexPrintf("nEltType*maxKeyOpts: %d \n",nEltType*maxKeyOpts); // DEBUG
		if (TypeKeyOpts==0) throw("Out of memory.");
		
	
		
				
		for (unsigned int iTyp=0; iTyp<nEltType; iTyp++)
		{ 
		// TypeID
		const mxArray* TypPtr0=mxGetCell(prhs[2],iTyp+nEltType*0);
		if (!mxIsNumeric(TypPtr0)) throw("Type ID should be numeric.");
		if (mxIsSparse(TypPtr0)) throw("Type ID should not be sparse.");
		if (mxIsComplex(TypPtr0)) throw("Type ID should not be complex.");
		if (!(mxGetNumberOfElements(TypPtr0)==1)) throw("Type ID should be a scalar.");
		TypeID[iTyp]= int(mxGetScalar(TypPtr0));
      
	  
		// for(int i = 0; i < nEltType; i++) {
                     // mexPrintf("TypeID[%d]: %d\n",i,TypeID[i]); // DEBUG
		// }
	  
		// TypeName
		const mxArray* TypPtr1=mxGetCell(prhs[2],iTyp+nEltType*1);
		if (!mxIsChar(TypPtr1)) throw("Element types should be input as stings.");
		TypeName[iTyp] =  mxArrayToString(TypPtr1);
      
		// for(int i = 0; i < nEltType; i++) {
                     // mexPrintf("TypeName[%d]: %s \n",i,TypeName[i]); // DEBUG
		// }
	  
		// mexPrintf("maxKeyOpts: %d \n",maxKeyOpts);
	  
		// TypeKeyOpts
		if (keyOpts)
		{
			const mxArray* TypPtr2=mxGetCell(prhs[2],iTyp+nEltType*2); // Keyoptions cell array
			if (!mxIsCell(TypPtr2)) throw("Keyopts should be input as a cell array of stings.");
			nKeyOpt[iTyp]= mxGetNumberOfElements(TypPtr2);
			if (nKeyOpt[iTyp] > maxKeyOpts) throw("Number of keyoptions is too large.");
			for (unsigned int iKeyOpt=0; iKeyOpt<nKeyOpt[iTyp]; iKeyOpt++)
			{
			const mxArray* keyOptPtr=mxGetCell(TypPtr2,iKeyOpt);
			if (!mxIsChar(keyOptPtr)) throw("Keyopts should be input as a cell array of stings.");
			TypeKeyOpts[iTyp+nEltType*iKeyOpt] = mxArrayToString(keyOptPtr);
			}
		}
		else nKeyOpt[iTyp]=0;
    
	
		}
		
		// */
		
	// // // mexAtExit(cleanup);
	
	
	// // // }
	

	/*
    if (!mxIsNumeric(prhs[0])) throw("Input argument 'nod' must be numeric.");
    if (mxIsSparse(prhs[0])) throw("Input argument 'nod' must not be sparse.");
    if (mxIsComplex(prhs[0])) throw("Input argument 'nod' must be real.");
    if (!(mxGetN(prhs[0])==4)) throw("Input argument 'nod' should have 4 columns.");
    const int nNod=mxGetM(prhs[0]);
    const double* const Nod=mxGetPr(prhs[0]);

    if (!mxIsNumeric(prhs[1])) throw("Input argument 'elt' must be numeric.");
    if (mxIsSparse(prhs[1])) throw("Input argument 'elt' must not be sparse.");
    if (mxIsComplex(prhs[1])) throw("Input argument 'elt' must be real.");
    if (mxGetN(prhs[1])<=3) throw("Input argument 'elt' should have at least 3 columns.");
    const double* const Elt=mxGetPr(prhs[1]);
    const int nElt=mxGetM(prhs[1]);
    const int maxEltColumn=mxGetN(prhs[1]);

    bool keyOpts=true;
    if (mxGetN(prhs[2])==3) keyOpts=true;
    else if  (mxGetN(prhs[2])==2) keyOpts=false;
    else throw("Input argument 'typ' should have 2 or 3 columns.");
    if (!(mxIsCell(prhs[2]))) throw("Input argument 'typ' should be a cell array.");
    const int nEltType=mxGetM(prhs[2]);
    const int maxKeyOpts = 50;  // Maximum number of keyoptions per element type
    int* const TypeID=new(nothrow) int[nEltType];
      if (TypeID==0) throw("Out of memory.");
    int* const nKeyOpt=new(nothrow) int[nEltType];
      if (nKeyOpt==0) throw("Out of memory.");
    char** const TypeName=new(nothrow) char*[nEltType];
      if (TypeName==0) throw("Out of memory.");
    char** const TypeKeyOpts=new(nothrow) char*[nEltType*maxKeyOpts];
      if (TypeKeyOpts==0) throw("Out of memory.");
    for (int iTyp=0; iTyp<nEltType; iTyp++)
    { 
      // TypeID
      const mxArray* TypPtr0=mxGetCell(prhs[2],iTyp+nEltType*0);
      if (!mxIsNumeric(TypPtr0)) throw("Type ID should be numeric.");
      if (mxIsSparse(TypPtr0)) throw("Type ID should not be sparse.");
      if (mxIsComplex(TypPtr0)) throw("Type ID should not be complex.");
      if (!(mxGetNumberOfElements(TypPtr0)==1)) throw("Type ID should be a scalar.");
      TypeID[iTyp]= int(mxGetScalar(TypPtr0));
      
      // TypeName
      const mxArray* TypPtr1=mxGetCell(prhs[2],iTyp+nEltType*1);
      if (!mxIsChar(TypPtr1)) throw("Element types should be input as stings.");
      TypeName[iTyp] =  mxArrayToString(TypPtr1);
      
      // TypeKeyOpts
      if (keyOpts)
      {
        const mxArray* TypPtr2=mxGetCell(prhs[2],iTyp+nEltType*2); // Keyoptions cell array
        if (!mxIsCell(TypPtr2)) throw("Keyopts should be input as a cell array of stings.");
        nKeyOpt[iTyp]= mxGetNumberOfElements(TypPtr2);
        if (nKeyOpt[iTyp] > maxKeyOpts) throw("Number of keyoptions is too large.");
        for (int iKeyOpt=0; iKeyOpt<nKeyOpt[iTyp]; iKeyOpt++)
        {
          const mxArray* keyOptPtr=mxGetCell(TypPtr2,iKeyOpt);
          if (!mxIsChar(keyOptPtr)) throw("Keyopts should be input as a cell array of stings.");
          TypeKeyOpts[iTyp+nEltType*iKeyOpt] = mxArrayToString(keyOptPtr);
        }
      }
      else nKeyOpt[iTyp]=0;
    }
	*/	
	
    
	
	
	// bool UmatOut=(plhs[0]!=0);
	// const bool UmatOut=(plhs[0]==0);
	// mexPrintf("UmatOut: %s \n", UmatOut ? "true": "false");
	// mexPrintf("plhs[0]: %d \n", plhs[0]);
	
	
	// ORIGINAL MESHING
	/*
	const int probDim=bemDimension(Elt,nElt,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType);
    const bool probAxi = isAxisym(Elt,nElt,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType);
    const bool probPeriodic = isPeriodic(Elt,nElt,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType);

    	
    // COLLOCATION POINTS: NODAL OR CENTROID
    int* const NodalColl=new(nothrow) int[nNod];
    if (NodalColl==0) throw("Out of memory.");
    int* const CentroidColl=new(nothrow) int[nElt];
    if (CentroidColl==0) throw("Out of memory.");
    int nCentroidColl;
    int nNodalColl;
    BemCollPoints(Elt,Nod,TypeID,nKeyOpt,TypeName,TypeKeyOpts,nEltType,
        nElt,maxEltColumn,nNod,NodalColl,CentroidColl,nNodalColl,nCentroidColl);

    // COLLOCATION POINT COORDINATES
    const int nTotalColl=nNodalColl+nCentroidColl;
    double* const CollPoints=new(nothrow) double[5*nTotalColl];
    if (CollPoints==0) throw("Out of memory.");
    BemCollCoords(Elt,Nod,TypeID,nKeyOpt,TypeName,TypeKeyOpts,nEltType,
                  CentroidColl,NodalColl,CollPoints,nTotalColl,nElt,nNod);
                 
   // mexPrintf("nTotalColl: %d\n",nTotalColl); // DEBUG
   // for(int i = 0; i < 5*nTotalColl; i++) {
                     // mexPrintf("CollPoints[%d]: %f\n",i,CollPoints[i]); // DEBUG
   // }
                  
    // CHECK FOR COINCIDENT NODES
    double* const CoincNodes=new(nothrow) double[2*nNod];
    if (CoincNodes==0) throw("Out of memory.");
    bool SlavesExist;
    BemCoincNodes(Nod,nNod,CoincNodes,SlavesExist);
	
	float time_mesh = (float) (clock() - start_mesh) / CLOCKS_PER_SEC; 
    mexPrintf("time for bemmat_mesh was %f seconds\n", time_mesh);	
	}
	
	*/
	
   	
	
    
	
	// if (Cache==true)
	// // // if (getCache==false)
	// // // {
		
		// mexPrintf("Cache test start... \n");
		// cleanup();
		// mexAtExit(cleanup);
		
		
		// /*
		
		// probDim=0;
		probDim=bemDimension(Elt,nElt,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType);
		// probAxi=0;
		probAxi = isAxisym(Elt,nElt,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType);
		// probPeriodic=0;
		probPeriodic = isPeriodic(Elt,nElt,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType);

    	// COLLOCATION POINTS: NODAL OR CENTROID
		delete [] NodalColl;
		NodalColl=new(nothrow) unsigned int[nNod];
		// mexPrintf("nNod: %d \n",nNod); // DEBUG
		if (NodalColl==0) throw("Out of memory.");
		
		delete [] CentroidColl;
		CentroidColl=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (CentroidColl==0) throw("Out of memory.");
		
		BemCollPoints(Elt,Nod,TypeID,nKeyOpt,TypeName,TypeKeyOpts,nEltType,
			nElt,maxEltColumn,nNod,NodalColl,CentroidColl,nNodalColl,nCentroidColl);
			
		// for(int i = 0; i < nNod; i++) {
                    // mexPrintf("NodalColl[%d]: %f\n",i,NodalColl[i]); // DEBUG
		// }
		
		// for(int i = 0; i < nElt; i++) {
                    // mexPrintf("CentroidColl[%d]: %f\n",i,CentroidColl[i]); // DEBUG
		// }

		// COLLOCATION POINT COORDINATES
		nTotalColl=nNodalColl+nCentroidColl;
		// mexPrintf("nTotalColl: %d\n",nTotalColl); // DEBUG
        
		delete [] CollPoints;
		CollPoints=new(nothrow) double[5*nTotalColl];
		// mexPrintf("5*nTotalColl: %d \n",5*nTotalColl); // DEBUG
		if (CollPoints==0) throw("Out of memory.");
		
		BemCollCoords(Elt,Nod,TypeID,nKeyOpt,TypeName,TypeKeyOpts,nEltType,
                  CentroidColl,NodalColl,CollPoints,nTotalColl,nElt,nNod);
                                  
		// CHECK FOR COINCIDENT NODES
		delete [] CoincNodes;
		CoincNodes=new(nothrow) double[2*nNod];
		// mexPrintf("2*nNod: %d \n",2*nNod); // DEBUG
		if (CoincNodes==0) throw("Out of memory.");
		
		BemCoincNodes(Nod,nNod,CoincNodes,SlavesExist);
		
//		for(unsigned int i = 0; i < 5*nTotalColl; i++) {
//                     mexPrintf("CollPoints[%d]: %f\n",i,CollPoints[i]); // DEBUG
//		}
		
		
		
		// */
		
		
		// mexPrintf("Cache test end... \n");
		// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
		// // mexAtExit(cleanup);
		// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
		
		
		delete [] EltParent;
		EltParent=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (EltParent==0) throw("Out of memory.");
		
		delete [] nEltNod;
		nEltNod=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nEltNod==0) throw("Out of memory.");
		
		delete [] nEltColl;
		nEltColl=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nEltColl==0) throw("Out of memory.");
		
		delete [] EltShapeN;
		EltShapeN=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (EltShapeN==0) throw("Out of memory.");
		
		delete [] EltShapeM;
		EltShapeM=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (EltShapeM==0) throw("Out of memory.");
		
		delete [] EltDim;
		EltDim=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (EltDim==0) throw("Out of memory.");
		
		delete [] AxiSym;
		AxiSym=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (AxiSym==0) throw("Out of memory.");
		
		delete [] Periodic;
		Periodic=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (Periodic==0) throw("Out of memory.");
		
		delete [] nGauss;
		nGauss=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nGauss==0) throw("Out of memory.");
		
		delete [] nEltDiv;
		nEltDiv=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nEltDiv==0) throw("Out of memory.");
		
		delete [] nGaussSing;
		nGaussSing=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nGaussSing==0) throw("Out of memory.");
		
		delete [] nEltDivSing;
		nEltDivSing=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nEltDivSing==0) throw("Out of memory.");
		
		
		
		
		for (unsigned int iElt=0; iElt<nElt; iElt++)  
		{
// 			unsigned int EltType = unsigned int(Elt[nElt+iElt]);
            unsigned int EltType = (unsigned int)(Elt[nElt+iElt]);
			unsigned int EltParent_loc;
			unsigned int nEltNod_loc;
			unsigned int nEltColl_loc;
			unsigned int EltShapeN_loc;
			unsigned int EltShapeM_loc;
			unsigned int EltDim_loc;
			unsigned int AxiSym_loc;
			unsigned int Periodic_loc;
			unsigned int nGauss_loc;
			unsigned int nEltDiv_loc;
			unsigned int nGaussSing_loc;
			unsigned int nEltDivSing_loc;
	
			eltdef(EltType,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,EltParent_loc,nEltNod_loc,
			nEltColl_loc,EltShapeN_loc,EltShapeM_loc,EltDim_loc,AxiSym_loc,Periodic_loc,nGauss_loc,nEltDiv_loc,
			nGaussSing_loc,nEltDivSing_loc);
			
			EltParent[iElt]=EltParent_loc;
			nEltNod[iElt]=nEltNod_loc;
			nEltColl[iElt]=nEltColl_loc;
			EltShapeN[iElt]=EltShapeN_loc;
			EltShapeM[iElt]=EltShapeM_loc;
			EltDim[iElt]=EltDim_loc;
			AxiSym[iElt]=AxiSym_loc;
			Periodic[iElt]=Periodic_loc;
			nGauss[iElt]=nGauss_loc;
			nEltDiv[iElt]=nEltDiv_loc;
			nGaussSing[iElt]=nGaussSing_loc;
			nEltDivSing[iElt]=nEltDivSing_loc;
		}	
		
		//
		delete [] ncumulEltCollIndex;
		ncumulEltCollIndex=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (ncumulEltCollIndex==0) throw("Out of memory.");
		ncumulEltCollIndex[0]=0;
		
		for (unsigned int iElt=1; iElt<nElt; iElt++)  // Enkel loop over nodige elementen
		{
			ncumulEltCollIndex[iElt]=ncumulEltCollIndex[iElt-1]+nEltColl[iElt];
		}
		
		NEltCollIndex=ncumulEltCollIndex[nElt-1]+nEltColl[nElt-1];
		
		
		//
		delete [] ncumulEltNod;
		ncumulEltNod=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (ncumulEltNod==0) throw("Out of memory.");
		ncumulEltNod[0]=0;
		
		for (unsigned int iElt=1; iElt<nElt; iElt++)  // Enkel loop over nodige elementen
		{
			ncumulEltNod[iElt]=ncumulEltNod[iElt-1]+nEltNod[iElt];
		}
		
		NEltNod=ncumulEltNod[nElt-1]+nEltNod[nElt-1];

		delete [] eltCollIndex;
		eltCollIndex=new(nothrow) unsigned int[NEltCollIndex];
		// mexPrintf("NEltCollIndex: %d \n",NEltCollIndex); // DEBUG
		if (eltCollIndex==0) throw("Out of memory.");
		
		// // // // // delete [] RegularColl;
		// // // // // RegularColl=new(nothrow) unsigned int[2*nTotalColl*nElt];
		// // // // // if (RegularColl==0) throw("Out of memory.");
		
		delete [] nRegularColl;
		nRegularColl=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nRegularColl==0) throw("Out of memory.");
		
		delete [] nSingularColl;
		nSingularColl=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (nSingularColl==0) throw("Out of memory.");
		
		delete [] ncumulSingularColl;
		ncumulSingularColl=new(nothrow) unsigned int[nElt];
		// mexPrintf("nElt: %d \n",nElt); // DEBUG
		if (ncumulSingularColl==0) throw("Out of memory.");
		ncumulSingularColl[0]=0;
		
		delete [] EltNod;
		EltNod=new(nothrow) double[3*NEltNod];
		// mexPrintf("3*NEltNod: %d \n",3*NEltNod); // DEBUG
		if (EltNod==0) throw("Out of memory.");		
				
				
		for (unsigned int iElt=0; iElt<nElt; iElt++)  
		{
		//
		unsigned int* const eltCollIndex_loc=new(nothrow) unsigned int[nEltColl[iElt]];
		// mexPrintf("nEltColl[iElt]: %d \n",nEltColl[iElt]); // DEBUG
		if (eltCollIndex_loc==0) throw("Out of memory.");
		
		BemEltCollIndex(Elt,iElt,nElt,CollPoints,nCentroidColl,nTotalColl,
                    nEltColl[iElt],nEltNod[iElt],eltCollIndex_loc);
					
		for(unsigned int iEltCollIndex=0; iEltCollIndex<nEltColl[iElt]; iEltCollIndex++)
		{
			eltCollIndex[ncumulEltCollIndex[iElt]+iEltCollIndex] = eltCollIndex_loc[iEltCollIndex];
					
		}
		
		//
		unsigned int* const RegularColl_loc=new(nothrow) unsigned int[2*nTotalColl];
		// // mexPrintf("2*nTotalColl: %d \n",2*nTotalColl); // DEBUG
		if (RegularColl_loc==0) throw("Out of memory.");
		unsigned int nRegularColl_loc;
		unsigned int nSingularColl_loc;
  
		BemRegularColl(Elt,iElt,nElt,Nod,nNod,CoincNodes,SlavesExist,
                   CollPoints,nCentroidColl,nTotalColl,RegularColl_loc,
                   nRegularColl_loc,nSingularColl_loc,TypeID,nKeyOpt,TypeName,
                   TypeKeyOpts,nEltType);
				   
			// // // // // for(unsigned int i=0; i<2*nTotalColl; i++)
			// // // // // {
				   // // // // // RegularColl[(uint64)(2*nTotalColl*iElt+i)]=RegularColl_loc[i];
			// // // // // }
			
		nRegularColl[iElt]=nRegularColl_loc;
		nSingularColl[iElt]=nSingularColl_loc;
		
		//
		double* const EltNod_loc=new(nothrow) double[3*nEltNod[iElt]];
		// // mexPrintf("3*nEltNod[iElt]: %d \n",3*nEltNod[iElt]); // DEBUG
		if (EltNod_loc==0) throw("Out of memory.");
		
		int NodIndex;
		unsigned int NodID;
  
		// DETERMINE COORDINATES OF ELEMENT NODES (OF ELEMENT IELT)
		for (unsigned int iEltNod=0; iEltNod<nEltNod[iElt]; iEltNod++)
		{
// 			NodID=unsigned int(Elt[(2+iEltNod)*nElt+iElt]);
            NodID=(unsigned int)(Elt[(2+iEltNod)*nElt+iElt]);
			BemNodeIndex(Nod,nNod,NodID,NodIndex);
			EltNod_loc[0*nEltNod[iElt]+iEltNod]=Nod[1*nNod+NodIndex];
			EltNod_loc[1*nEltNod[iElt]+iEltNod]=Nod[2*nNod+NodIndex];
			EltNod_loc[2*nEltNod[iElt]+iEltNod]=Nod[3*nNod+NodIndex];
		}
		
		
		for(unsigned int i=0; i<3*nEltNod[iElt]; i++)
		{
			EltNod[3*ncumulEltNod[iElt]+i] = EltNod_loc[i];
					
		}
		
		delete [] eltCollIndex_loc;
		delete [] RegularColl_loc;
		delete [] EltNod_loc;
		
		}
		
		for (unsigned int iElt=1; iElt<nElt; iElt++)  // Enkel loop over nodige elementen
		{
			ncumulSingularColl[iElt]=ncumulSingularColl[iElt-1]+nSingularColl[iElt];
		}
			
		// unsigned int NSingularColl=0;
		NSingularColl=ncumulSingularColl[nElt-1]+nSingularColl[nElt-1];
		// mexPrintf("NSingularColl: %u \n",NSingularColl);			
		
		
		delete [] RegularColl;
		RegularColl=new(nothrow) unsigned int[2*NSingularColl];
		// mexPrintf("2*NSingularColl: %d \n",2*NSingularColl); // DEBUG
		if (RegularColl==0) throw("Out of memory.");
		
				
		for (unsigned int iElt=0; iElt<nElt; iElt++)
		{
			unsigned int* const RegularColl_loc=new(nothrow) unsigned int[2*nTotalColl];
			// mexPrintf("2*nTotalColl: %d \n",2*nTotalColl); // DEBUG
			if (RegularColl_loc==0) throw("Out of memory.");
			unsigned int nRegularColl_loc;
			unsigned int nSingularColl_loc;
  
			BemRegularColl(Elt,iElt,nElt,Nod,nNod,CoincNodes,SlavesExist,
						   CollPoints,nCentroidColl,nTotalColl,RegularColl_loc,
						   nRegularColl_loc,nSingularColl_loc,TypeID,nKeyOpt,TypeName,
						   TypeKeyOpts,nEltType);
			
			unsigned int iColl=0;
			unsigned int iSingular=0;
			
			while (iColl<nTotalColl && iSingular<nSingularColl_loc)
			{
				if(RegularColl_loc[iColl]==0)
				{
				RegularColl[(uint64)(ncumulSingularColl[iElt]+iSingular)]=iColl;
				RegularColl[(uint64)(NSingularColl+ncumulSingularColl[iElt]+iSingular)]=RegularColl_loc[nTotalColl+iColl];
				iSingular++;
				}
				iColl++;
			}
			
			delete [] RegularColl_loc;
			// // // mexPrintf("iElt: %u \n",iElt);			
		}	
		
		// for (int i=0; i<2*NSingularColl; i++)
		// {
				// mexPrintf("RegularColl[i]: %u \n",RegularColl[i]);			
		// }
		
		//
		delete [] nXi;
		nXi=new(nothrow) unsigned int[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (nXi==0) throw("Out of memory.");
		
		bool* const GetTypeID=new(nothrow) bool[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (GetTypeID==0) throw("Out of memory.");		
		
		RefEltType=new(nothrow) unsigned int[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (RefEltType==0) throw("Out of memory.");
		
		for (unsigned int i=0; i<nEltType; i++)
		{
			GetTypeID[i]=false; // INITIALIZATION
		}
		
		bool GetAllTypeID =false;
		unsigned int iElt=0;
		while (iElt<nElt && GetAllTypeID==false)
		{
			bool GetEltTypeID=false;
			unsigned int iType=0;
			
			while (iType<nEltType && GetEltTypeID==false )
			{	
				
				if ((unsigned int)(Elt[nElt+iElt]) == TypeID[iType])
				{
					GetEltTypeID=true;
					if (GetTypeID[iType]==false)
					{
						RefEltType[iType]=iElt;
						GetTypeID[iType]=true;
						if (EltParent[iElt] == 1) nXi[iType]=nGauss[iElt];
						else if (EltParent[iElt] == 2) nXi[iType]=nEltDiv[iElt]*nEltDiv[iElt]*nGauss[iElt]*nGauss[iElt];
					}
					
					
				}
				iType++;
			}
		
			unsigned int auxvar=0;
			for (unsigned int i=0; i<nEltType; i++)
			{
				if (GetTypeID[i]==true)
				{
				auxvar++;
				}
			}
		if (auxvar==nEltType)
		{
			GetAllTypeID=true;
		}
		
		iElt++;
		}
		
		delete [] GetTypeID;
		
		//
		delete [] ncumulnXi;
		ncumulnXi=new(nothrow) unsigned int[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (ncumulnXi==0) throw("Out of memory.");
		ncumulnXi[0]=0;
		
		for (unsigned int iType=1; iType<nEltType; iType++)  // Enkel loop over nodige elementen
		{
			ncumulnXi[iType]=ncumulnXi[iType-1]+nXi[iElt];
		}
		
		NnXi=ncumulnXi[nEltType-1]+nXi[nEltType-1];
		
		
		//
		delete [] ncumulNshape;
		ncumulNshape=new(nothrow) unsigned int[nEltType];
		// mexPrintf("nEltType: %d \n",nEltType); // DEBUG
		if (ncumulNshape==0) throw("Out of memory.");
		ncumulNshape[0]=0;
		
		for (unsigned int iType=1; iType<nEltType; iType++)  // Enkel loop over nodige elementen
		{
			ncumulNshape[iType]=ncumulNshape[iType-1]+nXi[iType]*nEltNod[RefEltType[iType]];
		}
		
		NNshape=ncumulNshape[nEltType-1]+nXi[nEltType-1]*nEltNod[RefEltType[nEltType-1]];
		
		
		
		//
		delete [] xi;
		xi=new(nothrow) double[2*NnXi];
		// mexPrintf("2*NnXi: %d \n",2*NnXi); // DEBUG
		if (xi==0) throw("Out of memory.");		
		
		delete [] H;
		H=new(nothrow) double[NnXi];
		// mexPrintf("NnXi: %d \n",NnXi); // DEBUG
		if (H==0) throw("Out of memory.");	

		delete [] Nshape;
		Nshape=new(nothrow) double[NNshape];
		// mexPrintf("NNshape: %d \n",NNshape); // DEBUG
		if (Nshape==0) throw("Out of memory.");		
		
		delete [] Mshape;
		Mshape=new(nothrow) double[NNshape];
		// mexPrintf("NNshape: %d \n",NNshape); // DEBUG
		if (Mshape==0) throw("Out of memory.");		
		
		delete [] dNshape;
		dNshape=new(nothrow) double[2*NNshape];
		// mexPrintf("2*NNshape: %d \n",2*NNshape); // DEBUG
		if (dNshape==0) throw("Out of memory.");		
		
		for (unsigned int iType=0; iType<nEltType; iType++)  
		{
		//
		double* const xi_loc=new(nothrow) double[2*nXi[iType]];
		// mexPrintf("2*nXi[iType]: %d \n",2*nXi[iType]); // DEBUG
		if (xi_loc==0) throw("Out of memory.");
		
		double* const H_loc=new(nothrow) double[nXi[iType]];
		// mexPrintf("nXi[iType]: %d \n",nXi[iType]); // DEBUG
		if (H_loc==0) throw("Out of memory.");
				
		
		if (EltParent[RefEltType[iType]] == 1) gausspwtri(nGauss[RefEltType[iType]],xi_loc,H_loc);
		else gausspw2D(nEltDiv[RefEltType[iType]],nGauss[RefEltType[iType]],xi_loc,H_loc);
		
					
		for(unsigned int iXi=0; iXi<2*nXi[iType]; iXi++)
		{
			xi[2*ncumulnXi[iType]+iXi] = xi_loc[iXi];
		}
		for(unsigned int iH=0; iH<nXi[iType]; iH++)
		{
			H[ncumulnXi[iType]+iH] = H_loc[iH];
		}
		
		
		
		double* const N_loc=new(nothrow) double[nXi[iType]*nEltNod[RefEltType[iType]]];
		// mexPrintf("nXi[iType]*nEltNod[RefEltType[iType]]: %d \n",nXi[iType]*nEltNod[RefEltType[iType]]); // DEBUG
		if (N_loc==0) throw("Out of memory.");
		
		double* const M_loc=new(nothrow) double[nXi[iType]*nEltNod[RefEltType[iType]]];
		// mexPrintf("nXi[iType]*nEltNod[RefEltType[iType]]: %d \n",nXi[iType]*nEltNod[RefEltType[iType]]); // DEBUG
		if (M_loc==0) throw("Out of memory.");
		
		double* const dN_loc=new(nothrow) double[2*nXi[iType]*nEltNod[RefEltType[iType]]];
		// mexPrintf("2*nXi[iType]*nEltNod[RefEltType[iType]]: %d \n",2*nXi[iType]*nEltNod[RefEltType[iType]]); // DEBUG
		if (dN_loc==0) throw("Out of memory.");
		
	    shapefun(EltShapeN[RefEltType[iType]],nXi[iType],xi_loc,N_loc);
		shapefun(EltShapeM[RefEltType[iType]],nXi[iType],xi_loc,M_loc);
  
		shapederiv(EltShapeN[RefEltType[iType]],nXi[iType],xi_loc,dN_loc);
		
		
		for(unsigned int iN=0; iN<nXi[iType]*nEltNod[RefEltType[iType]]; iN++)
		{
			Nshape[ncumulNshape[iType]+iN] = N_loc[iN];
			Mshape[ncumulNshape[iType]+iN] = M_loc[iN];
		}
		for(unsigned int idN=0; idN<2*nXi[iType]*nEltNod[RefEltType[iType]]; idN++)
		{
			dNshape[2*ncumulNshape[iType]+idN] = dN_loc[idN];
		}
		
		delete [] xi_loc;
		delete [] H_loc;
		delete [] N_loc;
		delete [] M_loc;
		delete [] dN_loc;
		
		}
		
		
		// for(int i=0; i<nEltType; i++) {
                     // mexPrintf("RefEltType[%d]: %d\n",i,RefEltType[i]); // DEBUG
		// }			
		
		// delete [] RefEltType;

	}
	
	
	// for(int i=0; i<nEltType; i++) {
                     // mexPrintf("ncumulN[%d]: %d\n",i,ncumulN[i]); // DEBUG
	// }
	
	// mexPrintf("NN: %d\n",NN); // DEBUG
	
	
	// for(int i=0; i<NN; i++) {
                     // mexPrintf("N[%d]: %f \n",i,N[i]); // DEBUG
	// }
	
	// for(int i=0; i<nEltType; i++) {
                     // mexPrintf("nXi[%d]: %d\n",i,nXi[i]); // DEBUG
	// }
	
	// for(int i=0; i<nEltType; i++) {
                     // mexPrintf("ncumulnXi[%d]: %d\n",i,ncumulnXi[i]); // DEBUG
	// }
	
	// mexPrintf("NnXi: %d\n",NnXi); // DEBUG
	
	// for(int iElt=0; iElt<nElt; iElt++) {
                     // mexPrintf("ncumulEltNod[%d]: %d\n",iElt,ncumulEltNod[iElt]); // DEBUG
	// }
	
	// mexPrintf("NEltNod: %d\n",NEltNod); // DEBUG
	
	
	// for(int i=0; i<3*NEltNod; i++) {
                     // mexPrintf("EltNod[%d]: %f\n",i,EltNod[i]); // DEBUG
	// }
	
	/*
	for(int iElt=0; iElt<nElt; iElt++) {
                     mexPrintf("EltParent[%d]: %d\n",iElt,EltParent[iElt]); // DEBUG
   }
   
   for(int iElt=0; iElt<nElt; iElt++) {
                     mexPrintf("ncumulEltCollIndex[%d]: %d\n",iElt,ncumulEltCollIndex[iElt]); // DEBUG
   }
   
   mexPrintf("nElt: %d\n",nElt); // DEBUG
   mexPrintf("NEltCollIndex: %d\n",NEltCollIndex); // DEBUG
   
   
   for(int i=0; i<NEltCollIndex; i++) {
                     mexPrintf("eltCollIndex[%d]: %d\n",i,eltCollIndex[i]); // DEBUG
   }
   
     for(int i=0; i<2*nTotalColl*nElt; i++) {
                     mexPrintf("RegularColl [%i]: %i\n",i,RegularColl[i]); // DEBUG
   }
	
	*/
	
	// for(int i = 0; i < nNod; i++) {
                     // mexPrintf("NodalColl[%d]: %d\n",i,NodalColl[i]); // DEBUG
   // }
	
	
	// for(int i = 0; i < nElt; i++) {
                    // mexPrintf("CentroidColl[%d]: %d\n",i,CentroidColl[i]); // DEBUG
	// }
	
   // for(int i = 0; i < 5*nTotalColl; i++) {
                     // mexPrintf("CollPoints[%d]: %f\n",i,CollPoints[i]); // DEBUG
   // }
	
	
	// for(int i = 0; i < 2*nNod; i++) {
                     // mexPrintf("CoincNodes[%d]: %d\n",i,CoincNodes[i]); // DEBUG
   // }
	
	
	// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
	
	if (getCache==true)
	{
		// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG	
		if (CacheValid==false) throw("Mesh attributes are not cached!");
		// Check if Nod, Elt, Typ are not empty
		// if (Nod==0) throw("Nod not in cache!");
		
	}
	
	
	// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
    // INTEGRATE GREEN'S FUNCTION
    if (Cache==false && getCache==false)
	{
	
	// mexPrintf("Get s... \n");
	// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
	// bool UmatOut=true;
	
    // int ns=0;
    // int ms=0;
    // double* s=0;
    // int greenPos=3;    
    if (!mxIsChar(prhs[3])){// s passed  (evt mxGetM, mxGetN)
	   greenPos=4;
       if (nrhs<5) throw("Not enough input arguments.");
       if (!mxIsNumeric(prhs[3])) throw("Input argument 's' must be numeric.");
       if (mxIsSparse(prhs[3])) throw("Input argument 's' must not be sparse.");
       if (mxIsComplex(prhs[3])) throw("Input argument 's' must be real.");
       ms=mxGetM(prhs[3]);
       ns=mxGetN(prhs[3]);
       s=mxGetPr(prhs[3]);
		
	   if (!mxIsChar(prhs[4])){// su and st, check if su empty
	   UmatOut=false;
	   greenPos=5;
	   if (nrhs<6) throw("Not enough input arguments.");
		
	   if (!mxIsEmpty(prhs[3])) throw("Currently not supported...");
	   if (!mxIsNumeric(prhs[4])) throw("Input argument 's' must be numeric.");
       if (mxIsSparse(prhs[4])) throw("Input argument 's' must not be sparse.");
       if (mxIsComplex(prhs[4])) throw("Input argument 's' must be real.");
       ms=mxGetM(prhs[4]);
       ns=mxGetN(prhs[4]);
       s=mxGetPr(prhs[4]);
	   }
    }
	
	}
	
	
	
	
	
	// mexPrintf("UmatOut: %s \n", UmatOut ? "true": "false");
	// mexPrintf("TmatOut: %s \n", TmatOut ? "true": "false");
	
	if (Cache==false)
	{
	
		// mexPrintf("Integrate... \n");
		// /*
	
		// mexPrintf("Nod_pointer: %d\n",Nod); // DEBUG
		
       // for(int i = 0; i < 4*nNod; i++) {
                     // mexPrintf("Nod[%d]: %f\n",i,Nod[i]); // DEBUG
		// }
	   
	   
	 // for(int i = 0; i < 10*nElt; i++) {
                     // mexPrintf("Elt[%d]: %f\n",i,Elt[i]); // DEBUG
		// }  
	   
	   // mexPrintf("nNod: %d\n",nNod); // DEBUG
	   // mexPrintf("nElt: %d\n",nElt); // DEBUG
	   // mexPrintf("nEltType: %d \n",nEltType);
	   
        // mexPrintf("probAxi: %d\n",probAxi); // DEBUG
		// mexPrintf("probPeriodic: %d\n",probPeriodic); // DEBUG
		// mexPrintf("probDim: %d\n",probDim); // DEBUG
		
	   
		
    if (!mxIsChar(prhs[greenPos])) throw("Input argument 'green' must be a string.");
    const char* const green = mxArrayToString(prhs[greenPos]);

    if (strcasecmp(green,"user")==0)
    {
      IntegrateGreenUser(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,TypeID,
                         TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,nTotalColl,
                         UmatOut,TmatOut,greenPos,s,ms,ns,
						 EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						 ncumulEltCollIndex,eltCollIndex,
						 ncumulSingularColl,nSingularColl,NSingularColl, 
						 RegularColl,
						 ncumulEltNod,EltNod,
						 RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else if (strcasecmp(green,"fsgreenf")==0)
    {
      IntegrateFsGreenf(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                        TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                        nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else if (strcasecmp(green,"fsgreen2d_inplane")==0)
    {
      IntegrateFsGreen2d_inplane(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                                 TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                                 nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else if (strcasecmp(green,"fsgreen2d_inplane0")==0)
    {
      IntegrateFsGreen2d_inplane0(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                                  TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                                  nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else if (strcasecmp(green,"fsgreen2d_outofplane")==0)
    {
      IntegrateFsGreen2d_outofplane(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                                    TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                                    nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else if (strcasecmp(green,"fsgreen2d_outofplane0")==0)
    {
      IntegrateFsGreen2d_outofplane0(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                                     TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                                     nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else if (strcasecmp(green,"fsgreen3d")==0)
    {
      IntegrateFsGreen3d(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                         TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                         nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else if (strcasecmp(green,"fsgreen3d0")==0)
    {
      // IntegrateFsGreen3d0(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                          // TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                          // nTotalColl,nCentroidColl,CoincNodes,SlavesExist,UmatOut,TmatOut,greenPos,s,ms,ns);
		IntegrateFsGreen3d0(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                          TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                          nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);		  
						  
						  
						  
    }
    else if (strcasecmp(green,"fsgreen3dt")==0)
    {
      IntegrateFsGreen3dt(plhs,nrhs,prhs,probAxi,probPeriodic,probDim,Nod,nNod,Elt,nElt,
                          TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,CollPoints,
                          nTotalColl,UmatOut,TmatOut,greenPos,s,ms,ns,
						  EltParent,nEltNod,nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,nEltDivSing,
						  ncumulEltCollIndex,eltCollIndex,
						  ncumulSingularColl,nSingularColl,NSingularColl, 
						  RegularColl,
						  ncumulEltNod,EltNod,
						  RefEltType,ncumulnXi,nXi,xi,H,ncumulNshape,Nshape,Mshape,dNshape);
    }
    else
    {
      throw("Unknown fundamental solution type for input argument 'green'.");
    }
	
	// */
	}

	
	if (Cache==false && getCache==false)
	{
	
		// mexPrintf("Nod_ptr: %d \n",Nod);
		// mexPrintf("Elt_ptr: %d \n",Elt);
		// mexPrintf("TypeID_ptr: %d \n",TypeID);
		// mexPrintf("nKeyOpt_ptr: %d \n",nKeyOpt);
		// mexPrintf("TypeName_ptr: %d \n",TypeName);
		// mexPrintf("TypeKeyOpts_ptr: %d \n",TypeKeyOpts);
		// mexPrintf("NodalColl_ptr: %d \n",NodalColl);
		// mexPrintf("CentroidColl_ptr: %d \n",CentroidColl);
		// mexPrintf("CollPoints_ptr: %d \n",CollPoints);
		// mexPrintf("CoincNodes_ptr: %d \n",CoincNodes);
		
		
		// for(int i = 0; i < 4*nNod; i++) {
                     // mexPrintf("Nod[%d]: %f\n",i,Nod[i]); // DEBUG
		// }
		
		// // assert(delete [] TypeID);
		// // delete [] Nod;
		
		// for(int i = 0; i < 4*nNod; i++) {
                     // mexPrintf("Nod[%d]: %f\n",i,Nod[i]); // DEBUG
		// }
		
		// delete [] Elt;
		
		// // /*
		// for (int iTyp=0; iTyp<nEltType; iTyp++)
		// {
		// mxFree(TypeName[iTyp]);
		// for (int iKeyOpt=0; iKeyOpt<nKeyOpt[iTyp]; iKeyOpt++) mxFree(TypeKeyOpts[iTyp+nEltType*iKeyOpt]);
		// }
		
		
		// cleanup();
		// delete [] Nodtest;
		// delete [] Elttest;
		
		// delete [] Nod;
		// delete [] Elt;
		
		
		// delete [] TypeID;
		// delete [] nKeyOpt;
		// delete [] TypeName;
		// delete [] TypeKeyOpts;
		// delete [] NodalColl;
		// delete [] CentroidColl;
		// delete [] CollPoints;
		// delete [] CoincNodes;
		
		// */
		
		
		// delete Nod;
		// delete Elt;
		
		
		// delete TypeID;
		// delete nKeyOpt;
		// delete TypeName;
		// delete TypeKeyOpts;
		// delete NodalColl;
		// delete CentroidColl;
		// delete CollPoints;
		// delete CoincNodes;
		
	}
	
	  // mexPrintf("CacheValid: %s \n", CacheValid ? "true": "false");
	/*
    // DEALLOCATE MEMORY ALLOCATED BY "mxArrayToString" IN TYPE DEFINITIONS
    for (int iTyp=0; iTyp<nEltType; iTyp++)
    {
      mxFree(TypeName[iTyp]);
      for (int iKeyOpt=0; iKeyOpt<nKeyOpt[iTyp]; iKeyOpt++) mxFree(TypeKeyOpts[iTyp+nEltType*iKeyOpt]);
    }
	
    delete [] TypeID;
    delete [] nKeyOpt;
    delete [] TypeName;
    delete [] TypeKeyOpts;
    delete [] NodalColl;
    delete [] CentroidColl;
    delete [] CollPoints;
    delete [] CoincNodes;
	*/
	
	
	
  }
  catch (const char* exception)
  {
    mexErrMsgTxt(exception);
  }
  
  

  
  // float time_bemmat_mex = (float) (clock() - start_bemmat_mex) / CLOCKS_PER_SEC; 
  // double time_bemmat_mex =  (clock() - start_bemmat_mex) / (double) CLOCKS_PER_SEC; 
  // mexPrintf("time for bemmat_mex was %f seconds\n", time_bemmat_mex);	
	// mexPrintf("Nod_pointer: %d \n",Nod); // DEBUG
	
	
}
