/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2012 Ciengis
 *
 *  CppADCodeGen is distributed under multiple licenses:
 *
 *   - Eclipse Public License Version 1.0 (EPL1), and
 *   - GNU General Public License Version 3 (GPL3).
 *
 *  EPL1 terms and conditions can be found in the file "epl-v10.txt", while
 *  terms and conditions for the GPL3 can be found in the file "gpl3.txt".
 * ----------------------------------------------------------------------------
 * Author: Joao Leal
 */

#include <iosfwd>
#include <vector>
#include <cppad/cg/cppadcg.hpp>

using namespace CppAD;
using namespace CppAD::cg;

int main(void) {
    // use a special object for source code generation
    typedef CG<double> CGD;
    typedef AD<CGD> ADCG;

    // independent variable vector
    std::vector<ADCG> U(2);
    Independent(U);

    // dependent variable vector 
    std::vector<ADCG> Z(1);

    // the model
    ADCG a = U[0] / 1. + U[1] * U[1];
    Z[0] = a / 2;

    ADFun<CGD> fun(U, Z);

    /**
     * Create the dynamic library
     * (generates and compiles source code)
     */

    // generates source code
    ModelCSourceGen<double> compModelH(fun, "model");
    compModelH.setCreateJacobian(true);
    ModelLibraryCSourceGen<double> compDynH(compModelH);

    // compile source code
    DynamicModelLibraryProcessor<double> p(compDynH);

    GccCompiler<double> compiler;
    DynamicLib<double>* dynamicLib = p.createDynamicLibrary(compiler);

    /**
     * Use the dynamic library
     */
    GenericModel<double>* model = dynamicLib->model("model");
    std::vector<double> x(U.size());
    x[0] = 2.5;
    x[1] = 3.5;
    std::vector<double> jac = model->Jacobian(x);

    // print out the result
    std::cout << jac[0] << " " << jac[1] << std::endl;

    delete model;
    delete dynamicLib;
}
