#include "ElectroMagn2D.h"

#include <cmath>

#include <iostream>
#include <sstream>

#include "Params.h"
#include "Field2D.h"

#include "Patch.h"
#include <cstring>

#include "Profile.h"

#include "ElectroMagnBC.h"

using namespace std;

// ---------------------------------------------------------------------------------------------------------------------
// Constructor for Electromagn2D
// ---------------------------------------------------------------------------------------------------------------------
ElectroMagn2D::ElectroMagn2D(Params &params, DomainDecomposition* domain_decomposition, vector<Species*>& vecSpecies, Patch* patch) : 
  ElectroMagn(params, domain_decomposition, vecSpecies, patch),
isYmin(patch->isYmin()),
isYmax(patch->isYmax())
{    
    
    initElectroMagn2DQuantities(params, patch);
    
    // Charge currents currents and density for each species
    for (unsigned int ispec=0; ispec<n_species; ispec++) {
        Jx_s[ispec]  = new Field2D(Tools::merge("Jx_" ,vecSpecies[ispec]->name).c_str(), dimPrim);
        Jy_s[ispec]  = new Field2D(Tools::merge("Jy_" ,vecSpecies[ispec]->name).c_str(), dimPrim);
        Jz_s[ispec]  = new Field2D(Tools::merge("Jz_" ,vecSpecies[ispec]->name).c_str(), dimPrim);
        rho_s[ispec] = new Field2D(Tools::merge("Rho_",vecSpecies[ispec]->name).c_str(), dimPrim);
    }
    

    
}//END constructor Electromagn2D


ElectroMagn2D::ElectroMagn2D( ElectroMagn2D* emFields, Params &params, Patch* patch ) : 
    ElectroMagn(emFields, params, patch),
isYmin(patch->isYmin()),
isYmax(patch->isYmax())
{
    
    initElectroMagn2DQuantities(params, patch);
    
    // Charge currents currents and density for each species
    for (unsigned int ispec=0; ispec<n_species; ispec++) {
        if ( emFields->Jx_s[ispec] != NULL ) {
            if ( emFields->Jx_s[ispec]->data_ != NULL )
                Jx_s[ispec]  = new Field2D(dimPrim, 0, false, emFields->Jx_s[ispec]->name);
            else
                Jx_s[ispec]  = new Field2D(emFields->Jx_s[ispec]->name, dimPrim);
        }
        if ( emFields->Jy_s[ispec] != NULL ) {
            if ( emFields->Jy_s[ispec]->data_ != NULL )
                Jy_s[ispec]  = new Field2D(dimPrim, 1, false, emFields->Jy_s[ispec]->name);
            else
                Jy_s[ispec]  = new Field2D(emFields->Jy_s[ispec]->name, dimPrim);
        }
        if ( emFields->Jz_s[ispec] != NULL ) {
            if ( emFields->Jz_s[ispec]->data_ != NULL )
                Jz_s[ispec]  = new Field2D(dimPrim, 2, false, emFields->Jz_s[ispec]->name);
            else
                Jz_s[ispec]  = new Field2D(emFields->Jz_s[ispec]->name, dimPrim);
        }
        if ( emFields->rho_s[ispec] != NULL ) {
            if ( emFields->rho_s[ispec]->data_ != NULL )
                rho_s[ispec] = new Field2D(dimPrim, emFields->rho_s[ispec]->name );
            else
                rho_s[ispec]  = new Field2D(emFields->rho_s[ispec]->name, dimPrim);
        }
    }
    
}

// ---------------------------------------------------------------------------------------------------------------------
// Initialize quantities used in ElectroMagn2D
// ---------------------------------------------------------------------------------------------------------------------
void ElectroMagn2D::initElectroMagn2DQuantities(Params &params, Patch* patch)
{
    // --------------------------------------------------
    // Calculate quantities related to the simulation box
    // --------------------------------------------------
    
    // spatial-step and ratios time-step by spatial-step & spatial-step by time-step (in the x-direction)
    dx       = cell_length[0];
    dt_ov_dx = timestep/dx;
    dx_ov_dt = 1.0/dt_ov_dx;
    
    // spatial-step and ratios time-step by spatial-step & spatial-step by time-step (in the y-direction)
    dy       = cell_length[1];
    dt_ov_dy = timestep/dy;
    dy_ov_dt = 1.0/dt_ov_dy;
    
    // ----------------------
    // Electromagnetic fields
    // ----------------------
    //! \todo Homogenize 1D/2D dimPrim/dimDual or nx_p/nx_d/ny_p/ny_d
    
    dimPrim.resize( nDim_field );
    dimDual.resize( nDim_field );
    
    // Dimension of the primal and dual grids
    for (size_t i=0 ; i<nDim_field ; i++) {
        // Standard scheme
        dimPrim[i] = n_space[i]+1;
        dimDual[i] = n_space[i]+2;
        // + Ghost domain
        dimPrim[i] += 2*oversize[i];
        dimDual[i] += 2*oversize[i];
    }
    // number of nodes of the primal and dual grid in the x-direction
    nx_p = n_space[0]+1+2*oversize[0];
    nx_d = n_space[0]+2+2*oversize[0];
    // number of nodes of the primal and dual grid in the y-direction
    ny_p = n_space[1]+1+2*oversize[1];
    ny_d = n_space[1]+2+2*oversize[1];
    
    // Allocation of the EM fields
    Ex_  = new Field2D(dimPrim, 0, false, "Ex");
    Ey_  = new Field2D(dimPrim, 1, false, "Ey");
    Ez_  = new Field2D(dimPrim, 2, false, "Ez");
    Bx_  = new Field2D(dimPrim, 0, true,  "Bx");
    By_  = new Field2D(dimPrim, 1, true,  "By");
    Bz_  = new Field2D(dimPrim, 2, true,  "Bz");
    Bx_m = new Field2D(dimPrim, 0, true,  "Bx_m");
    By_m = new Field2D(dimPrim, 1, true,  "By_m");
    Bz_m = new Field2D(dimPrim, 2, true,  "Bz_m");
    
    // Allocation of filtered fields when Friedman filtering is required
    if (params.Friedman_filter){
        Exfilter.resize(3);
        Exfilter[0] = new Field2D(dimPrim, 0, false, "Ex_f");
        Exfilter[1] = new Field2D(dimPrim, 0, false, "Ex_m1");
        Exfilter[2] = new Field2D(dimPrim, 0, false, "Ex_m2");
        Eyfilter.resize(3);
        Eyfilter[0] = new Field2D(dimPrim, 1, false, "Ey_f");
        Eyfilter[1] = new Field2D(dimPrim, 1, false, "Ey_m1");
        Eyfilter[2] = new Field2D(dimPrim, 1, false, "Ey_m2");
        Ezfilter.resize(3);
        Ezfilter[0] = new Field2D(dimPrim, 2, false, "Ez_f");
        Ezfilter[1] = new Field2D(dimPrim, 2, false, "Ez_m1");
        Ezfilter[2] = new Field2D(dimPrim, 2, false, "Ez_m2");
    }
    
    // Total charge currents and densities
    Jx_   = new Field2D(dimPrim, 0, false, "Jx");
    Jy_   = new Field2D(dimPrim, 1, false, "Jy");
    Jz_   = new Field2D(dimPrim, 2, false, "Jz");
    rho_  = new Field2D(dimPrim, "Rho" );
    
    if(params.is_pxr == true) {
        rhoold_ = new Field2D(dimPrim,"Rho");
        Ex_pxr  = new Field2D(dimDual);
        Ey_pxr  = new Field2D(dimDual);
        Ez_pxr  = new Field2D(dimDual);
        Bx_pxr  = new Field2D(dimDual);
        By_pxr  = new Field2D(dimDual);
        Bz_pxr  = new Field2D(dimDual);
        Jx_pxr  = new Field2D(dimDual);
        Jy_pxr  = new Field2D(dimDual);
        Jz_pxr  = new Field2D(dimDual);
        rho_pxr = new Field2D(dimDual);
        rhoold_pxr  = new Field2D(dimDual);
    }


    // ----------------------------------------------------------------
    // Definition of the min and max index according to chosen oversize
    // ----------------------------------------------------------------
    index_bc_min.resize( nDim_field, 0 );
    index_bc_max.resize( nDim_field, 0 );
    for (unsigned int i=0 ; i<nDim_field ; i++) {
        index_bc_min[i] = oversize[i];
        index_bc_max[i] = dimDual[i]-oversize[i]-1;
    }
    /*
     MESSAGE("index_bc_min / index_bc_max / nx_p / nx_d" << index_bc_min[0]
     << " " << index_bc_max[0] << " " << nx_p<< " " << nx_d);
     */
    
    
    // Define limits of non duplicated elements
    // (by construction 1 (prim) or 2 (dual) elements shared between per MPI process)
    // istart
    for (unsigned int i=0 ; i<3 ; i++)
        for (unsigned int isDual=0 ; isDual<2 ; isDual++)
            istart[i][isDual] = 0;
    for (unsigned int i=0 ; i<nDim_field ; i++) {
        for (unsigned int isDual=0 ; isDual<2 ; isDual++) {
            istart[i][isDual] = oversize[i];
            if (patch->Pcoordinates[i]!=0) istart[i][isDual]+=1;
        }
    }
    
    // bufsize = nelements
    for (unsigned int i=0 ; i<3 ; i++)
        for (unsigned int isDual=0 ; isDual<2 ; isDual++)
            bufsize[i][isDual] = 1;
    
    for (unsigned int i=0 ; i<nDim_field ; i++) {
        for (int isDual=0 ; isDual<2 ; isDual++)
            bufsize[i][isDual] = n_space[i] + 1;
        
        for (int isDual=0 ; isDual<2 ; isDual++) {
            bufsize[i][isDual] += isDual;
            if ( params.number_of_patches[i]!=1 ) {
                
                if ( ( !isDual ) && (patch->Pcoordinates[i]!=0) )
                    bufsize[i][isDual]--;
                else if  (isDual) {
                    bufsize[i][isDual]--;
                    if ( (patch->Pcoordinates[i]!=0) && (patch->Pcoordinates[i]!=(unsigned int)params.number_of_patches[i]-1) )
                        bufsize[i][isDual]--;
                }
                
            } // if ( params.number_of_patches[i]!=1 )
        } // for (int isDual=0 ; isDual
    } // for (unsigned int i=0 ; i<nDim_field
}

