
/*********************************************************
**********************************************************

      Cosmology and SFR Utilities 
      [moved from sntools.c, Oct 2020]

**********************************************************
**********************************************************/

#include "sntools.h"
#include "sntools_cosmology.h"

// ***********************************
void init_HzFUN_INFO(int VBOSE, double *cosPar, char *fileName, 
		     HzFUN_INFO_DEF *HzFUN_INFO) {

  // initialize input struct HzFUN_INFO with either
  //  * wCDM params in cosPar (analytic cosmology theory)
  //  * Hz vs. z map read from fileName (used for interpolation)
  //
  // If fileName contains string OUT or out, then interpret
  // as output file to write H(z) using cosPar params, and
  // then read it back.

  int ipar;
  int MEMD   = MXMAP_HzFUN * sizeof(double);
  bool WR_OUTFILE;
  char fnam[] = "init_HzFUN_INFO";

  // ----------- BEGIN ------------

  if ( VBOSE ) {  print_banner(fnam); }

  // always store COSPAR_LIST ... even for map where analytic H(z,COSPAR)
  // is used at very low redshifts for integrals starting from z=0.
  for(ipar=0; ipar < NCOSPAR_HzFUN; ipar++ ) 
    { HzFUN_INFO->COSPAR_LIST[ipar] = cosPar[ipar]; }

  HzFUN_INFO->Nzbin_MAP = 0;

  // - - - - - - 
  HzFUN_INFO->USE_MAP = !IGNOREFILE(fileName) ;
  WR_OUTFILE = (strstr(fileName,"OUT") != NULL || 
		strstr(fileName,"out") != NULL ) ;

  if ( HzFUN_INFO->USE_MAP ) {

    HzFUN_INFO->FILENAME = (char*) malloc( MXPATHLEN * sizeof(char) );
    sprintf(HzFUN_INFO->FILENAME, "%s", fileName);

    // check debug option to write H(z) to file using COSPAR_LIST,
    // and then read it like any other HzFUN file
    if ( WR_OUTFILE ) { write_HzFUN_FILE(HzFUN_INFO);  }

    // read 2 column map of H(z)
    printf("   Read H(z) map from: %s \n", fileName );
    HzFUN_INFO->zCMB_MAP  = (double*) malloc(MEMD);
    HzFUN_INFO->HzFUN_MAP = (double*) malloc(MEMD);
    rd2columnFile(fileName, MXMAP_HzFUN, &HzFUN_INFO->Nzbin_MAP,
		  HzFUN_INFO->zCMB_MAP, HzFUN_INFO->HzFUN_MAP  );

    int Nzbin   = HzFUN_INFO->Nzbin_MAP;
    double zmin = HzFUN_INFO->zCMB_MAP[0]  ;
    double zmax = HzFUN_INFO->zCMB_MAP[Nzbin-1]  ;

    printf("\t Found %d redshift bins from %f to %f \n",
	   Nzbin, zmin, zmax ); fflush(stdout);

    // require first z-element to be zero
    if ( zmin != 0.0 ) {
      sprintf(c1err,"zCMB_min=%f, but must be zero.", zmin );
      sprintf(c2err,"Check H(z) map.") ;
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err);
    }
  }
  else {
    // COSPAR_LIST already loaded above.
    if ( VBOSE ) {
      double H0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_H0] ;
      double OM = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OM] ;
      double OL = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OL] ;
      double w0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_w0] ;
      double wa = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_wa] ;
      double Ok = 1.0 - OM - OL ;
      printf("\t H0         = %.2f      # km/s/Mpc \n",  H0);
      printf("\t OM, OL, Ok = %7.5f, %7.5f, %7.5f \n", OM, OL, Ok );
      printf("\t w0, wa     = %6.3f, %6.3f \n", w0, wa);

      checkval_D("H0", 1, &H0,  30.0, 100.0 );
      checkval_D("OM", 1, &OM,  0.0,  1.0 );
      checkval_D("OL", 1, &OM,  0.0,  1.0 );
      checkval_D("wa", 1, &wa, -3.0,  1.0 );

      fflush(stdout) ;
    }

  }

  return ;

} // end init_HzFUN_INFO


