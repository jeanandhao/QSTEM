/*
  QSTEM - image simulation for TEM/STEM/CBED
  Copyright (C) 2000-2010  Christoph Koch
  Copyright (C) 2010-2013  Christoph Koch, Michael Sarahan
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "crystal.hpp"
#include <boost/array.hpp>


CCrystal::CCrystal(std::string &structureFilename)
{
  m_Mm = boost::Array();
}

// This function reads the atomic positions from fileName and also adds 
// Thermal displacements to their positions, if muls.tds is turned on.
void CCrystal::ReadUnitCell(unsigned &natom, char *fileName, int handleVacancies) 
{
  int printFlag = 1;
  // char buf[NCMAX], *str,element[16];
  // FILE *fp;
  // float_t alpha,beta,gamma;
  int ncoord=0,ncx,ncy,ncz,icx,icy,icz,jz;
  // float_t dw,occ,dx,dy,dz,r;
  int i,i2,j,format=FORMAT_UNKNOWN,ix,iy,iz,atomKinds=0;
  // char s1[16],s2[16],s3[16];
  double boxXmin=0,boxXmax=0,boxYmin=0,boxYmax=0,boxZmin=0,boxZmax=0;
  double boxCenterX,boxCenterY,boxCenterZ,boxCenterXrot,boxCenterYrot,boxCenterZrot,bcX,bcY,bcZ;
  double x,y,z,totOcc;
  double choice,lastOcc;
  double *u = NULL;
  double **Mm = NULL;
  static int ncoord_old = 0;

  printFlag = muls->printLevel;

  /*
  if (Mm == NULL) {
    Mm = double2D(3,3,"Mm");
    memset(Mm[0],0,9*sizeof(double));
    muls->Mm = Mm;
    u = (double *)malloc(3*sizeof(double));
  }
  */

  StructureReaderPtr reader = GetStructureReader(fileName);
  ncoord = reader->GetNCoord();
  reader->ReadCellParams(m_Mm);
  
  CalculateCellDimensions();

  if (ncoord == 0) {
    printf("Error reading configuration file %s - ncoord =0\n",fileName);
    return NULL;
  }

  ncx = muls->nCellX;
  ncy = muls->nCellY;
  ncz = muls->nCellZ;

  if (printFlag) {
    printf("Lattice parameters: ax=%g by=%g cz=%g (%d atoms)\n",
           muls->ax,muls->by,muls->c,ncoord);
    
    if ((muls->cubex == 0) || (muls->cubey == 0) || (muls->cubez == 0))	 
      printf("Size of Super-lattice: ax=%g by=%g cz=%g (%d x %d x %d)\n",
             muls->ax*ncx,muls->by*ncy,muls->c*ncz,ncx,ncy,ncz);
    else
      printf("Size of Cube: ax=%g by=%g cz=%g\n",
             muls->cubex,muls->cubey,muls->cubez);
  }
  /************************************************************
   * now that we know how many coordinates there are
   * allocate the arrays 
   ************************************************************/

  natom = ncoord*ncx*ncy*ncz;
  atoms.resize(natom);

  atomKinds = 0;
  
  /***********************************************************
   * Read actual Data
   ***********************************************************/
  for(jz=0,i=ncoord-1; i>=0; i--) {
    
    switch (format) {
    case FORMAT_CFG: 
      if (readNextCFGAtom(&atoms[i],0,fileName) < 0) {
        printf("number of atoms does not agree with atoms in file!\n");
        return NULL;
      }
      break;
    case FORMAT_DAT: 
      
      if (readNextDATAtom(&atoms[i],0,fileName) < 0) {
        printf("number of atoms does not agree with atoms in file!\n");
        return NULL;
      }
      break;

    case FORMAT_CSSR:
      readNextCSSRAtom(&atoms[i],0,fileName);
      break;
    default: return NULL;
    }

    if((atoms[i].Znum < 1 ) || (atoms[i].Znum > NZMAX)) {
      /* for (j=ncoord-1;j>=i;j--)
         printf("%2d: %d (%g,%g,%g)\n",j,atoms[j].Znum,atoms[j].x,
         atoms[j].y,atoms[j].z);
      */
      printf("Error: bad atomic number %d in file %s (atom %d [%d: %g %g %g])\n",
             atoms[i].Znum,fileName,i,atoms[i].Znum,atoms[i].x,atoms[i].y,atoms[i].z);
      return;
    }

    // Keep a record of the kinds of atoms we are reading
    for (jz=0;jz<atomKinds;jz++)	if (muls->Znums[jz] == atoms[i].Znum) break;
    // allocate more memory, if there is a new element
    if (jz == atomKinds) {
      atomKinds++;
      if (atomKinds > muls->atomKinds) {
        muls->Znums = (int *)realloc(muls->Znums,atomKinds*sizeof(int));
        muls->atomKinds = atomKinds;
        // printf("%d kinds (%d)\n",atomKinds,atoms[i].Znum);
      }  
      muls->Znums[jz] = atoms[i].Znum;
    }

    
  } // for 1=ncoord-1:-1:0  - we've just read all the atoms.
  if (muls->tds) {
    if (muls->u2 == NULL) {
      // printf("AtomKinds: %d\n",muls->atomKinds);
      muls->u2 = (double *)malloc(atomKinds*sizeof(double));
      memset(muls->u2,0,atomKinds*sizeof(double));
    }
    if (muls->u2avg == NULL) {
      muls->u2avg = (double *)malloc(muls->atomKinds*sizeof(double));
      memset(muls->u2avg,0,muls->atomKinds*sizeof(double));
    }
  }

  ////////////////////////////////////////////////////////////////
  // Close the file for further reading, and restore file pointer 
  switch (format) {
  case FORMAT_CFG: 
    readNextCFGAtom(NULL,-1,NULL);
    break;
  case FORMAT_DAT: 
    readNextDATAtom(NULL,-1,NULL);
    break;
  case FORMAT_CSSR:
    readNextCSSRAtom(NULL,-1,NULL);
    break;
  }

  // First, we will sort the atoms by position:
  if (handleVacancies) {
    qsort(&atoms[0],ncoord,sizeof(atom),atomCompareZYX);
  }


  /////////////////////////////////////////////////////////////////
  // Compute the phonon displacement and remove atoms which appear 
  // twice or have some probability to be vacancies:
  if ((muls->cubex > 0) && (muls->cubey > 0) && (muls->cubez > 0)) {
    /* at this point the atoms should have fractional coordinates */
    // printf("Entering tiltBoxed\n");
    tiltBoxed(ncoord,natom,muls,atoms,handleVacancies);
    // printf("ncoord: %d, natom: %d\n",ncoord,*natom);
  }
  else {  // work in NCell mode
    // atoms are in fractional coordinates so far, we need to convert them to 
    // add the phonon displacement in this condition, because there we can 
    // actually do the correct Eigenmode treatment.
    // but we will probably just do Einstein vibrations anyway:
    replicateUnitCell(ncoord,natom,muls,atoms,handleVacancies);
    /**************************************************************
     * now, after we read all of the important coefficients, we
     * need to decide if this is workable
     **************************************************************/
    natom = ncoord*ncx*ncy*ncz;
    if (1) { // ((Mm[0][0]*Mm[1][1]*Mm[2][2] == 0) || (Mm[0][1]!=0)|| (Mm[0][2]!=0)|| (Mm[1][0]!=0)|| (Mm[1][2]!=0)|| (Mm[2][0]!=0)|| (Mm[2][1]!=0)) {
      // printf("Lattice is not orthogonal, or rotated\n");
      for(i=0;i<natom;i++) {
        /*
          x = Mm[0][0]*atoms[i].x+Mm[0][1]*atoms[i].y+Mm[0][2]*atoms[i].z;
          y = Mm[1][0]*atoms[i].x+Mm[1][1]*atoms[i].y+Mm[1][2]*atoms[i].z;
          z = Mm[2][0]*atoms[i].x+Mm[2][1]*atoms[i].y+Mm[2][2]*atoms[i].z;
        */
        
        // This converts also to cartesian coordinates
        x = Mm[0][0]*atoms[i].x+Mm[1][0]*atoms[i].y+Mm[2][0]*atoms[i].z;
        y = Mm[0][1]*atoms[i].x+Mm[1][1]*atoms[i].y+Mm[2][1]*atoms[i].z;
        z = Mm[0][2]*atoms[i].x+Mm[1][2]*atoms[i].y+Mm[2][2]*atoms[i].z;

        atoms[i].x = x;
        atoms[i].y = y;
        atoms[i].z = z;
        
      }      
    }
    /**************************************************************
     * Converting to cartesian coordinates
     *************************************************************/
    else { 
      for(i=0;i<natom;i++) {
        atoms[i].x *= muls->ax; 
        atoms[i].y *= muls->by; 
        atoms[i].z *= muls->c;
      }		 
    }
    // Now we have all the cartesian coordinates of all the atoms!
    muls->ax *= ncx;
    muls->by *= ncy;
    muls->c  *= ncz;

    /***************************************************************
     * Now let us tilt around the center of the full crystal
     */   
    
    bcX = ncx/2.0;
    bcY = ncy/2.0;
    bcZ = ncz/2.0;
    u[0] = Mm[0][0]*bcX+Mm[1][0]*bcY+Mm[2][0]*bcZ;
    u[1] = Mm[0][1]*bcX+Mm[1][1]*bcY+Mm[2][1]*bcZ;
    u[2] = Mm[0][2]*bcX+Mm[1][2]*bcY+Mm[2][2]*bcZ;
    boxCenterX = u[0];
    boxCenterY = u[1];
    boxCenterZ = u[2];
		
    // rotateVect(u,u,muls->ctiltx,muls->ctilty,muls->ctiltz);  // simply applies rotation matrix
    // boxCenterXrot = u[0]; boxCenterYrot = u[1];	boxCenterZrot = u[2];
		
    // Determine the size of the (rotated) super cell
    for (icx=0;icx<=ncx;icx+=ncx) for (icy=0;icy<=ncy;icy+=ncy) for (icz=0;icz<=ncz;icz+=ncz) {
          u[0] = Mm[0][0]*(icx-bcX)+Mm[1][0]*(icy-bcY)+Mm[2][0]*(icz-bcZ);
          u[1] = Mm[0][1]*(icx-bcX)+Mm[1][1]*(icy-bcY)+Mm[2][1]*(icz-bcZ);
          u[2] = Mm[0][2]*(icx-bcX)+Mm[1][2]*(icy-bcY)+Mm[2][2]*(icz-bcZ);
          rotateVect(u,u,muls->ctiltx,muls->ctilty,muls->ctiltz);  // simply applies rotation matrix
          // x = u[0]+boxCenterXrot; y = u[1]+boxCenterYrot; z = u[2]+boxCenterZrot;
          x = u[0]+boxCenterX; y = u[1]+boxCenterY; z = u[2]+boxCenterZ;
          if ((icx == 0) && (icy == 0) && (icz == 0)) {
            boxXmin = boxXmax = x;
            boxYmin = boxYmax = y;
            boxZmin = boxZmax = z;
          }
          else {
            boxXmin = boxXmin>x ? x : boxXmin; boxXmax = boxXmax<x ? x : boxXmax; 
            boxYmin = boxYmin>y ? y : boxYmin; boxYmax = boxYmax<y ? y : boxYmax; 
            boxZmin = boxZmin>z ? z : boxZmin; boxZmax = boxZmax<z ? z : boxZmax; 
          }
        }

    // printf("(%f, %f, %f): %f .. %f, %f .. %f, %f .. %f\n",muls->ax,muls->by,muls->c,boxXmin,boxXmax,boxYmin,boxYmax,boxZmin,boxZmax);

    if ((muls->ctiltx != 0) || (muls->ctilty != 0) || (muls->ctiltz != 0)) {			
      for(i=0;i<(natom);i++) {
        u[0] = atoms[i].x-boxCenterX; 
        u[1] = atoms[i].y-boxCenterY; 
        u[2] = atoms[i].z-boxCenterZ; 
        rotateVect(u,u,muls->ctiltx,muls->ctilty,muls->ctiltz);  // simply applies rotation matrix
        u[0] += boxCenterX;
        u[1] += boxCenterY; 
        u[2] += boxCenterZ; 
        atoms[i].x = u[0];
        atoms[i].y = u[1]; 
        atoms[i].z = u[2]; 
        // boxXmin = boxXmin>u[0] ? u[0] : boxXmin; boxXmax = boxXmax<u[0] ? u[0] : boxXmax; 
        // boxYmin = boxYmin>u[1] ? u[1] : boxYmin; boxYmax = boxYmax<u[1] ? u[1] : boxYmax; 
        // boxZmin = boxZmin>u[2] ? u[2] : boxZmin; boxZmax = boxZmax<u[2] ? u[2] : boxZmax; 
      }
    } /* if tilts != 0 ... */
    
    for(i=0;i<(natom);i++) {
      atoms[i].x-=boxXmin; 
      atoms[i].y-=boxYmin; 
      atoms[i].z-=boxZmin; 
    }
    muls->ax = boxXmax-boxXmin;
    muls->by = boxYmax-boxYmin;
    muls->c  = boxZmax-boxZmin;
    
    // printf("(%f, %f, %f): %f .. %f, %f .. %f, %f .. %f\n",muls->ax,muls->by,muls->c,boxXmin,boxXmax,boxYmin,boxYmax,boxZmin,boxZmax);
    /*******************************************************************
     * If one of the tilts was more than 30 degrees, we will re-assign 
     * the lattice constants ax, by, and c by boxing the sample with a box 
     ******************************************************************/

    // Offset the atoms in x- and y-directions:
    // Do this after the rotation!
    if ((muls->xOffset != 0) || (muls->yOffset != 0)) {
      for(i=0;i<natom;i++) {
        atoms[i].x += muls->xOffset; 
        atoms[i].y += muls->yOffset; 
      }		 
    }
  } // end of Ncell mode conversion to cartesian coords and tilting.
} // end of readUnitCell