// ---------------------------------------------------------------------------------------------------------------------
// Destructor for Electromagn2D
// ---------------------------------------------------------------------------------------------------------------------
ElectroMagn2D::~ElectroMagn2D()
{
}//END ElectroMagn2D



// ---------------------------------------------------------------------------------------------------------------------
// Begin of Solve Poisson methods
// ---------------------------------------------------------------------------------------------------------------------
// in VectorPatch::solvePoisson
//     - initPoisson
//     - compute_r
//     - compute_Ap
//     - compute_pAp
//     - update_pand_r
//     - update_p
//     - initE
//     - centeringE


void ElectroMagn2D::initPoisson(Patch *patch)
{
    Field2D* rho2D = static_cast<Field2D*>(rho_);
    
    // Min and max indices for calculation of the scalar product (for primal & dual grid)
    //     scalar products are computed accounting only on real nodes
    //     ghost cells are used only for the (non-periodic) boundaries
    // dual indexes suppressed during "patchization"
    // ----------------------------------------------------------------------------------
    
    index_min_p_.resize(2,0);
    index_max_p_.resize(2,0);
    
    index_min_p_[0] = oversize[0];
    index_min_p_[1] = oversize[1];
    index_max_p_[0] = nx_p - 2 - oversize[0];
    index_max_p_[1] = ny_p - 2 - oversize[1];
    if (patch->isXmin()) {
        index_min_p_[0] = 0;
    }
    if (patch->isXmax()) {
        index_max_p_[0] = nx_p-1;
    }
    
    phi_ = new Field2D(dimPrim);    // scalar potential
    r_   = new Field2D(dimPrim);    // residual vector
    p_   = new Field2D(dimPrim);    // direction vector
    Ap_  = new Field2D(dimPrim);    // A*p vector
    
    
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
            (*phi_)(i,j)   = 0.0;
            (*r_)(i,j)     = -(*rho2D)(i,j);
            (*p_)(i,j)     = (*r_)(i,j);
        }//j
    }//i

} // initPoisson

double ElectroMagn2D::compute_r()
{
    double rnew_dot_rnew_local(0.);
    for (unsigned int i=index_min_p_[0]; i<=index_max_p_[0]; i++) {
        for (unsigned int j=index_min_p_[1]; j<=index_max_p_[1]; j++) {
            rnew_dot_rnew_local += (*r_)(i,j)*(*r_)(i,j);
        }
    }
    return rnew_dot_rnew_local;
} // compute_r

void ElectroMagn2D::compute_Ap(Patch* patch)
{
    double one_ov_dx_sq       = 1.0/(dx*dx);
    double one_ov_dy_sq       = 1.0/(dy*dy);
    double two_ov_dx2dy2      = 2.0*(1.0/(dx*dx)+1.0/(dy*dy));
    
    // vector product Ap = A*p
    for (unsigned int i=1; i<nx_p-1; i++) {
        for (unsigned int j=1; j<ny_p-1; j++) {
            (*Ap_)(i,j) = one_ov_dx_sq*((*p_)(i-1,j)+(*p_)(i+1,j))
                + one_ov_dy_sq*((*p_)(i,j-1)+(*p_)(i,j+1))
                - two_ov_dx2dy2*(*p_)(i,j);
        }//j
    }//i
        
        
    // Xmin BC
    if ( patch->isXmin() ) {
        for (unsigned int j=1; j<ny_p-1; j++) {
            //Ap_(0,j)      = one_ov_dx_sq*(pXmin[j]+p_(1,j))
            (*Ap_)(0,j)      = one_ov_dx_sq*((*p_)(1,j))
                +              one_ov_dy_sq*((*p_)(0,j-1)+(*p_)(0,j+1))
                -              two_ov_dx2dy2*(*p_)(0,j);
        }
        // at corners
        //Ap_(0,0)           = one_ov_dx_sq*(pXmin[0]+p_(1,0))               // Xmin/Ymin
        //    +                   one_ov_dy_sq*(pYmin[0]+p_(0,1))
        (*Ap_)(0,0)           = one_ov_dx_sq*((*p_)(1,0))               // Xmin/Ymin
            +                   one_ov_dy_sq*((*p_)(0,1))
            -                   two_ov_dx2dy2*(*p_)(0,0);
        //Ap_(0,ny_p-1)      = one_ov_dx_sq*(pXmin[ny_p-1]+p_(1,ny_p-1))     // Xmin/Ymax
        //    +                   one_ov_dy_sq*(p_(0,ny_p-2)+pYmax[0])
        (*Ap_)(0,ny_p-1)      = one_ov_dx_sq*((*p_)(1,ny_p-1))     // Xmin/Ymax
            +                   one_ov_dy_sq*((*p_)(0,ny_p-2))
            -                   two_ov_dx2dy2*(*p_)(0,ny_p-1);
    }
        
    // Xmax BC
    if ( patch->isXmax() ) {
            
        for (unsigned int j=1; j<ny_p-1; j++) {
            //Ap_(nx_p-1,j) = one_ov_dx_sq*(p_(nx_p-2,j)+pXmax[j])
            (*Ap_)(nx_p-1,j) = one_ov_dx_sq*((*p_)(nx_p-2,j))
                +              one_ov_dy_sq*((*p_)(nx_p-1,j-1)+(*p_)(nx_p-1,j+1))
                -              two_ov_dx2dy2*(*p_)(nx_p-1,j);
        }
        // at corners
        //Ap_(nx_p-1,0)      = one_ov_dx_sq*(p_(nx_p-2,0)+pXmax[0])                 // Xmax/Ymin
        //    +                   one_ov_dy_sq*(pYmin[nx_p-1]+p_(nx_p-1,1))
        (*Ap_)(nx_p-1,0)      = one_ov_dx_sq*((*p_)(nx_p-2,0))                 // Xmax/Ymin
            +                   one_ov_dy_sq*((*p_)(nx_p-1,1))
            -                   two_ov_dx2dy2*(*p_)(nx_p-1,0);
        //Ap_(nx_p-1,ny_p-1) = one_ov_dx_sq*(p_(nx_p-2,ny_p-1)+pXmax[ny_p-1])       // Xmax/Ymax
        //    +                   one_ov_dy_sq*(p_(nx_p-1,ny_p-2)+pYmax[nx_p-1])
        (*Ap_)(nx_p-1,ny_p-1) = one_ov_dx_sq*((*p_)(nx_p-2,ny_p-1))       // Xmax/Ymax
            +                   one_ov_dy_sq*((*p_)(nx_p-1,ny_p-2))
            -                   two_ov_dx2dy2*(*p_)(nx_p-1,ny_p-1);
    }
        
} // compute_Ap

