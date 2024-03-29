/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2013,2014,2015,2016, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */

/*! \internal \file
 * \brief
 * Tests for the mdrun multi-simulation functionality
 *
 * \todo Test mdrun -multidir also
 *
 * \author Mark Abraham <mark.j.abraham@gmail.com>
 * \ingroup module_mdrun_integration_tests
 */
#include "gmxpre.h"

#include "multisimtest.h"

#include <cmath>

#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "gromacs/utility/basenetwork.h"
#include "gromacs/utility/path.h"
#include "gromacs/utility/real.h"
#include "gromacs/utility/stringutil.h"

#include "testutils/cmdlinetest.h"

#include "moduletest.h"
#include "terminationhelper.h"

namespace gmx
{
namespace test
{

MultiSimTest::MultiSimTest() : size_(gmx_node_num()),
                               rank_(gmx_node_rank()),
                               mdrunCaller_(new CommandLine)
{
    runner_.mdpInputFileName_  = fileManager_.getTemporaryFilePath(formatString("input%d.mdp", rank_));
    runner_.mdpOutputFileName_ = fileManager_.getTemporaryFilePath(formatString("output%d.mdp", rank_));

    /* grompp needs to name the .tpr file so that when mdrun appends
       the MPI rank, it will find the right file. If we just used
       "%d.tpr" then \c TestFileManager prefixes that with an
       underscore. Then, there is no way for mdrun to be told the
       right name, because if you add the underscore manually, you get
       a second one from \c TestFileManager. However, it's easy to
       just start the suffix with "topol" in both cases. */
    runner_.tprFileName_ = fileManager_.getTemporaryFilePath(formatString("topol%d.tpr", rank_));
    mdrunTprFileName_    = fileManager_.getTemporaryFilePath("topol.tpr");

    runner_.useTopGroAndNdxFromDatabase("spc2");

    mdrunCaller_->append("mdrun");
    mdrunCaller_->addOption("-multi", size_);
}

void MultiSimTest::organizeMdpFile(const char *controlVariable,
                                   int         numSteps)
{
    const realA  baseTemperature = 298;
    const realA  basePressure    = 1;
    std::string mdpFileContents =
        formatString("nsteps = %d\n"
                     "nstlog = 1\n"
                     "nstcalcenergy = 1\n"
                     "tcoupl = v-rescale\n"
                     "tc-grps = System\n"
                     "tau-t = 1\n"
                     "ref-t = %f\n"
                     // pressure coupling (if active)
                     "tau-p = 1\n"
                     "ref-p = %f\n"
                     "compressibility = 4.5e-5\n"
                     // velocity generation
                     "gen-vel = yes\n"
                     "gen-temp = %f\n"
                     // control variable specification
                     "%s\n",
                     numSteps,
                     baseTemperature + 0.0001*rank_,
                     basePressure * std::pow(1.01, rank_),
                     /* Set things up so that the initial KE decreases with
                        increasing replica number, so that the (identical)
                        starting PE decreases on the first step more for the
                        replicas with higher number, which will tend to force
                        replica exchange to occur. */
                     std::max(baseTemperature - 10 * rank_, realA(0)),
                     controlVariable);
    runner_.useStringAsMdpFile(mdpFileContents);
}

void MultiSimTest::runExitsNormallyTest()
{
    if (size_ <= 1)
    {
        /* Can't test multi-sim without multiple ranks. */
        return;
    }

    const char *pcoupl = GetParam();
    organizeMdpFile(pcoupl);
    /* Call grompp on every rank - the standard callGrompp() only runs
       grompp on rank 0. */
    EXPECT_EQ(0, runner_.callGromppOnThisRank());

    // mdrun names the files without the rank suffix
    runner_.tprFileName_ = mdrunTprFileName_;
    ASSERT_EQ(0, runner_.callMdrun(*mdrunCaller_));
}

void MultiSimTest::runMaxhTest()
{
    if (size_ <= 1)
    {
        /* Can't test replica exchange without multiple ranks. */
        return;
    }

    TerminationHelper helper(&fileManager_, mdrunCaller_.get(), &runner_);
    // Make sure -maxh has a chance to propagate
    int               numSteps = 100;
    organizeMdpFile("pcoupl = no", numSteps);
    /* Call grompp on every rank - the standard callGrompp() only runs
       grompp on rank 0. */
    EXPECT_EQ(0, runner_.callGromppOnThisRank());

    // mdrun names the files without the rank suffix
    runner_.tprFileName_ = mdrunTprFileName_;

    // The actual output checkpoint file gets a rank suffix, so
    // handle that in the expected result.
    std::string expectedCptFileName
        = Path::concatenateBeforeExtension(runner_.cptFileName_, formatString("%d", rank_));
    helper.runFirstMdrun(expectedCptFileName);
    helper.runSecondMdrun();
}

} // namespace
} // namespace
