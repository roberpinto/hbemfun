#ifndef _BEMXFER2D_
#define _BEMXFER2D_
void bemxfer2d(const double* const Nod,const unsigned int& nNod,
               const double* const Elt,const unsigned int& iElt,
               const unsigned int& nElt, const unsigned int* const EltCollIndex,
               const double* const Rec, const unsigned int nRec,
               bool* const boundaryRec,
               double* const URe, double* const UIm,
               double* const TRe, double* const TIm,
               const bool TmatOut,
               const unsigned int& nDof, const unsigned int& nRecDof,
               const unsigned int* const  TypeID,const unsigned int* const  nKeyOpt,
               const char* const* TypeName, const char* const* TypeKeyOpts,
               const unsigned int& nEltType,
               const void* const* const greenPtr, const unsigned int& nGrSet, 
               const unsigned int& nugComp, const bool& ugCmplx, const bool& tgCmplx);
#endif