void ElectroMagn2D::compute_Ap_relativistic_Poisson(Patch* patch, double gamma_mean)
{
    // gamma_mean is the average Lorentz factor of the species whose fields will be computed
    // See for example https://doi.org/10.1016/j.nima.2016.02.043 for more details 

    double one_ov_dx_sq_ov_gamma_sq       = 1.0/(dx*dx)/(gamma_mean*gamma_mean);
    double one_ov_dy_sq                   = 1.0/(dy*dy);
    double two_ov_dxgam2dy2               = 2.0*(1.0/(dx*dx)/(gamma_mean*gamma_mean)+1.0/(dy*dy));
    
    // vector product Ap = A*p
    for (unsigned int i=1; i<nx_p-1; i++) {
        for (unsigned int j=1; j<ny_p-1; j++) {
            (*Ap_)(i,j) = one_ov_dx_sq_ov_gamma_sq*((*p_)(i-1,j)+(*p_)(i+1,j))
                + one_ov_dy_sq*((*p_)(i,j-1)+(*p_)(i,j+1))
                - two_ov_dxgam2dy2*(*p_)(i,j);
        }//j
    }//i
        
        
    // Xmin BC
    if ( patch->isXmin() ) {
        for (unsigned int j=1; j<ny_p-1; j++) {
            //Ap_(0,j)      = one_ov_dx_sq*(pXmin[j]+p_(1,j))
            (*Ap_)(0,j)      = one_ov_dx_sq_ov_gamma_sq*((*p_)(1,j))
                +              one_ov_dy_sq*((*p_)(0,j-1)+(*p_)(0,j+1))
                -              two_ov_dxgam2dy2*(*p_)(0,j);
        }
        // at corners
        //Ap_(0,0)           = one_ov_dx_sq*(pXmin[0]+p_(1,0))               // Xmin/Ymin
        //    +                   one_ov_dy_sq*(pYmin[0]+p_(0,1))
        (*Ap_)(0,0)           = one_ov_dx_sq_ov_gamma_sq*((*p_)(1,0))               // Xmin/Ymin
            +                   one_ov_dy_sq*((*p_)(0,1))
            -                   two_ov_dxgam2dy2*(*p_)(0,0);
        //Ap_(0,ny_p-1)      = one_ov_dx_sq*(pXmin[ny_p-1]+p_(1,ny_p-1))     // Xmin/Ymax
        //    +                   one_ov_dy_sq*(p_(0,ny_p-2)+pYmax[0])
        (*Ap_)(0,ny_p-1)      = one_ov_dx_sq_ov_gamma_sq*((*p_)(1,ny_p-1))     // Xmin/Ymax
            +                   one_ov_dy_sq*((*p_)(0,ny_p-2))
            -                   two_ov_dxgam2dy2*(*p_)(0,ny_p-1);
    }
        
    // Xmax BC
    if ( patch->isXmax() ) {
            
        for (unsigned int j=1; j<ny_p-1; j++) {
            //Ap_(nx_p-1,j) = one_ov_dx_sq*(p_(nx_p-2,j)+pXmax[j])
            (*Ap_)(nx_p-1,j) = one_ov_dx_sq_ov_gamma_sq*((*p_)(nx_p-2,j))
                +              one_ov_dy_sq*((*p_)(nx_p-1,j-1)+(*p_)(nx_p-1,j+1))
                -              two_ov_dxgam2dy2*(*p_)(nx_p-1,j);
        }
        // at corners
        //Ap_(nx_p-1,0)      = one_ov_dx_sq*(p_(nx_p-2,0)+pXmax[0])                 // Xmax/Ymin
        //    +                   one_ov_dy_sq*(pYmin[nx_p-1]+p_(nx_p-1,1))
        (*Ap_)(nx_p-1,0)      = one_ov_dx_sq_ov_gamma_sq*((*p_)(nx_p-2,0))                 // Xmax/Ymin
            +                   one_ov_dy_sq*((*p_)(nx_p-1,1))
            -                   two_ov_dxgam2dy2*(*p_)(nx_p-1,0);
        //Ap_(nx_p-1,ny_p-1) = one_ov_dx_sq*(p_(nx_p-2,ny_p-1)+pXmax[ny_p-1])       // Xmax/Ymax
        //    +                   one_ov_dy_sq*(p_(nx_p-1,ny_p-2)+pYmax[nx_p-1])
        (*Ap_)(nx_p-1,ny_p-1) = one_ov_dx_sq_ov_gamma_sq*((*p_)(nx_p-2,ny_p-1))       // Xmax/Ymax
            +                   one_ov_dy_sq*((*p_)(nx_p-1,ny_p-2))
            -                   two_ov_dxgam2dy2*(*p_)(nx_p-1,ny_p-1);
    }
        
} // compute_Ap_relativistic_Poisson

double ElectroMagn2D::compute_pAp()
{
    double p_dot_Ap_local = 0.0;
    for (unsigned int i=index_min_p_[0]; i<=index_max_p_[0]; i++) {
        for (unsigned int j=index_min_p_[1]; j<=index_max_p_[1]; j++) {
            p_dot_Ap_local += (*p_)(i,j)*(*Ap_)(i,j);
        }
    }
    return p_dot_Ap_local;
} // compute_pAp

