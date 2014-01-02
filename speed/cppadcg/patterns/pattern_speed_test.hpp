#ifndef CPPAD_CG_PATTERNSPEEDTEST_INCLUDED
#define	CPPAD_CG_PATTERNSPEEDTEST_INCLUDED
/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2013 Ciengis
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

#include <cppadcg/cg.hpp>
#include <cppadcg/model/llvm/v3_2/cg_llvm.hpp>
#include "job_speed_listener.hpp"

namespace CppAD {

    class PatternSpeedTest {
    public:
        typedef double Base;
        typedef CppAD::CG<Base> CGD;
        typedef CppAD::AD<CGD> ADCGD;
    private:

        template<class T>
        class Model {
        protected:
            PatternSpeedTest& self_;
        public:

            Model(PatternSpeedTest& s) : self_(s) {
            }

            virtual std::vector<AD<T> > evaluate(const std::vector<AD<T> >& x, size_t repeat) = 0;
        };

        class ModelCppAD : public Model<Base> {
        public:

            ModelCppAD(PatternSpeedTest& s) : Model(s) {
            }

            virtual std::vector<AD<Base> > evaluate(const std::vector<AD<Base> >& x, size_t repeat) {
                return self_.modelCppAD(x, repeat);
            }
        };

        class ModelCppADCG : public Model<CG<Base> > {
        public:

            ModelCppADCG(PatternSpeedTest& s) : Model(s) {
            }

            virtual std::vector<AD<CG<Base> > > evaluate(const std::vector<AD<CG<Base> > >& x, size_t repeat) {
                return self_.modelCppADCG(x, repeat);
            }
        };
    protected:
        std::string libName_;
        bool testZeroOrder_;
        bool testJacobian_;
        bool testHessian_;
        std::vector<Base> xNorm_;
        std::vector<Base> eqNorm_;
        std::vector<std::set<size_t> > customJacSparsity_;
        std::vector<std::set<size_t> > customHessSparsity_;
        std::auto_ptr<DynamicLib<Base> > dynamicLib_;
        std::auto_ptr<LlvmModelLibrary<Base> > llvmLib_;
        std::auto_ptr<GenericModel<Base> > model_;
        std::vector<atomic_base<Base>*> atoms_;
        std::vector<std::string> compileFlags_;
        std::auto_ptr<ModelCSourceGen<double> > modelSourceGen_;
        std::auto_ptr<ModelLibraryCSourceGen<double> > libSourceGen_;
        JobSpeedListener listener_;
        bool verbose_;
        size_t nTimes_;
    private:
        std::vector<double> patternDection_; /// pattern detection
        std::vector<double> graphGen_; //
        std::vector<double> srcCodeGen_; /// source code generation
        std::vector<double> srcCodeComp_; /// source code compilation
        std::vector<double> dynLibComp_; /// compilation of the dynamic library
        std::vector<double> jit_; /// compilation of the dynamic library
        std::vector<double> total_; /// total time
    public:

        inline PatternSpeedTest(const std::string& libName,
                                bool verbose = false) :
            libName_(libName),
            testZeroOrder_(true),
            testJacobian_(true),
            testHessian_(true),
            verbose_(verbose),
            nTimes_(50) {

        }

        inline void setNumberOfExecutions(size_t nTimes) {
            nTimes_ = nTimes;
        }

        inline void setCompileFlags(const std::vector<std::string>& compileFlags) {
            compileFlags_ = compileFlags;
        }

        virtual std::vector<ADCGD> modelCppADCG(const std::vector<ADCGD>& x, size_t repeat) = 0;

        virtual std::vector<AD<Base> > modelCppAD(const std::vector<AD<Base> >& x, size_t repeat) = 0;

        inline void setAtomics(const std::vector<atomic_base<Base>*>& atoms) {
            atoms_ = atoms;
        }

