#include "dcft.h"
#include <libdpd/dpd.h>
#include <libtrans/integraltransform.h>
#include <libdiis/diismanager.h>
#include "defines.h"

using namespace boost;

namespace psi{ namespace dcft{

/**
 * Computes the DCFT density matrix and energy
 */
double
DCFTSolver::compute_energy()
{
    bool scfDone    = false;
    bool lambdaDone = false;
    bool densityConverged = false;
    bool energyConverged = false;
    double oldEnergy;
    scf_guess();
    mp2_guess();

    int cycle = 0;
    fprintf(outfile, "\n\n\t*=================================================================================*\n"
                     "\t* Cycle  RMS [F, Kappa]   RMS Lambda Error   delta E        Total Energy     DIIS *\n"
                     "\t*---------------------------------------------------------------------------------*\n");
    if(options_.get_str("ALGORITHM") == "TWOSTEP"){
        // This is the two-step update - in each macro iteration, update the orbitals first, then update lambda
        // to self-consistency, until converged.  When lambda is converged and only one scf cycle is needed to reach
        // the desired cutoff, we're done
        SharedMatrix tmp = boost::shared_ptr<Matrix>(new Matrix("temp", nirrep_, nsopi_, nsopi_));
        // Set up the DIIS manager
        dpdbuf4 Laa, Lab, Lbb;
        dpd_buf4_init(&Laa, PSIF_LIBTRANS_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                      ID("[O,O]"), ID("[V,V]"), 0, "Lambda <OO|VV>");
        dpd_buf4_init(&Lab, PSIF_LIBTRANS_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                      ID("[O,o]"), ID("[V,v]"), 0, "Lambda <Oo|Vv>");
        dpd_buf4_init(&Lbb, PSIF_LIBTRANS_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                      ID("[o,o]"), ID("[v,v]"), 0, "Lambda <oo|vv>");
        DIISManager scfDiisManager(maxdiis_, "DCFT DIIS Orbitals",DIISManager::LargestError,DIISManager::InCore);
        scfDiisManager.set_error_vector_size(2, DIISEntry::Matrix, scf_error_a_.get(),
                                                DIISEntry::Matrix, scf_error_b_.get());
        scfDiisManager.set_vector_size(2, DIISEntry::Matrix, Fa_.get(),
                                          DIISEntry::Matrix, Fb_.get());
        DIISManager lambdaDiisManager(maxdiis_, "DCFT DIIS Lambdas",DIISManager::LargestError,DIISManager::InCore);
        lambdaDiisManager.set_error_vector_size(3, DIISEntry::DPDBuf4, &Laa,
                                                   DIISEntry::DPDBuf4, &Lab,
                                                   DIISEntry::DPDBuf4, &Lbb);
        lambdaDiisManager.set_vector_size(3, DIISEntry::DPDBuf4, &Laa,
                                             DIISEntry::DPDBuf4, &Lab,
                                             DIISEntry::DPDBuf4, &Lbb);
        dpd_buf4_close(&Laa);
        dpd_buf4_close(&Lab);
        dpd_buf4_close(&Lbb);
        old_ca_->copy(Ca_);
        old_cb_->copy(Cb_);
        // Just so the correct value is printed in the first macro iteration
        scf_convergence_ = compute_scf_error_vector();
        // The macro-iterations
        while((!scfDone || !lambdaDone) && cycle++ < maxiter_){
            // The lambda iterations
            int nLambdaIterations = 0;
            lambdaDiisManager.reset_subspace();
            fprintf(outfile, "\t                          *** Macro Iteration %d ***\n"
                             "\tCumulant Iterations\n",cycle);
            lambdaDone = false;
            while(!lambdaDone && nLambdaIterations++ < options_.get_int("LAMBDA_MAXITER")){
                std::string diisString;
                build_tensors();
                build_intermediates();
                lambda_convergence_ = compute_lambda_residual();
                update_lambda_from_residual();
                if(lambda_convergence_ < diis_start_thresh_){
                    //Store the DIIS vectors
                    dpdbuf4 Laa, Lab, Lbb, Raa, Rab, Rbb, J;
                    dpd_buf4_init(&Raa, PSIF_DCFT_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                                  ID("[O,O]"), ID("[V,V]"), 0, "R <OO|VV>");
                    dpd_buf4_init(&Rab, PSIF_DCFT_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                                  ID("[O,o]"), ID("[V,v]"), 0, "R <Oo|Vv>");
                    dpd_buf4_init(&Rbb, PSIF_DCFT_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                                  ID("[o,o]"), ID("[v,v]"), 0, "R <oo|vv>");
                    dpd_buf4_init(&Laa, PSIF_DCFT_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                                  ID("[O,O]"), ID("[V,V]"), 0, "Lambda <OO|VV>");
                    dpd_buf4_init(&Lab, PSIF_DCFT_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                                  ID("[O,o]"), ID("[V,v]"), 0, "Lambda <Oo|Vv>");
                    dpd_buf4_init(&Lbb, PSIF_DCFT_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                                  ID("[o,o]"), ID("[v,v]"), 0, "Lambda <oo|vv>");

    //                    dpd_buf4_init(&J, PSIF_DCFT_DPD, 0, ID("[O,o]"), ID("[V,v]"),
    //                                  ID("[O,o]"), ID("[V,v]"), 0, "R <Oo|Vv>");
    //                    fprintf(outfile, "DOT = %f\n",dpd_buf4_dot(&Rab, &J));
    //                    dpd_buf4_close(&J);

                    if(lambdaDiisManager.add_entry(6, &Raa, &Rab, &Rbb, &Laa, &Lab, &Lbb)){
                        diisString += "S";
                    }
                    if(lambdaDiisManager.subspace_size() >= mindiisvecs_ && maxdiis_ > 0){
                        diisString += "/E";
                        lambdaDiisManager.extrapolate(3, &Laa, &Lab, &Lbb);
//                        lambdaDiisManager.reset_subspace();
//                    }else{
//                        update_lambda_from_residual();
                    }
                    dpd_buf4_close(&Raa);
                    dpd_buf4_close(&Rab);
                    dpd_buf4_close(&Rbb);
                    dpd_buf4_close(&Laa);
                    dpd_buf4_close(&Lab);
                    dpd_buf4_close(&Lbb);
//                }else{
//                    update_lambda_from_residual();
                }
                oldEnergy = new_total_energy_;
                compute_dcft_energy();
                lambdaDone = lambda_convergence_ < lambda_threshold_ &&
                                fabs(new_total_energy_ - oldEnergy) < lambda_threshold_;
                fprintf(outfile, "\t* %-3d   %12.3e      %12.3e   %12.3e  %21.15f  %-3s *\n",
                        nLambdaIterations, scf_convergence_, lambda_convergence_, new_total_energy_ - oldEnergy,
                        new_total_energy_, diisString.c_str());
                fflush(outfile);
            }
            build_tau();
            // Update the orbitals
            int nSCFCycles = 0;
            densityConverged = false;
            scfDiisManager.reset_subspace();
            fprintf(outfile, "\tOrbital Updates\n");
            while((!densityConverged || scf_convergence_ > scf_threshold_)
                    && nSCFCycles++ < options_.get_int("SCF_MAXITER")){
                std::string diisString;
                Fa_->copy(so_h_);
                Fb_->copy(so_h_);
                // This will build the new Fock matrix from the SO integrals
                process_so_ints();
                // The SCF energy has to be evaluated before adding Tau and orthonormalizing F
                oldEnergy = new_total_energy_;
#if REFACTORED
                Fa_->add(g_tau_a_);
                Fb_->add(g_tau_b_);
                compute_scf_energy();
                fprintf(outfile, "\n  !SCF ENERGY = %20.15f \n", scf_energy_);
#else
                compute_scf_energy();
                Fa_->add(g_tau_a_);
                Fb_->add(g_tau_b_);
                fprintf(outfile, "\n  !SCF ENERGY = %20.15f \n", scf_energy_);
#endif
                scf_convergence_ = compute_scf_error_vector();
                if(scf_convergence_ < diis_start_thresh_){
                    if(scfDiisManager.add_entry(4, scf_error_a_.get(), scf_error_b_.get(), Fa_.get(), Fb_.get()))
                        diisString += "S";
                }
                if(scfDiisManager.subspace_size() > mindiisvecs_){
                    diisString += "/E";
                    scfDiisManager.extrapolate(2, Fa_.get(), Fb_.get());
                }
                Fa_->transform(s_half_inv_);
                Fa_->diagonalize(tmp, epsilon_a_);
                old_ca_->copy(Ca_);
                Ca_->gemm(false, false, 1.0, s_half_inv_, tmp, 0.0);
                Fb_->transform(s_half_inv_);
                Fb_->diagonalize(tmp, epsilon_b_);
                old_cb_->copy(Cb_);
                Cb_->gemm(false, false, 1.0, s_half_inv_, tmp, 0.0);
                correct_mo_phases(false);
                if(!lock_occupation_) find_occupation(epsilon_a_);
                densityConverged = update_scf_density() < scf_threshold_;
                compute_dcft_energy();
                fprintf(outfile, "\t* %-3d   %12.3e      %12.3e   %12.3e  %21.15f  %-3s *\n",
                    nSCFCycles, scf_convergence_, lambda_convergence_, new_total_energy_ - oldEnergy,
                    new_total_energy_, diisString.c_str());
                fflush(outfile);
            }
            write_orbitals_to_checkpoint();
            scfDone = nSCFCycles == 1;
            transform_integrals();
        }
    }else{
        // This is the simultaneous orbital/lambda update algorithm
        SharedMatrix tmp = boost::shared_ptr<Matrix>(new Matrix("temp", nirrep_, nsopi_, nsopi_));
        // Set up the DIIS manager
        DIISManager diisManager(maxdiis_, "DCFT DIIS vectors");
        dpdbuf4 Laa, Lab, Lbb;
        dpd_buf4_init(&Laa, PSIF_LIBTRANS_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                      ID("[O,O]"), ID("[V,V]"), 0, "Lambda <OO|VV>");
        dpd_buf4_init(&Lab, PSIF_LIBTRANS_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                      ID("[O,o]"), ID("[V,v]"), 0, "Lambda <Oo|Vv>");
        dpd_buf4_init(&Lbb, PSIF_LIBTRANS_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                      ID("[o,o]"), ID("[v,v]"), 0, "Lambda <oo|vv>");
        diisManager.set_error_vector_size(5, DIISEntry::Matrix, scf_error_a_.get(),
                                             DIISEntry::Matrix, scf_error_b_.get(),
                                             DIISEntry::DPDBuf4, &Laa,
                                             DIISEntry::DPDBuf4, &Lab,
                                             DIISEntry::DPDBuf4, &Lbb);
        diisManager.set_vector_size(5, DIISEntry::Matrix, Fa_.get(),
                                       DIISEntry::Matrix, Fb_.get(),
                                       DIISEntry::DPDBuf4, &Laa,
                                       DIISEntry::DPDBuf4, &Lab,
                                       DIISEntry::DPDBuf4, &Lbb);
        dpd_buf4_close(&Laa);
        dpd_buf4_close(&Lab);
        dpd_buf4_close(&Lbb);
        while((!scfDone || !lambdaDone || !densityConverged || !energyConverged)
                && cycle++ < maxiter_){
            std::string diisString;
            oldEnergy = new_total_energy_;
            build_tau();
            Fa_->copy(so_h_);
            Fb_->copy(so_h_);
            // This will build the new Fock matrix from the SO integrals
            process_so_ints();
            // The SCF energy has to be evaluated before adding Tau and orthonormalizing F
#if REFACTORED
                Fa_->add(g_tau_a_);
                Fb_->add(g_tau_b_);
                compute_scf_energy();
                fprintf(outfile, "\n  !SCF ENERGY = %20.15f \n", scf_energy_);
#else
                compute_scf_energy();
                Fa_->add(g_tau_a_);
                Fb_->add(g_tau_b_);
                fprintf(outfile, "\n  !SCF ENERGY = %20.15f \n", scf_energy_);
#endif
            scf_convergence_ = compute_scf_error_vector();
            scfDone = scf_convergence_ < scf_threshold_;
            build_intermediates();
            lambda_convergence_ = compute_lambda_residual();
            lambdaDone = lambda_convergence_ < lambda_threshold_;
            update_lambda_from_residual();
            compute_dcft_energy();
            energyConverged = fabs(oldEnergy - new_total_energy_) < lambda_convergence_;
            if(scf_convergence_ < diis_start_thresh_ && lambda_convergence_ < diis_start_thresh_){
                //Store the DIIS vectors
                dpdbuf4 Laa, Lab, Lbb, Raa, Rab, Rbb;
                dpd_buf4_init(&Raa, PSIF_DCFT_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                              ID("[O,O]"), ID("[V,V]"), 0, "R <OO|VV>");
                dpd_buf4_init(&Rab, PSIF_DCFT_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                              ID("[O,o]"), ID("[V,v]"), 0, "R <Oo|Vv>");
                dpd_buf4_init(&Rbb, PSIF_DCFT_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                              ID("[o,o]"), ID("[v,v]"), 0, "R <oo|vv>");
                dpd_buf4_init(&Laa, PSIF_DCFT_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                              ID("[O,O]"), ID("[V,V]"), 0, "Lambda <OO|VV>");
                dpd_buf4_init(&Lab, PSIF_DCFT_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                              ID("[O,o]"), ID("[V,v]"), 0, "Lambda <Oo|Vv>");
                dpd_buf4_init(&Lbb, PSIF_DCFT_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                              ID("[o,o]"), ID("[v,v]"), 0, "Lambda <oo|vv>");
                if(diisManager.add_entry(10, scf_error_a_.get(), scf_error_b_.get(), &Raa, &Rab, &Rbb,
                                           Fa_.get(), Fb_.get(), &Laa, &Lab, &Lbb)){
                    diisString += "S";
                }
                if(diisManager.subspace_size() > mindiisvecs_){
                    diisString += "/E";
                    diisManager.extrapolate(5, Fa_.get(), Fb_.get(), &Laa, &Lab, &Lbb);
                }
                dpd_buf4_close(&Raa);
                dpd_buf4_close(&Rab);
                dpd_buf4_close(&Rbb);
                dpd_buf4_close(&Laa);
                dpd_buf4_close(&Lab);
                dpd_buf4_close(&Lbb);
            }
            Fa_->transform(s_half_inv_);
            Fa_->diagonalize(tmp, epsilon_a_);
            old_ca_->copy(Ca_);
            Ca_->gemm(false, false, 1.0, s_half_inv_, tmp, 0.0);
            Fb_->transform(s_half_inv_);
            Fb_->diagonalize(tmp, epsilon_b_);
            old_cb_->copy(Cb_);
            Cb_->gemm(false, false, 1.0, s_half_inv_, tmp, 0.0);
            if(!correct_mo_phases(false)){
                fprintf(outfile,"\t\tThere was a problem correcting the MO phases.\n"
                                "\t\tIf this does not converge, try ALGORITHM=TWOSTEP\n");
            }
            write_orbitals_to_checkpoint();
            transform_integrals();
            if(!lock_occupation_) find_occupation(epsilon_a_);
            densityConverged = update_scf_density() < scf_threshold_;
            // If we've performed enough lambda updates since the last orbitals
            // update, reset the counter so another SCF update is performed
            fprintf(outfile, "\t* %-3d   %12.3e      %12.3e   %12.3e  %21.15f  %-3s *\n",
                    cycle, scf_convergence_, lambda_convergence_, new_total_energy_ - oldEnergy,
                    new_total_energy_, diisString.c_str());
            fflush(outfile);
        }
    }
    if(!scfDone || !lambdaDone || !densityConverged)
        throw ConvergenceError<int>("DCFT", maxiter_, lambda_threshold_,
                               lambda_convergence_, __FILE__, __LINE__);

    Process::environment.globals["CURRENT ENERGY"] = new_total_energy_;
    Process::environment.globals["DCFT ENERGY"] = new_total_energy_;
    Process::environment.globals["DCFT SCF ENERGY"] = scf_energy_;

    fprintf(outfile, "\t*=================================================================================*\n");
    fprintf(outfile, "\n\t*DCFT SCF Energy            = %20.15f\n", scf_energy_);
    fprintf(outfile, "\t*DCFT Total Energy          = %20.15f\n", new_total_energy_);

    if(!options_.get_bool("RELAX_ORBITALS")){
        fprintf(outfile, "Warning!  The orbitals were not relaxed\n");
    }

    print_opdm();


    if(options_.get_bool("COMPUTE_TPDM")) dump_density();
    mulliken_charges();
    check_n_representability();

    if(!options_.get_bool("RELAX_ORBITALS") && options_.get_bool("IGNORE_TAU")){
        psio_->open(PSIF_LIBTRANS_DPD, PSIO_OPEN_OLD);
        /*
         * Comout the CEPA-0 correlation energy
         * E = 1/4 L_IJAB <IJ||AB>
         *        +L_IjAb <Ij|Ab>
         *    +1/4 L_ijab <ij||ab>
         */
        dpdbuf4 I, L;
        // Alpha - Alpha
        dpd_buf4_init(&I, PSIF_LIBTRANS_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                      ID("[O,O]"), ID("[V,V]"), 1, "MO Ints <OO|VV>");
        dpd_buf4_init(&L, PSIF_DCFT_DPD, 0, ID("[O,O]"), ID("[V,V]"),
                      ID("[O,O]"), ID("[V,V]"), 0, "Lambda <OO|VV>");
        double eAA = 0.25 * dpd_buf4_dot(&L, &I);
        dpd_buf4_close(&I);
        dpd_buf4_close(&L);

        // Alpha - Beta
        dpd_buf4_init(&I, PSIF_LIBTRANS_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                      ID("[O,o]"), ID("[V,v]"), 0, "MO Ints <Oo|Vv>");
        dpd_buf4_init(&L, PSIF_DCFT_DPD, 0, ID("[O,o]"), ID("[V,v]"),
                      ID("[O,o]"), ID("[V,v]"), 0, "Lambda <Oo|Vv>");
        double eAB = dpd_buf4_dot(&L, &I);
        dpd_buf4_close(&I);
        dpd_buf4_close(&L);

        // Beta - Beta
        dpd_buf4_init(&I, PSIF_LIBTRANS_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                      ID("[o,o]"), ID("[v,v]"), 1, "MO Ints <oo|vv>");
        dpd_buf4_init(&L, PSIF_DCFT_DPD, 0, ID("[o,o]"), ID("[v,v]"),
                      ID("[o,o]"), ID("[v,v]"), 0, "Lambda <oo|vv>");
        double eBB = 0.25 * dpd_buf4_dot(&L, &I);
        dpd_buf4_close(&I);
        dpd_buf4_close(&L);
        fprintf(outfile, "\t!CEPA0 Total Energy         = %20.15f\n",
                scf_energy_ + eAA + eAB + eBB);
        psio_->close(PSIF_LIBTRANS_DPD, 1);
    }

    // Free up memory and close files
    finalize();
    return(new_total_energy_);
}

}} // Namespaces

