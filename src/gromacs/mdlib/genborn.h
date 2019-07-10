/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2008, The GROMACS development team.
 * Copyright (c) 2013,2014,2015, by the GROMACS development team, led by
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
#ifndef GMX_MDLIB_GENBORN_H
#define GMX_MDLIB_GENBORN_H

#include "gromacs/math/utilities.h"
#include "gromacs/math/vectypes.h"

struct gmx_genborn_t;
struct gmx_enerdata_t;
struct gmx_localtop_t;
struct gmx_mtop_t;
struct t_commrec;
struct t_forcerec;
struct t_graph;
struct t_idef;
struct t_inputrec;
struct t_mdatoms;
struct t_nblist;
struct t_nrnb;
struct t_pbc;

typedef struct
{
    int  nbonds;
    int  bond[10];
    realA length[10];
} genborn_bonds_t;

typedef struct gbtmpnbls *gbtmpnbls_t;

/* Struct to hold all the information for GB */
typedef struct gmx_genborn_t
{
    int nr;                   /* number of atoms, length of arrays below */
    int n12;                  /* number of 1-2 (bond) interactions       */
    int n13;                  /* number of 1-3 (angle) terms             */
    int n14;                  /* number of 1-4 (torsion) terms           */
    int nalloc;               /* Allocation of local arrays (with DD)    */


    /* Arrays below that end with _globalindex are used for setting up initial values of
     * all gb parameters and values. They all have length natoms, which for DD is the
     * global atom number.
     * Values are then taken from these arrays to local copies, that have names without
     * _globalindex, in the routine make_local_gb(), which is called once for single
     * node runs, and for DD at every call to dd_partition_system
     */

    realA       *gpol;              /* Atomic polarisation energies */
    realA       *gpol_globalindex;  /*  */
    realA       *gpol_still_work;   /* Work array for Still model */
    realA       *gpol_hct_work;     /* Work array for HCT/OBC models */
    realA       *bRad;              /* Atomic Born radii */
    realA       *vsolv;             /* Atomic solvation volumes */
    realA       *vsolv_globalindex; /*  */
    realA       *gb_radius;         /* Radius info, copied from atomtypes */
    realA       *gb_radius_globalindex;

    int        *use;                /* Array that till if this atom does GB */
    int        *use_globalindex;    /* Global array for parallelization */

    realA        es;                 /* Solvation energy and derivatives */
    realA       *asurf;              /* Atomic surface area */
    rvec       *dasurf;             /* Surface area derivatives */
    realA        as;                 /* Total surface area */

    realA       *drobc;              /* Parameters for OBC chain rule calculation */
    realA       *param;              /* Precomputed factor rai*atype->S_hct for HCT/OBC */
    realA       *param_globalindex;  /*  */

    realA       *log_table;          /* Table for logarithm lookup */

    realA        obc_alpha;          /* OBC parameters */
    realA        obc_beta;           /* OBC parameters */
    realA        obc_gamma;          /* OBC parameters */
    realA        gb_doffset;         /* Dielectric offset for Still/HCT/OBC */
    realA        gb_epsilon_solvent; /*   */
    realA        epsilon_r;          /* Used for inner dielectric */

    realA        sa_surface_tension; /* Surface tension for non-polar solvation */

    realA       *work;               /* Used for parallel summation and in the chain rule, length natoms         */
    realA       *buf;                /* Used for parallel summation and in the chain rule, length natoms         */
    int        *count;              /* Used for setting up the special gb nblist, length natoms                 */
    gbtmpnbls_t nblist_work;        /* Used for setting up the special gb nblist, dim natoms*nblist_work_nalloc */
    int         nblist_work_nalloc; /* Length of second dimension of nblist_work                                */
}
gmx_genborn_t;
/* Still parameters - make sure to edit in genborn_sse.c too if you change these! */
#define STILL_P1  0.073*0.1              /* length        */
#define STILL_P2  0.921*0.1*CAL2JOULE    /* energy*length */
#define STILL_P3  6.211*0.1*CAL2JOULE    /* energy*length */
#define STILL_P4  15.236*0.1*CAL2JOULE
#define STILL_P5  1.254

#define STILL_P5INV (1.0/STILL_P5)
#define STILL_PIP5  (M_PI*STILL_P5)


/* Initialise GB stuff */
int init_gb(struct gmx_genborn_t **p_born,
            struct t_forcerec *fr, const struct t_inputrec *ir,
            const gmx_mtop_t *mtop, int gb_algorithm);


/* Born radii calculations, both with and without SSE acceleration */
int calc_gb_rad(struct t_commrec *cr, struct t_forcerec *fr, struct t_inputrec *ir, gmx_localtop_t *top, rvec x[], t_nblist *nl, struct gmx_genborn_t *born, t_mdatoms *md, t_nrnb     *nrnb);



/* Bonded GB interactions */
realA gb_bonds_tab(rvec x[], rvec f[], rvec fshift[], realA *charge, realA *p_gbtabscale,
                  realA *invsqrta, realA *dvda, realA *GBtab, t_idef *idef, realA epsilon_r,
                  realA gb_epsilon_solvent, realA facel, const struct t_pbc *pbc,
                  const struct t_graph *graph);




/* Functions for calculating adjustments due to ie chain rule terms */
void
calc_gb_forces(struct t_commrec *cr, t_mdatoms *md, struct gmx_genborn_t *born, gmx_localtop_t *top,
               rvec x[], rvec f[], struct t_forcerec *fr, t_idef *idef, int gb_algorithm, int sa_algorithm, t_nrnb *nrnb,
               const struct t_pbc *pbc, const struct t_graph *graph, struct gmx_enerdata_t *enerd);


int
make_gb_nblist(struct t_commrec *cr, int gb_algorithm,
               rvec x[], matrix box,
               struct t_forcerec *fr, t_idef *idef, struct t_graph *graph, struct gmx_genborn_t *born);

void
make_local_gb(const struct t_commrec *cr, struct gmx_genborn_t *born, int gb_algorithm);

#endif