// ****************************************
void write_HzFUN_FILE(HzFUN_INFO_DEF *HzFUN_INFO ) {

  // BEWARE: FOR DEBUG ONLY !
  // write 2-column H(z) to text file, which is used
  // to test reading H(z) file. Write in log(z) bins
  // to make sure it can read non-uniform bins.
  // Goal is to compare analytic Hzfun_wCDM with Hzfun_interp.
  //
  char *outFile = HzFUN_INFO->FILENAME;
  double zCMB_min = 0.005;
  double zCMB_max = ZMAX_SNANA ;
  double logz_min = log10(zCMB_min);
  double logz_max = log10(zCMB_max);
  int    Nzbin    = 200 ;
  double logz_bin = (logz_max - logz_min) / (double)Nzbin;

  FILE *fp; 
  double logz, z, Hz ;
  int  iz;
  char fnam[] = "write_HzFUN_FILE" ;
  
  // ---------- BEGIN ------------

  printf("   Write H(z) to %s \n", outFile); 
  printf("\t %d bins for zCMB = %.4f to %.4f \n", 
	 Nzbin, zCMB_min, zCMB_max);
  printf("\t logz(min,max,bin) = %f, %f, %f \n",
	 logz_min, logz_max, logz_bin );
  fflush(stdout);

  fp = fopen(outFile,"wt");
  if ( !fp ) {
    sprintf(c1err,"Unable to open '%s' in write-text mode");
    sprintf(c2err,"Check permisssions and if file already exists.") ;
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);
  }


  // write documentation keys
  fprintf(fp,"DOCUMENTATION: \n");
  fprintf(fp,"  NOTES: \n");
  fprintf(fp,"  - Auto generated by snlc_sim.exe  \n");
  fprintf(fp,"  COSPAR: \n");
  fprintf(fp,"    H0: %.2f \n" ,HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_H0] );
  fprintf(fp,"    OM: %.4f \n" ,HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OM] );
  fprintf(fp,"    OL: %.4f \n" ,HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OL] );
  fprintf(fp,"    w0: %.2f \n" ,HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_w0] );
  fprintf(fp,"    wa: %.2f \n" ,HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_wa] );
  fprintf(fp, "DOCUMENTATION_END: \n\n");

  // turn off map use temporarily so that COSPAR is used in Hzfun().

  HzFUN_INFO->USE_MAP = false ;
  for(iz=0; iz < Nzbin; iz++ ) {
    if ( iz == 0 )
      { z = 0.0 ; }
    else {
      logz = logz_min + logz_bin * (double)(iz-1) ;
      z    = pow(TEN,logz);
    }
    Hz   = Hzfun(z,HzFUN_INFO);
    fprintf(fp," %7.5f  %9.4f\n", z, Hz);
  }

  fclose(fp);
  HzFUN_INFO->USE_MAP = true ; // restore using map

  return ;

} // end write_HzFUN_FILE


// ****************************
double SFR_integral(double z, HzFUN_INFO_DEF *HzFUN_INFO) {

  /***
   Integrate SFR(t) from 0 to current time.
   For convenience, integrate  over 'a' instead of redshift
   [since z goes to infinity]
 
        /a
       |   SFR(a') 
     c |  ----------- da'
       |  a' * H(a')
       /0

  ***/

  double H0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_H0];
  int    ia, NABIN = 100 ;
  double AMIN, AMAX, ABIN, atmp, ztmp, xa, tmp  ;
  double sum, sfr, aH ;

  double SECONDS_PER_YEAR = 3600. * 24. * 365. ;

  // ---------- BEGIN ------------

  AMIN = 0.0 ;
  AMAX = 1. / (1. + z) ;
  ABIN = (AMAX - AMIN) / (double)NABIN ;

  sum = 0.0 ;

  for ( ia=1; ia <= NABIN; ia++ ) {
    xa   = (double)ia ;
    atmp = AMIN + ABIN * ( xa - 0.5 ) ;
    ztmp = (1. / atmp) - 1.0 ;

    sfr = SFRfun_BG03(ztmp,H0);
    aH  = atmp * Hzfun(ztmp, HzFUN_INFO);
    sum += sfr / aH ;
  }

  // convert H (km/s/Mpc) to H(/year)

  tmp = (1.0E6 * PC_km) / SECONDS_PER_YEAR ;
  // xxx tmp = 1.0 / SECONDS_PER_YEAR ;
  sum *= (ABIN * tmp) ;
  return sum ;

}  // end of function SFR_integral