void ElectroMagn2D::update_pand_r(double r_dot_r, double p_dot_Ap)
{
    double alpha_k = r_dot_r/p_dot_Ap;
    for(unsigned int i=0; i<nx_p; i++) {
        for(unsigned int j=0; j<ny_p; j++) {
            (*phi_)(i,j) += alpha_k * (*p_)(i,j);
            (*r_)(i,j)   -= alpha_k * (*Ap_)(i,j);
        }
    }

} // update_pand_r

void ElectroMagn2D::update_p(double rnew_dot_rnew, double r_dot_r)
{
    double beta_k = rnew_dot_rnew/r_dot_r;
    for (unsigned int i=0; i<nx_p; i++) {
        for(unsigned int j=0; j<ny_p; j++) {
            (*p_)(i,j) = (*r_)(i,j) + beta_k * (*p_)(i,j);
        }
    }
} // update_p

void ElectroMagn2D::initE(Patch *patch)
{
    Field2D* Ex2D  = static_cast<Field2D*>(Ex_);
    Field2D* Ey2D  = static_cast<Field2D*>(Ey_);
    Field2D* rho2D = static_cast<Field2D*>(rho_);

    // ------------------------------------------
    // Compute the electrostatic fields Ex and Ey
    // ------------------------------------------
    
    // Ex
    DEBUG("Computing Ex from scalar potential, Poisson problem");
    for (unsigned int i=1; i<nx_d-1; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(i,j) = ((*phi_)(i-1,j)-(*phi_)(i,j))/dx;
        }
    }
    // Ey
    DEBUG("Computing Ey from scalar potential, Poisson problem");
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=1; j<ny_d-1; j++) {
            (*Ey2D)(i,j) = ((*phi_)(i,j-1)-(*phi_)(i,j))/dy;
        }
    }

    // Apply BC on Ex and Ey
    // ---------------------
    // Ex / Xmin
    if (patch->isXmin()) {
        DEBUG("Computing Xmin BC on Ex, Poisson problem");
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(0,j) = (*Ex2D)(1,j) + ((*Ey2D)(0,j+1)-(*Ey2D)(0,j))*dx/dy  - dx*(*rho2D)(0,j);
        }
    }
    // Ex / Xmax
    if (patch->isXmax()) {
        DEBUG("Computing Xmax BC on Ex, Poisson problem");
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(nx_d-1,j) = (*Ex2D)(nx_d-2,j) - ((*Ey2D)(nx_p-1,j+1)-(*Ey2D)(nx_p-1,j))*dx/dy + dx*(*rho2D)(nx_p-1,j);
        }
    }

    delete phi_;
    delete r_;
    delete p_;
    delete Ap_;

} // initE

void ElectroMagn2D::initE_relativistic_Poisson(Patch *patch, double gamma_mean)
{
    // gamma_mean is the average Lorentz factor of the species whose fields will be computed
    // See for example https://doi.org/10.1016/j.nima.2016.02.043 for more details 

    Field2D* Ex2D  = static_cast<Field2D*>(Ex_rel_);
    Field2D* Ey2D  = static_cast<Field2D*>(Ey_rel_);
    Field2D* rho2D = static_cast<Field2D*>(rho_);

    // ------------------------------------------
    // Compute the fields Ex and Ey
    // ------------------------------------------
    
    

    // Ex
    DEBUG("Computing Ex from scalar potential, relativistic Poisson problem");
    for (unsigned int i=1; i<nx_p-1; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(i,j) = ((*phi_)(i-1,j)-(*phi_)(i,j))/dx/gamma_mean/gamma_mean;
        }
    }
    // Ey
    DEBUG("Computing Ey from scalar potential, relativistic Poisson problem");
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=1; j<ny_p-1; j++) {
            (*Ey2D)(i,j) = ((*phi_)(i,j-1)-(*phi_)(i,j))/dy;
        }
    }

    // Apply BC on Ex and Ey
    // ---------------------
    // Ex / Xmin
    if (patch->isXmin()) {
        DEBUG("Computing Xmin BC on Ex, relativistic Poisson problem");
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(0,j) = (*Ex2D)(1,j) + ((*Ey2D)(0,j+1)-(*Ey2D)(0,j))*dx/dy  - dx*(*rho2D)(0,j);
        }
    }
    // Ex / Xmax
    if (patch->isXmax()) {
        DEBUG("Computing Xmax BC on Ex, relativistic Poisson problem");
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(nx_d-1,j) = (*Ex2D)(nx_d-2,j) - ((*Ey2D)(nx_p-1,j+1)-(*Ey2D)(nx_p-1,j))*dx/dy + dx*(*rho2D)(nx_p-1,j);
        }
    }

    // // Ey / Ymin
    // if (patch->isYmin()) {
    //     DEBUG("Computing Ymin BC on Ey, relativistic Poisson problem");
    //     for (unsigned int i=0; i<nx_p; i++) {
    //         (*Ey2D)(i,0) = (*Ey2D)(i,1) + ((*Ex2D)(i+1,0)-(*Ex2D)(i,0))*dy/dx  - dy*(*rho2D)(i,0);
    //     }
    // }
    // 
    // // Ey / Ymax
    // if (patch->isYmax()) {
    //     DEBUG("Computing Ymax BC on Ey, relativistic Poisson problem");
    //     for (unsigned int i=0; i<nx_p; i++) {
    //         (*Ey2D)(i,ny_d-1) = (*Ey2D)(i,ny_d-2) - ((*Ex2D)(i+1,ny_p-1)-(*Ex2D)(i,ny_p-1))*dy/dx + dy*(*rho2D)(i,ny_p-1);
    //     }
    // }

    delete phi_;
    delete r_;
    delete p_;
    delete Ap_;

} // initE_relativistic_Poisson

void ElectroMagn2D::initB_relativistic_Poisson(Patch *patch, double gamma_mean)
{
    // gamma_mean is the average Lorentz factor of the species whose fields will be computed
    // See for example https://doi.org/10.1016/j.nima.2016.02.043 for more details 

    Field2D* Ey2D  = static_cast<Field2D*>(Ey_rel_);
    Field2D* Bz2D  = static_cast<Field2D*>(Bz_rel_);
    // Bx is zero everywhere
    Field2D* Bx2D  = static_cast<Field2D*>(Bx_rel_);
    // ------------------------------------------
    // Compute the field Bz; Bx and By are identically zero
    // ------------------------------------------
    
    double beta_mean = sqrt(1.-1./gamma_mean/gamma_mean);

    // Bx is identically zero
    // (hypothesis of negligible J transverse with respect to Jx)
    // Bz
    DEBUG("Computing Bx, relativistic Poisson problem");
    for (unsigned int i=0; i<nx_d; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
            (*Bx2D)(i,j) = 0.;
        }
    }

    // Bz
    DEBUG("Computing Bz from scalar potential, relativistic Poisson problem");
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=0; j<ny_d; j++) {
            (*Bz2D)(i,j) = beta_mean*(*Ey2D)(i,j);
        }
    }


} // initE_relativistic_Poisson

