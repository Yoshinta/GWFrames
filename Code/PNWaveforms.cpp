// Copyright (c) 2014, Michael Boyle
// See LICENSE file for details

#include <unistd.h>
#include <sys/param.h>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <gsl/gsl_odeiv2.h>
#include "PNWaveforms.hpp"
#include "PostNewtonian/C++/PNEvolution.hpp"
#include "PostNewtonian/C++/PNWaveformModes.hpp"
#include "Quaternions.hpp"
#include "Utilities.hpp"
#include "Errors.hpp"

// These macros are useful for debugging
#define INFOTOCERR std::cerr << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": "
#define INFOTOCOUT std::cout << __FILE__ << ":" << __LINE__ << ":" << __func__ << ": "

using std::vector;
using std::string;
using Quaternions::Quaternion;
using std::cerr;
using std::endl;

// Local utility functions
std::string VectorStringForm(const std::vector<double>& V) {
  std::stringstream S;
  S << "[" << std::setprecision(16);
  for(unsigned int i=0; i<V.size()-1; ++i) {
    S << V[i] << ", ";
  }
  S << V[V.size()-1] << "]";
  return S.str();
}
inline double dotproduct(const double* a, const double* b) {
  return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

/// Default constructor for an empty object
GWFrames::PNWaveform::PNWaveform() :
  Waveform(), mchi1(0), mchi2(0), mOmega_orb(0), mOmega_prec(0), mL(0), mPhi_orb(0)
{
  SetFrameType(GWFrames::Coorbital);
  SetDataType(GWFrames::h);
  SetRIsScaledOut(true);
  SetMIsScaledOut(true);
  { // Overwrite the history from Waveform
    char path[MAXPATHLEN];
    char* result = getcwd(path, MAXPATHLEN);
    if(!result) {
      cerr << "\n" << __FILE__ << ":" << __LINE__ << ": getcwd error." << endl;
      throw(GWFrames_FailedSystemCall);
    }
    string pwd = path;
    char host[MAXHOSTNAMELEN];
    gethostname(host, MAXHOSTNAMELEN);
    string hostname = host;
    time_t rawtime;
    time ( &rawtime );
    string date = asctime ( localtime ( &rawtime ) );
    history.str("");
    history.clear();
    history << "### Code revision (`git rev-parse HEAD` or arXiv version) = " << CodeRevision << std::endl
            << "### pwd = " << pwd << std::endl
            << "### hostname = " << hostname << std::endl
            << "### date = " << date // comes with a newline
            << "### PNWaveform(); // empty constructor" << std::endl;
  }
}

/// Copy constructor
GWFrames::PNWaveform::PNWaveform(const PNWaveform& a) :
  Waveform(a), mchi1(a.mchi1), mchi2(a.mchi2), mOmega_orb(a.mOmega_orb), mOmega_prec(a.mOmega_prec), mL(a.mL), mPhi_orb(a.mPhi_orb)
{
  /// Simply copies all fields in the input object to the constructed
  /// object, including history
  history.seekp(0, std::ios_base::end);
}


/// Constructor of PN waveform from parameters
GWFrames::PNWaveform::PNWaveform(const std::string& Approximant, const double delta,
                                 const std::vector<double>& chi1_i, const std::vector<double>& chi2_i,
                                 const double Omega_orb_i, double Omega_orb_0,
                                 const Quaternions::Quaternion& R_frame_i, const unsigned int MinStepsPerOrbit,
                                 const double PNWaveformModeOrder, const double PNOrbitalEvolutionOrder) :
  Waveform(), mchi1(0), mchi2(0), mOmega_orb(0), mOmega_prec(0), mL(0), mPhi_orb(0)
{
  /// See GWFrames/Code/SWIG/Extensions.py for the docstring for this object

  const double v_i = std::pow(Omega_orb_i, 1./3.);
  const double m1 = (1.0+delta)/2.0;
  const double m2 = (1.0-delta)/2.0;
  const double v_0 = ( Omega_orb_0<=0.0 ? v_i : std::pow(Omega_orb_0, 1./3.) );

  SetFrameType(GWFrames::Coorbital);
  SetDataType(GWFrames::h);
  SetRIsScaledOut(true);
  SetMIsScaledOut(true);

  { // Overwrite the history from Waveform
    char path[MAXPATHLEN];
    char* result = getcwd(path, MAXPATHLEN);
    if(!result) {
      cerr << "\n" << __FILE__ << ":" << __LINE__ << ": getcwd error." << endl;
      throw(GWFrames_FailedSystemCall);
    }
    string pwd = path;
    char host[MAXHOSTNAMELEN];
    gethostname(host, MAXHOSTNAMELEN);
    string hostname = host;
    time_t rawtime;
    time ( &rawtime );
    string date = asctime ( localtime ( &rawtime ) );
    history.str("");
    history.clear();
    history << "# Code revision (`git rev-parse HEAD` or arXiv version) = " << CodeRevision << std::endl
            << "# pwd = " << pwd << std::endl
            << "# hostname = " << hostname << std::endl
            << "# date = " << date // comes with a newline
            << "W = PNWaveform(" << Approximant << ", " << delta << ", " << VectorStringForm(chi1_i) << ", " << VectorStringForm(chi2_i)
            << ", " << Omega_orb_i << ", " << Omega_orb_0 << ", " << R_frame_i << ", " << MinStepsPerOrbit
            << ", " << PNWaveformModeOrder << ", " << PNOrbitalEvolutionOrder << ");" << std::endl;
  }

  vector<double> v;

  PostNewtonian::EvolvePN_Q(Approximant, PNOrbitalEvolutionOrder, v_0, v_i, m1, m2, chi1_i, chi2_i, R_frame_i,
                            t, v, mchi1, mchi2, frame, mPhi_orb, mL,
                            MinStepsPerOrbit);

  mOmega_orb = GWFrames::pow(v,3)*PostNewtonian::ellHat(frame);
  mOmega_prec = Quaternions::vec(Quaternions::FrameAngularVelocity(frame, t)) - mOmega_orb;

  // Set up the (ell,m) data
  // We need (2*ell+1) coefficients for each value of ell from 2 up to
  // ellMax_PNWaveforms.  That's a total of
  //   >>> from sympy import summation, symbols
  //   >>> ell, ellMax_PNWaveforms = symbols('ell ellMax_PNWaveforms', integer=True)
  //   >>> summation(2*ell+1, (ell, 2, ellMax_PNWaveforms))
  //   ellMax_PNWaveforms**2 + 2*ellMax_PNWaveforms - 3
  // The reverse process of finding the index of element (ell,m) is
  // done by taking the element of the array with index
  //   >>> summation(2*ell+1, (ell, 2, ell-1)) + ell + m
  //   ell**2 + ell + m - 4
  lm.resize(PNWaveforms_ellMax*(PNWaveforms_ellMax+2)-3, vector<int>(2,0));
  {
    unsigned int i=0;
    for(int ell=2; ell<=PNWaveforms_ellMax; ++ell) {
      for(int m=-ell; m<=ell; ++m) {
        lm[i][0] = ell;
        lm[i][1] = m;
        ++i;
      }
    }
  }

  // Evaluate the waveform data itself, noting that we always use the
  // frame in standard position (BHs on the x axis, with angular
  // velocity along the positive z axis).  This can then be
  // transformed to a stationary frame, using the stored 'frame' data.
  //
  // The frame rotor is the rotor necessary to take a vector in the
  // co-orbital onto its equivalent in the inertial frame.  The chi
  // vectors are given in the inertial frame, but are needed in the
  // co-orbital frame.  Thus, we rotate with the inverse (conjugate)
  // rotor in the following.
  data = MatrixC(PostNewtonian::WaveformModes(m1, m2, v,
                                              Quaternions::vec(Quaternions::conjugate(frame)*Quaternions::QuaternionArray(mchi1)*frame),
                                              Quaternions::vec(Quaternions::conjugate(frame)*Quaternions::QuaternionArray(mchi2)*frame),
                                              PNWaveformModeOrder));

} // end PN constructor


/// Total angular velocity of PN binary at an instant of time
std::vector<double> GWFrames::PNWaveform::Omega_tot(const unsigned int iTime) const {
  std::vector<double> Tot(3);
  Tot[0] = mOmega_orb[iTime][0]+mOmega_prec[iTime][0];
  Tot[1] = mOmega_orb[iTime][1]+mOmega_prec[iTime][1];
  Tot[2] = mOmega_orb[iTime][2]+mOmega_prec[iTime][2];
  return Tot;
}

/// Total angular velocity of PN binary at each instant of time
std::vector<std::vector<double> > GWFrames::PNWaveform::Omega_tot() const {
  const double NT = NTimes();
  std::vector<std::vector<double> > Tot(NT, std::vector<double>(3));
  for(unsigned int i_t=0; i_t<NT; ++i_t) {
    Tot[i_t] = Omega_tot(i_t);
  }
  return Tot;
}

/// Vector of magnitudes of Omega_orb at each instant of time
std::vector<double> GWFrames::PNWaveform::Omega_orbMag() const {
  const double NT = NTimes();
  std::vector<double> Mag(NT);
  for(unsigned int i=0; i<NT; ++i) {
    Mag[i] = GWFrames::abs(mOmega_orb[i]);
  }
  return Mag;
}

/// Vector of magnitudes of Omega_prec at each instant of time
std::vector<double> GWFrames::PNWaveform::Omega_precMag() const {
  const double NT = NTimes();
  std::vector<double> Mag(NT);
  for(unsigned int i=0; i<NT; ++i) {
    Mag[i] = GWFrames::abs(mOmega_prec[i]);
  }
  return Mag;
}

/// Vector of magnitudes of Omega_tot at each instant of time
std::vector<double> GWFrames::PNWaveform::Omega_totMag() const {
  const double NT = NTimes();
  std::vector<double> Mag(NT);
  for(unsigned int i=0; i<NT; ++i) {
    Mag[i] = GWFrames::abs(Omega_tot(i));
  }
  return Mag;
}

/// Vector of magnitudes of angular momentum L at each instant of time
std::vector<double> GWFrames::PNWaveform::LMag() const {
  const double NT = NTimes();
  std::vector<double> Mag(NT);
  for(unsigned int i=0; i<NT; ++i) {
    Mag[i] = GWFrames::abs(mL[i]);
  }
  return Mag;
}