        inline void measureSpeed(size_t m,
                                 size_t n,
                                 size_t repeat) {
            size_t n2 = repeat * n;
            std::vector<Base> x(n2);
            for (size_t j = 0; j < n2; j++)
                x[j] = 0.5 * (j + 1);

            std::vector<std::set<size_t> > relatedDepCandidates = createRelatedDepCandidates(m, repeat);

            measureSpeed(relatedDepCandidates, repeat, x);
        }

        inline void measureSpeed(size_t m,
                                 size_t repeat,
                                 const std::vector<Base>& xb) {

            std::vector<std::set<size_t> > relatedDepCandidates = createRelatedDepCandidates(m, repeat);

            measureSpeed(relatedDepCandidates, repeat, xb);
        }

        inline void measureSpeed(const std::vector<std::set<size_t> >& relatedDepCandidates,
                                 size_t repeat,
                                 const std::vector<Base>& xb) {
            using namespace CppAD;

            std::cout << libName_ << "\n";
            std::cout << "n=" << repeat << "\n";

            /*******************************************************************
             * CppADCG (without Loops)
             ******************************************************************/
            measureSpeedCppADCG(repeat, xb);

            /*******************************************************************
             * CppADCG (Loops)
             ******************************************************************/
            measureSpeedCppADWithLoops(relatedDepCandidates, repeat, xb);

            measureSpeedCppADWithLoopsLlvm(relatedDepCandidates, repeat, xb);

            /*******************************************************************
             * CppAD
             ******************************************************************/
            measureSpeedCppAD(repeat, xb);
        }

        inline static size_t parseProgramArguments(int pos, int argc, char **argv, size_t defaultRepeat) {
            if (argc > pos) {
                std::istringstream is(argv[pos]);
                size_t repeat;
                is >> repeat;
                return repeat;
            }
            return defaultRepeat;
        }

    protected:

        inline void measureSpeedCppADCG(size_t repeat,
                                        const std::vector<Base>& xb) {
            using namespace CppAD;

            /*******************************************************************
             * CppADCG (without Loops)
             ******************************************************************/
            std::cout << "\n"
                    "********************************************************************************\n"
                    "CppADCG (without Loops) GCC\n"
                    "********************************************************************************\n" << std::endl;
            ModelCppADCG model(*this);

            std::auto_ptr<ADFun<CGD> > fun;

            /**
             * preparation
             */
            std::vector<double> dt(nTimes_);
            for (size_t i = 0; i < nTimes_; i++) {
                // tape
                double t0 = system::currentTime();
                fun.reset(tapeModel(model, xb, repeat));
                dt[i] = system::currentTime() - t0;

                // create dynamic lib
                createDynamicLib(*fun.get(), std::vector<std::set<size_t> >(), xb, i == nTimes_ - 1, REVERSE, testJacobian_, testHessian_);
            }
            printStat("model tape", dt);
            printCGResults();

            /**
             * execution
             */
            // evaluation speed
            executionSpeedCppADCG(xb);
        }

        inline void measureSpeedCppADWithLoops(const std::vector<std::set<size_t> >& relatedDepCandidates,
                                               size_t repeat,
                                               const std::vector<Base>& xb) {
            using namespace CppAD;

            std::cout << "\n"
                    "********************************************************************************\n"
                    "CppADCG (with Loops) GCC\n"
                    "********************************************************************************\n" << std::endl;
            ModelCppADCG model(*this);

            std::auto_ptr<ADFun<CGD> > fun;

            /**
             * preparation
             */
            std::vector<double> dt(nTimes_);
            for (size_t i = 0; i < nTimes_; i++) {
                // tape
                double t0 = system::currentTime();
                fun.reset(tapeModel(model, xb, repeat));
                dt[i] = system::currentTime() - t0;

                // create dynamic lib
                createDynamicLib(*fun.get(), relatedDepCandidates, xb, i == nTimes_ - 1, REVERSE, testJacobian_, testHessian_);
            }
            printStat("model tape", dt);

            printCGResults();


            /*if (testJacobian_) {
                testSourceCodeGen(fun, m, repeat, libName, xb, FORWARD, true, false, true);
                testSourceCodeGen(fun, m, repeat, libName, xb, REVERSE, true, false);
                testSourceCodeGen(fun, m, repeat, libName, xb, REVERSE, true, false, true);
            }

            if (testHessian_) {
                testSourceCodeGen(fun, m, repeat, mExtra, libName, xb, FORWARD, false, true, false, true);
            }*/

            /**
             * execution
             */
            // evaluation speed
            executionSpeedCppADCG(xb);
        }

