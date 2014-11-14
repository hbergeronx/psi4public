/*
 *@BEGIN LICENSE
 *
 * PSI4: an ab initio quantum chemistry software package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *@END LICENSE
 */

#include <libqt/qt.h>
#include "dfocc.h"

using namespace psi;
using namespace std;


namespace psi{ namespace dfoccwave{
  
//======================================================================
//             OMP2 Manager
//======================================================================             
void DFOCC::omp2_manager()
{
        time4grad = 0;// means I will not compute the gradient
	mo_optimized = 0;// means MOs are not optimized
	orbs_already_opt = 0;// means orbitals are not optimized yet.
	orbs_already_sc = 0;// menas orbitals are not semicanonical yet.
        timer_on("DF CC Integrals");
        df_corr();
        trans_corr();
        df_ref();
        trans_ref();
        outfile->Printf("\tNumber of basis functions in the DF-HF basis: %3d\n", nQ_ref);
        outfile->Printf("\tNumber of basis functions in the DF-CC basis: %3d\n", nQ);
        
        timer_off("DF CC Integrals");

        // memalloc for density intermediates
        Jc = SharedTensor1d(new Tensor1d("DF_BASIS_SCF J_Q", nQ_ref));
        g1Qc = SharedTensor1d(new Tensor1d("DF_BASIS_SCF G1_Q", nQ_ref));
        g1Qt = SharedTensor1d(new Tensor1d("DF_BASIS_SCF G1t_Q", nQ_ref));
        g1Q = SharedTensor1d(new Tensor1d("DF_BASIS_CC G1_Q", nQ));
        g1Qt2 = SharedTensor1d(new Tensor1d("DF_BASIS_CC G1t_Q", nQ));

        // Fock 
        fock();

        // QCHF
        if (qchf_ == "TRUE") qchf();

        // ROHF REF
        if (reference == "ROHF") t1_1st_sc();
	t2_1st_sc();
	Emp2L=Emp2;
        EcorrL=Emp2L-Escf;
	Emp2L_old=Emp2;
	
	outfile->Printf("\n");
	if (reference == "ROHF") outfile->Printf("\tComputing DF-MP2 energy using SCF MOs (DF-ROHF-MP2)... \n"); 
	else outfile->Printf("\tComputing DF-MP2 energy using SCF MOs (Canonical DF-MP2)... \n"); 
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tDF-HF Energy (a.u.)                : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCS-MP2 Total Energy (a.u.)     : %20.14f\n", Escsmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SOS-MP2 Total Energy (a.u.)     : %20.14f\n", Esosmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCSN-MP2 Total Energy (a.u.)    : %20.14f\n", Escsnmp2);
	if (reference == "ROHF") outfile->Printf("\tDF-MP2 Singles Energy (a.u.)       : %20.14f\n", Emp2_t1);
	if (reference == "ROHF") outfile->Printf("\tDF-MP2 Doubles Energy (a.u.)       : %20.14f\n", Ecorr - Emp2_t1);
	outfile->Printf("\tDF-MP2 Correlation Energy (a.u.)   : %20.14f\n", Ecorr);
	outfile->Printf("\tDF-MP2 Total Energy (a.u.)         : %20.14f\n", Emp2);
	outfile->Printf("\t======================================================================= \n");
	
	Process::environment.globals["CURRENT ENERGY"] = Emp2;
	Process::environment.globals["DF-MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["DF-SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["DF-SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["DF-SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;

        Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;
        Process::environment.globals["CURRENT CORRELATION ENERGY"] = Emp2 - Escf;
        Process::environment.globals["DF-MP2 CORRELATION ENERGY"] = Emp2 - Escf;
        Process::environment.globals["DF-SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
        Process::environment.globals["DF-SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
        Process::environment.globals["DF-SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;

        Process::environment.globals["DF-MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["DF-MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        // S2
        if (comput_s2_ == "TRUE" && reference_ == "UNRESTRICTED") s2_response();

	omp2_opdm();
	omp2_tpdm();
        mp2l_energy();
        separable_tpdm();
	gfock_vo();
	gfock_ov();
        gfock_oo();
        gfock_vv();
	idp();
	mograd();
        occ_iterations();
	
        // main if
        if (rms_wog <= tol_grad && fabs(DE) >= tol_Eod && regularization == "FALSE") {
           orbs_already_opt = 1;
	   if (conver == 1) outfile->Printf("\n\tOrbitals are optimized now.\n");
	   else if (conver == 0) { 
                    outfile->Printf("\n\tMAX MOGRAD did NOT converged, but RMS MOGRAD converged!!!\n");
	            outfile->Printf("\tI will consider the present orbitals as optimized.\n");
           }
	   outfile->Printf("\tTransforming MOs to the semicanonical basis... \n");
	   
	   semi_canonic();
	   outfile->Printf("\tSwitching to the standard DF-MP2 computation... \n");
	   
           trans_corr();
           trans_ref();
           fock();
	   t2_1st_sc();
           conver = 1;
           if (dertype == "FIRST") {
	       omp2_opdm();
	       omp2_tpdm();
               separable_tpdm();
               gfock_vo();
               gfock_ov();
               gfock_oo();
               gfock_vv();
           }
        }// end main if 

        else if (rms_wog <= tol_grad && fabs(DE) >= tol_Eod && regularization == "TRUE") {
	   outfile->Printf("\tOrbital gradient converged, but energy did not... \n");
	   outfile->Printf("\tA tighter rms_mograd_convergence tolerance is recommended... \n");
           throw PSIEXCEPTION("A tighter rms_mograd_convergence tolerance is recommended.");
        }

  if (conver == 1) {
        ref_energy();
	mp2_energy();
        if (orbs_already_opt == 1) Emp2L = Emp2;
	
	outfile->Printf("\n");
	outfile->Printf("\tComputing MP2 energy using optimized MOs... \n");
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tDF-HF Energy (a.u.)                : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCS-MP2 Total Energy (a.u.)     : %20.14f\n", Escsmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SOS-MP2 Total Energy (a.u.)     : %20.14f\n", Esosmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCSN-MP2 Total Energy (a.u.)    : %20.14f\n", Escsnmp2);
	outfile->Printf("\tDF-MP2 Correlation Energy (a.u.)   : %20.14f\n", Emp2 - Escf);
	outfile->Printf("\tDF-MP2 Total Energy (a.u.)         : %20.14f\n", Emp2);
	outfile->Printf("\t======================================================================= \n");
	

	outfile->Printf("\n");
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\t================ DF-OMP2 FINAL RESULTS ================================ \n");
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tDF-HF Energy (a.u.)                : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCS-OMP2 Total Energy (a.u.)    : %20.14f\n", Escsmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SOS-OMP2 Total Energy (a.u.)    : %20.14f\n", Esosmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCSN-OMP2 Total Energy (a.u.)   : %20.14f\n", Escsnmp2);
	outfile->Printf("\tDF-OMP2 Correlation Energy (a.u.)  : %20.14f\n", Emp2L-Escf);
	outfile->Printf("\tEdfomp2 - Eref (a.u.)              : %20.14f\n", Emp2L-Eref);
	outfile->Printf("\tDF-OMP2 Total Energy (a.u.)        : %20.14f\n", Emp2L);
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\n");
	

	// Set the global variables with the energies
	Process::environment.globals["DF-OMP2 TOTAL ENERGY"] = Emp2L;
	Process::environment.globals["DF-SCS-OMP2 TOTAL ENERGY"] =  Escsmp2;
	Process::environment.globals["DF-SOS-OMP2 TOTAL ENERGY"] =  Esosmp2;
	Process::environment.globals["DF-SCSN-OMP2 TOTAL ENERGY"] = Escsnmp2;
	Process::environment.globals["CURRENT ENERGY"] = Emp2L;
	Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;
	Process::environment.globals["CURRENT CORRELATION ENERGY"] = Emp2L-Escf;

        Process::environment.globals["DF-OMP2 CORRELATION ENERGY"] = Emp2L - Escf;
        Process::environment.globals["DF-SCS-OMP2 CORRELATION ENERGY"] =  Escsmp2 - Escf;
        Process::environment.globals["DF-SOS-OMP2 CORRELATION ENERGY"] =  Esosmp2 - Escf;
        Process::environment.globals["DF-SCSN-OMP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;

        // if scs on	
	if (do_scs == "TRUE") {
	    if (scs_type_ == "SCS") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsmp2;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsmp2 - Escf;
            }

	    else if (scs_type_ == "SCSN") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsnmp2;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsnmp2 - Escf;
            }
	}
    
        // else if sos on	
	else if (do_sos == "TRUE") {
	     if (sos_type_ == "SOS") {
	         Process::environment.globals["CURRENT ENERGY"] = Esosmp2;
  	         Process::environment.globals["CURRENT CORRELATION ENERGY"] = Esosmp2 - Escf;
             }
	}
 
	//if (natorb == "TRUE") nbo();
	//if (occ_orb_energy == "TRUE") semi_canonic(); 

        // OEPROP
        if (oeprop_ == "TRUE") oeprop();

        // S2
        if (comput_s2_ == "TRUE" && reference_ == "UNRESTRICTED") s2_response();
        //if (comput_s2_ == "TRUE" && reference_ == "UNRESTRICTED") s2_lagrangian();

        // Compute Analytic Gradients
        if (dertype == "FIRST") dfgrad();

  }// end if (conver == 1)
}// end omp2_manager 

//======================================================================
//             MP2 Manager
//======================================================================             
void DFOCC::mp2_manager()
{
        time4grad = 0;// means i will not compute the gradient
	mo_optimized = 0;// means MOs are not optimized
        timer_on("DF CC Integrals");
        df_corr();
        if (dertype == "NONE" && oeprop_ == "FALSE" && ekt_ip_ == "FALSE" && comput_s2_ == "FALSE" && qchf_ == "FALSE") {
            trans_mp2();
        }
        else {
            trans_corr();
            df_ref();
            trans_ref();
            outfile->Printf("\tNumber of basis functions in the DF-HF basis: %3d\n", nQ_ref);

            // memalloc for density intermediates
            Jc = SharedTensor1d(new Tensor1d("DF_BASIS_SCF J_Q", nQ_ref));
            g1Qc = SharedTensor1d(new Tensor1d("DF_BASIS_SCF G1_Q", nQ_ref));
            g1Qt = SharedTensor1d(new Tensor1d("DF_BASIS_SCF G1t_Q", nQ_ref));
            g1Q = SharedTensor1d(new Tensor1d("DF_BASIS_CC G1_Q", nQ));
            g1Qt2 = SharedTensor1d(new Tensor1d("DF_BASIS_CC G1t_Q", nQ));
            if (reference == "ROHF") {
                g1Qp = SharedTensor1d(new Tensor1d("DF_BASIS_SCF G1p_Q", nQ_ref));
            }
        }
        outfile->Printf("\tNumber of basis functions in the DF-CC basis: %3d\n", nQ);
        
        timer_off("DF CC Integrals");

        // QCHF
        if (qchf_ == "TRUE") qchf();

        // ROHF REF
        //outfile->Printf("\tI am here.\n"); 
        if (reference == "ROHF") t1_1st_sc();
        if (dertype == "NONE" && oeprop_ == "FALSE" && ekt_ip_ == "FALSE" && comput_s2_ == "FALSE") mp2_direct();
        else {
             fock();
             if (mp2_amp_type_ == "DIRECT") mp2_direct();
             else { 
	         t2_1st_sc();
                 mp2_energy();
             }
        }
	Emp2L=Emp2;
        EcorrL=Emp2L-Escf;
	
	outfile->Printf("\n");
	if (reference == "ROHF") outfile->Printf("\tComputing DF-MP2 energy using SCF MOs (DF-ROHF-MP2)... \n"); 
	else outfile->Printf("\tComputing DF-MP2 energy using SCF MOs (Canonical DF-MP2)... \n"); 
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tDF-HF Energy (a.u.)                : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCS-MP2 Total Energy (a.u.)     : %20.14f\n", Escsmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SOS-MP2 Total Energy (a.u.)     : %20.14f\n", Esosmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCSN-MP2 Total Energy (a.u.)    : %20.14f\n", Escsnmp2);
	if (reference_ == "ROHF") outfile->Printf("\tDF-MP2 Singles Energy (a.u.)       : %20.14f\n", Emp2_t1);
	if (reference_ == "ROHF") outfile->Printf("\tDF-MP2 Doubles Energy (a.u.)       : %20.14f\n", Ecorr - Emp2_t1);
	outfile->Printf("\tDF-MP2 Correlation Energy (a.u.)   : %20.14f\n", Ecorr);
	outfile->Printf("\tDF-MP2 Total Energy (a.u.)         : %20.14f\n", Emp2);
	outfile->Printf("\t======================================================================= \n");
	
	Process::environment.globals["CURRENT ENERGY"] = Emp2;
	Process::environment.globals["DF-MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["DF-SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["DF-SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["DF-SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;

        Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;
        Process::environment.globals["CURRENT CORRELATION ENERGY"] = Emp2 - Escf;
        Process::environment.globals["DF-MP2 CORRELATION ENERGY"] = Emp2 - Escf;
        Process::environment.globals["DF-SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
        Process::environment.globals["DF-SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
        Process::environment.globals["DF-SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;

        Process::environment.globals["DF-MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["DF-MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        // S2
        //if (comput_s2_ == "TRUE" && reference_ == "UNRESTRICTED" && dertype == "NONE") s2_response();
        if (comput_s2_ == "TRUE" && reference_ == "UNRESTRICTED") {
            if (reference == "UHF" || reference == "UKS") s2_response();
        }

        // Compute Analytic Gradients
        if (dertype == "FIRST" || oeprop_ == "TRUE" || ekt_ip_ == "TRUE") {
            outfile->Printf("\n\tComputing unrelaxed response density matrices...\n");
            
 	    omp2_opdm();
	    omp2_tpdm();
            prepare4grad();
            if (oeprop_ == "TRUE") oeprop();
            if (dertype == "FIRST") dfgrad();
            //if (ekt_ip_ == "TRUE") ekt_ip(); 

        }// if (dertype == "FIRST" || ekt_ip_ == "TRUE") 

}// end mp2_manager 


//======================================================================
//             CCSD Manager
//======================================================================             
void DFOCC::ccsd_manager()
{

        time4grad = 0;// means i will not compute the gradient
	mo_optimized = 0;// means MOs are not optimized
        timer_on("DF CC Integrals");
        df_corr();
        trans_corr();
        timer_off("DF CC Integrals");
        timer_on("DF REF Integrals");
        df_ref();
        trans_ref();
        timer_off("DF REF Integrals");
        outfile->Printf("\tNumber of basis functions in the DF-HF basis: %3d\n", nQ_ref);
        outfile->Printf("\tNumber of basis functions in the DF-CC basis: %3d\n", nQ);

        // Memory allocation
        T1c = SharedTensor1d(new Tensor1d("DF_BASIS_CC T1_Q", nQ));
        Jc = SharedTensor1d(new Tensor1d("DF_BASIS_SCF J_Q", nQ_ref));

     if (reference_ == "RESTRICTED") {
        t1A = SharedTensor2d(new Tensor2d("T1 <I|A>", naoccA, navirA));
        t1newA = SharedTensor2d(new Tensor2d("New T1 <I|A>", naoccA, navirA));
        FiaA = SharedTensor2d(new Tensor2d("Fint <I|A>", naoccA, navirA));
        FtijA = SharedTensor2d(new Tensor2d("Ftilde <I|J>", naoccA, naoccA));
        FtabA = SharedTensor2d(new Tensor2d("Ftilde <A|B>", navirA, navirA));

        // memory requirements
        // DF-HF B(Q,mn)
        cost_ampAA = 0;
        cost_ampAA = (ULI)nQ_ref * (ULI)nso2_;
        cost_ampAA /= (ULI)1024 * (ULI)1024;
        cost_ampAA *= (ULI)sizeof(double);
        cost_amp = (ULI)3.0 * cost_ampAA;
        outfile->Printf("\n\tMemory requirement for B-HF (Q|mu nu) is     : %6lu MB \n", cost_ampAA);
        outfile->Printf("\tMemory requirement for 3*B-HF (Q|mu nu) is   : %6lu MB \n", cost_amp);
 
        // DF-HF B(Q,ab)
        cost_ampAA = 0;
        cost_ampAA = (ULI)nQ_ref * (ULI)nvir2AA;
        cost_ampAA /= (ULI)1024 * (ULI)1024;
        cost_ampAA *= (ULI)sizeof(double);
        cost_amp = (ULI)3.0 * cost_ampAA;
        outfile->Printf("\tMemory requirement for B-HF (Q|ab) is        : %6lu MB \n", cost_ampAA);
        outfile->Printf("\tMemory requirement for 3*B-HF (Q|ab) is      : %6lu MB \n", cost_amp);

        // DF-CC B(Q,mn)
        cost_ampAA = 0;
        cost_ampAA = (ULI)nQ * (ULI)nso2_;
        cost_ampAA /= (ULI)1024 * (ULI)1024;
        cost_ampAA *= (ULI)sizeof(double);
        cost_amp = (ULI)3.0 * cost_ampAA;
        outfile->Printf("\n\tMemory requirement for B-CC (Q|mu nu) is     : %6lu MB \n", cost_ampAA);
        outfile->Printf("\tMemory requirement for 3*B-CC (Q|mu nu) is   : %6lu MB \n", cost_amp);
 
        // DF-CC B(Q,ab)
        cost_ampAA = 0;
        cost_ampAA = (ULI)nQ * (ULI)nvir2AA;
        cost_ampAA /= (ULI)1024 * (ULI)1024;
        cost_ampAA *= (ULI)sizeof(double);
        cost_amp = (ULI)3.0 * cost_ampAA;
        outfile->Printf("\tMemory requirement for B-CC (Q|ab) is        : %6lu MB \n", cost_ampAA);
        outfile->Printf("\tMemory requirement for 3*B-CC (Q|ab) is      : %6lu MB \n", cost_amp);
     }  // end if (reference_ == "RESTRICTED")

     else if (reference_ == "UNRESTRICTED") {
        t1A = SharedTensor2d(new Tensor2d("T1 <I|A>", naoccA, navirA));
        t1B = SharedTensor2d(new Tensor2d("T1 <i|a>", naoccB, navirB));
        FiaA = SharedTensor2d(new Tensor2d("Fint <I|A>", naoccA, navirA));
        FiaB = SharedTensor2d(new Tensor2d("Fint <i|a>", naoccB, navirB));
        FtijA = SharedTensor2d(new Tensor2d("Ftilde <I|J>", naoccA, naoccA));
        FtabA = SharedTensor2d(new Tensor2d("Ftilde <A|B>", navirA, navirA));
        FtijB = SharedTensor2d(new Tensor2d("Ftilde <i|j>", naoccB, naoccB));
        FtabB = SharedTensor2d(new Tensor2d("Ftilde <a|b>", navirB, navirB));
     }// else if (reference_ == "UNRESTRICTED")

        // memalloc for density intermediates
        if (qchf_ == "TRUE" || dertype == "FIRST") { 
            g1Qc = SharedTensor1d(new Tensor1d("DF_BASIS_SCF G1_Q", nQ_ref));
            g1Qt = SharedTensor1d(new Tensor1d("DF_BASIS_SCF G1t_Q", nQ_ref));
            g1Q = SharedTensor1d(new Tensor1d("DF_BASIS_CC G1_Q", nQ));
            g1Qt2 = SharedTensor1d(new Tensor1d("DF_BASIS_CC G1t_Q", nQ));
        }

        // QCHF
        if (qchf_ == "TRUE") qchf();

        // Fock
        if (dertype == "FIRST" && oeprop_ == "TRUE" && ekt_ip_ == "TRUE") fock();

        // Compute MP2 energy
        if (reference == "ROHF") t1_1st_sc();
        ccsd_mp2();
	
	outfile->Printf("\n");
	if (reference == "ROHF") outfile->Printf("\tComputing DF-MP2 energy (DF-ROHF-MP2)... \n"); 
	else outfile->Printf("\tComputing DF-MP2 energy ... \n"); 
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tDF-HF Energy (a.u.)                : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCS-MP2 Total Energy (a.u.)     : %20.14f\n", Escsmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SOS-MP2 Total Energy (a.u.)     : %20.14f\n", Esosmp2);
	if (reference_ == "UNRESTRICTED") outfile->Printf("\tDF-SCSN-MP2 Total Energy (a.u.)    : %20.14f\n", Escsnmp2);
	if (reference_ == "ROHF") outfile->Printf("\tDF-MP2 Singles Energy (a.u.)       : %20.14f\n", Emp2_t1);
	if (reference_ == "ROHF") outfile->Printf("\tDF-MP2 Doubles Energy (a.u.)       : %20.14f\n", Ecorr - Emp2_t1);
	outfile->Printf("\tDF-MP2 Correlation Energy (a.u.)   : %20.14f\n", Ecorr);
	outfile->Printf("\tDF-MP2 Total Energy (a.u.)         : %20.14f\n", Emp2);
	outfile->Printf("\t======================================================================= \n");
	
	Process::environment.globals["DF-MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["DF-SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["DF-SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["DF-SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;
        Process::environment.globals["DF-MP2 CORRELATION ENERGY"] = Emp2 - Escf;
        Process::environment.globals["DF-SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
        Process::environment.globals["DF-SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
        Process::environment.globals["DF-SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;
        Process::environment.globals["DF-MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["DF-MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        // Perform CCSD iterations
        ccsd_iterations();

	outfile->Printf("\n");
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\t================ CCSD FINAL RESULTS =================================== \n");
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tDF-CCSD Correlation Energy (a.u.)  : %20.14f\n", Ecorr);
	outfile->Printf("\tDF-CCSD Total Energy (a.u.)        : %20.14f\n", Eccsd);
	outfile->Printf("\t======================================================================= \n");
	outfile->Printf("\n");
	
	Process::environment.globals["CURRENT ENERGY"] = Eccsd;
        Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;
        Process::environment.globals["CURRENT CORRELATION ENERGY"] = Eccsd - Escf;
	Process::environment.globals["DF-CCSD TOTAL ENERGY"] = Eccsd;
        Process::environment.globals["DF-CCSD CORRELATION ENERGY"] = Eccsd - Escf;


        // Compute Analytic Gradients
        if (dertype == "FIRST" || oeprop_ == "TRUE" || ekt_ip_ == "TRUE") {
            outfile->Printf("\n\tComputing unrelaxed response density matrices...\n");
            
 	    omp2_opdm();
	    omp2_tpdm();
            prepare4grad();
            if (oeprop_ == "TRUE") oeprop();
            if (dertype == "FIRST") dfgrad();
            //if (ekt_ip_ == "TRUE") ekt_ip(); 
        }// if (dertype == "FIRST" || ekt_ip_ == "TRUE") 

}// end ccsd_manager 

//======================================================================
//             OMP3 Manager
//======================================================================             
void DFOCC::omp3_manager()
{
        /*
	mo_optimized = 0;
	orbs_already_opt = 0;
	orbs_already_sc = 0;
        time4grad = 0;// means i will not compute the gradient
        timer_on("trans_ints");
	if (reference_ == "RESTRICTED") trans_ints_rhf();  
	else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
        timer_off("trans_ints");
        timer_on("REF Energy");
	ref_energy();
        timer_off("REF Energy");
        timer_on("T2(1)");
	omp3_t2_1st_sc();
        timer_off("T2(1)");
        timer_on("MP2 Energy");
	omp3_mp2_energy();
        timer_off("MP2 Energy");

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using SCF MOs (Canonical MP2)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;
	Process::environment.globals["SCS-MP2-VDW TOTAL ENERGY"] = Escsmp2vdw;
	Process::environment.globals["SOS-PI-MP2 TOTAL ENERGY"] = Esospimp2;

        Process::environment.globals["MP2 CORRELATION ENERGY"] = Emp2 - Escf;
        Process::environment.globals["SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
        Process::environment.globals["SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
        Process::environment.globals["SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;
        Process::environment.globals["SCS-MP2-VDW CORRELATION ENERGY"] = Escsmp2vdw - Escf;
        Process::environment.globals["SOS-PI-MP2 CORRELATION ENERGY"] = Esospimp2 - Escf;

        Process::environment.globals["MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        timer_on("T2(2)");
	t2_2nd_sc();
        timer_off("T2(2)");
        timer_on("MP3 Energy");
	mp3_energy();
        timer_off("MP3 Energy");
	Emp3L=Emp3;
        EcorrL=Emp3L-Escf;
	Emp3L_old=Emp3;
        if (ip_poles == "TRUE") omp3_ip_poles();
	
	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP3 energy using SCF MOs (Canonical MP3)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp3AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp3AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp3BB);
	outfile->Printf("\tMP2.5 Correlation Energy (a.u.)    : %20.14f\n", (Emp2 - Escf) + 0.5 * (Emp3-Emp2));
	outfile->Printf("\tMP2.5 Total Energy (a.u.)          : %20.14f\n", 0.5 * (Emp3+Emp2));
	outfile->Printf("\tSCS-MP3 Total Energy (a.u.)        : %20.14f\n", Escsmp3);
	outfile->Printf("\tSOS-MP3 Total Energy (a.u.)        : %20.14f\n", Esosmp3);
	outfile->Printf("\tSCSN-MP3 Total Energy (a.u.)       : %20.14f\n", Escsnmp3);
	outfile->Printf("\tSCS-MP3-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp3vdw);
	outfile->Printf("\tSOS-PI-MP3 Total Energy (a.u.)     : %20.14f\n", Esospimp3);
	outfile->Printf("\t3rd Order Energy (a.u.)            : %20.14f\n", Emp3-Emp2);
	outfile->Printf("\tMP3 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP3 Total Energy (a.u.)            : %20.14f\n", Emp3);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["MP3 TOTAL ENERGY"] = Emp3;
	Process::environment.globals["SCS-MP3 TOTAL ENERGY"] = Escsmp3;
	Process::environment.globals["SOS-MP3 TOTAL ENERGY"] = Esosmp3;
	Process::environment.globals["SCSN-MP3 TOTAL ENERGY"] = Escsnmp3;
	Process::environment.globals["SCS-MP3-VDW TOTAL ENERGY"] = Escsmp3vdw;
	Process::environment.globals["SOS-PI-MP3 TOTAL ENERGY"] = Esospimp3;

	Process::environment.globals["MP2.5 CORRELATION ENERGY"] = (Emp2 - Escf) + 0.5 * (Emp3-Emp2);
	Process::environment.globals["MP2.5 TOTAL ENERGY"] = 0.5 * (Emp3+Emp2);
	Process::environment.globals["MP3 CORRELATION ENERGY"] = Emp3 - Escf;
	Process::environment.globals["SCS-MP3 CORRELATION ENERGY"] = Escsmp3 - Escf;
	Process::environment.globals["SOS-MP3 CORRELATION ENERGY"] = Esosmp3 - Escf;
	Process::environment.globals["SCSN-MP3 CORRELATION ENERGY"] = Escsnmp3 - Escf;
	Process::environment.globals["SCS-MP3-VDW CORRELATION ENERGY"] = Escsmp3vdw - Escf;
	Process::environment.globals["SOS-PI-MP3 CORRELATION ENERGY"] = Esospimp3 - Escf;

	omp3_response_pdms();
	gfock();
	idp();
	mograd();
        occ_iterations();
	
        if (rms_wog <= tol_grad && fabs(DE) >= tol_Eod) {
           orbs_already_opt = 1;
	   if (conver == 1) outfile->Printf("\n\tOrbitals are optimized now.\n");
	   else if (conver == 0) { 
                    outfile->Printf("\n\tMAX MOGRAD did NOT converged, but RMS MOGRAD converged!!!\n");
	            outfile->Printf("\tI will consider the present orbitals as optimized.\n");
           }
	   outfile->Printf("\tSwitching to the standard MP3 computation after semicanonicalization of the MOs... \n");
	   
	   semi_canonic();
	   if (reference_ == "RESTRICTED") trans_ints_rhf();  
	   else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
	   omp3_t2_1st_sc();
	   t2_2nd_sc();
           conver = 1;
           if (dertype == "FIRST") {
               omp3_response_pdms();
	       gfock();
           }
        }     

  if (conver == 1) {
        ref_energy();
	omp3_mp2_energy();
	mp3_energy();
        if (orbs_already_opt == 1) Emp3L = Emp3;

        if (ip_poles == "TRUE") {
	   if (orbs_already_sc == 0) {
               semi_canonic();
	       if (reference_ == "RESTRICTED") trans_ints_rhf();  
	       else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
	       omp3_t2_1st_sc();
	       t2_2nd_sc();
           }
           omp3_ip_poles();
        }

        // EKT
        if (ekt_ip_ == "TRUE") { 
            if (orbs_already_sc == 1) {
	        omp3_response_pdms();
	        gfock();
            }
            gfock_diag();
            if (ekt_ip_ == "TRUE") ekt_ip();
        }

        outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using optimized MOs... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP3 energy using optimized MOs... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp3AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp3AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp3BB);
	outfile->Printf("\tMP2.5 Correlation Energy (a.u.)    : %20.14f\n", (Emp2 - Escf) + 0.5 * (Emp3-Emp2));
	outfile->Printf("\tMP2.5 Total Energy (a.u.)          : %20.14f\n", 0.5 * (Emp3+Emp2));
	outfile->Printf("\tSCS-MP3 Total Energy (a.u.)        : %20.14f\n", Escsmp3);
	outfile->Printf("\tSOS-MP3 Total Energy (a.u.)        : %20.14f\n", Esosmp3);
	outfile->Printf("\tSCSN-MP3 Total Energy (a.u.)       : %20.14f\n", Escsnmp3);
	outfile->Printf("\tSCS-MP3-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp3vdw);
	outfile->Printf("\tSOS-PI-MP3 Total Energy (a.u.)     : %20.14f\n", Esospimp3);
	outfile->Printf("\t3rd Order Energy (a.u.)            : %20.14f\n", Emp3-Emp2);
	outfile->Printf("\tMP3 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP3 Total Energy (a.u.)            : %20.14f\n", Emp3);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	


	outfile->Printf("\n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\t================ OMP3 FINAL RESULTS ========================================== \n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tSCS-OMP3 Total Energy (a.u.)       : %20.14f\n", Escsmp3);
	outfile->Printf("\tSOS-OMP3 Total Energy (a.u.)       : %20.14f\n", Esosmp3);
	outfile->Printf("\tSCSN-OMP3 Total Energy (a.u.)      : %20.14f\n", Escsnmp3);
	outfile->Printf("\tSCS-OMP3-VDW Total Energy (a.u.    : %20.14f\n", Escsmp3vdw);
	outfile->Printf("\tSOS-PI-OMP3 Total Energy (a.u.)    : %20.14f\n", Esospimp3);
	outfile->Printf("\tOMP3 Correlation Energy (a.u.)     : %20.14f\n", Emp3L-Escf);
	outfile->Printf("\tEomp3 - Eref (a.u.)                : %20.14f\n", Emp3L-Eref);
	outfile->Printf("\tOMP3 Total Energy (a.u.)           : %20.14f\n", Emp3L);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n");
	
	
	// Set the global variables with the energies
	Process::environment.globals["OMP3 TOTAL ENERGY"] = Emp3L;
	Process::environment.globals["SCS-OMP3 TOTAL ENERGY"] =  Escsmp3;
	Process::environment.globals["SOS-OMP3 TOTAL ENERGY"] =  Esosmp3;
	Process::environment.globals["SCSN-OMP3 TOTAL ENERGY"] = Escsnmp3;
	Process::environment.globals["SCS-OMP3-VDW TOTAL ENERGY"] = Escsmp3vdw;
	Process::environment.globals["SOS-PI-OMP3 TOTAL ENERGY"] = Esospimp3;
	Process::environment.globals["CURRENT ENERGY"] = Emp3L;
	Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;
	Process::environment.globals["CURRENT CORRELATION ENERGY"] = Emp3L-Escf;

	Process::environment.globals["OMP3 CORRELATION ENERGY"] = Emp3L - Escf;
	Process::environment.globals["SCS-OMP3 CORRELATION ENERGY"] =  Escsmp3 - Escf;
	Process::environment.globals["SOS-OMP3 CORRELATION ENERGY"] =  Esosmp3 - Escf;
	Process::environment.globals["SCSN-OMP3 CORRELATION ENERGY"] = Escsnmp3 - Escf;
	Process::environment.globals["SCS-OMP3-VDW CORRELATION ENERGY"] = Escsmp3vdw - Escf;
	Process::environment.globals["SOS-PI-OMP3 CORRELATION ENERGY"] = Esospimp3 - Escf;

        // if scs on	
	if (do_scs == "TRUE") {
	    if (scs_type_ == "SCS") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsmp3;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsmp3 - Escf;
            }

	    else if (scs_type_ == "SCSN") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsnmp3;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsnmp3 - Escf;
            }

	    else if (scs_type_ == "SCSVDW") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsmp3vdw;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsmp3vdw - Escf;
            }
	}
    
        // else if sos on	
	else if (do_sos == "TRUE") {
	     if (sos_type_ == "SOS") {
	         Process::environment.globals["CURRENT ENERGY"] = Esosmp3;
	         Process::environment.globals["CURRENT CORRELATION ENERGY"] = Esosmp3 - Escf;
             }

	     else if (sos_type_ == "SOSPI") {
	             Process::environment.globals["CURRENT ENERGY"] = Esospimp3;
	             Process::environment.globals["CURRENT CORRELATION ENERGY"] = Esospimp3 - Escf;
             }
	}

	if (natorb == "TRUE") nbo();
	if (occ_orb_energy == "TRUE") semi_canonic(); 

        // Compute Analytic Gradients
        if (dertype == "FIRST") {
            time4grad = 1;
	    outfile->Printf("\tAnalytic gradient computation is starting...\n");
	    
            coord_grad();
	    outfile->Printf("\tNecessary information has been sent to DERIV, which will take care of the rest.\n");
	    
        }

  }// end if (conver == 1)
  */
}// end omp3_manager 

//======================================================================
//             MP3 Manager
//======================================================================             
void DFOCC::mp3_manager()
{
        /*
        time4grad = 0;// means i will not compute the gradient
        timer_on("trans_ints");
	if (reference_ == "RESTRICTED") trans_ints_rhf();  
	else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
        timer_off("trans_ints");
        Eref = Escf;
        timer_on("T2(1)");
	omp3_t2_1st_sc();
        timer_off("T2(1)");
        timer_on("MP2 Energy");
	omp3_mp2_energy();
        timer_off("MP2 Energy");

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using SCF MOs (Canonical MP2)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;
	Process::environment.globals["SCS-MP2-VDW TOTAL ENERGY"] = Escsmp2vdw;
	Process::environment.globals["SOS-PI-MP2 TOTAL ENERGY"] = Esospimp2;

        Process::environment.globals["MP2 CORRELATION ENERGY"] = Emp2 - Escf;
        Process::environment.globals["SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
        Process::environment.globals["SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
        Process::environment.globals["SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;
        Process::environment.globals["SCS-MP2-VDW CORRELATION ENERGY"] = Escsmp2vdw - Escf;
        Process::environment.globals["SOS-PI-MP2 CORRELATION ENERGY"] = Esospimp2 - Escf;

        Process::environment.globals["MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        timer_on("T2(2)");
	t2_2nd_sc();
        timer_off("T2(2)");
        timer_on("MP3 Energy");
	mp3_energy();
        timer_off("MP3 Energy");
	Emp3L=Emp3;
        EcorrL=Emp3L-Escf;
	Emp3L_old=Emp3;
        if (ip_poles == "TRUE") omp3_ip_poles();
	
	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP3 energy using SCF MOs (Canonical MP3)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp3AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp3AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp3BB);
	outfile->Printf("\tMP2.5 Correlation Energy (a.u.)    : %20.14f\n", (Emp2 - Escf) + 0.5 * (Emp3-Emp2));
	outfile->Printf("\tMP2.5 Total Energy (a.u.)          : %20.14f\n", 0.5 * (Emp3+Emp2));
	outfile->Printf("\tSCS-MP3 Total Energy (a.u.)        : %20.14f\n", Escsmp3);
	outfile->Printf("\tSOS-MP3 Total Energy (a.u.)        : %20.14f\n", Esosmp3);
	outfile->Printf("\tSCSN-MP3 Total Energy (a.u.)       : %20.14f\n", Escsnmp3);
	outfile->Printf("\tSCS-MP3-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp3vdw);
	outfile->Printf("\tSOS-PI-MP3 Total Energy (a.u.)     : %20.14f\n", Esospimp3);
	outfile->Printf("\t3rd Order Energy (a.u.)            : %20.14f\n", Emp3-Emp2);
	outfile->Printf("\tMP3 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP3 Total Energy (a.u.)            : %20.14f\n", Emp3);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["CURRENT ENERGY"] = Emp3;
	Process::environment.globals["CURRENT CORRELATION ENERGY"] = Emp3 - Escf;
	Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;

	Process::environment.globals["MP3 TOTAL ENERGY"] = Emp3;
	Process::environment.globals["SCS-MP3 TOTAL ENERGY"] = Escsmp3;
	Process::environment.globals["SOS-MP3 TOTAL ENERGY"] = Esosmp3;
	Process::environment.globals["SCSN-MP3 TOTAL ENERGY"] = Escsnmp3;
	Process::environment.globals["SCS-MP3-VDW TOTAL ENERGY"] = Escsmp3vdw;
	Process::environment.globals["SOS-PI-MP3 TOTAL ENERGY"] = Esospimp3;

	Process::environment.globals["MP2.5 CORRELATION ENERGY"] = (Emp2 - Escf) + 0.5 * (Emp3-Emp2);
	Process::environment.globals["MP2.5 TOTAL ENERGY"] = 0.5 * (Emp3+Emp2);
	Process::environment.globals["MP3 CORRELATION ENERGY"] = Emp3 - Escf;
	Process::environment.globals["SCS-MP3 CORRELATION ENERGY"] = Escsmp3 - Escf;
	Process::environment.globals["SOS-MP3 CORRELATION ENERGY"] = Esosmp3 - Escf;
	Process::environment.globals["SCSN-MP3 CORRELATION ENERGY"] = Escsnmp3 - Escf;
	Process::environment.globals["SCS-MP3-VDW CORRELATION ENERGY"] = Escsmp3vdw - Escf;
	Process::environment.globals["SOS-PI-MP3 CORRELATION ENERGY"] = Esospimp3 - Escf;

        // if scs on	
	if (do_scs == "TRUE") {
	    if (scs_type_ == "SCS") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsmp3;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsmp3 - Escf;
            }

	    else if (scs_type_ == "SCSN") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsnmp3;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsnmp3 - Escf;
            }

	    else if (scs_type_ == "SCSVDW") {
	       Process::environment.globals["CURRENT ENERGY"] = Escsmp3vdw;
	       Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escsmp3vdw - Escf;
            }
	}
    
        // else if sos on	
	else if (do_sos == "TRUE") {
	     if (sos_type_ == "SOS") {
	         Process::environment.globals["CURRENT ENERGY"] = Esosmp3;
	         Process::environment.globals["CURRENT CORRELATION ENERGY"] = Esosmp3 - Escf;
             }

	     else if (sos_type_ == "SOSPI") {
	             Process::environment.globals["CURRENT ENERGY"] = Esospimp3;
	             Process::environment.globals["CURRENT CORRELATION ENERGY"] = Esospimp3 - Escf;
             }
	}

        // Compute Analytic Gradients
        if (dertype == "FIRST" || ekt_ip_ == "TRUE") {
            time4grad = 1;
	    outfile->Printf("\tAnalytic gradient computation is starting...\n");
            outfile->Printf("\tComputing response density matrices...\n");
            
	    omp3_response_pdms();
            outfile->Printf("\tComputing off-diagonal blocks of GFM...\n");
            
	    gfock();
            outfile->Printf("\tForming independent-pairs...\n");
            
	    idp2();
            outfile->Printf("\tComputing orbital gradient...\n");
            
	    mograd();
            coord_grad();

            if (ekt_ip_ == "TRUE") {
                ekt_ip();
            }

            else if (ekt_ip_ == "FALSE") {
	        outfile->Printf("\tNecessary information has been sent to DERIV, which will take care of the rest.\n");
	        
            }

        }
        */

}// end mp3_manager 


//======================================================================
//             OCEPA Manager
//======================================================================             
void DFOCC::ocepa_manager()
{
        /*
	mo_optimized = 0;// means MOs are not optimized yet.
	orbs_already_opt = 0;
	orbs_already_sc = 0;
        time4grad = 0;// means i will not compute the gradient
        timer_on("trans_ints");
	if (reference_ == "RESTRICTED") trans_ints_rhf();  
	else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
        timer_off("trans_ints");
        timer_on("REF Energy");
	ref_energy();
        timer_off("REF Energy");
        timer_on("T2(1)");
	ocepa_t2_1st_sc();
        timer_off("T2(1)");
        timer_on("MP2 Energy");
	ocepa_mp2_energy();
        timer_off("MP2 Energy");
        Ecepa = Emp2;
	EcepaL = Ecepa;
        EcorrL = Ecorr;
	EcepaL_old = Ecepa;

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using SCF MOs (Canonical MP2)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	
	Process::environment.globals["MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;
	Process::environment.globals["SCS-MP2-VDW TOTAL ENERGY"] = Escsmp2vdw;
	Process::environment.globals["SOS-PI-MP2 TOTAL ENERGY"] = Esospimp2;

	Process::environment.globals["MP2 CORRELATION ENERGY"] = Emp2 - Escf;
	Process::environment.globals["SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
	Process::environment.globals["SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
	Process::environment.globals["SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;
	Process::environment.globals["SCS-MP2-VDW CORRELATION ENERGY"] = Escsmp2vdw - Escf;
	Process::environment.globals["SOS-PI-MP2 CORRELATION ENERGY"] = Esospimp2 - Escf;

        Process::environment.globals["MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

	ocepa_response_pdms();
	gfock();
	idp();
	mograd();
        if (rms_wog > tol_grad) occ_iterations();
        else {
           orbs_already_opt = 1;
	   outfile->Printf("\n\tOrbitals are already optimized, switching to the canonical CEPA computation... \n");
	   
           cepa_iterations();
        }
	
        if (rms_wog <= tol_grad && fabs(DE) >= tol_Eod) {
           orbs_already_opt = 1;
	   if (conver == 1) outfile->Printf("\n\tOrbitals are optimized now.\n");
	   else if (conver == 0) { 
                    outfile->Printf("\n\tMAX MOGRAD did NOT converged, but RMS MOGRAD converged!!!\n");
	            outfile->Printf("\tI will consider the present orbitals as optimized.\n");
           }
	   outfile->Printf("\tSwitching to the standard CEPA computation... \n");
	   
           ref_energy();
	   cepa_energy();
           Ecepa_old = EcepaL;
           cepa_iterations();
           if (dertype == "FIRST") {
               ocepa_response_pdms();
	       gfock();
           }
        } 

  if (conver == 1) {
        ref_energy();
	cepa_energy();
        if (orbs_already_opt == 1) EcepaL = Ecepa;

        // EKT
        if (ekt_ip_ == "TRUE") { 
            if (orbs_already_opt == 1) {
	        ocepa_response_pdms();
	        gfock();
            }
            gfock_diag();
            if (ekt_ip_ == "TRUE") ekt_ip();
        }

	outfile->Printf("\n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\t================ OCEPA FINAL RESULTS ========================================= \n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tSCS-OCEPA(0) Total Energy (a.u.)   : %20.14f\n", Escscepa);
	outfile->Printf("\tSOS-OCEPA(0) Total Energy (a.u.)   : %20.14f\n", Esoscepa);
	outfile->Printf("\tOCEPA(0) Correlation Energy (a.u.) : %20.14f\n", EcepaL-Escf);
	outfile->Printf("\tEocepa - Eref (a.u.)               : %20.14f\n", EcepaL-Eref);
	outfile->Printf("\tOCEPA(0) Total Energy (a.u.)       : %20.14f\n", EcepaL);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n");
	
	
	// Set the global variables with the energies
	Process::environment.globals["OCEPA(0) TOTAL ENERGY"] = EcepaL;
	Process::environment.globals["SCS-OCEPA(0) TOTAL ENERGY"] =  Escscepa;
	Process::environment.globals["SOS-OCEPA(0) TOTAL ENERGY"] =  Esoscepa;
	Process::environment.globals["CURRENT ENERGY"] = EcepaL;
	Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;
	Process::environment.globals["CURRENT CORRELATION ENERGY"] = EcepaL-Escf;

	Process::environment.globals["OCEPA(0) CORRELATION ENERGY"] = EcepaL - Escf;
	Process::environment.globals["SCS-OCEPA(0) CORRELATION ENERGY"] =  Escscepa - Escf;
	Process::environment.globals["SOS-OCEPA(0) CORRELATION ENERGY"] =  Esoscepa - Escf;

        // if scs on	
	if (do_scs == "TRUE") {
	    Process::environment.globals["CURRENT ENERGY"] = Escscepa;
	    Process::environment.globals["CURRENT CORRELATION ENERGY"] = Escscepa - Escf;

	}
    
        // else if sos on	
	else if (do_sos == "TRUE") {
	         Process::environment.globals["CURRENT ENERGY"] = Esoscepa;
	         Process::environment.globals["CURRENT CORRELATION ENERGY"] = Esoscepa - Escf;
	}

	if (natorb == "TRUE") nbo();
	if (occ_orb_energy == "TRUE") semi_canonic(); 
	
        // Compute Analytic Gradients
        if (dertype == "FIRST") {
            time4grad = 1;
	    outfile->Printf("\tAnalytic gradient computation is starting...\n");
	    
            coord_grad();
	    outfile->Printf("\tNecessary information has been sent to DERIV, which will take care of the rest.\n");
	    
        }

  }// end if (conver == 1)
  */
}// end ocepa_manager 


//======================================================================
//             CEPA Manager
//======================================================================             
void DFOCC::cepa_manager()
{
        /*
        timer_on("trans_ints");
	if (reference_ == "RESTRICTED") trans_ints_rhf();  
	else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
        timer_off("trans_ints");
        Eref = Escf;
        timer_on("T2(1)");
	ocepa_t2_1st_sc();
        timer_off("T2(1)");
        timer_on("MP2 Energy");
	ocepa_mp2_energy();
        timer_off("MP2 Energy");
        Ecepa = Emp2;
	Ecepa_old = Emp2;

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using SCF MOs (Canonical MP2)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	
	Process::environment.globals["MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;
	Process::environment.globals["SCS-MP2-VDW TOTAL ENERGY"] = Escsmp2vdw;
	Process::environment.globals["SOS-PI-MP2 TOTAL ENERGY"] = Esospimp2;

	Process::environment.globals["MP2 CORRELATION ENERGY"] = Emp2 - Escf;
	Process::environment.globals["SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
	Process::environment.globals["SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
	Process::environment.globals["SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;
	Process::environment.globals["SCS-MP2-VDW CORRELATION ENERGY"] = Escsmp2vdw - Escf;
	Process::environment.globals["SOS-PI-MP2 CORRELATION ENERGY"] = Esospimp2 - Escf;

        Process::environment.globals["MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        // Perform CEPA iterations
        cepa_iterations();

	outfile->Printf("\n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\t================ CEPA FINAL RESULTS ========================================== \n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tCEPA(0) Correlation Energy (a.u.)  : %20.14f\n", Ecorr);
	outfile->Printf("\tCEPA(0) Total Energy (a.u.)        : %20.14f\n", Ecepa);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n");
	
	
	// Set the global variables with the energies
	Process::environment.globals["CEPA(0) TOTAL ENERGY"] = Ecepa;
	Process::environment.globals["CURRENT ENERGY"] = Ecepa;
	Process::environment.globals["CURRENT REFERENCE ENERGY"] = Eref;
	Process::environment.globals["CURRENT CORRELATION ENERGY"] = Ecorr;
        //EcepaL = Ecepa;
        
        // Compute Analytic Gradients
        if (dertype == "FIRST" || ekt_ip_ == "TRUE") {
            time4grad = 1;
	    outfile->Printf("\tAnalytic gradient computation is starting...\n");
            outfile->Printf("\tComputing response density matrices...\n");
            
	    ocepa_response_pdms();
            outfile->Printf("\tComputing off-diagonal blocks of GFM...\n");
            
	    gfock();
            outfile->Printf("\tForming independent-pairs...\n");
            
	    idp2();
            outfile->Printf("\tComputing orbital gradient...\n");
            
	    mograd();
            coord_grad();

            if (ekt_ip_ == "TRUE") {
                ekt_ip();
            }

            else if (ekt_ip_ == "FALSE") {
	        outfile->Printf("\tNecessary information has been sent to DERIV, which will take care of the rest.\n");
	        
            }

        }
        */
}// end cepa_manager 


//======================================================================
//             OMP2.5 Manager
//======================================================================             
void DFOCC::omp2_5_manager()
{
        /*
	mo_optimized = 0;
	orbs_already_opt = 0;
	orbs_already_sc = 0;
        time4grad = 0;// means i will not compute the gradient
        timer_on("trans_ints");
	if (reference_ == "RESTRICTED") trans_ints_rhf();  
	else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
        timer_off("trans_ints");
        timer_on("REF Energy");
	ref_energy();
        timer_off("REF Energy");
        timer_on("T2(1)");
	omp3_t2_1st_sc();
        timer_off("T2(1)");
        timer_on("MP2 Energy");
	omp3_mp2_energy();
        timer_off("MP2 Energy");

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using SCF MOs (Canonical MP2)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;
	Process::environment.globals["SCS-MP2-VDW TOTAL ENERGY"] = Escsmp2vdw;
	Process::environment.globals["SOS-PI-MP2 TOTAL ENERGY"] = Esospimp2;

	Process::environment.globals["MP2 CORRELATION ENERGY"] = Emp2 - Escf;
	Process::environment.globals["SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
	Process::environment.globals["SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
	Process::environment.globals["SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;
	Process::environment.globals["SCS-MP2-VDW CORRELATION ENERGY"] = Escsmp2vdw - Escf;
	Process::environment.globals["SOS-PI-MP2 CORRELATION ENERGY"] = Esospimp2 - Escf;

        Process::environment.globals["MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        timer_on("T2(2)");
	t2_2nd_sc();
        timer_off("T2(2)");
        timer_on("MP3 Energy");
	mp3_energy();
        timer_off("MP3 Energy");
	Emp3L=Emp3;
        EcorrL=Emp3L-Escf;
	Emp3L_old=Emp3;
        if (ip_poles == "TRUE") omp3_ip_poles();
	
	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2.5 energy using SCF MOs (Canonical MP2.5)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp3AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp3AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp3BB);
	outfile->Printf("\t0.5 Energy Correction (a.u.)       : %20.14f\n", Emp3-Emp2);
	outfile->Printf("\tMP2.5 Correlation Energy (a.u.)    : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2.5 Total Energy (a.u.)          : %20.14f\n", Emp3);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["MP2.5 TOTAL ENERGY"] = Emp3;

	omp3_response_pdms();
	gfock();
        //ccl_energy();
	idp();
	mograd();
        occ_iterations();
	
        if (rms_wog <= tol_grad && fabs(DE) >= tol_Eod) {
           orbs_already_opt = 1;
	   if (conver == 1) outfile->Printf("\n\tOrbitals are optimized now.\n");
	   else if (conver == 0) { 
                    outfile->Printf("\n\tMAX MOGRAD did NOT converged, but RMS MOGRAD converged!!!\n");
	            outfile->Printf("\tI will consider the present orbitals as optimized.\n");
           }
	   outfile->Printf("\tSwitching to the standard MP2.5 computation after semicanonicalization of the MOs... \n");
	   
	   semi_canonic();
	   if (reference_ == "RESTRICTED") trans_ints_rhf();  
	   else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
	   omp3_t2_1st_sc();
	   t2_2nd_sc();
           conver = 1;
           if (dertype == "FIRST") {
               omp3_response_pdms();
	       gfock();
           }
        }     

  if (conver == 1) {
        ref_energy();
	omp3_mp2_energy();
	mp3_energy();
        if (orbs_already_opt == 1) Emp3L = Emp3;

        if (ip_poles == "TRUE") {
	   if (orbs_already_sc == 0) {
               semi_canonic();
	       if (reference_ == "RESTRICTED") trans_ints_rhf();  
	       else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
	       omp3_t2_1st_sc();
	       t2_2nd_sc();
           }
           omp3_ip_poles();
        }

        // EKT
        if (ekt_ip_ == "TRUE") { 
            if (orbs_already_sc == 1) {
	        omp3_response_pdms();
	        gfock();
            }
            gfock_diag();
            if (ekt_ip_ == "TRUE") ekt_ip();
        }

        outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using optimized MOs... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2.5 energy using optimized MOs... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp3AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp3AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp3BB);
	outfile->Printf("\t0.5 Energy Correction (a.u.)       : %20.14f\n", Emp3-Emp2);
	outfile->Printf("\tMP2.5 Correlation Energy (a.u.)    : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2.5 Total Energy (a.u.)          : %20.14f\n", Emp3);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	


	outfile->Printf("\n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\t================ OMP2.5 FINAL RESULTS ======================================== \n");
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tOMP2.5 Correlation Energy (a.u.)   : %20.14f\n", Emp3L-Escf);
	outfile->Printf("\tEomp2.5 - Eref (a.u.)              : %20.14f\n", Emp3L-Eref);
	outfile->Printf("\tOMP2.5 Total Energy (a.u.)         : %20.14f\n", Emp3L);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n");
	
	
	// Set the global variables with the energies
	Process::environment.globals["OMP2.5 TOTAL ENERGY"] = Emp3L;
	Process::environment.globals["OMP2.5 CORRELATION ENERGY"] = Emp3L - Escf;
	Process::environment.globals["CURRENT ENERGY"] = Emp3L;
	Process::environment.globals["CURRENT REFERENCE ENERGY"] = Escf;
	Process::environment.globals["CURRENT CORRELATION ENERGY"] = Emp3L-Escf;

	if (natorb == "TRUE") nbo();
	if (occ_orb_energy == "TRUE") semi_canonic(); 

        // Compute Analytic Gradients
        if (dertype == "FIRST") {
            time4grad = 1;
	    outfile->Printf("\tAnalytic gradient computation is starting...\n");
	    
            coord_grad();
	    outfile->Printf("\tNecessary information has been sent to DERIV, which will take care of the rest.\n");
	    
        }

  }// end if (conver == 1)
  */
}// end omp2.5_manager 


//======================================================================
//             MP2.5 Manager
//======================================================================             
void DFOCC::mp2_5_manager()
{
        /*
        time4grad = 0;// means i will not compute the gradient
        timer_on("trans_ints");
	if (reference_ == "RESTRICTED") trans_ints_rhf();  
	else if (reference_ == "UNRESTRICTED") trans_ints_uhf();  
        timer_off("trans_ints");
        Eref = Escf;
        timer_on("T2(1)");
	omp3_t2_1st_sc();
        timer_off("T2(1)");
        timer_on("MP2 Energy");
	omp3_mp2_energy();
        timer_off("MP2 Energy");

	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2 energy using SCF MOs (Canonical MP2)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp2AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp2AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp2BB);
	outfile->Printf("\tScaled_SS Correlation Energy (a.u.): %20.14f\n", Escsmp2AA+Escsmp2BB);
	outfile->Printf("\tScaled_OS Correlation Energy (a.u.): %20.14f\n", Escsmp2AB);
	outfile->Printf("\tSCS-MP2 Total Energy (a.u.)        : %20.14f\n", Escsmp2);
	outfile->Printf("\tSOS-MP2 Total Energy (a.u.)        : %20.14f\n", Esosmp2);
	outfile->Printf("\tSCSN-MP2 Total Energy (a.u.)       : %20.14f\n", Escsnmp2);
	outfile->Printf("\tSCS-MP2-VDW Total Energy (a.u.)    : %20.14f\n", Escsmp2vdw);
	outfile->Printf("\tSOS-PI-MP2 Total Energy (a.u.)     : %20.14f\n", Esospimp2);
	outfile->Printf("\tMP2 Correlation Energy (a.u.)      : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2 Total Energy (a.u.)            : %20.14f\n", Emp2);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["MP2 TOTAL ENERGY"] = Emp2;
	Process::environment.globals["SCS-MP2 TOTAL ENERGY"] = Escsmp2;
	Process::environment.globals["SOS-MP2 TOTAL ENERGY"] = Esosmp2;
	Process::environment.globals["SCSN-MP2 TOTAL ENERGY"] = Escsnmp2;
	Process::environment.globals["SCS-MP2-VDW TOTAL ENERGY"] = Escsmp2vdw;
	Process::environment.globals["SOS-PI-MP2 TOTAL ENERGY"] = Esospimp2;

	Process::environment.globals["MP2 CORRELATION ENERGY"] = Emp2 - Escf;
	Process::environment.globals["SCS-MP2 CORRELATION ENERGY"] = Escsmp2 - Escf;
	Process::environment.globals["SOS-MP2 CORRELATION ENERGY"] = Esosmp2 - Escf;
	Process::environment.globals["SCSN-MP2 CORRELATION ENERGY"] = Escsnmp2 - Escf;
	Process::environment.globals["SCS-MP2-VDW CORRELATION ENERGY"] = Escsmp2vdw - Escf;
	Process::environment.globals["SOS-PI-MP2 CORRELATION ENERGY"] = Esospimp2 - Escf;

        Process::environment.globals["MP2 OPPOSITE-SPIN CORRELATION ENERGY"] = Emp2AB;
        Process::environment.globals["MP2 SAME-SPIN CORRELATION ENERGY"] = Emp2AA+Emp2BB;

        timer_on("T2(2)");
	t2_2nd_sc();
        timer_off("T2(2)");
        timer_on("MP3 Energy");
	mp3_energy();
        timer_off("MP3 Energy");
	Emp3L=Emp3;
        EcorrL=Emp3L-Escf;
	Emp3L_old=Emp3;
        if (ip_poles == "TRUE") omp3_ip_poles();
	
	outfile->Printf("\n"); 
	outfile->Printf("\tComputing MP2.5 energy using SCF MOs (Canonical MP2.5)... \n"); 
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\tNuclear Repulsion Energy (a.u.)    : %20.14f\n", Enuc);
	outfile->Printf("\tSCF Energy (a.u.)                  : %20.14f\n", Escf);
	outfile->Printf("\tREF Energy (a.u.)                  : %20.14f\n", Eref);
	outfile->Printf("\tAlpha-Alpha Contribution (a.u.)    : %20.14f\n", Emp3AA);
	outfile->Printf("\tAlpha-Beta Contribution (a.u.)     : %20.14f\n", Emp3AB);
	outfile->Printf("\tBeta-Beta Contribution (a.u.)      : %20.14f\n", Emp3BB);
	outfile->Printf("\t0.5 Energy Correction (a.u.)       : %20.14f\n", Emp3-Emp2);
	outfile->Printf("\tMP2.5 Correlation Energy (a.u.)    : %20.14f\n", Ecorr);
	outfile->Printf("\tMP2.5 Total Energy (a.u.)          : %20.14f\n", Emp3);
	outfile->Printf("\t============================================================================== \n");
	outfile->Printf("\n"); 
	
	Process::environment.globals["MP2.5 TOTAL ENERGY"] = Emp3;
	Process::environment.globals["MP2.5 CORRELATION ENERGY"] = Emp3 - Escf;
	Process::environment.globals["CURRENT ENERGY"] = Emp3L;
	Process::environment.globals["CURRENT REFERENCE ENERGY"] = Eref;
	Process::environment.globals["CURRENT CORRELATION ENERGY"] = Emp3L-Escf;

        // Compute Analytic Gradients
        if (dertype == "FIRST" || ekt_ip_ == "TRUE") {
            time4grad = 1;
	    outfile->Printf("\tAnalytic gradient computation is starting...\n");
            outfile->Printf("\tComputing response density matrices...\n");
            
	    omp3_response_pdms();
            outfile->Printf("\tComputing off-diagonal blocks of GFM...\n");
            
	    gfock();
            outfile->Printf("\tForming independent-pairs...\n");
            
	    idp2();
            outfile->Printf("\tComputing orbital gradient...\n");
            
	    mograd();
            coord_grad();

            if (ekt_ip_ == "TRUE") {
                ekt_ip();
            }

            else if (ekt_ip_ == "FALSE"") {
	        outfile->Printf("\tNecessary information has been sent to DERIV, which will take care of the rest.\n");
	        
            }

        }
        */

}// end omp2.5_manager 


}} // End Namespaces