void ElectroMagn2D::center_fields_from_relativistic_Poisson(Patch *patch){

    double one_over_two  = 1./2.;  

    // Static-cast of the fields
    Field2D* Ex2D = static_cast<Field2D*>(Ex_rel_);
    //Field2D* Ey2D = static_cast<Field2D*>(Ey_rel_);
    Field2D* Bz2D = static_cast<Field2D*>(Bz_rel_);

    // Temporary fields for interpolation
    Field2D* Bz2Dnew  = new Field2D( Bz_rel_->dims_  );

    // ------------ Interpolation to center the fields the Yee cell - before this operation, the fields B are all centered as E
    // (see ElectroMagn3D::initB_relativistic_Poisson) 
    //
    // p: cell nodes, d: between cell nodes, or cell edges
    
    // For all the fields, the proper centering (which is given to their "new" version) is reported
    // and then the old centering is reported (see ElectroMagn3D::initB_relativistic_Poisson)  
    

    // ----- Bz centering : (d,d) old centering is like Ey (p,d)
    for (unsigned int i=1 ; i < Bz_rel_->dims_[0]-2; i++){ // x loop
        for (unsigned int j=0 ; j < Bz_rel_->dims_[1]-2 ; j++){ // y loop
                (*Bz2Dnew)(i,j) = one_over_two*((*Bz2D)(i,j)+(*Bz2D)(i-1,j));
        } // end y loop
    } // end x loop


    // -------- Back substitution
    for (unsigned int i=0 ; i < Bz_rel_->dims_[0]-1; i++){ // x loop
        for (unsigned int j=0 ; j < Bz_rel_->dims_[1]-1 ; j++){ // y loop
                (*Bz2D)(i,j) = (*Bz2Dnew)(i,j);     
        } // end y loop
    } // end x loop


    // Clean the temporary variables
    delete Bz2Dnew;

} 

void ElectroMagn2D::initRelativisticPoissonFields(Patch *patch)
{
    // init temporary fields for relativistic field initialization, to be added to the already present electromagnetic fields

    Ex_rel_  = new Field2D(dimPrim, 0, false, "Ex_rel");
    Ey_rel_  = new Field2D(dimPrim, 1, false, "Ey_rel");
    Ez_rel_  = new Field2D(dimPrim, 2, false, "Ez_rel");
    Bx_rel_  = new Field2D(dimPrim, 0, true,  "Bx_rel"); // will be identically zero
    By_rel_  = new Field2D(dimPrim, 2, false,  "By_rel"); // is equal to -beta*Ez, thus it inherits the same centering of Ez
    Bz_rel_  = new Field2D(dimPrim, 1, false,  "Bz_rel"); // is equal to  beta*Ey, thus it inherits the same centering of Ey

} // initRelativisticPoissonFields

void ElectroMagn2D::sum_rel_fields_to_em_fields(Patch *patch)
{
    Field2D* Ex2Drel  = static_cast<Field2D*>(Ex_rel_);
    Field2D* Ey2Drel  = static_cast<Field2D*>(Ey_rel_);
    Field2D* Ez2Drel  = static_cast<Field2D*>(Ez_rel_);
    Field2D* Bx2Drel  = static_cast<Field2D*>(Bx_rel_);
    Field2D* By2Drel  = static_cast<Field2D*>(By_rel_);
    Field2D* Bz2Drel  = static_cast<Field2D*>(Bz_rel_);

    Field2D* Ex2D  = static_cast<Field2D*>(Ex_);
    Field2D* Ey2D  = static_cast<Field2D*>(Ey_);
    Field2D* Ez2D  = static_cast<Field2D*>(Ez_);
    Field2D* Bx2D  = static_cast<Field2D*>(Bx_);
    Field2D* By2D  = static_cast<Field2D*>(By_);
    Field2D* Bz2D  = static_cast<Field2D*>(Bz_);
    Field2D* Bx2D0  = static_cast<Field2D*>(Bx_m);
    Field2D* By2D0  = static_cast<Field2D*>(By_m);
    Field2D* Bz2D0  = static_cast<Field2D*>(Bz_m);

    // Ex (d,p,p)
    for (unsigned int i=0; i<nx_d; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
                (*Ex2D)(i,j) = (*Ex2D)(i,j) + (*Ex2Drel)(i,j);
        }
    }

    // Ey (p,d,p)
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=0; j<ny_d; j++) {
                (*Ey2D)(i,j) = (*Ey2D)(i,j) + (*Ey2Drel)(i,j);
        }
    }

    // Ez (p,p,d)
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
                (*Ez2D)(i,j) = (*Ez2D)(i,j) + (*Ez2Drel)(i,j);
        }
    }

    // Bx (p,d,d)
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=0; j<ny_d; j++) {
                (*Bx2D)(i,j) = (*Bx2D)(i,j) + (*Bx2Drel)(i,j);
                (*Bx2D0)(i,j)= (*Bx2D0)(i,j) + (*Bx2Drel)(i,j);
        }
    }

    // By (d,p,d) - remember that Byrel is centered as Ezrel (p,d,p)
    for (unsigned int i=1; i<nx_d-1; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
                (*By2D)(i,j) = (*By2D)(i,j) + 0.5 * ( (*By2Drel)(i,j) + (*By2Drel)(i-1,j) );
                (*By2D0)(i,j)= (*By2D0)(i,j) + 0.5 * ( (*By2Drel)(i,j) + (*By2Drel)(i-1,j) );
        }
    }

    // Bz (d,d,p) - remember that Bzrel is centered as Eyrel (p,d,p)
    for (unsigned int i=1; i<nx_d-1; i++) {
        for (unsigned int j=0; j<ny_d; j++) {
                (*Bz2D)(i,j) = (*Bz2D)(i,j) + 0.5 * ( (*Bz2Drel)(i,j) + (*Bz2Drel)(i-1,j) );
                (*Bz2D0)(i,j)= (*Bz2D0)(i,j) + 0.5 * ( (*Bz2Drel)(i,j) + (*Bz2Drel)(i-1,j) );
        }
    }

    // BC on the cells which are present in dual but not in primal grid : Brel identically zero at x edges

    // delete temporary fields used for relativistic initialization
    delete Ex_rel_;
    delete Ey_rel_;
    delete Ez_rel_;
    delete Bx_rel_;
    delete By_rel_;
    delete Bz_rel_;


} // sum_rel_fields_to_em_fields

void ElectroMagn2D::centeringE( std::vector<double> E_Add )
{
    Field2D* Ex2D  = static_cast<Field2D*>(Ex_);
    Field2D* Ey2D  = static_cast<Field2D*>(Ey_);

    // Centering electrostatic fields
    for (unsigned int i=0; i<nx_d; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(i,j) += E_Add[0];
        }
    }
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=0; j<ny_d; j++) {
            (*Ey2D)(i,j) += E_Add[1];
        }
    }
} // centeringE

void ElectroMagn2D::centeringErel( std::vector<double> E_Add )
{
    Field2D* Ex2D  = static_cast<Field2D*>(Ex_rel_);
    Field2D* Ey2D  = static_cast<Field2D*>(Ey_rel_);

    // Centering electrostatic fields
    for (unsigned int i=0; i<nx_d; i++) {
        for (unsigned int j=0; j<ny_p; j++) {
            (*Ex2D)(i,j) += E_Add[0];
        }
    }
    for (unsigned int i=0; i<nx_p; i++) {
        for (unsigned int j=0; j<ny_d; j++) {
            (*Ey2D)(i,j) += E_Add[1];
        }
    }
} // centeringErel

// ---------------------------------------------------------------------------------------------------------------------
// End of Solve Poisson methods 
// ---------------------------------------------------------------------------------------------------------------------