        inline void measureSpeedCppADWithLoopsLlvm(const std::vector<std::set<size_t> >& relatedDepCandidates,
                                                   size_t repeat,
                                                   const std::vector<Base>& xb) {
            using namespace CppAD;

            std::cout << "\n"
                    "********************************************************************************\n"
                    "CppADCG (with Loops) LLVM\n"
                    "********************************************************************************\n" << std::endl;
            JacobianADMode jacMode = REVERSE;
            bool forReverseOne = false;
            bool reverseTwo = false;

            bool withLoops = !relatedDepCandidates.empty();
            std::string libBaseName = createLibBaseName(jacMode, forReverseOne, reverseTwo);

            /**
             * preparation
             */
            for (size_t i = 0; i < nTimes_; i++) {
                // create dynamic lib
                createJitModelLib(libBaseName, xb, withLoops);
            }
            printCGResults();

            /**
             * execution
             */
            // evaluation speed
            executionSpeedCppADCG(xb);
        }

        inline void printCGResults() {
            // save results
            if (!patternDection_.empty())
                printStat("             loop detection", patternDection_);
            if (!graphGen_.empty())
                printStat("         (graph generation)", graphGen_);
            if (!srcCodeGen_.empty())
                printStat("            code generation", srcCodeGen_);
            if (!srcCodeComp_.empty())
                printStat("           code compilation", srcCodeComp_);
            if (!dynLibComp_.empty())
                printStat("dynamic library compilation", dynLibComp_);
            if (!jit_.empty())
                printStat("    JIT library preparation", jit_);
            printStat("                      total", total_);

            patternDection_.clear();
            graphGen_.clear();
            srcCodeGen_.clear();
            srcCodeComp_.clear();
            dynLibComp_.clear();
            jit_.clear();
            total_.clear();
        }

        static void printStat(const std::string& title, const std::vector<double>& times) {
            std::cout << title << ": ";
            printStat(times);
            std::cout << std::endl;
        }

        static void printStat(const std::vector<double>& times) {
            assert(!times.empty());
            std::vector<double> sorted = times;
            std::sort(sorted.begin(), sorted.end());

            size_t middle1 = sorted.size() / 2;
            double q25, median, q75;
            if (sorted.size() % 2 == 1) {
                median = sorted[middle1];
                if (sorted.size() <= 3) {
                    q25 = sorted.front();
                    q75 = sorted.back();
                } else {
                    size_t s = middle1 / 2;
                    q25 = (sorted[s] + sorted[s - 1]) / 2;
                    q75 = (sorted[middle1 + s] + sorted[middle1 + s + 1]) / 2;
                }
            } else {
                median = (sorted[middle1] + sorted[middle1 - 1]) / 2;
                if (sorted.size() <= 2) {
                    q25 = sorted.front();
                    q75 = sorted.back();
                } else {
                    size_t s = (middle1 - 1) / 2;
                    q25 = (sorted[s] + sorted[s - 1]) / 2;
                    q75 = (sorted[middle1 + s] + sorted[middle1 + s + 1]) / 2;
                }
            }

            double min = sorted.front();
            double max = sorted.back();

            std::cout << std::setw(12) << mean(times) << " +- " << std::setw(12) << stdDev(times)
                    << "      "
                    << std::setw(12) << min << "|--["
                    << std::setw(12) << q25 << ", "
                    << std::setw(12) << median << ", "
                    << std::setw(12) << q75 << "]--|"
                    << std::setw(12) << max;
        }

