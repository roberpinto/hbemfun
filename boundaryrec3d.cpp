#include "eltdef.h"
#include "bemcollpoints.h"
#include "shapefun.h"
#include "bemdimension.h"
#include <valarray>
#include "fminstep.h"
#include <limits>
#include <complex>

#include "mex.h"

using namespace std;

inline double sqr(const double& a)
{
  return a*a;
}

//==============================================================================
double recDist3d(valarray<double>& xi,const void** const varargin)
/*  Returns the distance
 *  between a receiver point and a point with natural coordinates xi 
 *  on an element defined by its nodes and geometry shape function.
 */
{
  const unsigned int nEltNod= *((const unsigned int*)varargin[0]);
  const unsigned int EltShapeN=*((const unsigned int*)varargin[1]);
  double* const EltNod=(double* const)varargin[2];
  const double* const Rec=(const double* const)varargin[3];
  const unsigned int nRec=*((const unsigned int*)varargin[4]); 
  const unsigned int iRec=*((const unsigned int*)varargin[5]); 
  
  double* const Xi=new(nothrow) double[2];
    if (Xi==0) throw("Out of memory.");
  double* const N=new(nothrow) double[1*nEltNod];
    if (N==0) throw("Out of memory.");
  Xi[0]=(double)xi[0];
  Xi[1]=(double)xi[1];
  
  shapefun(EltShapeN,1,Xi,N);
  
  delete [] Xi;
  
  double xi_x=0.0;
  double xi_y=0.0;
  double xi_z=0.0;
  for (unsigned int iEltNod=0; iEltNod<nEltNod; iEltNod++)
  {
    xi_x+=N[iEltNod]*EltNod[0*nEltNod+iEltNod];
    xi_y+=N[iEltNod]*EltNod[1*nEltNod+iEltNod];
    xi_z+=N[iEltNod]*EltNod[2*nEltNod+iEltNod];
  }
  
  double dist=sqrt(sqr(xi_x-Rec[0*nRec+iRec])+sqr(xi_y-Rec[1*nRec+iRec])
                                                   +sqr(xi_z-Rec[2*nRec+iRec]));
  
  delete [] N;
  
  return dist;
}
//==============================================================================
void boundaryRec3d(const double* const Nod, const unsigned int& nNod,
                   const double* const Elt, const unsigned int& nElt, const unsigned int& iElt,
                   const unsigned int* const TypeID,
                   const char* const TypeName[], const char* const TypeKeyOpts[],
                   const unsigned int* const nKeyOpt,
                   const unsigned int& nEltType, 
                   const double* const CollPoints,
                   const unsigned int& nTotalColl, const unsigned int& nCentroidColl,
                   const double* const Rec, 
                   const unsigned int& nRec, const unsigned int& nRecDof,
                   bool* const boundaryRec,
                   double* const TRe,const bool TmatOut,
                   const unsigned int& nDof,
                   const unsigned int& nGrSet)