// Uses m_Mm to calculate ax, by, cz, and alpha, beta, gamma
void CCrystal::CalculateCellDimensions()
{
  m_ax = sqrt(m_Mm[0][0]*m_Mm[0][0]+m_Mm[0][1]*m_Mm[0][1]+m_Mm[0][2]*m_Mm[0][2]);
  m_by = sqrt(m_Mm[1][0]*m_Mm[1][0]+m_Mm[1][1]*m_Mm[1][1]+m_Mm[1][2]*m_Mm[1][2]);
  m_cz = sqrt(m_Mm[2][0]*m_Mm[2][0]+m_Mm[2][1]*m_Mm[2][1]+m_Mm[2][2]*m_Mm[2][2]);
  m_cGamma = atan2(m_Mm[1][1],m_Mm[1][0]);
  m_cBeta = acos(m_Mm[2][0]/m_cz);
  m_cAlpha = acos(m_Mm[2][1]*sin(m_cGamma)/m_c+cos(m_cBeta)*cos(m_cGamma));
  m_cGamma /= (float)PI180;
  m_cBeta  /= (float)PI180;
  m_cAlpha /= (float)PI180;
}

// TODO: old version return a pointer to new atom positions.  Can we do this in place?
//		If not, just set atoms to the new vector.
void CCrystal::TiltBoxed(int ncoord,int &natom, std::vector<atom> &atoms,int handleVacancies) {
	int atomKinds = 0;
	int iatom,jVac,jequal,jChoice,i2,ix,iy,iz,atomCount = 0,atomSize;
	static double *axCell,*byCell,*czCell=NULL;
	static double **Mm = NULL, **Mminv = NULL, **MpRed = NULL, **MpRedInv = NULL;
	static double **MbPrim = NULL, **MbPrimInv = NULL, **MmOrig = NULL,**MmOrigInv=NULL;
	static double **a = NULL,**aOrig = NULL,**b= NULL,**bfloor=NULL,**blat=NULL;
	static double *uf;
	static int oldAtomSize = 0;
	double x,y,z,dx,dy,dz; 
	double totOcc,lastOcc,choice;
	std::vector<atom> unitAtoms, newAtom;
	int nxmin,nxmax,nymin,nymax,n zmin,nzmax,jz;
	int Ncells;
	static double *u;
	static long idum = -1;


	// if (iseed == 0) iseed = -(long) time( NULL );
	Ncells = m_nCellX * m_nCellY * m_nCellZ;

	/* calculate maximum length in supercell box, which is naturally 
	* the room diagonal:

	maxLength = sqrt(m_cubex*m_cubex+
	m_cubey*m_cubey+
	m_cubez*m_cubez);
	*/

	if (Mm == NULL) {
		MmOrig		= double2D(3,3,"MmOrig");
		MmOrigInv	= double2D(3,3,"MmOrigInv");
		MbPrim		= double2D(3,3,"MbPrim");	// double version of primitive lattice basis 
		MbPrimInv	= double2D(3,3,"MbPrim"); // double version of inverse primitive lattice basis 
		MpRed		= double2D(3,3,"MpRed");    /* conversion lattice to obtain red. prim. coords 
												* from reduced cubic rect.
												*/
		MpRedInv	= double2D(3,3,"MpRedInv");    /* conversion lattice to obtain red. cub. coords 
												   * from reduced primitive lattice coords
												   */
		Mm			= double2D(3,3,"Mm");
		Mminv		= double2D(3,3,"Mminv");
		axCell = Mm[0]; byCell = Mm[1]; czCell = Mm[2];
		a			= double2D(1,3,"a");
		aOrig		= double2D(1,3,"aOrig");
		b			= double2D(1,3,"b");
		bfloor		= double2D(1,3,"bfloor");
		blat		= double2D(1,3,"blat");
		uf			= (double *)malloc(3*sizeof(double));
		u			= (double *)malloc(3*sizeof(double));
	}


	dx = 0; dy = 0; dz = 0;
	dx = m_xOffset;
	dy = m_yOffset;
	/* find the rotated unit cell vectors .. 
	* muls does still hold the single unit cell vectors in ax,by, and c
	*/
	// makeCellVectMuls(muls, axCell, byCell, czCell);
	// We need to copy the transpose of m_Mm to Mm.
	// we therefore cannot use the following command:
	// memcpy(Mm[0],m_Mm[0],3*3*sizeof(double));
	for (ix=0;ix<3;ix++) for (iy=0;iy<3;iy++) Mm[ix][iy]=m_Mm[iy][ix];

	memcpy(MmOrig[0],Mm[0],3*3*sizeof(double));
	inverse_3x3(MmOrigInv[0],MmOrig[0]);
	/* remember that the angles are in rad: */
	rotateMatrix(Mm[0],Mm[0],m_ctiltx,m_ctilty,m_ctiltz);
	/*
	// This is wrong, because it implements Mrot*(Mm'):
	rotateVect(axCell,axCell,m_ctiltx,m_ctilty,m_ctiltz);
	rotateVect(byCell,byCell,m_ctiltx,m_ctilty,m_ctiltz);
	rotateVect(czCell,czCell,m_ctiltx,m_ctilty,m_ctiltz);
	*/
	inverse_3x3(Mminv[0],Mm[0]);  // computes Mminv from Mm!
	/* find out how far we will have to go in unit of unit cell vectors.
	* when creating the supercell by checking the number of unit cell vectors 
	* necessary to reach every corner of the supercell box.
	*/
	// showMatrix(MmOrig,3,3,"Morig");
	// printf("%d %d\n",(int)Mm, (int)MmOrig);
	memset(a[0],0,3*sizeof(double));
	// matrixProduct(a,1,3,Mminv,3,3,b);
	matrixProduct(Mminv,3,3,a,3,1,b);
	// showMatrix(Mm,3,3,"M");
	// showMatrix(Mminv,3,3,"M");
	nxmin = nxmax = (int)floor(b[0][0]-dx); 
	nymin = nymax = (int)floor(b[0][1]-dy); 
	nzmin = nzmax = (int)floor(b[0][2]-dz);
	for (ix=0;ix<=1;ix++) for (iy=0;iy<=1;iy++)	for (iz=0;iz<=1;iz++) {
		a[0][0]=ix*m_cubex-dx; a[0][1]=iy*m_cubey-dy; a[0][2]=iz*m_cubez-dz;

		// matrixProduct(a,1,3,Mminv,3,3,b);
		matrixProduct(Mminv,3,3,a,3,1,b);

		// showMatrix(b,1,3,"b");
		if (nxmin > (int)floor(b[0][0])) nxmin=(int)floor(b[0][0]);
		if (nxmax < (int)ceil( b[0][0])) nxmax=(int)ceil( b[0][0]);
		if (nymin > (int)floor(b[0][1])) nymin=(int)floor(b[0][1]);
		if (nymax < (int)ceil( b[0][1])) nymax=(int)ceil( b[0][1]);
		if (nzmin > (int)floor(b[0][2])) nzmin=(int)floor(b[0][2]);
		if (nzmax < (int)ceil( b[0][2])) nzmax=(int)ceil( b[0][2]);	  
	}

	// nxmin--;nxmax++;nymin--;nymax++;nzmin--;nzmax++;
        
	unitAtoms.resize(ncoord);
	memcpy(&unitAtoms[0],&atoms[0],ncoord*sizeof(atom));
	atomSize = (1+(nxmax-nxmin)*(nymax-nymin)*(nzmax-nzmin)*ncoord);
	if (atomSize != oldAtomSize) {
          atoms.resize(atomSize);
          oldAtomSize = atomSize;
	}
	// showMatrix(Mm,3,3,"Mm");
	// showMatrix(Mminv,3,3,"Mminv");
	// printf("Range: (%d..%d, %d..%d, %d..%d)\n",
	// nxmin,nxmax,nymin,nymax,nzmin,nzmax);

	atomCount = 0;  
	jVac = 0;
	memset(u,0,3*sizeof(double));
	for (iatom=0;iatom<ncoord;) {
		// printf("%d: (%g %g %g) %d\n",iatom,unitAtoms[iatom].x,unitAtoms[iatom].y,
		//   unitAtoms[iatom].z,unitAtoms[iatom].Znum);
		memcpy(&newAtom,&unitAtoms[iatom],sizeof(atom));
		for (jz=0;jz<m_atomKinds;jz++)	if (m_Znums[jz] == newAtom.Znum) break;
		// allocate more memory, if there is a new element
		/*
		if (jz == atomKinds) {
			atomKinds++;
			if (atomKinds > m_atomKinds) {
				m_Znums = (int *)realloc(m_Znums,atomKinds*sizeof(int));
				m_atomKinds = atomKinds;
				// printf("%d kinds (%d)\n",atomKinds,atoms[i].Znum);
			}  
			m_Znums[jz] = newAtom.Znum;
		}
		*/
		/////////////////////////////////////////////////////
		// look for atoms at equal position
		if ((handleVacancies) && (newAtom.Znum > 0)) {
			totOcc = newAtom.occ;
			for (jequal=iatom+1;jequal<ncoord;jequal++) {
				// if there is anothe ratom that comes close to within 0.1*sqrt(3) A we will increase 
				// the total occupany and the counter jequal.
				if ((fabs(newAtom.x-unitAtoms[jequal].x) < 1e-6) && (fabs(newAtom.y-unitAtoms[jequal].y) < 1e-6) && (fabs(newAtom.z-unitAtoms[jequal].z) < 1e-6)) {
					totOcc += unitAtoms[jequal].occ;
				}
				else break;
			} // jequal-loop
		}
		else {
			jequal = iatom+1;
			totOcc = 1;
		}



		// printf("%d: %d\n",atomCount,jz);
		for (ix=nxmin;ix<=nxmax;ix++) {
			for (iy=nymin;iy<=nymax;iy++) {
				for (iz=nzmin;iz<=nzmax;iz++) {
					// atom position in cubic reduced coordinates: 
					aOrig[0][0] = ix+newAtom.x; aOrig[0][1] = iy+newAtom.y; aOrig[0][2] = iz+newAtom.z;

					// Now is the time to remove atoms that are on the same position or could be vacancies:
					// if we encountered atoms in the same position, or the occupancy of the current atom is not 1, then
					// do something about it:
					// All we need to decide is whether to include the atom at all (if totOcc < 1
					// of which of the atoms at equal positions to include
					jChoice = iatom;  // This will be the atom we wil use.
					if ((totOcc < 1) || (jequal > iatom+1)) { // found atoms at equal positions or an occupancy less than 1!
						// ran1 returns a uniform random deviate between 0.0 and 1.0 exclusive of the endpoint values. 
						// 
						// if the total occupancy is less than 1 -> make sure we keep this
						// if the total occupancy is greater than 1 (unphysical) -> rescale all partial occupancies!
						if (totOcc < 1.0) choice = ran1(&idum);   
						else choice = totOcc*ran1(&idum);
						// printf("Choice: %g %g %d, %d %d\n",totOcc,choice,j,i,jequal);
						lastOcc = 0;
						for (i2=iatom;i2<jequal;i2++) {
							// atoms[atomCount].Znum = unitAtoms[i2].Znum; 
							// if choice does not match the current atom:
							// choice will never be 0 or 1(*totOcc) 
							if ((choice <lastOcc) || (choice >=lastOcc+unitAtoms[i2].occ)) {
								// printf("Removing atom %d, Z=%d\n",jCell+i2,atoms[jCell+i2].Znum);
								// atoms[atomCount].Znum =  0;  // vacancy
								jVac++;
							}
							else {
								jChoice = i2;
							}
							lastOcc += unitAtoms[i2].occ;
						}
						// printf("Keeping atom %d (%d), Z=%d\n",jChoice,iatom,unitAtoms[jChoice].Znum);
					}
					// if (jChoice != iatom) memcpy(&newAtom,unitAtoms+jChoice,sizeof(atom));
					if (jChoice != iatom) {
						for (jz=0;jz<m_atomKinds;jz++)	if (m_Znums[jz] == unitAtoms[jChoice].Znum) break;
					}




					// here we need to call phononDisplacement:
					// phononDisplacement(u,muls,iatom,ix,iy,iz,atomCount,atoms[i].dw,*natom,atoms[i].Znum);
					if (m_Einstein == 1) {
						// phononDisplacement(u,muls,iatom,ix,iy,iz,1,newAtom.dw,10,newAtom.Znum);
						if (m_tds) {
							phononDisplacement(u,muls,jChoice,ix,iy,iz,1,unitAtoms[jChoice].dw,atomSize,jz);
							a[0][0] = aOrig[0][0]+u[0]; a[0][1] = aOrig[0][1]+u[1]; a[0][2] = aOrig[0][2]+u[2];
						}
						else {
							a[0][0] = aOrig[0][0]; a[0][1] = aOrig[0][1]; a[0][2] = aOrig[0][2];
						}
					}
					else {
						printf("Cannot handle phonon-distribution mode for boxed sample yet - sorry!!\n");
						exit(0);
					}
					// matrixProduct(aOrig,1,3,Mm,3,3,b);
					matrixProduct(Mm,3,3,aOrig,3,1,b);

					// if (atomCount < 2) {showMatrix(a,1,3,"a");showMatrix(b,1,3,"b");}
					// b now contains atom positions in cartesian coordinates */
					x  = b[0][0]+dx; 
					y  = b[0][1]+dy; 
					z  = b[0][2]+dz; 
					if ((x >= 0) && (x <= m_cubex) &&
						(y >= 0) && (y <= m_cubey) &&
						(z >= 0) && (z <= m_cubez)) {
							// matrixProduct(a,1,3,Mm,3,3,b);
							matrixProduct(Mm,3,3,a,3,1,b);
							atoms[atomCount].x		= b[0][0]+dx; 
							atoms[atomCount].y		= b[0][1]+dy; 
							atoms[atomCount].z		= b[0][2]+dz; 
							atoms[atomCount].dw		= unitAtoms[jChoice].dw;
							atoms[atomCount].occ	= unitAtoms[jChoice].occ;
							atoms[atomCount].q		= unitAtoms[jChoice].q;
							atoms[atomCount].Znum	= unitAtoms[jChoice].Znum;
								
							atomCount++;	
							/*
							if (unitAtoms[jChoice].Znum > 22)
								printf("Atomcount: %d, Z = %d\n",atomCount,unitAtoms[jChoice].Znum);
							*/
					}

				} /* iz ... */
			} /* iy ... */
		} /* ix ... */
		iatom = jequal;
	} /* iatom ... */
	if (m_printLevel > 2) printf("Removed %d atoms because of multiple occupancy or occupancy < 1\n",jVac);
	m_ax = m_cubex;
	m_by = m_cubey;
	m_c  = m_cubez;
	*natom = atomCount;
	// call phononDisplacement again to update displacement data:
	phononDisplacement(u,muls,iatom,ix,iy,iz,0,newAtom.dw,*natom,jz);


	free(unitAtoms);
	return atoms;
}  // end of 'tiltBoxed(...)'