        inline void measureSpeedCppAD(size_t repeat,
                                      const std::vector<Base>& xb) {
            std::cout << "\n"
                    "********************************************************************************\n"
                    "CppAD\n"
                    "********************************************************************************\n" << std::endl;
            ModelCppAD model(*this);

            std::auto_ptr<ADFun<Base> > fun;

            /**
             * preparation
             */
            std::vector<double> dt1(nTimes_), dt2(nTimes_);
            for (size_t i = 0; i < nTimes_; i++) {
                // tape
                double t0 = system::currentTime();
                fun.reset(tapeModel(model, xb, repeat));
                dt1[i] = system::currentTime() - t0;

                // optimize tape
                t0 = system::currentTime();
                fun->optimize();
                dt2[i] = system::currentTime() - t0;
            }
            printStat("model tape", dt1);
            printStat("optimize tape", dt2);

            /**
             * execution
             */
            // evaluation speed
            executionSpeedCppAD(*fun.get(), xb);
        }

        template<class T>
        ADFun<T>* tapeModel(Model<T>& model,
                            const std::vector<Base>& xb,
                            size_t repeat) {
            /**
             * Tape model
             */
            std::vector<AD<T> > x(xb.size());
            for (size_t j = 0; j < xb.size(); j++)
                x[j] = xb[j];
            CppAD::Independent(x);
            if (xNorm_.size() > 0) {
                assert(x.size() == xNorm_.size());
                for (size_t j = 0; j < x.size(); j++)
                    x[j] *= xNorm_[j];
            }

            std::vector<AD<T> > y = model.evaluate(x, repeat);
            if (eqNorm_.size() > 0) {
                assert(y.size() == eqNorm_.size());
                for (size_t i = 0; i < y.size(); i++)
                    y[i] /= eqNorm_[i];
            }

            std::auto_ptr<ADFun<T> > fun(new ADFun<T>());
            fun->Dependent(y);

            return fun.release();
        }

        inline std::vector<std::set<size_t> > createRelatedDepCandidates(size_t m,
                                                                         size_t repeat) {
            std::vector<std::set<size_t> > relatedDepCandidates(m);
            for (size_t i = 0; i < repeat; i++) {
                for (size_t ii = 0; ii < m; ii++) {
                    relatedDepCandidates[ii].insert(i * m + ii);
                }
            }
            return relatedDepCandidates;
        }

        inline std::string createLibBaseName(JacobianADMode jacMode,
                                             bool jacobian = true,
                                             bool hessian = true,
                                             bool forReverseOne = false,
                                             bool reverseTwo = false) const {
            std::string libBaseName = libName_;
            if (jacobian) {
                if (!forReverseOne) libBaseName += "d";
                if (jacMode == FORWARD) libBaseName += "F";
                else if (jacMode == REVERSE) libBaseName += "R";
            }
            if (hessian && reverseTwo)
                libBaseName += "rev2";
            return libBaseName;
        }