// ****************************
double SFRfun_BG03(double z, double H0) {

  /***
      Compute sfr(z) = (a+b*z)/(1+(z/c)^d)*h solar_mass/yr/Mpc^3
      where H0 unit is km/s/Mpc and  h = H0/(100 km/s/Mpc)
     
      Oct 16 2020: rename SFRfun -> SFRfun_BG03
  ***/

  // -- Baldry and Glazebrook IMF --
  // https://ui.adsabs.harvard.edu/abs/2003ApJ...593..258B

  double a = 0.0118 ;
  double b = 0.08 ;
  double c = 3.3 ;
  double d = 5.2 ;
  double tmp1, tmp2, zc, h, SFRLOC;

  // ------------ BEGIN --------------
  zc   = z/c ;
  tmp1 = a + b*z ;
  tmp2 = 1.0 + pow(zc,d) ;
  h    = H0 / 100. ; 
  SFRLOC = h * tmp1 / tmp2 ;
  return(SFRLOC) ;
}  // end SFRfun_BG03


// *******************************************
double SFRfun_MD14(double z, double *params) {

  // Created Dec 2016 by R.Kessler
  // use function from  Madau & Dickoson 2014,
  // that was also used in Strolger 2015 for CC rate.
  // This function intended for CC rate, so beware
  // using for other purposes (e.g., no H0 factor here).

  double A      = params[0];
  double B      = params[1];
  double C      = params[2];
  double D      = params[3];
  double z1     = 1.0 + z;
  double top    = A*pow(z1,C);
  double bottom = 1.0 + pow( (z1/B), D );
  double SFR    = top/bottom; // also see Eq 8+9 of Strolger 2015 
  return(SFR);

} // end SFRfun_MD14



// *******************************************
double dVdz_integral(int OPT, double zmax, HzFUN_INFO_DEF *HzFUN_INFO) {

  //
  // return integral of dV/dz = r(z)^2/H(z) dz
  // OPT = 0:  returns standard volume integral
  // OPT = 1:  returns  z-wgted integral

  double sum, tmp, dz, ztmp, wz, xz ;
  int Nzbin, iz;

  // ---- BEGIN ----------

  // compute exact integral

  Nzbin = (int)( zmax * 1000.0 ) ;
  if ( Nzbin < 10 ) { Nzbin = 10 ; }
  dz   = zmax / (float)Nzbin ;   // integration binsize
  sum = 0.0;

  for ( iz=0; iz < Nzbin; iz++ ) {
    xz   = (double)iz ;
    ztmp = dz * (xz + 0.5) ;
    tmp  = dVdz(ztmp,HzFUN_INFO);

    wz = 1.0;
    if ( OPT == 1 ) { wz = ztmp; }

    sum += wz * tmp;

  }

  sum *= dz ;

  //  printf(" xxxx dVdz_integral = %e (approx=%e) \n", sum, sumtmp  );

  return sum ;

}  // end of dVdz_integral


double dvdz_integral__(int *OPT, double *zmax, double *COSPAR) {
		       
  HzFUN_INFO_DEF HzFUN_INFO;
  int ipar;

  HzFUN_INFO.USE_MAP = false ;
  for (ipar=0; ipar < NCOSPAR_HzFUN; ipar++ ) 
    { HzFUN_INFO.COSPAR_LIST[ipar] = COSPAR[ipar]; }

  return dVdz_integral(*OPT, *zmax, &HzFUN_INFO);
}


// **********************************
double dVdz(double z, HzFUN_INFO_DEF *HzFUN_INFO) {
  // returns dV/dz = r(z)^2 / H(z)
  double r, H, tmp ;
  double zmin = 0.0;
  r = Hzinv_integral (zmin, z, HzFUN_INFO);
  H = Hzfun ( z, HzFUN_INFO);
  tmp = LIGHT_km * r * r / H ;
  return tmp;
}  // end of dVdz