// ---------------------------------------------------------------------------------------------------------------------
// Save the former Magnetic-Fields (used to center them)
// ---------------------------------------------------------------------------------------------------------------------
void ElectroMagn2D::saveMagneticFields(bool is_spectral)
{
    // Static cast of the fields
    if(!is_spectral  ){
   	 Field2D* Bx2D   = static_cast<Field2D*>(Bx_);
   	 Field2D* By2D   = static_cast<Field2D*>(By_);
   	 Field2D* Bz2D   = static_cast<Field2D*>(Bz_);
   	 Field2D* Bx2D_m = static_cast<Field2D*>(Bx_m);
   	 Field2D* By2D_m = static_cast<Field2D*>(By_m);
   	 Field2D* Bz2D_m = static_cast<Field2D*>(Bz_m);
   	 
   	 // Magnetic field Bx^(p,d)
   	 for (unsigned int i=0 ; i<nx_p ; i++) {
   	     memcpy(&((*Bx2D_m)(i,0)), &((*Bx2D)(i,0)),ny_d*sizeof(double) );
   	     //for (unsigned int j=0 ; j<ny_d ; j++) {
   	     //    (*Bx2D_m)(i,j)=(*Bx2D)(i,j);
   	     //}
   	 
   	 // Magnetic field By^(d,p)
   	     memcpy(&((*By2D_m)(i,0)), &((*By2D)(i,0)),ny_p*sizeof(double) );
   	     //for (unsigned int j=0 ; j<ny_p ; j++) {
   	     //    (*By2D_m)(i,j)=(*By2D)(i,j);
   	     //}
   	 
   	 // Magnetic field Bz^(d,d)
   	     memcpy(&((*Bz2D_m)(i,0)), &((*Bz2D)(i,0)),ny_d*sizeof(double) );
   	     //for (unsigned int j=0 ; j<ny_d ; j++) {
   	     //    (*Bz2D_m)(i,j)=(*Bz2D)(i,j);
   	     //}
   	 }// end for i
   	     memcpy(&((*By2D_m)(nx_p,0)), &((*By2D)(nx_p,0)),ny_p*sizeof(double) );
   	     //for (unsigned int j=0 ; j<ny_p ; j++) {
   	     //    (*By2D_m)(nx_p,j)=(*By2D)(nx_p,j);
   	     //}
   	     memcpy(&((*Bz2D_m)(nx_p,0)), &((*Bz2D)(nx_p,0)),ny_d*sizeof(double) );
   	     //for (unsigned int j=0 ; j<ny_d ; j++) {
   	     //    (*Bz2D_m)(nx_p,j)=(*Bz2D)(nx_p,j);
   	     //}
    }
    else{
        Bx_m = Bx_;
        By_m = By_;
        Bz_m = Bz_;
    }   
}//END saveMagneticFields


// ---------------------------------------------------------------------------------------------------------------------
// Apply a single pass binomial filter on currents
// ---------------------------------------------------------------------------------------------------------------------
void ElectroMagn2D::binomialCurrentFilter()
{
    // Static-cast of the currents
    Field2D* Jx2D = static_cast<Field2D*>(Jx_);
    Field2D* Jy2D = static_cast<Field2D*>(Jy_);
    Field2D* Jz2D = static_cast<Field2D*>(Jz_);
    
    // applying a single pass of the binomial filter
    // 9-point filter: (4*point itself + 2*(4*direct neighbors) + 1*(4*cross neghbors))/16
    
    // on Jx^(d,p) -- external points are treated by exchange
    Field2D *tmp   = new Field2D(dimPrim, 0, false);
    tmp->copyFrom(Jx2D);
    for (unsigned int i=1; i<nx_d-1; i++) {
        for (unsigned int j=1; j<ny_p-1; j++) {
            (*Jx2D)(i,j) = ((*tmp)(i+1,j-1) + 2.*(*tmp)(i+1,j) + (*tmp)(i+1,j+1) + 2.*(*tmp)(i,j-1) + 4.*(*tmp)(i,j) + 2.*(*tmp)(i,j+1) + (*tmp)(i-1,j-1) + 2.*(*tmp)(i-1,j) + (*tmp)(i-1,j+1))/16.;
        }
    }
    delete tmp;
    
    // on Jy^(p,d) -- external points are treated by exchange
    tmp   = new Field2D(dimPrim, 1, false);
    tmp->copyFrom(Jy2D);
    for (unsigned int i=1; i<nx_p-1; i++) {
        for (unsigned int j=1; j<ny_d-1; j++) {
            (*Jy2D)(i,j) = ((*tmp)(i+1,j-1) + 2.*(*tmp)(i+1,j) + (*tmp)(i+1,j+1) + 2.*(*tmp)(i,j-1) + 4.*(*tmp)(i,j) + 2.*(*tmp)(i,j+1) + (*tmp)(i-1,j-1) + 2.*(*tmp)(i-1,j) + (*tmp)(i-1,j+1))/16.;
        }
    }
    delete tmp;
    
    // on Jz^(p,p) -- external points are treated by exchange
    tmp   = new Field2D(dimPrim, 2, false);
    tmp->copyFrom(Jz2D);
    for (unsigned int i=1; i<nx_p-1; i++) {
        for (unsigned int j=1; j<ny_p-1; j++) {
            (*Jz2D)(i,j) = ((*tmp)(i+1,j-1) + 2.*(*tmp)(i+1,j) + (*tmp)(i+1,j+1) + 2.*(*tmp)(i,j-1) + 4.*(*tmp)(i,j) + 2.*(*tmp)(i,j+1) + (*tmp)(i-1,j-1) + 2.*(*tmp)(i-1,j) + (*tmp)(i-1,j+1))/16.;
        }
    }
    delete tmp;
    
}//END binomialCurrentFilter


//// ---------------------------------------------------------------------------------------------------------------------
//// Solve the Maxwell-Ampere equation
//// ---------------------------------------------------------------------------------------------------------------------
//void ElectroMagn2D::solveMaxwellAmpere()
//{
//    // Static-cast of the fields
//    Field2D* Ex2D = static_cast<Field2D*>(Ex_);
//    Field2D* Ey2D = static_cast<Field2D*>(Ey_);
//    Field2D* Ez2D = static_cast<Field2D*>(Ez_);
//    Field2D* Bx2D = static_cast<Field2D*>(Bx_);
//    Field2D* By2D = static_cast<Field2D*>(By_);
//    Field2D* Bz2D = static_cast<Field2D*>(Bz_);
//    Field2D* Jx2D = static_cast<Field2D*>(Jx_);
//    Field2D* Jy2D = static_cast<Field2D*>(Jy_);
//    Field2D* Jz2D = static_cast<Field2D*>(Jz_);
//    // Electric field Ex^(d,p)
//    for (unsigned int i=0 ; i<nx_d ; i++) {
//        #pragma omp simd
//        for (unsigned int j=0 ; j<ny_p ; j++) {
//            (*Ex2D)(i,j) += -timestep*(*Jx2D)(i,j) + dt_ov_dy * ( (*Bz2D)(i,j+1) - (*Bz2D)(i,j) );
//        }
//    }
//    
//    // Electric field Ey^(p,d)
//    for (unsigned int i=0 ; i<nx_p ; i++) {
//        #pragma omp simd
//        for (unsigned int j=0 ; j<ny_d ; j++) {
//            (*Ey2D)(i,j) += -timestep*(*Jy2D)(i,j) - dt_ov_dx * ( (*Bz2D)(i+1,j) - (*Bz2D)(i,j) );
//        }
//    }
//    
//    // Electric field Ez^(p,p)
//    for (unsigned int i=0 ;  i<nx_p ; i++) {
//        #pragma omp simd
//        for (unsigned int j=0 ; j<ny_p ; j++) {
//            (*Ez2D)(i,j) += -timestep*(*Jz2D)(i,j)
//            +               dt_ov_dx * ( (*By2D)(i+1,j) - (*By2D)(i,j) )
//            -               dt_ov_dy * ( (*Bx2D)(i,j+1) - (*Bx2D)(i,j) );
//        }
//    }
//
//}//END solveMaxwellAmpere