        inline void createSource(const std::string& libBaseName,
                                 ADFun<CGD>& fun,
                                 const std::vector<std::set<size_t> >& relatedDepCandidates,
                                 const std::vector<Base>& xTypical,
                                 JacobianADMode jacMode,
                                 bool jacobian = true,
                                 bool hessian = true,
                                 bool forReverseOne = false,
                                 bool reverseTwo = false) {
            bool withLoops = !relatedDepCandidates.empty();

            assert(fun.Domain() == xTypical.size());

            /**
             * Create the source code
             */
            modelSourceGen_.reset(new ModelCSourceGen<double>(fun, libBaseName + (withLoops ? "Loops" : "NoLoops")));
            modelSourceGen_->setCreateForwardZero(true);
            modelSourceGen_->setJacobianADMode(jacMode);
            modelSourceGen_->setCreateSparseJacobian(jacobian);
            modelSourceGen_->setCreateSparseHessian(hessian);
            modelSourceGen_->setCreateForwardOne(forReverseOne && jacMode == FORWARD);
            modelSourceGen_->setCreateReverseOne(forReverseOne && jacMode == REVERSE);
            modelSourceGen_->setCreateReverseTwo(reverseTwo);
            modelSourceGen_->setRelatedDependents(relatedDepCandidates);
            modelSourceGen_->setTypicalIndependentValues(xTypical);

            if (!customJacSparsity_.empty())
                modelSourceGen_->setCustomSparseJacobianElements(customJacSparsity_);

            if (!customHessSparsity_.empty())
                modelSourceGen_->setCustomSparseHessianElements(customHessSparsity_);

            libSourceGen_.reset(new ModelLibraryCSourceGen<double>(*modelSourceGen_.get()));
            libSourceGen_->setVerbose(this->verbose_);
            libSourceGen_->addListener(listener_);

            SaveFilesModelLibraryProcessor<double>::saveLibrarySourcesTo(*libSourceGen_.get(), "sources_" + libBaseName);

            if (!relatedDepCandidates.empty())
                patternDection_.push_back(listener_.patternDection);
            graphGen_.push_back(listener_.graphGen);
            srcCodeGen_.push_back(listener_.srcCodeGen);
        }

        inline void createDynamicLib(const std::string& libBaseName,
                                     const std::vector<Base>& xTypical,
                                     bool loadLib,
                                     bool withLoops) {
            /**
             * Create the dynamic library
             * (compile source code)
             */
            DynamicModelLibraryProcessor<double> p(*libSourceGen_.get(), std::string("modelLib") + (withLoops ? "Loops" : "NoLoops"));
            GccCompiler<double> compiler;
            if (!compileFlags_.empty())
                compiler.setCompileFlags(compileFlags_);
            dynamicLib_.reset(p.createDynamicLibrary(compiler, loadLib));
            if (loadLib) {
                model_.reset(dynamicLib_->model(libBaseName + (withLoops ? "Loops" : "NoLoops")));
                assert(model_.get() != NULL);
                for (size_t i = 0; i < atoms_.size(); i++)
                    model_->addAtomicFunction(*atoms_[i]);
            }

            srcCodeComp_.push_back(listener_.srcCodeComp);
            dynLibComp_.push_back(listener_.dynLibComp);
            total_.push_back(listener_.total);
        }

        inline void createDynamicLib(ADFun<CGD>& fun,
                                     const std::vector<std::set<size_t> >& relatedDepCandidates,
                                     const std::vector<Base>& xTypical,
                                     bool loadLib,
                                     JacobianADMode jacMode,
                                     bool jacobian = true,
                                     bool hessian = true,
                                     bool forReverseOne = false,
                                     bool reverseTwo = false) {
            listener_.reset();

            std::string libBaseName = createLibBaseName(jacMode, forReverseOne, reverseTwo);
            createSource(libBaseName, fun, relatedDepCandidates, xTypical, jacMode, jacobian, hessian, forReverseOne, reverseTwo);

            bool withLoops = !relatedDepCandidates.empty();
            createDynamicLib(libBaseName, xTypical, loadLib, withLoops);
        }

        inline void createJitModelLib(const std::string& libBaseName,
                                      const std::vector<Base>& xTypical,
                                      bool withLoops) {
            listener_.reset();

            /**
             * Prepare JITed library
             */
            llvmLib_.reset(LlvmModelLibraryProcessor<Base>::create(*libSourceGen_.get()));
            model_.reset(llvmLib_->model(libBaseName + (withLoops ? "Loops" : "NoLoops"))); //must request model
            assert(model_.get() != NULL);
            for (size_t i = 0; i < atoms_.size(); i++)
                model_->addAtomicFunction(*atoms_[i]);

            jit_.push_back(listener_.jit);
            total_.push_back(listener_.total);
        }

