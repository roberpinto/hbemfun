#ifndef _GREENEVAL2D_
#define _GREENEVAL2D_
void greeneval2d(const void* const* const greenPtr, const unsigned int& nGrSet,
                 const unsigned int& nugComp, const unsigned int& ntgComp,
                 const bool& ugCmplx, const bool& tgCmplx, const bool& tg0Cmplx,
                 const double& xiR, const double& xiZ, const double& Xsgn, unsigned int& r1, unsigned int& r2,
                 unsigned int& z1, unsigned int& z2, unsigned int& zs1, double* const interpr,
                 double* const interpz, bool& extrapFlag, const bool& TmatOut,
                 const double* const Coll, const unsigned int& nColl, const unsigned int& iColl,
                 const unsigned int& zPos, double* const UgrRe, double* const UgrIm, 
                 double* const TgrRe, double* const TgrIm, double* const Tgr0Re,
                 double* const Tgr0Im);
#endif