// ---------------------------------------------------------------------------------------------------------------------
// Center the Magnetic Fields (used to push the particle)
// ---------------------------------------------------------------------------------------------------------------------
void ElectroMagn2D::centerMagneticFields()
{
    // Static cast of the fields
    Field2D* Bx2D   = static_cast<Field2D*>(Bx_);
    Field2D* By2D   = static_cast<Field2D*>(By_);
    Field2D* Bz2D   = static_cast<Field2D*>(Bz_);
    Field2D* Bx2D_m = static_cast<Field2D*>(Bx_m);
    Field2D* By2D_m = static_cast<Field2D*>(By_m);
    Field2D* Bz2D_m = static_cast<Field2D*>(Bz_m);
    
    // Magnetic field Bx^(p,d)
    for (unsigned int i=0 ; i<nx_p ; i++) {
        #pragma omp simd
        for (unsigned int j=0 ; j<ny_d ; j++) {
            (*Bx2D_m)(i,j) = ( (*Bx2D)(i,j) + (*Bx2D_m)(i,j) )*0.5;
        }
//    }
    
    // Magnetic field By^(d,p)
//    for (unsigned int i=0 ; i<nx_d ; i++) {
        #pragma omp simd
        for (unsigned int j=0 ; j<ny_p ; j++) {
            (*By2D_m)(i,j) = ( (*By2D)(i,j) + (*By2D_m)(i,j) )*0.5;
        }
//    }
    
    // Magnetic field Bz^(d,d)
//    for (unsigned int i=0 ; i<nx_d ; i++) {
        #pragma omp simd
        for (unsigned int j=0 ; j<ny_d ; j++) {
            (*Bz2D_m)(i,j) = ( (*Bz2D)(i,j) + (*Bz2D_m)(i,j) )*0.5;
        } // end for j
      } // end for i
        #pragma omp simd
        for (unsigned int j=0 ; j<ny_p ; j++) {
            (*By2D_m)(nx_p,j) = ( (*By2D)(nx_p,j) + (*By2D_m)(nx_p,j) )*0.5;
        }
        #pragma omp simd
        for (unsigned int j=0 ; j<ny_d ; j++) {
            (*Bz2D_m)(nx_p,j) = ( (*Bz2D)(nx_p,j) + (*Bz2D_m)(nx_p,j) )*0.5;
        } // end for j

    
}//END centerMagneticFields