// ******************************************
double Hzinv_integral(double zmin, double zmax, HzFUN_INFO_DEF *HzFUN_INFO) {

  double H0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_H0];
  double H0_per_sec = H0 / ( 1.0E6 * PC_km);
  double OM = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OM];
  double OL = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OL];

  int iz, Nzbin ;
  double dz, Hz, xz, ztmp, sum, Hzinv, KAPPA, SQRT_KAPPA ; 

  // ------ return integral c*r(z) = int c*dz/H(z) -------------
  // Note that D_L = (1+z)*Hzinv_integral

  sum = 0.0;

  Nzbin = (int)( (zmax-zmin) * 1000.0 ) ;
  if ( Nzbin < 10 ) { Nzbin = 10 ; }
  dz  = (zmax-zmin) / (double)Nzbin ;      // integration binsize

  for ( iz=0; iz < Nzbin; iz++ ) {
    xz   = (double)iz ;
    ztmp = zmin + dz * (xz + 0.5) ;
    Hz   = Hzfun (ztmp, HzFUN_INFO);
    sum += (1.0/Hz) ;
  }

  // remove H0 factor from inetgral before checking curvature.

  sum *= (dz * H0) ;

  // check for curvature
  KAPPA      = 1.0 - OM - OL ; 
  SQRT_KAPPA = sqrt(fabs(KAPPA));

  if ( KAPPA < -0.00001 ) 
    { Hzinv = sin( SQRT_KAPPA * sum ) / SQRT_KAPPA ; }
  else if ( KAPPA > 0.00001 ) 
    { Hzinv = sinh( SQRT_KAPPA * sum ) / SQRT_KAPPA ; }
  else
    { Hzinv = sum ; }


  // return Hzinv with c/H0 factor
  return (Hzinv * LIGHT_km / H0 ) ;

} // end of Hzinv_integral


// ******************************************
double Hainv_integral(double amin, double amax, HzFUN_INFO_DEF *HzFUN_INFO) {

  // Same as Hzinv_integral, but integrate over a instead of over z.
  // dz/E(z) :  z=1/a-1   dz = -da/a^2

  double H0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_H0];
  double H0_per_sec = H0 / ( 1.0E6 * PC_km);
  double OM = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OM];
  double OL = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OL];

  int ia, Nabin ;
  double da, Hz, xa, atmp, ztmp, tmp, sum, Hzinv, KAPPA, SQRT_KAPPA ; 
  char fnam[] = "Hainv_integral";

  // ------ return integral c*r(z) = int c*dz/H(z) -------------
  // Note that D_L = (1+z)*Hzinv_integral

  sum = 0.0;

  Nabin = (int)( (amax-amin) * 1000.0 ) ;
  if ( Nabin < 10 ) { Nabin = 10 ; }
  da   = (amax-amin) / (double)Nabin ;   // integration binsize

  for ( ia=0; ia < Nabin; ia++ ) {
    xa   = (double)ia ;
    atmp = amin + da * (xa + 0.5) ;
    ztmp = 1./atmp - 1.0 ;
    Hz   = Hzfun (ztmp, HzFUN_INFO);
    tmp  = 1.0/( Hz * atmp * atmp) ;
    sum += tmp ;
  }

  // remove H0 factor from inetgral before checking curvature.

  sum *= (da * H0) ;

  // check for curvature
  KAPPA      = 1.0 - OM - OL ; 
  SQRT_KAPPA = sqrt(fabs(KAPPA));

  if ( KAPPA < -0.00001 ) 
    { Hzinv = sin( SQRT_KAPPA * sum ) / SQRT_KAPPA ; }
  else if ( KAPPA > 0.00001 ) 
    { Hzinv = sinh( SQRT_KAPPA * sum ) / SQRT_KAPPA ; }
  else
    { Hzinv = sum ; }


  // return Hzinv with c/H0 factor
  return (Hzinv * LIGHT_km / H0 ) ;

} // end of Hainv_integral



// ******************************************
double Hzfun(double zCMB, HzFUN_INFO_DEF *HzFUN_INFO ) {

  // Driver to return H(zCMB) from analytic wCDM, or from interpolating map.
  bool   USE_MAP = HzFUN_INFO->USE_MAP ;
  double Hz ;
  char fnam[] = "Hzfun" ;

  // ------ returns H(z) -------------
  
  if ( USE_MAP ) {
    // interpolate from map read from file.
    Hz = Hzfun_interp(zCMB, HzFUN_INFO);
  }
  else {
    Hz = Hzfun_wCDM(zCMB, HzFUN_INFO);
  }

  return Hz ;

} // end of Hzfun