////////////////////////////////////////////////////////////////////////
// replicateUnitCell
// 
// Replicates the unit cell NcellX x NCellY x NCellZ times
// applies phonon displacement and removes vacancies and atoms appearing
// on same position:
// ncoord is the number of atom positions that has already been read.
// memory for the whole atom-array of size natom has already been allocated
// but the sites beyond natom are still empty.
void CCrystal::ReplicateUnitCell(int ncoord,int *natom,MULS *muls,atom* atoms,int handleVacancies) {
	int i,j,i2,jChoice,ncx,ncy,ncz,icx,icy,icz,jz,jCell,jequal,jVac;
	int 	atomKinds = 0;
	double totOcc;
	double choice,lastOcc;
	double *u;
	// seed for random number generation
	static long idum = -1;

	ncx = m_nCellX;
	ncy = m_nCellY;
	ncz = m_nCellZ;
	u = (double *)malloc(3*sizeof(double));

	atomKinds = m_atomKinds;
	//////////////////////////////////////////////////////////////////////////////
	// Look for atoms which share the same position:
	jVac = 0;  // no atoms have been removed yet
	for (i=ncoord-1;i>=0;) {

		////////////////
		if ((handleVacancies) && (atoms[i].Znum > 0)) {
			totOcc = atoms[i].occ;
			for (jequal=i-1;jequal>=0;jequal--) {
				// if there is anothe ratom that comes close to within 0.1*sqrt(3) A we will increase 
				// the total occupany and the counter jequal.
				if ((fabs(atoms[i].x-atoms[jequal].x) < 1e-6) && (fabs(atoms[i].y-atoms[jequal].y) < 1e-6) && (fabs(atoms[i].z-atoms[jequal].z) < 1e-6)) {
					totOcc += atoms[jequal].occ;
				}
				else break;
			} // jequal-loop
		}
		else {
			jequal = i-1;
			totOcc = 1;
			// Keep a record of the kinds of atoms we are reading
		}
		if (jequal == i-1) {
			for (jz=0;jz<atomKinds;jz++)	if (m_Znums[jz] == atoms[i].Znum) break;
		}

		////////////////
		memset(u,0,3*sizeof(double));
		/* replicate unit cell ncx,y,z times: */
		/* We have to start with the last atoms first, because once we added the displacements 
		* to the original unit cell (icx=icy=icz=0), we cannot use those positions			
		* as unit cell coordinates for the other atoms anymore
		*/
		// printf("Will start phonon displacement (%f)\n",m_tds,m_temperature);
		// for (jz=0;jz<m_atomKinds;jz++)	if (atoms[i].Znum == m_Znums[jz]) break;

		for (icx=ncx-1;icx>=0;icx--) {
			for (icy=ncy-1;icy>=0;icy--) {
				for (icz=ncz-1;icz>=0;icz--) {
					jCell = (icz+icy*ncz+icx*ncy*ncz)*ncoord;
					j = jCell+i;
					/* We will also add the phonon displacement to the atomic positions now: */
					atoms[j].dw = atoms[i].dw;
					atoms[j].occ = atoms[i].occ;
					atoms[j].q = atoms[i].q;
					atoms[j].Znum = atoms[i].Znum; 

					// Now is the time to remove atoms that are on the same position or could be vacancies:
					// if we encountered atoms in the same position, or the occupancy of the current atom is not 1, then
					// do something about it:
					jChoice = i;
					if ((totOcc < 1) || (jequal < i-1)) { // found atoms at equal positions or an occupancy less than 1!
						// ran1 returns a uniform random deviate between 0.0 and 1.0 exclusive of the endpoint values. 
						// 
						// if the total occupancy is less than 1 -> make sure we keep this
						// if the total occupancy is greater than 1 (unphysical) -> rescale all partial occupancies!
						if (totOcc < 1.0) choice = ran1(&idum);   
						else choice = totOcc*ran1(&idum);
						// printf("Choice: %g %g %d, %d %d\n",totOcc,choice,j,i,jequal);
						lastOcc = 0;
						for (i2=i;i2>jequal;i2--) {
							atoms[jCell+i2].dw = atoms[i2].dw;
							atoms[jCell+i2].occ = atoms[i2].occ;
							atoms[jCell+i2].q = atoms[i2].q;
							atoms[jCell+i2].Znum = atoms[i2].Znum; 

							// if choice does not match the current atom:
							// choice will never be 0 or 1(*totOcc) 
							if ((choice <lastOcc) || (choice >=lastOcc+atoms[i2].occ)) {
								// printf("Removing atom %d, Z=%d\n",jCell+i2,atoms[jCell+i2].Znum);
								atoms[jCell+i2].Znum =  0;  // vacancy
								jVac++;
							}
							else {
								jChoice = i2;
							}
							lastOcc += atoms[i2].occ;
						}

						// Keep a record of the kinds of atoms we are reading
						for (jz=0;jz<atomKinds;jz++) {
							if (m_Znums[jz] == atoms[jChoice].Znum) break;
						}
					}
					// printf("i2=%d, %d (%d) [%g %g %g]\n",i2,jequal,jz,atoms[jequal].x,atoms[jequal].y,atoms[jequal].z);

					// this function does nothing, if m_tds == 0
					// if (j % 5 == 0) printf("atomKinds: %d (jz = %d, %d)\n",atomKinds,jz,atoms[jChoice].Znum);
					phononDisplacement(u,muls,jChoice,icx,icy,icz,j,atoms[jChoice].dw,*natom,jz);
					// printf("atomKinds: %d (jz = %d, %d)\n",atomKinds,jz,atoms[jChoice].Znum);

					for (i2=i;i2>jequal;i2--) {
						atoms[jCell+i2].x = atoms[i2].x+icx+u[0];
						atoms[jCell+i2].y = atoms[i2].y+icy+u[1];
						atoms[jCell+i2].z = atoms[i2].z+icz+u[2];
					}
				}  // for (icz=ncz-1;icz>=0;icz--)
			} // for (icy=ncy-1;icy>=0;icy--) 
		} // for (icx=ncx-1;icx>=0;icx--)
		i=jequal;
	} // for (i=ncoord-1;i>=0;)
	if ((jVac > 0 ) &&(m_printLevel)) printf("Removed %d atoms because of occupancies < 1 or multiple atoms in the same place\n",jVac);

}


