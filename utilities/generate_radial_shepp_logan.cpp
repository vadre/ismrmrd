/*
 * generate_radial_shepp_logan.cpp
 * 
 * Copyright 2014 Ghislain Antony Vaillant <ghisvail@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#include <iostream>
#include <valarray>
#include <cmath>
#include <boost/program_options.hpp>
#include "ismrmrd_phantom.h"
#include "ismrmrd_hdf5.h"
#include "ismrmrd_nfft.h"
#include "ismrmrd.hxx"

using namespace ISMRMRD;
namespace po = boost::program_options;

int main(int argc, char** argv)
{
  unsigned int ndim = 2;      // 2D trajectory
  unsigned int readout;       // Number of readout samples
  unsigned int profiles;      // Number of angular profiles
	unsigned int ncoils;        // Number of coils
  unsigned int ros;           // Oversampling factor in the readout direction
	unsigned int repetitions;   // Number of repetitions
	unsigned int acc_factor;    // Acceleration factor
  std::string outfile;
  std::string dataset;


  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("readout,m", po::value<unsigned int>(&readout)->default_value(256), "Readout size")
    ("profiles,p", po::value<unsigned int>(&profiles)->default_value(256), "Number of angular profiles")
    ("coils,c", po::value<unsigned int>(&ncoils)->default_value(8), "Number of Coils")
    ("oversampling,O", po::value<unsigned int>(&ros)->default_value(2), "Readout oversampling")
    //("repetitions,r", po::value<unsigned int>(&repetitions)->default_value(1), "Number of repetitions")
    //("acceleration,a", po::value<unsigned int>(&acc_factor)->default_value(1), "Acceleration factor")
    ("output,o", po::value<std::string>(&outfile)->default_value("testdata.h5"), "Output file name")
    ("dataset,d", po::value<std::string>(&dataset)->default_value("dataset"), "Output dataset name")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
      std::cout << desc << "\n";
      return 1;
  }

  /* K-space dimensions */
  unsigned int os_readout = readout * ros;
  std::vector<unsigned int> ksdims;
  ksdims.push_back(os_readout);
  ksdims.push_back(profiles);
    
  /* Acquired image dimensions */
  std::vector<unsigned int> esdims;
  esdims.push_back(os_readout);
  esdims.push_back(os_readout);

  /* Reconstructed image dimensions */
  std::vector<unsigned int> rsdims;
  rsdims.push_back(readout);
  rsdims.push_back(readout);

  /* Multi-coil dimensions */
  std::vector<unsigned int> ksdims_mcoil = ksdims;
  ksdims_mcoil.push_back(ncoils);
  std::vector<unsigned int> esdims_mcoil = esdims;
  esdims_mcoil.push_back(ncoils);

  std::cout << "Generating radial Shepp-Logan phantom" << std::endl;

  std::cout << "DEBUG: Generating phantom data" << std::endl;
  unsigned int matrix_size = readout;
	boost::shared_ptr<NDArrayContainer<std::complex<float> > > phantom = 
      shepp_logan_phantom(rsdims[0]);
	boost::shared_ptr<NDArrayContainer<std::complex<float> > > coils = 
      generate_birdcage_sensititivies(rsdims[0], ncoils, 1.5);  
    
  NDArrayContainer<std::complex<float> > coil_images(esdims_mcoil);
  coil_images.data_ = std::complex<float>(0.0, 0.0);
  size_t iindex, oindex, cindex;
  for (unsigned int c = 0; c < ncoils; c++) {
    for (unsigned int y = 0; y < rsdims[1]; y++) {
      for (unsigned int x = 0; x < rsdims[0]; x++) {
        oindex = c * esdims[1] * esdims[0] + 
            (y + ((esdims[1] - rsdims[1]) >> 1)) * esdims[0] +
            x + ((esdims[0] - rsdims[0]) >> 1);
        iindex = y * rsdims[0] + x;
        cindex = c * rsdims[1] * rsdims[0] + iindex;
        coil_images.data_[oindex] = phantom->data_[iindex] * coils->data_[cindex];
      }
    }
  }

  /* compute fully-sampled trajectory */
  std::cout << "DEBUG: Generating trajectory" << std::endl;
  std::valarray<float> kr(ksdims[0]);
  for (unsigned int ife = 0; ife < kr.size(); ife++)
    kr[ife] = (float)ife * (1.0f / kr.size()) - 0.5f;

  std::valarray<float> full_traj(ksdims[0]*ksdims[1]*ndim);
  std::slice ix_kx, ix_ky;
  float angle_ipe, cos_angle, sin_angle;
  for (unsigned int ipe = 0; ipe < ksdims[1]; ipe++) {
    angle_ipe = (float)ipe * (M_PI / profiles);
    cos_angle = cos(angle_ipe);
    sin_angle = sin(angle_ipe);
    /* lots of redundant FLOPS here, left for clarity */
    ix_kx = std::slice(ipe*ksdims[0]*ndim, ksdims[0], ndim);
    ix_ky = std::slice(1+ipe*ksdims[0]*ndim, ksdims[0], ndim);    
    full_traj[ix_kx] = kr * cos_angle;
    full_traj[ix_ky] = kr * sin_angle;
  }

  /* instantiate the NFFT plan */
  std::cout << "DEBUG: instantiate plan" << std::endl;
  NFFT2 plan = NFFT2(esdims[0], esdims[1], ksdims[0]*ksdims[1]);
  plan.precompute(full_traj);

  /* itok */
  std::cout << "DEBUG: generating k-space data" << std::endl;
  NDArrayContainer<std::complex<float> > coil_ksdata(ksdims_mcoil);  
  std::valarray<std::complex<float> > imdata_ico(esdims[0]*esdims[1]);
  std::valarray<std::complex<float> > ksdata_ico(ksdims[0]*ksdims[1]);
  std::slice ix_ico_ispace, ix_ico_kspace;
  for (unsigned int ico = 0; ico < ncoils; ico++) {
    std::cout << "DEBUG: coil #" << ico << std::endl;
    ix_ico_ispace = std::slice(ico*esdims[0]*esdims[1],
        esdims[0]*esdims[1], 1);
    std::valarray<std::complex<float> > imdata_ico =
        coil_images.data_[ix_ico_ispace];
    ix_ico_kspace = std::slice(ico*ksdims[0]*ksdims[1],
        ksdims[0]*ksdims[1], 1);
    std::valarray<std::complex<float> > ksdata_ico = 
        coil_ksdata.data_[ix_ico_kspace];
    plan.trafo(imdata_ico, ksdata_ico);
    coil_ksdata.data_[ix_ico_kspace] = ksdata_ico;
  }
  
  // create dataset
  IsmrmrdDataset d(outfile.c_str(), dataset.c_str());
  Acquisition acq;
	acq.setActiveChannels(ncoils);
	acq.setAvailableChannels(ncoils);
	acq.setNumberOfSamples(os_readout);
	acq.setCenterSample(os_readout>>1);
  acq.setSampleTimeUs(5.0);  // arbitrary

  /* record acquisiton of each phase encoding line individually */
  std::cout << "DEBUG: Looping through each encoding line" << std::endl;
  for (unsigned int ipe = 0; ipe < profiles; ipe++) {
    std::cout << "DEBUG: processing PE line #" << ipe << std::endl;
    
    /* reset acquisition flags */
    acq.setFlags(0);

    /* set relevant acquisition flags:
     * ACQ_FIRST_IN_SLICE for first profile
     * ACQ_LAST_IN_SLICE for last profile
     */
    if (ipe == 0)
      acq.setFlag(ISMRMRD::FlagBit(ISMRMRD::ACQ_FIRST_IN_SLICE));
    if (ipe == profiles-1)
      acq.setFlag(ISMRMRD::FlagBit(ISMRMRD::ACQ_LAST_IN_SLICE));

    /* set encoding index */
    acq.getIdx().kspace_encode_step_1 = ipe;

    /* set data:
     * Copy the relevant portion of the fully-sampled data to the current 
     * acquisition instance. The coil data are concatenated into one vector.
     */
    std::cout << "DEBUG: copy data" << std::endl;
    std::valarray<float> data(ncoils*os_readout*2);
    for (unsigned int ico = 0; ico < ncoils; ico++) {
      memcpy(&data[ico*os_readout*2],
             &coil_ksdata.data_[(ico*profiles+ipe)*os_readout],
             sizeof(float)*os_readout*2);
    }
    acq.setData(data);
 
    /* set trajectory:
     * here, just copy the relevant portion of the fully-sampled trajectory
     */
    std::cout << "DEBUG: copy trajectory" << std::endl;
    std::valarray<float> traj(os_readout*ndim);
    for (unsigned int ife = 0; ife < os_readout; ife++) {
      traj[ife*ndim] = full_traj[(ipe*os_readout+ife)*ndim];
      traj[1+ife*ndim] = full_traj[1+(ipe*os_readout+ife)*ndim];
    }
    acq.setTrajectoryDimensions(2);
    acq.setTraj(traj);
    
    /* add current acquisition to dataset */
    std::cout << "DEBUG: append acquisition" << std::endl;
    d.appendAcquisition(&acq);
  }

  std::cout << "DEBUG: generating XML header" << std::endl;
  /* create XML header */
  ISMRMRD::experimentalConditionsType exp(63500000); //~1.5T
  ISMRMRD::acquisitionSystemInformationType sys;
  sys.institutionName("ISMRM Synthetic Imaging Lab");
  sys.receiverChannels(ncoils);

  ISMRMRD::ismrmrdHeader h(exp);
  h.acquisitionSystemInformation(sys);

  /* create an encoding section */
  ISMRMRD::encodingSpaceType es(
      ISMRMRD::matrixSize(esdims[0], esdims[1], 1),
      ISMRMRD::fieldOfView_mm(ros*300, ros*300, 6)
      );
  ISMRMRD::encodingSpaceType rs(
      ISMRMRD::matrixSize(rsdims[0], rsdims[1], 1),
      ISMRMRD::fieldOfView_mm(300, 300, 6)
      );
  ISMRMRD::encodingLimitsType el;
  el.kspace_encoding_step_1(ISMRMRD::limitType(0, ksdims[1]-1, 0));
  ISMRMRD::encoding e(es, rs, el, ISMRMRD::trajectoryType::radial);

  /* add the encoding section to the header */
  h.encoding().push_back(e);

  /* serialize the header */
  xml_schema::namespace_infomap map;
  map[""].name = "http://www.ismrm.org/ISMRMRD";
  map[""].schema = "ismrmrd.xsd";
  std::stringstream str;
  ISMRMRD::ismrmrdHeader_(str, h, map);
  std::string xml_header = str.str();

  /* write the header to the data file. */
  d.writeHeader(xml_header);

  return 0;
}