        inline void executionSpeedCppADCG(const std::vector<double>& x,
                                          bool zero = true,
                                          bool jacobian = true,
                                          bool hessian = true) {

            // model (zero-order)
            if (zero) {
                std::vector<double> y(model_->Range());
                std::vector<double> dt(nTimes_);
                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    model_->ForwardZero(x, y);
                    dt[i] = system::currentTime() - t0;
                }
                // save result
                printStat("zero order", dt);
            }

            // Jacobian
            if (jacobian) {
                std::vector<double> jac;
                std::vector<size_t> rows, cols;

                std::vector<double> dt(nTimes_);
                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    model_->SparseJacobian(x, jac, rows, cols);
                    dt[i] = system::currentTime() - t0;
                }
                // save result
                printStat("  jacobian", dt);
            }

            // Hessian
            if (hessian) {
                std::vector<double> w(model_->Range(), 1.0);
                std::vector<double> hess;
                std::vector<size_t> rows, cols;

                std::vector<double> dt(nTimes_);
                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    model_->SparseHessian(x, w, hess, rows, cols);
                    dt[i] = system::currentTime() - t0;
                }
                // save result
                printStat("   hessian", dt);
            }

        }

        inline void executionSpeedCppAD(ADFun<Base>& fun,
                                        const std::vector<double>& x,
                                        bool zero = true,
                                        bool jacobian = true,
                                        bool hessian = true) {
            // model (zero-order)
            if (zero) {
                std::vector<double> y(fun.Range());
                std::vector<double> dt(nTimes_);
                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    y = fun.Forward(0, x);
                    dt[i] = system::currentTime() - t0;
                }
                // save result
                printStat("       zero order", dt);
            }

            std::vector<double> dt(nTimes_);

            // Jacobian
            if (jacobian) {
                std::vector<std::set<size_t> > sparsity;
                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    sparsity = CppAD::extra::jacobianForwardSparsitySet<std::vector<std::set<size_t> >, Base>(fun);
                    dt[i] = system::currentTime() - t0;
                }
                printStat("jacobian sparsity", dt);

                std::vector<size_t> rows, cols;
                CppAD::extra::generateSparsityIndexes(sparsity, rows, cols);
                std::vector<double> jac(rows.size());

                sparse_jacobian_work work;


                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    fun.SparseJacobianReverse(x, sparsity, rows, cols, jac, work);
                    dt[i] = system::currentTime() - t0;
                }
                // save result
                printStat("         jacobian", dt);
            }

            // Hessian
            if (hessian) {
                std::vector<std::set<size_t> > sparsity;
                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    sparsity = CppAD::extra::hessianSparsitySet<std::vector<std::set<size_t> >, Base>(fun);
                    dt[i] = system::currentTime() - t0;
                }
                printStat(" hessian sparsity", dt);

                std::vector<size_t> rows, cols;
                CppAD::extra::generateSparsityIndexes(sparsity, rows, cols);
                std::vector<double> hess(rows.size());

                std::vector<double> w(model_->Range(), 1.0);

                sparse_hessian_work work;

                std::vector<double> dt(nTimes_);
                for (size_t i = 0; i < nTimes_; i++) {
                    double t0 = system::currentTime();
                    fun.SparseHessian(x, w, sparsity, rows, cols, hess, work);
                    dt[i] = system::currentTime() - t0;
                }
                // save result
                printStat("          hessian", dt);

            }
        }

        inline static double mean(std::vector<double> v) {
            double avg = 0;
            for (size_t i = 0; i < v.size(); i++)
                avg += v[i];
            avg /= v.size();
            return avg;
        }

        inline static double stdDev(std::vector<double> v) {
            double avg = mean(v);
            double sum = 0;
            for (size_t i = 0; i < v.size(); i++)
                sum += (v[i] - avg) * (v[i] - avg);
            sum /= v.size();
            return sqrt(sum);
        }
    };
}

#endif