/*******************************************************************************
* int phononDisplacement: 
* This function will calculate the phonon displacement for a given atom i of the
* unit cell, which has been replicated to the larger cell (icx,icy,icz)
* The phonon displacement is either defined by the phonon data file, or, 
* the Einstein model, if the appropriate flags in the muls struct are set
* The displacement will be given in fractional coordinates of a single unit cell.
*
* Input parameters:
* Einstein-mode:
* need only: dw, Znum, atomCount
* atomCount: give statistics report, if 0, important only for non-Einstein mode
* maxAtom: total number of atoms (will be called first, i.e. atomCount=maxAtoms-1:-1:0)
*
* Phonon-file mode:
* ...
*
********************************************************************************/ 
//  phononDisplacement(u,muls,jChoice,icx,icy,icz,j,atoms[jChoice].dw,*natom,jz);
//  j == atomCount
int CCrystal::PhononDisplacement(double *u,MULS *muls,int id,int icx,int icy,
	int icz,int atomCount,double dw,int maxAtom,int ZnumIndex) {
	int ix,iy,idd; // iz;
	static FILE *fpPhonon = NULL;
	static int Nk, Ns;        // number of k-vectors and atoms per primitive unit cell
	static float_tt *massPrim;   // masses for every atom in primitive basis
	static float_tt **omega;     // array of eigenvalues for every k-vector 
	static complex_tt ***eigVecs;  // array of eigenvectors for every k-vector
	static float_tt **kVecs;     // array for Nk 3-dim k-vectors
	static double **q1=NULL, **q2=NULL;
	int ik,lambda,icoord; // Ncells, nkomega;
	double kR,kRi,kRr,wobble;
	static double *u2=NULL,*u2T,ux=0,uy=0,uz=0; // u2Collect=0; // Ttotal=0;
	// static double uxCollect=0,uyCollect=0,uzCollect=0;
	static int *u2Count = NULL,*u2CountT,runCount = 1,u2Size = -1;
	static long iseed=0;
	static double **Mm=NULL,**MmInv=NULL;
	// static double **MmOrig=NULL,**MmOrigInv=NULL;
	static double *axCell,*byCell,*czCell,*uf,*b;
	static double wobScale = 0,sq3,scale=0;

	if (m_tds == 0) return 0;

	/***************************************************************************
	* We will give a statistics report, everytime, when we find atomCount == 0
	***************************************************************************/

	if (atomCount == 0) {
							   for (ix=0;ix<m_atomKinds;ix++) {
								   // u2Collect += u2[ix]/u2Count[ix];
								   // uxCollect += ux/maxAtom; uyCollect += uy/maxAtom; uzCollect += uz/maxAtom;
								   /*
								   printf("STATISTICS: sqrt(<u^2>): %g, CM: (%g %g %g) %d atoms, wob: %g\n"
								   "                         %g, CM: (%g %g %g) run %d\n",
								   sqrt(u2/u2Count),ux/u2Count,uy/u2Count,uz/u2Count,u2Count,scale*sqrt(dw*wobScale),
								   sqrt(u2Collect/runCount),uxCollect/runCount,uyCollect/runCount,uzCollect/runCount,
								   runCount);
								   */
								   // printf("Count: %d %g\n",u2Count[ix],u2[ix]);
								   u2[ix] /= u2Count[ix];
								   if (runCount > 0) 
									   m_u2avg[ix] = sqrt(((runCount-1)*(m_u2avg[ix]*m_u2avg[ix])+u2[ix])/runCount);
								   else
									   m_u2avg[ix] = sqrt(u2[ix]);

								   m_u2[ix]    = sqrt(u2[ix]);

								   u2[ix]=0; u2Count[ix]=0;
							   }
							   runCount++;
							   ux=0; uy=0; uz=0; 
	}
	if (Mm == NULL) {
									   // MmOrig = double2D(3,3,"MmOrig");
									   // MmOrigInv = double2D(3,3,"MmOrigInv");
									   Mm = double2D(3,3,"Mm");
									   MmInv = double2D(3,3,"Mminv");
									   uf = double1D(3,"uf");
		b = double1D(3,"uf");

									   axCell=Mm[0]; byCell=Mm[1]; czCell=Mm[2];
		// memcpy(Mm[0],m_Mm[0],3*3*sizeof(double));
		// We need to copy the transpose of m_Mm to Mm.
		// we therefore cannot use the following command:
		// memcpy(Mm[0],m_Mm[0],3*3*sizeof(double));
		// or Mm = m_Mm;
		for (ix=0;ix<3;ix++) for (iy=0;iy<3;iy++) Mm[ix][iy]=m_Mm[iy][ix];

		
									   // makeCellVectMuls(muls, axCell, byCell, czCell);
									   // memcpy(MmOrig[0],Mm[0],3*3*sizeof(double));
									   inverse_3x3(MmInv[0],Mm[0]);
	}
	if (ZnumIndex >= u2Size) {
							   
	   // printf("created phonon displacements %d!\n",ZnumIndex);								
	   u2 = (double *)realloc(u2,(ZnumIndex+1)*sizeof(double));
	   u2Count = (int *)realloc(u2Count,(ZnumIndex+1)*sizeof(int));
							   
		// printf("%d .... %d\n",ZnumIndex,u2Size);
	   if (u2Size < 1) {
		   for (ix=0;ix<=ZnumIndex;ix++) {
			   u2[ix] = 0;
			   u2Count[ix] = 0;
		   }
	   }
	   else {
		   for (ix=u2Size;ix<=ZnumIndex;ix++) {
			   u2[ix] = 0;
			   u2Count[ix] = 0;
		   }
	   }						  
		// printf("%d ..... %d\n",ZnumIndex,u2Size);

	   u2Size = ZnumIndex+1;
	}  


						   /***************************************************************************
						   * Thermal Diffuse Scattering according to accurate phonon-dispersion or 
						   * just Einstein model
						   *
						   * Information in the phonon file will be stored in binary form as follows:
						   * Nk (number of k-points: 32-bit integer)
						   * Ns (number of atomic species 32-bit integer)
						   * M_1 M_2 ... M_Ns  (32-bit floats)
						   * kx(1) ky(1) kz(1) (32-bit floats)
						   * w_1(1) q_11 q_21 ... q_(3*Ns)1    (32-bit floats)
						   * w_2(1) q_12 q_22 ... q_(3*Ns)2
						   * :
						   * w_(3*Ns)(1) q_1Ns q_2Ns ... q_(3*Ns)Ns
						   * kx(2) ky(2) kz(2) 
						   * :
						   * kx(Nk) ky(Nk) kz(Nk)
						   * :
						   * 
						   *
						   * Note: only k-vectors in half of the Brioullin zone must be given, since 
						   * w(k) = w(-k)  
						   * also: 2D arrays will be read slowly varying index = first index (i*m+j)
						   **************************************************************************/

						   if (wobScale == 0) {
							   wobScale = 1.0/(8*PID*PID);   
		sq3 = 1.0/sqrt(3.0);  /* sq3 is an additional needed factor which stems from
												   * int_-infty^infty exp(-x^2/2) x^2 dx = sqrt(pi)
												   * introduced in order to match the wobble factor with <u^2>
												   */
							   scale = (float) sqrt(m_tds_temp/300.0) ;
							   iseed = -(long)(time(NULL));
						   }


						   if ((m_Einstein == 0) && (fpPhonon == NULL)) {
                                                     if ((fpPhonon = fopen(m_phononFile.c_str(),"r")) == NULL) {
								   printf("Cannot find phonon mode file, will use random displacements!");
								   m_Einstein = 1;
								   //m_phononFile = NULL;
							   }
							   else {

								   if (2*sizeof(float) != sizeof(fftwf_complex)) {
									   printf("phononDisplacement: data type mismatch: fftw_complex != 2*float!\n");
									   exit(0);
								   }
								   fread(&Nk,sizeof(int),1,fpPhonon);
								   fread(&Ns,sizeof(int),1,fpPhonon);
								   massPrim =(float *)malloc(Ns*sizeof(float));  // masses for every atom in primitive basis
								   fread(massPrim,sizeof(float),Ns,fpPhonon);
								   kVecs = float32_2D(Nk,3,"kVecs");
								   omega = float32_2D(Nk,3*Ns,"omega");          /* array of eigenvalues for every k-vector 
																				 * omega is given in THz, but the 2pi-factor
																				 * is still there, i.e. f=omega/2pi
																				 */
								   eigVecs = complex3D(Nk,3*Ns,3*Ns,"eigVecs"); // array of eigenvectors for every k-vector
								   for (ix=0;ix<Nk;ix++) {
									   fread(kVecs[ix],sizeof(float),3,fpPhonon);  // k-vector
									   for (iy=0;iy<3*Ns;iy++) {
										   fread(omega[ix]+iy,sizeof(float),1,fpPhonon);
										   fread(eigVecs[ix][iy],2*sizeof(float),3*Ns,fpPhonon);
									   }	
								   }
								   /*
								   printf("Masses: ");
								   for (ix=0;ix<Ns;ix++) printf(" %g",massPrim[ix]);
								   printf("\n");
								   for (ix=0;ix<3;ix++) {
								   printf("(%5f %5f %5f):  ",kVecs[ix][0],kVecs[ix][1],kVecs[ix][2]);
								   for (idd=0;idd<Ns;idd++) for (iy=0;iy<3;iy++)
								   printf("%6g ",omega[ix][iy+3*idd]);
								   printf("\n");
								   }
								   */      
								   /* convert omega into q scaling factors, since we need those, instead of true omega:    */
								   /* The 1/sqrt(2) term is from the dimensionality ((q1,q2) -> d=2)of the random numbers */
								   for (ix=0;ix<Nk;ix++) {
									   for (idd=0;idd<Ns;idd++) for (iy=0;iy<3;iy++) {
										   // quantize the energy distribution:
										   // tanh and exp give different results will therefore use exp
										   // nkomega = (int)(1.0/tanh(THZ_HBAR_2KB*omega[ix][iy+3*id]/m_tds_temp));
										   // wobble  =      (1.0/tanh(THZ_HBAR_2KB*omega[ix][iy+3*id]/m_tds_temp)-0.5);
										   // nkomega = (int)(1.0/(exp(THZ_HBAR_KB*omega[ix][iy+3*id]/m_tds_temp)-1)+0.5);
										   if (omega[ix][iy+3*idd] > 1e-4) {
											   wobble = m_tds_temp>0 ? (1.0/(exp(THZ_HBAR_KB*omega[ix][iy+3*idd]/m_tds_temp)-1)):0;
											   // if (ix == 0) printf("%g: %d %g\n",omega[ix][iy+3*id],nkomega,wobble);
											   wobble = sqrt((wobble+0.5)/(2*PID*Nk*2*massPrim[idd]*omega[ix][iy+3*idd]* THZ_AMU_HBAR));  
										   }
										   else wobble = 0;
										   /* Ttotal += 0.25*massPrim[id]*((wobble*wobble)/(2*Ns))*
										   omega[ix][iy+3*id]*omega[ix][iy+3*id]*AMU_THZ2_A2_KB;
										   */
										   omega[ix][iy+3*idd] = wobble;
									   }  // idd
									   // if (ix == 0) printf("\n");
								   }
								   // printf("Temperature: %g K\n",Ttotal);
								   // printf("%d %d %d\n",(int)(0.4*(double)Nk/11.0),(int)(0.6*(double)Nk),Nk);
								   q1 = double2D(3*Ns,Nk,"q1");
								   q2 = double2D(3*Ns,Nk,"q2");

							   }
							   fclose(fpPhonon);    
						   }  // end of if phononfile

						   // 
						   // in the previous bracket: the phonon file is only read once.
						   /////////////////////////////////////////////////////////////////////////////////////
						   if ((m_Einstein == 0) && (atomCount == maxAtom-1)) {
							   if (Nk > 800)
								   printf("Will create phonon displacements for %d k-vectors - please wait ...\n",Nk);
							   for (lambda=0;lambda<3*Ns;lambda++) for (ik=0;ik<Nk;ik++) {
								   q1[lambda][ik] = (omega[ik][lambda] * gasdev( &iseed ));
								   q2[lambda][ik] = (omega[ik][lambda] * gasdev( &iseed ));
							   }
							   // printf("Q: %g %g %g\n",q1[0][0],q1[5][8],q1[0][3]);
						   }
   /********************************************************************************
	* Do the Einstein model independent vibrations !!!
	*******************************************************************************/
	if (m_Einstein) {	    
	   /* convert the Debye-Waller factor to sqrt(<u^2>) */
	   wobble = scale*sqrt(dw*wobScale);
	   u[0] = (wobble*sq3 * gasdev( &iseed ));
	   u[1] = (wobble*sq3 * gasdev( &iseed ));
	   u[2] = (wobble*sq3 * gasdev( &iseed ));
	   ///////////////////////////////////////////////////////////////////////
	   // Book keeping:
		u2[ZnumIndex] += u[0]*u[0]+u[1]*u[1]+u[2]*u[2];
		ux += u[0]; uy += u[1]; uz += u[2];
		u2Count[ZnumIndex]++;


	  /* Finally we must convert the displacement for this atom back into its fractional
	   * coordinates so that we can add it to the current position in vector a
	   */
	   matrixProduct(&u,1,3,MmInv,3,3,&uf);
// test:
	   /*
	   matrixProduct(&uf,1,3,Mm,3,3,&b);
	   if (atomCount % 5 == 0) {
			printf("Z = %d, DW = %g, u=[%g %g %g]\n       u'=[%g %g %g]\n",m_Znums[ZnumIndex],dw,u[0],u[1],u[2],b[0],b[1],b[2]);
			showMatrix(Mm,3,3,"Mm");
			showMatrix(MmInv,3,3,"MmInv");
	   }
	   */
// end test
	   memcpy(u,uf,3*sizeof(double));
	}
	else {
	   // id seems to be the index of the correct atom, i.e. ranges from 0 .. Natom
	   printf("created phonon displacements %d, %d, %d %d (eigVecs: %d %d %d)!\n",ZnumIndex,Ns,Nk,id,Nk,3*Ns,3*Ns);
	   /* loop over k and lambda:  */
	   memset(u,0,3*sizeof(double));
	   for (lambda=0;lambda<3*Ns;lambda++) for (ik=0;ik<Nk;ik++) {
		   // if (kVecs[ik][2] == 0){
		   kR = 2*PID*(icx*kVecs[ik][0]+icy*kVecs[ik][1]+icz*kVecs[ik][2]);
		   //  kR = 2*PID*(blat[0][0]*kVecs[ik][0]+blat[0][1]*kVecs[ik][1]+blat[0][2]*kVecs[ik][2]);
		   kRr = cos(kR); kRi = sin(kR);
		   for (icoord=0;icoord<3;icoord++) {
									   u[icoord] += q1[lambda][ik]*(eigVecs[ik][lambda][icoord+3*id][0]*kRr-
										   eigVecs[ik][lambda][icoord+3*id][1]*kRi)-
										   q2[lambda][ik]*(eigVecs[ik][lambda][icoord+3*id][0]*kRi+
										   eigVecs[ik][lambda][icoord+3*id][1]*kRr);
		   }
		}
		// printf("u: %g %g %g\n",u[0],u[1],u[2]);
	   /* Convert the cartesian displacements back to reduced coordinates
	   */ 
	   ///////////////////////////////////////////////////////////////////////
	   // Book keeping:
		u2[ZnumIndex] += u[0]*u[0]+u[1]*u[1]+u[2]*u[2];
		ux += u[0]; uy += u[1]; uz += u[2];
		u[0] /= m_ax;
		u[1] /= m_by;
		u[2] /= m_c;
		u2Count[ZnumIndex]++;

   } /* end of if Einstein */

	// printf("%d ... %d\n",ZnumIndex,u2Size);
   // printf("atomCount: %d (%d) %d %d\n",atomCount,m_Einstein,ZnumIndex,u2Size);


	/*
	// Used for Debugging the memory leak on Aug. 20, 2010:
	if (_CrtCheckMemory() == 0) {
	   printf("Found bad memory check in phononDisplacement! %d %d\n",ZnumIndex,m_atomKinds);
	}
	*/


	return 0;  
}


int CCrystal::AtomCompareZnum(const void *atPtr1,const void *atPtr2) {
	int comp = 0;
	atom *atom1, *atom2;

	atom1 = (atom *)atPtr1;
	atom2 = (atom *)atPtr2;
	/* Use the fact that z is the first element in the atom struct */
	comp = (atom1->Znum == atom2->Znum) ? 0 : ((atom1->Znum > atom2->Znum) ? -1 : 1); 
	return comp;
}

int CCrystal::AtomCompareZYX(const void *atPtr1,const void *atPtr2) {
	int comp = 0;
	atom *atom1, *atom2;

	atom1 = (atom *)atPtr1;
	atom2 = (atom *)atPtr2;
	/* Use the fact that z is the first element in the atom struct */
	comp = (atom1->z == atom2->z) ? 0 : ((atom1->z > atom2->z) ? 1 : -1); 
	if (comp == 0) {
		comp = (atom1->y == atom2->y) ? 0 : ((atom1->y > atom2->y) ? 1 : -1); 
		if (comp == 0) {
			comp = (atom1->x == atom2->x) ? 0 : ((atom1->x > atom2->x) ? 1 : -1); 
		}
	}
	return comp;
}