// *************************************************
double Hzfun_wCDM(double zCMB, HzFUN_INFO_DEF *HzFUN_INFO) {

  // Created Oct 2020
  // Refactor using HzFUN_INFO struct, and also use wa.
  //
  // July 26 2021: fix awful bug for wa; initially I had assumed
  //    that integral inside exponent was evaluated numerically,
  //    but it's not. Include analytic integral.

  double H0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_H0] ; // km/s/Mpc
  double OM = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OM] ;
  double OL = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_OL] ;
  double w0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_w0] ;
  double wa = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_wa] ;
  double Hz, sqHz, a, ZZ, Z2, Z3, ZL, KAPPA ;
  double argpow, argexp ;   

  KAPPA = 1.0 - OM - OL ;  // curvature
  ZZ    = 1.0 + zCMB ;    Z2=ZZ*ZZ ;   Z3=Z2*ZZ ; 
  a     = 1.0/ZZ;

  argpow    =  3.0 * (1.0 + w0 + wa) ;
  argexp    = -3.0 * wa * zCMB * a ;
  ZL        = pow(ZZ,argpow) * exp(argexp);

  sqHz  = OM*Z3  + KAPPA*Z2 + OL*ZL;
  Hz    = H0 * sqrt ( sqHz ) ;
  return(Hz);

} // end Hzfun_wCDM


// ******************************************
double Hzfun_interp(double zCMB, HzFUN_INFO_DEF *HzFUN_INFO) {

    int OPT_INTERP = 1; // 1=linear; 2=quadratic
    int Nzbin      = HzFUN_INFO->Nzbin_MAP;
    double *zMAP   = HzFUN_INFO->zCMB_MAP ;   // zCMB array from map
    double *HzMAP  = HzFUN_INFO->HzFUN_MAP ;  // H(z) array from map
    double Hz;
    char fnam[] = "Hzfun_interp";

    Hz = interp_1DFUN(OPT_INTERP, zCMB, Nzbin, zMAP, HzMAP, fnam);
    return(Hz);

} // end Hzfun_interp

// ******************************************
double dLmag ( double zCMB, double zHEL, 
	       HzFUN_INFO_DEF *HzFUN_INFO, ANISOTROPY_INFO_DEF *ANISOTROPY_INFO  ) {
	       	       
  // returns luminosity distance in mags:
  //   dLmag = 5 * log10(DL/10pc)
  //
  // Feb 2023: pass ANISOTROPY_INFO to enable anistropy models

  double rz, dl, arg, mu, zero=0.0 ;
  // ----------- BEGIN -----------
  rz     = Hzinv_integral(zero,zCMB,HzFUN_INFO) ;
  rz    *= (1.0E6*PC_km);  // H -> 1/sec units
  dl     = ( 1.0 + zHEL ) * rz ;
  arg    = dl / (10.0 * PC_km);
  mu     = 5.0 * log10( arg );

  if ( ANISOTROPY_INFO->USE_FLAG ) {
    H0 = HzFUN_INFO->COSPAR_LIST[ICOSPAR_HzFUN_H0];
    // xxx for S. Sah to complete
    //starting the function definition  : Taylor expanded luminosity distance for titlted universe 
    // Model taken from paper  arXiv:gr-qc/0309109v4 
    double J0_dipole = ANISOTROPY_INFO->J0;
    
    dl= (LIGHT_km*zHEL/H0)*(1.0+1.0/2.0*(1-q_dipole(zHEL,ANISOTROPY_INFO))*zHEL-(1.0/6.0)*(1-q_dipole(zHEL,ANISOTROPY_INFO)-3*pow(q_dipole(zHEL,ANISOTROPY_INFO),2)+J0_dipole)*pow(zHEL,2));
    arg    = dl / (10.0 * PC_km);
    mu     = 5.0 * log10( arg );

    
    
  }

  return mu ;
}  // end of dLmag