// Create a new field
Field * ElectroMagn2D::createField(string fieldname)
{
    if     (fieldname.substr(0,2)=="Ex" ) return new Field2D(dimPrim, 0, false, fieldname);
    else if(fieldname.substr(0,2)=="Ey" ) return new Field2D(dimPrim, 1, false, fieldname);
    else if(fieldname.substr(0,2)=="Ez" ) return new Field2D(dimPrim, 2, false, fieldname);
    else if(fieldname.substr(0,2)=="Bx" ) return new Field2D(dimPrim, 0, true,  fieldname);
    else if(fieldname.substr(0,2)=="By" ) return new Field2D(dimPrim, 1, true,  fieldname);
    else if(fieldname.substr(0,2)=="Bz" ) return new Field2D(dimPrim, 2, true,  fieldname);
    else if(fieldname.substr(0,2)=="Jx" ) return new Field2D(dimPrim, 0, false, fieldname);
    else if(fieldname.substr(0,2)=="Jy" ) return new Field2D(dimPrim, 1, false, fieldname);
    else if(fieldname.substr(0,2)=="Jz" ) return new Field2D(dimPrim, 2, false, fieldname);
    else if(fieldname.substr(0,3)=="Rho") return new Field2D(dimPrim, fieldname );
    
    ERROR("Cannot create field "<<fieldname);
    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------
// Compute the total density and currents from species density and currents
// ---------------------------------------------------------------------------------------------------------------------
void ElectroMagn2D::computeTotalRhoJ()
{
    // static cast of the total currents and densities
    Field2D* Jx2D    = static_cast<Field2D*>(Jx_);
    Field2D* Jy2D    = static_cast<Field2D*>(Jy_);
    Field2D* Jz2D    = static_cast<Field2D*>(Jz_);
    Field2D* rho2D   = static_cast<Field2D*>(rho_);
    
    // -----------------------------------
    // Species currents and charge density
    // -----------------------------------
    for (unsigned int ispec=0; ispec<n_species; ispec++) {
        if( Jx_s[ispec] ) {
            Field2D* Jx2D_s  = static_cast<Field2D*>(Jx_s[ispec]);
            for (unsigned int i=0 ; i<=nx_p ; i++)
                for (unsigned int j=0 ; j<ny_p ; j++)
                    (*Jx2D)(i,j) += (*Jx2D_s)(i,j);
        }
        if( Jy_s[ispec] ) {
            Field2D* Jy2D_s  = static_cast<Field2D*>(Jy_s[ispec]);
            for (unsigned int i=0 ; i<nx_p ; i++)
                for (unsigned int j=0 ; j<=ny_p ; j++)
                    (*Jy2D)(i,j) += (*Jy2D_s)(i,j);
        }
        if( Jz_s[ispec] ) {
            Field2D* Jz2D_s  = static_cast<Field2D*>(Jz_s[ispec]);
            for (unsigned int i=0 ; i<nx_p ; i++)
                for (unsigned int j=0 ; j<ny_p ; j++)
                    (*Jz2D)(i,j) += (*Jz2D_s)(i,j);
        }
        if( rho_s[ispec] ) {
            Field2D* rho2D_s  = static_cast<Field2D*>(rho_s[ispec]);
            for (unsigned int i=0 ; i<nx_p ; i++)
                for (unsigned int j=0 ; j<ny_p ; j++)
                    (*rho2D)(i,j) += (*rho2D_s)(i,j);
        }
    }//END loop on species ispec
//END computeTotalRhoJ
}


// ---------------------------------------------------------------------------------------------------------------------
// Compute electromagnetic energy flows vectors on the border of the simulation box
// ---------------------------------------------------------------------------------------------------------------------
void ElectroMagn2D::computePoynting() {

    Field2D* Ex2D     = static_cast<Field2D*>(Ex_);
    Field2D* Ey2D     = static_cast<Field2D*>(Ey_);
    Field2D* Ez2D     = static_cast<Field2D*>(Ez_);
    Field2D* Bx2D_m   = static_cast<Field2D*>(Bx_m);
    Field2D* By2D_m   = static_cast<Field2D*>(By_m);
    Field2D* Bz2D_m   = static_cast<Field2D*>(Bz_m);

    if (isXmin) {
        unsigned int iEy=istart[0][Ey2D->isDual(0)];
        unsigned int iBz=istart[0][Bz2D_m->isDual(0)];
        unsigned int iEz=istart[0][Ez2D->isDual(0)];
        unsigned int iBy=istart[0][By2D_m->isDual(0)];
        
        unsigned int jEy=istart[1][Ey2D->isDual(1)];
        unsigned int jBz=istart[1][Bz2D_m->isDual(1)];
        unsigned int jEz=istart[1][Ez2D->isDual(1)];
        unsigned int jBy=istart[1][By2D_m->isDual(1)];
        
        for (unsigned int j=0; j<=bufsize[1][Ez2D->isDual(1)]; j++) {
            
            double Ey__ = 0.5*((*Ey2D)(iEy,jEy+j) + (*Ey2D)(iEy, jEy+j+1));
            double Bz__ = 0.25*((*Bz2D_m)(iBz,jBz+j)+(*Bz2D_m)(iBz+1,jBz+j)+(*Bz2D_m)(iBz,jBz+j+1)+(*Bz2D_m)(iBz+1,jBz+j+1));
            double Ez__ = (*Ez2D)(iEz,jEz+j);
            double By__ = 0.5*((*By2D_m)(iBy,jBy+j) + (*By2D_m)(iBy+1, jBy+j));
            
            poynting_inst[0][0] = dy*timestep*(Ey__*Bz__ - Ez__*By__);
            poynting[0][0]+= poynting_inst[0][0];
        }
    }//if Xmin
    
    
    if (isXmax) {
        unsigned int iEy=istart[0][Ey2D->isDual(0)]  + bufsize[0][Ey2D->isDual(0)] -1;
        unsigned int iBz=istart[0][Bz2D_m->isDual(0)] + bufsize[0][Bz2D_m->isDual(0)]-1;
        unsigned int iEz=istart[0][Ez2D->isDual(0)]  + bufsize[0][Ez2D->isDual(0)] -1;
        unsigned int iBy=istart[0][By2D_m->isDual(0)] + bufsize[0][By2D_m->isDual(0)]-1;
        
        unsigned int jEy=istart[1][Ey2D->isDual(1)];
        unsigned int jBz=istart[1][Bz2D_m->isDual(1)];
        unsigned int jEz=istart[1][Ez2D->isDual(1)];
        unsigned int jBy=istart[1][By2D_m->isDual(1)];
        
        for (unsigned int j=0; j<=bufsize[1][Ez2D->isDual(1)]; j++) {
            
            double Ey__ = 0.5*((*Ey2D)(iEy,jEy+j) + (*Ey2D)(iEy, jEy+j+1));
            double Bz__ = 0.25*((*Bz2D_m)(iBz,jBz+j)+(*Bz2D_m)(iBz+1,jBz+j)+(*Bz2D_m)(iBz,jBz+j+1)+(*Bz2D_m)(iBz+1,jBz+j+1));
            double Ez__ = (*Ez2D)(iEz,jEz+j);
            double By__ = 0.5*((*By2D_m)(iBy,jBy+j) + (*By2D_m)(iBy+1, jBy+j));
            
            poynting_inst[1][0] = dy*timestep*(Ey__*Bz__ - Ez__*By__);
            poynting[1][0] -= poynting_inst[1][0];
        }
    }//if Xmaxer
    
    if (isYmin) {
        
        unsigned int iEz=istart[0][Ez_->isDual(0)];
        unsigned int iBx=istart[0][Bx_m->isDual(0)]; 
        unsigned int iEx=istart[0][Ex_->isDual(0)];
        unsigned int iBz=istart[0][Bz_m->isDual(0)]; 
        
        unsigned int jEz=istart[1][Ez_->isDual(1)];
        unsigned int jBx=istart[1][Bx_m->isDual(1)];
        unsigned int jEx=istart[1][Ex_->isDual(1)];
        unsigned int jBz=istart[1][Bz_m->isDual(1)];
        
        for (unsigned int i=0; i<=bufsize[0][Ez2D->isDual(0)]; i++) {
            double Ez__ = (*Ez2D)(iEz+i,jEz);
            double Bx__ = 0.5*((*Bx2D_m)(iBx+i,jBx) + (*Bx2D_m)(iBx+i, jBx+1));
            double Ex__ = 0.5*((*Ex2D)(iEx+i,jEx) + (*Ex2D)(iEx+i+1, jEx));
            double Bz__ = 0.25*((*Bz2D_m)(iBz+i,jBz)+(*Bz2D_m)(iBz+i+1,jBz)+(*Bz2D_m)(iBz+i,jBz+1)+(*Bz2D_m)(iBz+i+1,jBz+1));
            
            poynting_inst[0][1] = dx*timestep*(Ez__*Bx__ - Ex__*Bz__);
            poynting[0][1] += poynting_inst[0][1];
        }
    }// if Ymin
    
    if (isYmax) {
        unsigned int iEz=istart[0][Ez2D->isDual(0)];
        unsigned int iBx=istart[0][Bx2D_m->isDual(0)];
        unsigned int iEx=istart[0][Ex2D->isDual(0)];
        unsigned int iBz=istart[0][Bz2D_m->isDual(0)];
        
        unsigned int jEz=istart[1][Ez2D->isDual(1)]  + bufsize[1][Ez2D->isDual(1)] -1;
        unsigned int jBx=istart[1][Bx2D_m->isDual(1)] + bufsize[1][Bx2D_m->isDual(1)]-1;
        unsigned int jEx=istart[1][Ex2D->isDual(1)]  + bufsize[1][Ex2D->isDual(1)] -1;
        unsigned int jBz=istart[1][Bz2D_m->isDual(1)] + bufsize[1][Bz2D_m->isDual(1)]-1;
        
        for (unsigned int i=0; i<=bufsize[0][Ez_->isDual(0)]; i++) {
            double Ez__ = (*Ez2D)(iEz+i,jEz);
            double Bx__ = 0.5*((*Bx2D_m)(iBx+i,jBx) + (*Bx2D_m)(iBx+i, jBx+1));
            double Ex__ = 0.5*((*Ex2D)(iEx+i,jEx) + (*Ex2D)(iEx+i+1, jEx));
            double Bz__ = 0.25*((*Bz2D_m)(iBz+i,jBz)+(*Bz2D_m)(iBz+i+1,jBz)+(*Bz2D_m)(iBz+i,jBz+1)+(*Bz2D_m)(iBz+i+1,jBz+1));
            
            poynting_inst[1][1] = dx*timestep*(Ez__*Bx__ - Ex__*Bz__);
            poynting[1][1] -= poynting_inst[1][1];
        }
    }//if Ymax

}

void ElectroMagn2D::applyExternalField(Field* my_field,  Profile *profile, Patch* patch) {
    
    Field2D* field2D=static_cast<Field2D*>(my_field);
        
    vector<double> pos(2,0);
    pos[0]      = dx*((double)(patch->getCellStartingGlobalIndex(0))+(field2D->isDual(0)?-0.5:0.));
    double pos1 = dy*((double)(patch->getCellStartingGlobalIndex(1))+(field2D->isDual(1)?-0.5:0.));
    int N0 = (int)field2D->dims()[0];
    int N1 = (int)field2D->dims()[1];
    
    // UNSIGNED INT LEADS TO PB IN PERIODIC BCs
    for (int i=0 ; i<N0 ; i++) {
        pos[1] = pos1;
        for (int j=0 ; j<N1 ; j++) {
            (*field2D)(i,j) += profile->valueAt(pos);
            pos[1] += dy;
        }
        pos[0] += dx;
    }
    
}


void ElectroMagn2D::initAntennas(Patch* patch)
{
    
    // Filling the space profiles of antennas
    for (unsigned int i=0; i<antennas.size(); i++) {
        if      (antennas[i].fieldName == "Jx")
            antennas[i].field = new Field2D(dimPrim, 0, false, "Jx");
        else if (antennas[i].fieldName == "Jy")
            antennas[i].field = new Field2D(dimPrim, 1, false, "Jy");
        else if (antennas[i].fieldName == "Jz")
            antennas[i].field = new Field2D(dimPrim, 2, false, "Jz");
        
        if (antennas[i].field) 
            applyExternalField(antennas[i].field, antennas[i].space_profile, patch);
    }

}

