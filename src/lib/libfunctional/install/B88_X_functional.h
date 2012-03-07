#ifndef B88_X_functional_h
#define B88_X_functional_h
/**********************************************************
* B88_X_functional.h: declarations for B88_X_functional for KS-DFT
* Robert Parrish, robparrish@gmail.com
* Autogenerated by MATLAB Script on 06-Mar-2012
*
***********************************************************/
#include "functional.h"

namespace psi { namespace functional {

class B88_X_Functional : public Functional {
public:
    B88_X_Functional(int npoints, int deriv);
    virtual ~B88_X_Functional();
    virtual void computeRKSFunctional(boost::shared_ptr<RKSFunctions> prop);
    virtual void computeUKSFunctional(boost::shared_ptr<RKSFunctions> prop);
};
}}
#endif