/*
 *  Look up interface receivers.
 *
 */
{
  double eltxMin= std::numeric_limits<double>::infinity();
  double eltyMin= std::numeric_limits<double>::infinity();
  double eltzMin= std::numeric_limits<double>::infinity();
  double eltxMax=-std::numeric_limits<double>::infinity();
  double eltyMax=-std::numeric_limits<double>::infinity();
  double eltzMax=-std::numeric_limits<double>::infinity();

  unsigned int EltType = (unsigned int)(Elt[nElt+iElt]);
  unsigned int EltParent;
  unsigned int nEltNod;
  unsigned int nEltColl;
  unsigned int EltShapeN;
  unsigned int EltShapeM;
  unsigned int EltDim;
  unsigned int AxiSym;
  unsigned int Periodic;
  unsigned int nGauss;
  unsigned int nEltDiv;
  unsigned int nGaussSing;
  unsigned int nEltDivSing;
  eltdef(EltType,TypeID,TypeName,TypeKeyOpts,nKeyOpt,nEltType,EltParent,nEltNod,
         nEltColl,EltShapeN,EltShapeM,EltDim,AxiSym,Periodic,nGauss,nEltDiv,nGaussSing,
         nEltDivSing);

  unsigned int* const EltCollIndex=new(nothrow) unsigned int[nEltColl];
  if (EltCollIndex==0) throw("Out of memory.");
  BemEltCollIndex(Elt,iElt,nElt,CollPoints,nCentroidColl,nTotalColl,
                  nEltColl,nEltNod,EltCollIndex);

  // DETERMINE COORDINATES OF ELEMENT NODES (OF ELEMENT IELT)
  double* const EltNod =new(nothrow) double[3*nEltNod];
    if (EltNod==0) throw("Out of memory.");
  for (unsigned int iEltNod=0; iEltNod<nEltNod; iEltNod++)
  {
    unsigned int NodID=(unsigned int)(Elt[(2+iEltNod)*nElt+iElt]);
    int NodIndex;
    BemNodeIndex(Nod,nNod,NodID,NodIndex);
    EltNod[0*nEltNod+iEltNod]=Nod[1*nNod+NodIndex];
    EltNod[1*nEltNod+iEltNod]=Nod[2*nNod+NodIndex];
    EltNod[2*nEltNod+iEltNod]=Nod[3*nNod+NodIndex];

    eltxMin=min(eltxMin,EltNod[0*nEltNod+iEltNod]);
    eltyMin=min(eltyMin,EltNod[1*nEltNod+iEltNod]);
    eltzMin=min(eltzMin,EltNod[2*nEltNod+iEltNod]);
    eltxMax=max(eltxMax,EltNod[0*nEltNod+iEltNod]);
    eltyMax=max(eltyMax,EltNod[1*nEltNod+iEltNod]);
    eltzMax=max(eltzMax,EltNod[2*nEltNod+iEltNod]);
  }
  double diag=sqrt(sqr(eltxMax-eltxMin)+sqr(eltyMax-eltyMin)
                                                         +sqr(eltyMax-eltyMin));
  eltxMin=eltxMin-0.25*diag;
  eltyMin=eltyMin-0.25*diag;
  eltzMin=eltzMin-0.25*diag;
  eltxMax=eltxMax+0.25*diag;
  eltyMax=eltyMax+0.25*diag;
  eltzMax=eltzMax+0.25*diag;

  for (unsigned int iRec=0; iRec<nRec; iRec++)
  {
    if (!(boundaryRec[iRec]))  // If receiver is not yet matched to an element
    {
      // Check if Receiver is near element iElt
      if ((Rec[0*nRec+iRec]>=eltxMin)&(Rec[0*nRec+iRec]<=eltxMax)
         &(Rec[1*nRec+iRec]>=eltyMin)&(Rec[1*nRec+iRec]<=eltyMax)
         &(Rec[2*nRec+iRec]>=eltzMin)&(Rec[2*nRec+iRec]<=eltzMax))
      {
        
        valarray<double> xiRec(0.0,2);
        const valarray<double> xiRes(0.1,2);
        const valarray<double> xiTol(1e-4,2);
        
        const void** const varargin=new(nothrow) const void*[6];
        varargin[0]=&nEltNod;
        varargin[1]=&EltShapeN;
        varargin[2]=EltNod;
        varargin[3]=Rec;
        varargin[4]=&nRec;
        varargin[5]=&iRec;
        
        // Minimize distance between receiver and element.
        fminstep(recDist3d,xiRec,xiRes,xiTol,30,varargin);
        
        if (EltParent==1) // Triangular element
        {
          if (xiRec[0]<0.0) xiRec[0]=0.0;
          if (xiRec[1]<0.0) xiRec[1]=0.0;
          if ((xiRec[0]+xiRec[1])>1.0) 
          {
            double xi0=xiRec[0];
            double eta0=xiRec[1];
            xiRec[0]=xiRec[0]/(xi0+eta0);
            xiRec[1]=xiRec[1]/(xi0+eta0);
          }
        }
        else              // Quadrilateral element
        {
          if (xiRec[0]> 1.0) xiRec[0]= 1.0;
          if (xiRec[0]<-1.0) xiRec[0]=-1.0;
          if (xiRec[1]> 1.0) xiRec[1]= 1.0;
          if (xiRec[1]<-1.0) xiRec[1]=-1.0;
        }
        double dist=recDist3d(xiRec,varargin);
        
        //mexPrintf("Elt: %i, Rec: %i, xiRec: %f %f\n",iElt,iRec,xiRec[0],xiRec[1]);
        //mexPrintf("Distance: %f accuracy: %f \n",dist,0.01*diag);
        
        if (dist<0.05*diag)
        {
          boundaryRec[iRec]=true;
          
          // Evaluate interpolation function
          double* const Xi=new(nothrow) double[2];
            if (Xi==0) throw("Out of memory.");
          double* const M=new(nothrow) double[nEltColl];
            if (M==0) throw("Out of memory.");
          Xi[0]=(double)xiRec[0];
          Xi[1]=(double)xiRec[1];
          shapefun(EltShapeM,1,Xi,M);
          
          if (TmatOut)
          {
            for (unsigned int iEltColl=0; iEltColl<nEltColl; iEltColl++)
            {
              for (unsigned int iGrSet=0; iGrSet<nGrSet; iGrSet++)
              {
                const unsigned int ind0 =nRecDof*nDof*iGrSet;
                unsigned int rowBeg=3*iRec;
                unsigned int colBeg=3*EltCollIndex[iEltColl];
                TRe[ind0+nRecDof*(colBeg+0)+rowBeg+0] = -M[iEltColl]; // txx
                TRe[ind0+nRecDof*(colBeg+1)+rowBeg+1] = -M[iEltColl]; // tyy
                TRe[ind0+nRecDof*(colBeg+2)+rowBeg+2] = -M[iEltColl]; // tzz
              }
            }
          }
          delete [] Xi;
          delete [] M;
        }
      }
    }
  }
  delete [] EltCollIndex;
  delete [] EltNod;
}
