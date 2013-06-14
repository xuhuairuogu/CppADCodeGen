/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2012 Ciengis
 *
 *  CppADCodeGen is distributed under multiple licenses:
 *
 *   - Common Public License Version 1.0 (CPL1), and
 *   - GNU General Public License Version 2 (GPL2).
 *
 * CPL1 terms and conditions can be found in the file "epl-v10.txt", while
 * terms and conditions for the GPL2 can be found in the file "gpl2.txt".
 * ----------------------------------------------------------------------------
 * Author: Joao Leal
 */
#include "CppADCGDynamicTest.hpp"

namespace CppAD {

    class CppADCGDynamicTest1 : public CppADCGDynamicTest {
    public:

        inline CppADCGDynamicTest1(bool verbose = false, bool printValues = false) :
            CppADCGDynamicTest("dynamic", verbose, printValues) {
        }

        virtual std::vector<ADCGD> model(const std::vector<ADCGD>& u) {
            std::vector<ADCGD> Z(2);

            Z[0] = cos(u[0]);
            Z[1] = u[1] * u[2] + sin(u[0]);

            return Z;
        }

    };

}
using namespace CppAD;
using namespace std;

TEST_F(CppADCGDynamicTest1, DynamicFull) {
    // use a special object for source code generation
    typedef CG<double> CGD;
    typedef AD<CGD> ADCG;

    // independent variables
    std::vector<ADCG> u(3);
    u[0] = 1;
    u[1] = 1;
    u[2] = 1;

    std::vector<double> x(u.size());
    x[0] = 1;
    x[1] = 2;
    x[2] = 1;

    this->testDynamicFull(u, x, 1);
}

TEST_F(CppADCGDynamicTest1, DynamicCustomElements) {
    // use a special object for source code generation
    typedef CG<double> CGD;
    typedef AD<CGD> ADCG;

    // independent variables
    std::vector<ADCG> u(3);
    u[0] = 1;
    u[1] = 1;
    u[2] = 1;

    std::vector<double> x(u.size());
    x[0] = 1;
    x[1] = 2;
    x[2] = 1;

    std::vector<size_t> jacRow(3), jacCol(3); // all elements except 1
    jacRow[0] = 0;
    jacCol[0] = 0;
    jacRow[1] = 1;
    jacCol[1] = 0;
    jacRow[2] = 1;
    jacCol[2] = 2;

    std::vector<size_t> hessRow(2), hessCol(2); // all elements except 1
    hessRow[0] = 0;
    hessCol[0] = 0;
    hessRow[1] = 2;
    hessCol[1] = 1;

    this->testDynamicCustomElements(u, x, jacRow, jacCol, hessRow, hessCol);
}