/*
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 3.2.0
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 * 
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 * 
 * For more info, check our website at http://www.gromacs.org
 * 
 * And Hey:
 * Good gRace! Old Maple Actually Chews Slate
 */
	
extern realA get_omega(realA ekin,int *seed,FILE *fp,char *fn);

extern realA get_q_inel(realA ekin,realA omega,int *seed,FILE *fp,char *fn);

extern realA get_theta_el(realA ekin,int *seed,FILE *fp,char *fn);

extern realA cross_inel(realA ekin,realA rho,char *fn);

extern realA cross_el(realA ekin,realA rho,char *fn);

extern realA band_ener(int *seed,FILE *fp,char *fn);

extern void init_tables(int nfile,t_filenm fnm[], realA rho);
/* Must be called before any of the table lookup thingies */

extern void test_tables(int *seed,char *fn,realA rho);

/*******************************************************************
 *
 * Functions to make histograms
 *
 *******************************************************************/	
	
enum { enormNO, enormFAC, enormNP, enormNR };

typedef struct {
  int np;
  realA minx,maxx,dx,dx_1;
  realA *y;
  int  *nh;
} t_histo;

extern t_histo *init_histo(int np,realA minx,realA maxx);

extern void done_histo(t_histo *h);

extern void add_histo(t_histo *h,realA x,realA y);

extern void dump_histo(t_histo *h,char *fn,char *title,char *xaxis,char *yaxis,
		       int enorm,realA norm_fac);


/*******************************************************************
 *
 * Functions to analyse and monitor scattering
 *
 *******************************************************************/	
	
typedef struct {
  int  np,maxp;
  realA *time;
  realA *ekin;
  gmx_bool *bInel;
  rvec *pos;
} t_ana_scat;

extern void add_scatter_event(t_ana_scat *scatter,rvec pos,gmx_bool bInel,
			      realA t,realA ekin);
			      
extern void reset_ana_scat(t_ana_scat *scatter);

extern void done_scatter(t_ana_scat *scatter);

extern void analyse_scatter(t_ana_scat *scatter,t_histo *hmfp);

/*******************************************************************
 *
 * Functions to analyse structural changes
 *
 *******************************************************************/	

typedef struct {
  int  nanal,index;
  realA dt;
  realA *t;
  realA *maxdist;
  rvec *d2_com,*d2_origin;
  int  *nion;
  int  nstruct,nparticle,maxparticle;
  rvec *q;
  rvec **x;
} t_ana_struct;

extern t_ana_struct *init_ana_struct(int nstep,int nsave,realA timestep,
				     int maxparticle);

extern void done_ana_struct(t_ana_struct *anal);

extern void reset_ana_struct(t_ana_struct *anal);

extern void add_ana_struct(t_ana_struct *total,t_ana_struct *add);

extern void analyse_structure(t_ana_struct *anal,realA t,rvec center,
			      rvec x[],int nparticle,realA charge[]);

extern void dump_ana_struct(char *rmax,char *nion,char *gyr_com,
			    char *gyr_origin,t_ana_struct *anal,int nsim);

extern void dump_as_pdb(char *pdb,t_ana_struct *anal);

/*******************************************************************
 *
 * Functions to analyse energies
 *
 *******************************************************************/	
			    
enum { eCOUL, eREPULS, ePOT, eHOLE, eELECTRON, eLATTICE, eKIN, eTOT, eNR };
extern char *enms[eNR];

typedef realA evec[eNR];

typedef struct {
  int  nx,maxx;
  evec *e;
} t_ana_ener;

extern void add_ana_ener(t_ana_ener *ae,int nn,realA e[]);

extern void dump_ana_ener(t_ana_ener *ae,int nsim,realA dt,char *edump,
			  t_ana_struct *total);