// dipolar q for tilted cosmology
double F_dipole( double zHEL, ANISOTROPY_INFO_DEF *ANISOTROPY_INFO) {
    double S_dipole = ANISOTROPY_INFO->S; 
    return exp(-zHEL/S_dipole);    
    
}

double angular_separation( ANISOTROPY_INFO_DEF *ANISOTROPY_INFO) {
    double lon1 = ANISOTROPY_INFO->GLON *PI/180;
    double lat1 = ANISOTROPY_INFO->GLAT *PI/180;
    double lon2 = 264.021 *PI/180;
    double lat2 = 48.253*PI/180;
    double dlon = lon2 - lon1;
    double dlat = lat2 - lat1;
    double a = pow(sin(dlat/2), 2) + cos(lat1) * cos(lat2) * pow(sin(dlon/2), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double sep = c * 180 / PI;
    return sep;
    
}
    
double q_dipole(double zHEL, ANISOTROPY_INFO_DEF *ANISOTROPY_INFO){
    double q;
    double qd = ANISOTROPY_INFO->qd;
    double qm = ANISOTROPY_INFO->qm;
    double sep = angular_separation(ANISOTROPY_INFO);
    q = qm + qd*F_dipole(zHEL,ANISOTROPY_INFO)*cos(sep*PI/180);
    return q;
}

// ******************************************
double dlmag_fortc__(double *zCMB, double *zHEL, double *H0,
		     double *OM, double *OL, double *w0, double *wa) {
	       	     
  // C interface to fortran;
  // returns luminosity distance in mags:   dLmag = 5 * log10(DL/10pc)
  //
  double mu;
  HzFUN_INFO_DEF HzFUN_INFO ;
  ANISOTROPY_INFO_DEF ANISOTROPY_INFO;

  // ----------- BEGIN -----------

  HzFUN_INFO.COSPAR_LIST[ICOSPAR_HzFUN_H0] = *H0 ;
  HzFUN_INFO.COSPAR_LIST[ICOSPAR_HzFUN_OM] = *OM ;
  HzFUN_INFO.COSPAR_LIST[ICOSPAR_HzFUN_OL] = *OL ;
  HzFUN_INFO.COSPAR_LIST[ICOSPAR_HzFUN_w0] = *w0 ;
  HzFUN_INFO.COSPAR_LIST[ICOSPAR_HzFUN_wa] = *wa ;
  HzFUN_INFO.USE_MAP = false ;

  ANISOTROPY_INFO.USE_FLAG = false;
  mu = dLmag(*zCMB, *zHEL, &HzFUN_INFO, &ANISOTROPY_INFO );

  /* xxx
  printf(" xxx dlmag_fortc: z=%.3f/%.3f,  H0=%.2f OM,OL=%.3f,%.3f \n",
	 *zCMB, *zHEL, *H0, *OM, *OL); fflush(stdout);
	printf(" xxx dlmag_fortc: mu = %f \n", mu);
	 xxx */

  return mu ;

}  // end of dlmag_fort__


double zcmb_dLmag_invert( double MU, HzFUN_INFO_DEF *HzFUN_INFO, 
			  ANISOTROPY_INFO_DEF *ANISOTROPY_INFO) {

  // Created Jan 4 2018
  // for input distance modulus (MU), solve for zCMB.

  double zCMB, zCMB_start, dmu, DMU, mutmp, DL ;
  double DMU_CONVERGE = 1.0E-4 ;
  int    NITER=0;
  char fnam[] = "zcmb_dLmag_invert" ;

  // ---------- BEGIN ----------

  // use naive Hubble law to estimate zCMB_start
  DL    = pow( 10.0,(MU/5.0) ) * 1.0E-5 ; // Mpc
  zCMB_start = (70.0*DL)/LIGHT_km ;
  zCMB_start *= exp(-zCMB_start/6.0); // very ad-hoc estimate

  zCMB = zCMB_start ;
  DMU = 9999.0 ;
  while ( DMU > DMU_CONVERGE ) {
    mutmp  = dLmag(zCMB, zCMB, HzFUN_INFO, ANISOTROPY_INFO ); // MU for trial zCMB
    dmu    = mutmp - MU ;             // error on mu
    DMU    = fabs(dmu);
    zCMB  *= exp(-dmu/2.0); 

    NITER++ ;
    if ( NITER > 500 ) {
      sprintf(c1err,"Could not solve for zCMB after NITER=%d", NITER);
      sprintf(c2err,"MU=%f  dmu=%f  ztmp=%f", MU, dmu, zCMB);
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err);
    }
  } // end dz                                                                   

  int LDMP=0 ;
  if ( LDMP ) {
    printf(" xxx --------------------------------------------- \n");
    printf(" xxx MU=%.4f -> DL = %.2f Mpc  zCMB(start) = %.5f \n",
	   MU, DL, zCMB_start );
    printf(" xxx zCMB -> %.5f  DMU=%f after %d iterations \n",
	   zCMB, DMU, NITER);
  }

  return(zCMB);

} // end zcmb_dLmag_invert



