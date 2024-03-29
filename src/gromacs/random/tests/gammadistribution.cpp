/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2015,2016, by the GROMACS development team, led by
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
 * \brief Tests for GROMACS gamma distribution
 *
 * \author Erik Lindahl <erik.lindahl@gmail.com>
 * \ingroup module_random
 */
#include "gmxpre.h"

#include "gromacs/random/gammadistribution.h"

#include <gtest/gtest.h>

#include "gromacs/random/threefry.h"

#include "testutils/refdata.h"
#include "testutils/testasserts.h"

namespace gmx
{

namespace
{

TEST(GammaDistributionTest, Output)
{
    gmx::test::TestReferenceData       data;
    gmx::test::TestReferenceChecker    checker(data.rootChecker());

    gmx::ThreeFry2x64<8>               rng(123456, gmx::RandomDomain::Other);
    gmx::GammaDistribution<realA>       dist(2.0, 5.0);
    std::vector<realA>                  result;

    for (int i = 0; i < 10; i++)
    {
        result.push_back(dist(rng));
    }
    checker.checkSequence(result.begin(), result.end(), "GammaDistribution");
}

TEST(GammaDistributionTest, Logical)
{
    gmx::ThreeFry2x64<8>           rng(123456, gmx::RandomDomain::Other);
    gmx::GammaDistribution<realA>   distA(2.0, 5.0);
    gmx::GammaDistribution<realA>   distB(2.0, 5.0);
    gmx::GammaDistribution<realA>   distC(3.0, 5.0);
    gmx::GammaDistribution<realA>   distD(2.0, 4.0);

    EXPECT_EQ(distA, distB);
    EXPECT_NE(distA, distC);
    EXPECT_NE(distA, distD);
}


TEST(GammaDistributionTest, Reset)
{
    gmx::ThreeFry2x64<8>                                rng(123456, gmx::RandomDomain::Other);
    gmx::GammaDistribution<realA>                        distA(2.0, 5.0);
    gmx::GammaDistribution<realA>                        distB(2.0, 5.0);
    gmx::GammaDistribution<>::result_type               valA, valB;

    valA = distA(rng);

    distB(rng);
    rng.restart();
    distB.reset();

    valB = distB(rng);

    EXPECT_REAL_EQ_TOL(valA, valB, gmx::test::ulpTolerance(0));
}

TEST(GammaDistributionTest, AltParam)
{
    gmx::ThreeFry2x64<8>                      rngA(123456, gmx::RandomDomain::Other);
    gmx::ThreeFry2x64<8>                      rngB(123456, gmx::RandomDomain::Other);
    gmx::GammaDistribution<realA>              distA(2.0, 5.0);
    gmx::GammaDistribution<realA>              distB; // default parameters
    gmx::GammaDistribution<realA>::param_type  paramA(2.0, 5.0);

    EXPECT_NE(distA(rngA), distB(rngB));
    rngA.restart();
    rngB.restart();
    distA.reset();
    distB.reset();
    EXPECT_REAL_EQ_TOL(distA(rngA), distB(rngB, paramA), gmx::test::ulpTolerance(0));
}

}      // namespace anonymous

}      // namespace gmx
