/* checklicense.cpp
 *
 * Mattias Schevenels
 * July 2009
 */

#include "checklicense.h"
#include "mex.h"
#include <string>
#include <string.h>
#include <ctime>
#include <cmath>
#include "isinf.h"
#include "ripemd128.h"

using namespace std;

/******************************************************************************/

#define BEMFUNLICENSE_HASH "B1FB853F618571AD2B2244E016BDB968"

/******************************************************************************/

static int licenseStatus = -1;
static time_t lastVerified = 0;

/******************************************************************************/

void checkintegrity(const string& function, const string& hash)
{
  mxArray* lhs[1];
  mxArray* rhs[2];

  rhs[0] = mxCreateString("which");
  rhs[1] = mxCreateString(function.c_str());

  mexCallMATLAB(1,lhs,2,rhs,"builtin");

  char* c = new(nothrow) char[mxGetNumberOfElements(lhs[0])+1];
  if (c==0) throw("Out of memory.");
  mxGetString(lhs[0],c,mxGetNumberOfElements(lhs[0])+1);

  string licfile(c);
  string lichash;
  rmdfile(licfile,lichash);
  if (lichash!=hash) throw("BEMFUN license error: the function BEMFUNLICENSE is invalid.");

  mxDestroyArray(rhs[0]);
  mxDestroyArray(rhs[1]);
  mxDestroyArray(lhs[0]);
  delete [] c;
}

/******************************************************************************/

void checklicense()
{
  // Determine current time
  const double now = time(0);

  // Reset license status to unknown every 2 hours
  if (now-lastVerified>=7200) licenseStatus = -1;

  // If the license status is unknown, determine it using BEMFUNLICENSE.
  if (licenseStatus==-1)
  {
    checkintegrity("bemfunlicense",BEMFUNLICENSE_HASH);

    mxArray* lhs[1];
    mxArray* rhs[1];

    rhs[0] = mxCreateString("VerifyOnce");

    mexCallMATLAB(1,lhs,1,rhs,"bemfunlicense");

    licenseStatus = (int)mxGetScalar(lhs[0]);
    lastVerified = (time_t)now;

    mxDestroyArray(rhs[0]);
    mxDestroyArray(lhs[0]);
  }
}