// ************************************************
double zhelio_zcmb_translator (double z_input, double RA, double DEC, 
			       char *coordSys, int OPT ) {

  /**********************

   Created Dec 2013 by R.Kessler
   General redshift-translator function to between helio and cmb frame.
   [replaces Z2CMB that translated only in 1 direction]

   OPT > 0  -> z_input = z_helio, return z_out = zcmb
   OPT < 0  -> z_input = z_cmb,   return z_out = zhelio
   RA,DEC   = sky coordinates
   coordSys = coordinate system; e.g., 'eq' or 'gal' or 'J2000'


   l = longitude = RA (deg)
   b = lattitue  = DEC  (deg)

   Use exact formuala,
   
    1 + z_cmb = ( 1 + z_helio ) / ( 1 - V0 . nhat/c )

   where V0 is the CMB velocity vector, 
   nhat is the unit vector between us and the SN,
   and c = 3E5 km/s.

   Note that the NED-calculator used in JRK07 is 
   an approximation, z_cmb = z_helio + V0.nhat/c,
   that is OK when z_helio << 1.

 ****************/

  double  ra_gal, dec_gal, ss, ccc, c1, c2, c3, vdotn, z_out  ;
  char fnam[] = "zhelio_zcmb_translator" ;

  // --------------- BEGIN ------------

  // on negative redshift, just return input redshift with
  // no calculation. Allows flags such as -9 to be unperturbed.
  if ( z_input < 1.0E-10 ) { return z_input ; }

  if ( strcmp(coordSys,"eq"   ) == 0 || 
       strcmp(coordSys,"J2000") == 0 ) {

    // input and output in degrees
    slaEqgal( RA, DEC, &ra_gal, &dec_gal ) ;
  }
  else if ( strcmp(coordSys,"gal") == 0 ) {
    ra_gal  = RA ;
    dec_gal = DEC ;
  }
  else {
    sprintf(c1err,"Invalid coordSys = '%s' ", coordSys );
    sprintf(c2err,"OPT=%d z_in=%f RA=%f DEC=%f", OPT, z_input, RA, DEC);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err );
  }

  // get projection

  ss  = sin(RADIAN*dec_gal) * sin(RADIAN*CMBapex_b);
  c1  = cos(RADIAN*dec_gal) ;
  c2  = cos(RADIAN*CMBapex_b) ;
  c3  = cos(RADIAN*(ra_gal-CMBapex_l));
  ccc = c1 * c2 * c3 ;
  vdotn = CMBapex_v * ( ss + ccc ) / LIGHT_km ;

  z_out = -9.0 ;

  if ( OPT > 0 ) {
    z_out  = ( 1. + z_input ) / ( 1. - vdotn ) - 1. ; 
  }
  else if ( OPT < 0 )  {
    z_out  = ( 1. + z_input) * ( 1. - vdotn ) - 1.0 ;  
  }
  else if ( OPT == 0 ) {
    sprintf(c1err,"Invalid OPT=0" );
    sprintf(c2err,"z_input=%f  RA=%f  DEC=%f", z_input,RA,DEC);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);
  }
   
  return(z_out) ;

} // end of zhelio_zcmb_translator 

double zhelio_zcmb_translator__ (double *z_input, double *RA, double *DEC,
                                 char *coordSys, int *OPT ) {
  return zhelio_zcmb_translator(*z_input, *RA, *DEC, coordSys, *OPT) ;
}


// end:

